<#
.SYNOPSIS
Safely deploys and tests AuroraAZ in the isolated AuroraAZLab installation.

.DESCRIPTION
This harness only transfers files below Hdd1:\AuroraAZLab. It never writes,
renames, or removes files below Hdd1:\Aurora. Deployment is transactional:
the candidate is uploaded under a unique staged name, downloaded again, and
SHA-256 verified before it is renamed to Plugins\NetDbgDll.xex.

FTP credentials are obtained from locally saved WinSCP sessions. NOVA
credentials are read from AURORAAZ_NOVA_USERNAME and
AURORAAZ_NOVA_PASSWORD unless explicitly supplied. No credential is written
to a script, manifest, transcript, or command line by this harness.

The default action is Plan and performs no network operation. Test performs a
complete deploy/launch/evidence/disable/restore cycle. Its finally block
attempts to disable the lab plugin and restores the production Aurora title
through NOVA. SITE REBOOT is an explicit opt-in fallback because some consoles
power off instead of restarting when that FTP command is issued.

.EXAMPLE
.\scripts\invoke-auroraaz-lab.ps1 -Action Plan `
    -Artifact .\build\ci\artifact\AuroraAZ.xex

.EXAMPLE
.\scripts\invoke-auroraaz-lab.ps1 -Action Test `
    -Artifact .\build\ci\artifact\AuroraAZ.xex

.EXAMPLE
.\scripts\invoke-auroraaz-lab.ps1 -Action Deploy `
    -Artifact .\build\ci\artifact\AuroraAZ.xex
.\scripts\invoke-auroraaz-lab.ps1 -Action Launch
#>
[CmdletBinding()]
param(
    [ValidateSet('Plan', 'Deploy', 'Launch', 'Collect', 'Disable', 'Restore', 'Test')]
    [string]$Action = 'Plan',

    [string]$Artifact,

    [string]$NovaBaseUri = $(
        if ([string]::IsNullOrWhiteSpace($env:AURORAAZ_NOVA_URL)) {
            'http://192.168.1.103:9999'
        }
        else {
            $env:AURORAAZ_NOVA_URL
        }
    ),

    [string]$NovaUsername = $env:AURORAAZ_NOVA_USERNAME,

    [string]$NovaPassword = $env:AURORAAZ_NOVA_PASSWORD,

    [string]$ProductionFtpSession = 'Xbox 360',

    [string]$LabFtpSession = 'AuroraAZ Lab',

    [string]$WinScpPath,

    [string]$EvidenceRoot,

    [ValidateRange(0, 600)]
    [int]$TestDurationSec = 10,

    [ValidateRange(5, 180)]
    [int]$LaunchTimeoutSec = 45,

    [ValidateRange(15, 300)]
    [int]$RestoreTimeoutSec = 90,

    [ValidateRange(5, 120)]
    [int]$NovaTimeoutSec = 30,

    [ValidateCount(0, 20)]
    [string[]]$EvidenceName = @(
        'AuroraAZ-M2a-input-A.bin',
        'AuroraAZ-M2a-input-B.bin',
        'AuroraAZ-M2a.bin',
        'AuroraAZ-M1.bin',
        'AuroraAZ-M1-worker.bin',
        'debug.log'
    ),

    [switch]$SkipScreenshot,

    [switch]$AllowSiteRebootFallback,

    [switch]$DryRun
)

Set-StrictMode -Version 2.0
$ErrorActionPreference = 'Stop'

$script:RepositoryRoot = Split-Path -Parent $PSScriptRoot
$script:LabRoot = '/Hdd1/AuroraAZLab'
$script:PluginDirectory = "$($script:LabRoot)/Plugins"
$script:LogDirectory = "$($script:LabRoot)/Data/Logs"
$script:ActivePluginPath = "$($script:PluginDirectory)/NetDbgDll.xex"
$script:LabDevicePath = '\Device\Harddisk0\Partition1\AuroraAZLab'
$script:ProductionDevicePath = '\Device\Harddisk0\Partition1\Aurora'
$script:LabTitleSuffix = '\AuroraAZLab\Aurora.xex'
$script:ProductionTitleSuffix = '\Aurora\Aurora.xex'
$script:RunId = '{0}-{1}' -f `
    [DateTime]::UtcNow.ToString('yyyyMMdd-HHmmssfff'), `
    ([Guid]::NewGuid().ToString('N').Substring(0, 8))
$script:StageNonce = [Guid]::NewGuid().ToString('N').Substring(0, 8)
$script:DisableNonce = [Guid]::NewGuid().ToString('N').Substring(0, 6)

if ([string]::IsNullOrWhiteSpace($EvidenceRoot)) {
    $EvidenceRoot = Join-Path $script:RepositoryRoot 'build\lab-harness'
}
$script:EvidenceDirectory = Join-Path `
    ([System.IO.Path]::GetFullPath($EvidenceRoot)) $script:RunId

function Write-Step {
    param([Parameter(Mandatory = $true)][string]$Message)
    Write-Host ('[AuroraAZ lab] {0}' -f $Message)
}

function Assert-SafeSessionName {
    param(
        [Parameter(Mandatory = $true)][string]$Name,
        [Parameter(Mandatory = $true)][string]$ParameterName
    )

    if ([string]::IsNullOrWhiteSpace($Name) -or
        $Name -notmatch '\A[A-Za-z0-9 _.()\-]+\z') {
        throw "$ParameterName contains unsupported characters. Use a locally saved WinSCP session name."
    }
}

function Assert-SafePluginPath {
    param([Parameter(Mandatory = $true)][string]$RemotePath)

    $normal = $RemotePath.Replace('\', '/')
    $activePattern = '\A/Hdd1/AuroraAZLab/Plugins/NetDbgDll\.xex\z'
    $inactivePattern = ('\A/Hdd1/AuroraAZLab/Plugins/NetDbgDll\.xex\.(?:' +
        'staged-[a-f0-9]{12}-[a-f0-9]{8}|' +
        'disabled-[a-f0-9]{12}-[a-f0-9]{6})\z')
    $remoteName = $normal.Substring($normal.LastIndexOf('/') + 1)
    if (($normal -notmatch $activePattern -and
            $normal -notmatch $inactivePattern) -or
        $normal.Contains('..') -or
        $remoteName.Length -gt 42) {
        throw "Refusing FTP plugin operation outside the fixed AuroraAZLab target: $RemotePath"
    }
}

function Assert-SafeLogPath {
    param([Parameter(Mandatory = $true)][string]$RemotePath)

    $normal = $RemotePath.Replace('\', '/')
    $pattern = '\A/Hdd1/AuroraAZLab/Data/Logs/[A-Za-z0-9][A-Za-z0-9._-]*\z'
    $remoteName = $normal.Substring($normal.LastIndexOf('/') + 1)
    if ($normal -notmatch $pattern -or $normal.Contains('..') -or
        $remoteName.Length -gt 42) {
        throw "Refusing evidence download outside AuroraAZLab Data/Logs: $RemotePath"
    }
}

function Assert-SafeEvidenceName {
    param([Parameter(Mandatory = $true)][string]$Name)

    if ([string]::IsNullOrWhiteSpace($Name) -or
        $Name.Length -gt 42 -or $Name.Contains('..') -or
        $Name.EndsWith('.', [StringComparison]::Ordinal) -or
        $Name -notmatch '\A[A-Za-z0-9][A-Za-z0-9._-]*\z') {
        throw "EvidenceName must be a simple filename, not a path: $Name"
    }
}

function Quote-WinScpValue {
    param([Parameter(Mandatory = $true)][string]$Value)
    return '"' + $Value.Replace('"', '""') + '"'
}

function Resolve-WinScpExecutable {
    if (-not [string]::IsNullOrWhiteSpace($WinScpPath)) {
        $candidate = [System.IO.Path]::GetFullPath($WinScpPath)
        if (-not (Test-Path -LiteralPath $candidate -PathType Leaf)) {
            throw "WinSCP console executable was not found: $candidate"
        }
        return $candidate
    }

    $candidates = @(
        (Join-Path ${env:ProgramFiles(x86)} 'WinSCP\WinSCP.com'),
        (Join-Path $env:ProgramFiles 'WinSCP\WinSCP.com')
    ) | Where-Object { -not [string]::IsNullOrWhiteSpace($_) }

    foreach ($candidate in $candidates) {
        if (Test-Path -LiteralPath $candidate -PathType Leaf) {
            return [System.IO.Path]::GetFullPath($candidate)
        }
    }

    $command = Get-Command WinSCP.com -ErrorAction SilentlyContinue
    if ($null -ne $command) {
        return $command.Source
    }

    throw 'WinSCP.com was not found. Install WinSCP or pass -WinScpPath.'
}

function Invoke-WinScpCommands {
    param(
        [Parameter(Mandatory = $true)][string]$SessionName,
        [Parameter(Mandatory = $true)][string[]]$Command,
        [switch]$AllowFailure
    )

    Assert-SafeSessionName -Name $SessionName -ParameterName 'FTP session name'
    $scriptPath = [System.IO.Path]::GetTempFileName()
    try {
        $lines = @(
            'option batch abort',
            'option confirm off',
            'option transfer binary',
            ('open {0}' -f (Quote-WinScpValue $SessionName))
        ) + $Command + @('exit')
        [System.IO.File]::WriteAllLines(
            $scriptPath,
            $lines,
            [System.Text.UTF8Encoding]::new($false)
        )

        $rawOutput = & $script:WinScpExecutable `
            "/script=$scriptPath" `
            '/loglevel=0' `
            '/nointeractiveinput' 2>&1
        $exitCode = $LASTEXITCODE
        $outputText = ($rawOutput | Out-String)

        if ($exitCode -ne 0 -and -not $AllowFailure.IsPresent) {
            # WinSCP output can include authentication identity. Keep it in
            # memory for parsing, but never echo it or place it in an error.
            throw "WinSCP operation failed with exit code $exitCode. Verify the saved session and console state."
        }

        return [pscustomobject]@{
            ExitCode = $exitCode
            Output = $outputText
        }
    }
    finally {
        if (Test-Path -LiteralPath $scriptPath) {
            [System.IO.File]::Delete($scriptPath)
        }
    }
}

function Invoke-WinScpRetry {
    param(
        [Parameter(Mandatory = $true)][string]$SessionName,
        [Parameter(Mandatory = $true)][string[]]$Command,
        [ValidateRange(1, 8)][int]$Attempts = 8
    )

    for ($attempt = 1; $attempt -le $Attempts; $attempt++) {
        $result = Invoke-WinScpCommands `
            -SessionName $SessionName `
            -Command $Command `
            -AllowFailure
        if ($result.ExitCode -eq 0) {
            return $result
        }
        if ($attempt -lt $Attempts) {
            Start-Sleep -Seconds 4
        }
    }

    throw "WinSCP operation failed after $Attempts attempts. Verify the saved session and console state."
}

function Get-RemoteDirectoryListing {
    param(
        [Parameter(Mandatory = $true)][string]$SessionName,
        [Parameter(Mandatory = $true)][string]$RemoteDirectory
    )

    if ($RemoteDirectory -notin @($script:PluginDirectory, $script:LogDirectory)) {
        throw "Refusing to list a directory outside the AuroraAZLab allowlist: $RemoteDirectory"
    }

    # Aurora's FtpDll ignores the path argument to LIST. Enter the strictly
    # allowlisted directory first and then issue a bare listing.
    $result = Invoke-WinScpRetry -SessionName $SessionName -Command @(
        "cd $RemoteDirectory",
        'ls'
    )
    return [string]$result.Output
}

function Test-RemoteNameInListing {
    param(
        [Parameter(Mandatory = $true)][string]$Listing,
        [Parameter(Mandatory = $true)][string]$Name
    )

    Assert-SafeEvidenceName -Name $Name
    # WinSCP returns CRLF text. In .NET multiline mode, `$` matches before LF
    # but leaves CR as a literal character, so include it in trailing space.
    $pattern = '(?im)(?:^|[ \t])' + [regex]::Escape($Name) + '[ \t\r]*$'
    return [regex]::IsMatch($Listing, $pattern)
}

function Invoke-VerifiedPluginRename {
    param(
        [Parameter(Mandatory = $true)][string]$SessionName,
        [Parameter(Mandatory = $true)][string]$SourceName,
        [Parameter(Mandatory = $true)][string]$DestinationName,
        [Parameter(Mandatory = $true)][string]$OperationLabel
    )

    Assert-SafeEvidenceName -Name $SourceName
    Assert-SafeEvidenceName -Name $DestinationName
    if ($SourceName.Equals($DestinationName, [StringComparison]::OrdinalIgnoreCase)) {
        throw "$OperationLabel source and destination must differ."
    }

    $sourcePath = "$($script:PluginDirectory)/$SourceName"
    $destinationPath = "$($script:PluginDirectory)/$DestinationName"
    Assert-SafePluginPath -RemotePath $sourcePath
    Assert-SafePluginPath -RemotePath $destinationPath

    # Establish both preconditions immediately before the one-shot rename.
    # In particular, never let FTP server overwrite semantics decide what
    # happens to a colliding destination.
    $before = Get-RemoteDirectoryListing `
        -SessionName $SessionName `
        -RemoteDirectory $script:PluginDirectory
    if (-not (Test-RemoteNameInListing -Listing $before -Name $SourceName)) {
        throw "$OperationLabel source is absent; no rename was attempted."
    }
    if (Test-RemoteNameInListing -Listing $before -Name $DestinationName) {
        throw "$OperationLabel destination already exists; refusing to overwrite it."
    }

    # Never retry a state-changing rename. A dropped control connection can
    # mean the server completed it without returning the success reply.
    $rename = Invoke-WinScpCommands `
        -SessionName $SessionName `
        -Command @(
            "cd $($script:PluginDirectory)",
            ('mv {0} {1}' -f `
                (Quote-WinScpValue $SourceName), `
                (Quote-WinScpValue $DestinationName))
        ) `
        -AllowFailure

    # The observed directory state, not WinSCP's exit code, is authoritative.
    $after = Get-RemoteDirectoryListing `
        -SessionName $SessionName `
        -RemoteDirectory $script:PluginDirectory
    $sourceExists = Test-RemoteNameInListing -Listing $after -Name $SourceName
    $destinationExists = Test-RemoteNameInListing `
        -Listing $after `
        -Name $DestinationName

    if (-not $sourceExists -and $destinationExists) {
        return [pscustomobject]@{
            Source = $sourcePath
            Destination = $destinationPath
            WinScpExitCode = $rename.ExitCode
            Verified = $true
        }
    }
    if ($sourceExists -and -not $destinationExists) {
        throw "$OperationLabel did not take effect; the source remains intact."
    }

    throw ("$OperationLabel left an ambiguous remote state. Do not launch " +
        'AuroraAZLab until the exact plugin directory has been inspected.')
}

function Get-ArtifactInfo {
    param([Parameter(Mandatory = $true)][string]$Path)

    if ([string]::IsNullOrWhiteSpace($Path)) {
        throw 'This action requires -Artifact.'
    }

    $fullPath = [System.IO.Path]::GetFullPath($Path)
    if (-not (Test-Path -LiteralPath $fullPath -PathType Leaf)) {
        throw "Artifact was not found: $fullPath"
    }

    if (-not [System.IO.Path]::GetFileName($fullPath).Equals(
        'AuroraAZ.xex', [StringComparison]::OrdinalIgnoreCase)) {
        throw 'The deployment artifact must be named AuroraAZ.xex.'
    }

    $item = Get-Item -LiteralPath $fullPath
    if ($item.Length -le 0 -or $item.Length -gt 16MB) {
        throw "AuroraAZ.xex has an implausible size: $($item.Length) bytes."
    }

    $hash = (Get-FileHash -LiteralPath $fullPath -Algorithm SHA256).Hash.ToUpperInvariant()
    return [pscustomobject]@{
        Path = $fullPath
        Bytes = $item.Length
        Sha256 = $hash
        ShortHash = $hash.Substring(0, 12).ToLowerInvariant()
    }
}

function New-NovaClient {
    Add-Type -AssemblyName System.Net.Http

    $novaUri = $null
    if (-not [uri]::TryCreate(
        $NovaBaseUri.TrimEnd('/'),
        [System.UriKind]::Absolute,
        [ref]$novaUri)) {
        throw "NovaBaseUri is not a valid absolute URI: $NovaBaseUri"
    }
    if ($novaUri.Scheme -notin @('http', 'https') -or
        [string]::IsNullOrWhiteSpace($novaUri.Host)) {
        throw 'NovaBaseUri must be an absolute HTTP or HTTPS URI.'
    }

    $hasUsername = -not [string]::IsNullOrWhiteSpace($NovaUsername)
    $hasPassword = -not [string]::IsNullOrWhiteSpace($NovaPassword)
    if ($hasUsername -xor $hasPassword) {
        throw 'Provide both NOVA username and password, or neither.'
    }

    $handler = [System.Net.Http.HttpClientHandler]::new()
    $handler.AllowAutoRedirect = $false
    $client = [System.Net.Http.HttpClient]::new($handler)
    $client.Timeout = [TimeSpan]::FromSeconds($NovaTimeoutSec)
    $client.DefaultRequestHeaders.Accept.ParseAdd('application/json')

    try {
        if ($hasUsername) {
            $pairs = [System.Collections.Generic.List[
                System.Collections.Generic.KeyValuePair[string, string]
            ]]::new()
            $pairs.Add(
                [System.Collections.Generic.KeyValuePair[string, string]]::new(
                    'username', $NovaUsername
                )
            )
            $pairs.Add(
                [System.Collections.Generic.KeyValuePair[string, string]]::new(
                    'password', $NovaPassword
                )
            )
            $form = [System.Net.Http.FormUrlEncodedContent]::new($pairs)
            try {
                $response = $client.PostAsync(
                    [uri]::new($novaUri, '/authenticate'), $form
                ).GetAwaiter().GetResult()
                try {
                    $body = $response.Content.ReadAsStringAsync().GetAwaiter().GetResult()
                    if (-not $response.IsSuccessStatusCode) {
                        throw "NOVA authentication failed with HTTP $([int]$response.StatusCode)."
                    }
                    try {
                        $authentication = $body | ConvertFrom-Json
                    }
                    catch {
                        throw 'NOVA authentication returned invalid JSON.'
                    }
                }
                finally {
                    $response.Dispose()
                }
            }
            finally {
                $form.Dispose()
            }

            if ([string]::IsNullOrWhiteSpace([string]$authentication.token)) {
                throw 'NOVA authentication succeeded without returning a JWT.'
            }
            $client.DefaultRequestHeaders.Authorization =
                [System.Net.Http.Headers.AuthenticationHeaderValue]::new(
                    'Bearer', [string]$authentication.token
                )
        }

        return [pscustomobject]@{
            Client = $client
            Handler = $handler
            BaseUri = $novaUri
        }
    }
    catch {
        $client.Dispose()
        $handler.Dispose()
        throw
    }
}

function Invoke-NovaJsonGet {
    param(
        [Parameter(Mandatory = $true)]$Nova,
        [Parameter(Mandatory = $true)][string]$RelativePath
    )

    $response = $Nova.Client.GetAsync(
        [uri]::new($Nova.BaseUri, $RelativePath)
    ).GetAwaiter().GetResult()
    try {
        $body = $response.Content.ReadAsStringAsync().GetAwaiter().GetResult()
        if (-not $response.IsSuccessStatusCode) {
            throw "NOVA GET $RelativePath failed with HTTP $([int]$response.StatusCode)."
        }
        try {
            return $body | ConvertFrom-Json
        }
        catch {
            throw "NOVA GET $RelativePath returned invalid JSON."
        }
    }
    finally {
        $response.Dispose()
    }
}

function Get-NovaTitle {
    $nova = New-NovaClient
    try {
        return Invoke-NovaJsonGet -Nova $nova -RelativePath '/title'
    }
    finally {
        $nova.Client.Dispose()
        $nova.Handler.Dispose()
    }
}

function Assert-NovaTitle {
    param([Parameter(Mandatory = $true)][string]$ExpectedSuffix)

    $title = Get-NovaTitle
    $path = [string]$title.path
    if ([string]::IsNullOrWhiteSpace($path) -or
        -not $path.EndsWith($ExpectedSuffix, [StringComparison]::OrdinalIgnoreCase)) {
        throw "Unexpected running title. Expected *$ExpectedSuffix."
    }
    return $title
}

function Wait-NovaTitle {
    param(
        [Parameter(Mandatory = $true)][string]$ExpectedSuffix,
        [Parameter(Mandatory = $true)][int]$TimeoutSec
    )

    $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSec)
    do {
        try {
            $title = Get-NovaTitle
            $path = [string]$title.path
            if (-not [string]::IsNullOrWhiteSpace($path) -and
                $path.EndsWith($ExpectedSuffix, [StringComparison]::OrdinalIgnoreCase)) {
                return $title
            }
        }
        catch {
            # Title transitions can briefly make NOVA unavailable.
        }
        Start-Sleep -Seconds 2
    } while ([DateTime]::UtcNow -lt $deadline)

    throw "NOVA did not report *$ExpectedSuffix within $TimeoutSec seconds."
}

function Invoke-NovaTitleLaunch {
    param(
        [Parameter(Mandatory = $true)][string]$CurrentTitleSuffix,
        [Parameter(Mandatory = $true)][string]$TargetDevicePath,
        [Parameter(Mandatory = $true)][string]$TargetTitleSuffix,
        [Parameter(Mandatory = $true)][string]$Description,
        [Parameter(Mandatory = $true)][int]$TimeoutSec
    )

    $isLabTarget = (
        $TargetDevicePath -eq $script:LabDevicePath -and
        $TargetTitleSuffix -eq $script:LabTitleSuffix
    )
    $isProductionTarget = (
        $TargetDevicePath -eq $script:ProductionDevicePath -and
        $TargetTitleSuffix -eq $script:ProductionTitleSuffix
    )
    if (-not $isLabTarget -and -not $isProductionTarget) {
        throw 'Refusing a NOVA launch outside the two fixed Aurora title targets.'
    }

    $currentTitle = Get-NovaTitle
    $currentPath = [string]$currentTitle.path
    if (-not [string]::IsNullOrWhiteSpace($currentPath) -and
        $currentPath.EndsWith(
            $TargetTitleSuffix,
            [StringComparison]::OrdinalIgnoreCase)) {
        return $currentTitle
    }
    if ([string]::IsNullOrWhiteSpace($currentPath) -or
        -not $currentPath.EndsWith(
            $CurrentTitleSuffix,
            [StringComparison]::OrdinalIgnoreCase)) {
        throw "Unexpected running title. Expected *$CurrentTitleSuffix before $Description."
    }

    Write-Step $Description

    $nova = New-NovaClient
    try {
        $boundary = '--------------------------{0}' -f `
            [Guid]::NewGuid().ToString('N')
        $multipart =
            [System.Net.Http.MultipartFormDataContent]::new($boundary)
        # The framework quotes the boundary parameter by default. Rev1622's
        # narrow parser rejects that legal form even though curl's unquoted
        # boundary succeeds.
        $boundaryParameter =
            $multipart.Headers.ContentType.Parameters |
                Where-Object Name -eq 'boundary'
        $boundaryParameter.Value = $boundary
        try {
            # NOVA's endpoint rejects StringContent parts because they carry
            # a text/plain Content-Type. curl -F sends untyped form fields;
            # ByteArrayContent reproduces that accepted wire contract.
            $execPart = [System.Net.Http.ByteArrayContent]::new(
                [System.Text.Encoding]::UTF8.GetBytes('Aurora.xex'))
            $pathPart = [System.Net.Http.ByteArrayContent]::new(
                [System.Text.Encoding]::UTF8.GetBytes($TargetDevicePath))
            $typePart = [System.Net.Http.ByteArrayContent]::new(
                [System.Text.Encoding]::UTF8.GetBytes('0'))
            # HttpClient's Add(content, name) overload emits an unquoted
            # Content-Disposition name under current PowerShell/.NET builds.
            # NOVA's Rev1622 multipart parser accepts curl's quoted form only.
            $execDisposition =
                [System.Net.Http.Headers.ContentDispositionHeaderValue]::new(
                    'form-data')
            $execDisposition.Name = '"exec"'
            $execPart.Headers.ContentDisposition = $execDisposition
            $pathDisposition =
                [System.Net.Http.Headers.ContentDispositionHeaderValue]::new(
                    'form-data')
            $pathDisposition.Name = '"path"'
            $pathPart.Headers.ContentDisposition = $pathDisposition
            $typeDisposition =
                [System.Net.Http.Headers.ContentDispositionHeaderValue]::new(
                    'form-data')
            $typeDisposition.Name = '"type"'
            $typePart.Headers.ContentDisposition = $typeDisposition
            $multipart.Add($execPart)
            $multipart.Add($pathPart)
            $multipart.Add($typePart)
            $response = $nova.Client.PostAsync(
                [uri]::new($nova.BaseUri, '/title/launch'), $multipart
            ).GetAwaiter().GetResult()
            try {
                if (-not $response.IsSuccessStatusCode) {
                    throw "NOVA title launch failed with HTTP $([int]$response.StatusCode)."
                }
            }
            finally {
                $response.Dispose()
            }
        }
        finally {
            $multipart.Dispose()
        }
    }
    finally {
        $nova.Client.Dispose()
        $nova.Handler.Dispose()
    }

    return Wait-NovaTitle `
        -ExpectedSuffix $TargetTitleSuffix `
        -TimeoutSec $TimeoutSec
}

function Invoke-NovaLabLaunch {
    return Invoke-NovaTitleLaunch `
        -CurrentTitleSuffix $script:ProductionTitleSuffix `
        -TargetDevicePath $script:LabDevicePath `
        -TargetTitleSuffix $script:LabTitleSuffix `
        -Description 'Launching the isolated AuroraAZLab title through NOVA.' `
        -TimeoutSec $LaunchTimeoutSec
}

function Invoke-NovaProductionLaunch {
    return Invoke-NovaTitleLaunch `
        -CurrentTitleSuffix $script:LabTitleSuffix `
        -TargetDevicePath $script:ProductionDevicePath `
        -TargetTitleSuffix $script:ProductionTitleSuffix `
        -Description 'Restoring production Aurora through NOVA title launch.' `
        -TimeoutSec $RestoreTimeoutSec
}

function Deploy-LabPlugin {
    param([Parameter(Mandatory = $true)]$ArtifactInfo)

    $null = Assert-NovaTitle -ExpectedSuffix $script:ProductionTitleSuffix
    # These exact lengths consume, but never exceed, FATX's 42-character
    # filename limit: 13 + 8 + 12 + 1 + 8 = 42.
    $stageName = 'NetDbgDll.xex.staged-{0}-{1}' -f `
        $ArtifactInfo.ShortHash, $script:StageNonce
    $stagePath = "$($script:PluginDirectory)/$stageName"
    Assert-SafePluginPath -RemotePath $stagePath
    Assert-SafePluginPath -RemotePath $script:ActivePluginPath

    $listing = Get-RemoteDirectoryListing `
        -SessionName $ProductionFtpSession `
        -RemoteDirectory $script:PluginDirectory
    if (Test-RemoteNameInListing -Listing $listing -Name 'NetDbgDll.xex') {
        throw ('The active lab target already exists. Run -Action Disable first; ' +
            'the harness will not replace or displace an unknown active plugin.')
    }
    if (Test-RemoteNameInListing -Listing $listing -Name $stageName) {
        throw 'The generated inactive stage name already exists; no upload was attempted.'
    }

    New-Item -ItemType Directory -Force -Path $script:EvidenceDirectory | Out-Null

    Write-Step "Staging AuroraAZ.xex ($($ArtifactInfo.ShortHash), $($ArtifactInfo.Bytes) bytes)."
    $null = Invoke-WinScpRetry -SessionName $ProductionFtpSession -Command @(
        "cd $($script:PluginDirectory)",
        ('put -nopreservetime -transfer=binary {0} {1}' -f `
            (Quote-WinScpValue $ArtifactInfo.Path), `
            (Quote-WinScpValue $stageName))
    )

    $roundTripPath = Join-Path `
        $script:EvidenceDirectory `
        ("roundtrip-$($ArtifactInfo.ShortHash).xex")
    $null = Invoke-WinScpRetry -SessionName $ProductionFtpSession -Command @(
        "cd $($script:PluginDirectory)",
        ('get -nopreservetime -transfer=binary {0} {1}' -f `
            (Quote-WinScpValue $stageName), `
            (Quote-WinScpValue $roundTripPath))
    )

    $roundTripHash = (Get-FileHash -LiteralPath $roundTripPath -Algorithm SHA256).Hash.ToUpperInvariant()
    if ($roundTripHash -ne $ArtifactInfo.Sha256) {
        throw ('FTP round-trip SHA-256 mismatch. The staged file was left ' +
            'inactive for inspection; activation was not attempted.')
    }

    Write-Step 'Round-trip hash matched; activating by recoverable rename.'
    $activation = Invoke-VerifiedPluginRename `
        -SessionName $ProductionFtpSession `
        -SourceName $stageName `
        -DestinationName 'NetDbgDll.xex' `
        -OperationLabel 'Lab plugin activation'

    return [pscustomobject]@{
        Artifact = $ArtifactInfo.Path
        Bytes = $ArtifactInfo.Bytes
        Sha256 = $ArtifactInfo.Sha256
        RoundTrip = $roundTripPath
        ActiveRemotePath = $script:ActivePluginPath
        Activation = $activation
    }
}

function Disable-LabPluginWithSession {
    param([Parameter(Mandatory = $true)][string]$SessionName)

    $listing = Get-RemoteDirectoryListing `
        -SessionName $SessionName `
        -RemoteDirectory $script:PluginDirectory
    if (-not (Test-RemoteNameInListing -Listing $listing -Name 'NetDbgDll.xex')) {
        return $null
    }

    New-Item -ItemType Directory -Force -Path $script:EvidenceDirectory | Out-Null
    $activeCopy = Join-Path $script:EvidenceDirectory 'active-before-disable.xex'
    if (Test-Path -LiteralPath $activeCopy) {
        $activeCopy = Join-Path `
            $script:EvidenceDirectory `
            ('active-before-disable-{0}.xex' -f [Guid]::NewGuid().ToString('N').Substring(0, 8))
    }
    Assert-SafePluginPath -RemotePath $script:ActivePluginPath
    $null = Invoke-WinScpRetry -SessionName $SessionName -Command @(
        "cd $($script:PluginDirectory)",
        ('get -nopreservetime -transfer=binary {0} {1}' -f `
            (Quote-WinScpValue 'NetDbgDll.xex'), `
            (Quote-WinScpValue $activeCopy))
    )
    $hash = (Get-FileHash -LiteralPath $activeCopy -Algorithm SHA256).Hash.ToUpperInvariant()
    # 13 + 10 + 12 + 1 + 6 = 42, exactly the FATX filename limit.
    $disabledName = 'NetDbgDll.xex.disabled-{0}-{1}' -f `
        $hash.Substring(0, 12).ToLowerInvariant(), $script:DisableNonce
    $disabledPath = "$($script:PluginDirectory)/$disabledName"
    Assert-SafePluginPath -RemotePath $disabledPath

    Write-Step "Disabling the lab plugin by rename ($($hash.Substring(0, 12).ToLowerInvariant()))."
    $rename = Invoke-VerifiedPluginRename `
        -SessionName $SessionName `
        -SourceName 'NetDbgDll.xex' `
        -DestinationName $disabledName `
        -OperationLabel 'Lab plugin disable'

    return [pscustomobject]@{
        Sha256 = $hash
        LocalCopy = $activeCopy
        DisabledRemotePath = $disabledPath
        Rename = $rename
    }
}

function Disable-LabPlugin {
    $errors = [System.Collections.Generic.List[string]]::new()
    foreach ($sessionName in @($LabFtpSession, $ProductionFtpSession) | Select-Object -Unique) {
        try {
            return Disable-LabPluginWithSession -SessionName $sessionName
        }
        catch {
            $errors.Add($sessionName)
        }
    }
    throw ('Unable to inspect/disable the exact lab target through any configured ' +
        'FTP session. Sessions attempted: ' + ($errors -join ', '))
}

function Collect-LabEvidence {
    New-Item -ItemType Directory -Force -Path $script:EvidenceDirectory | Out-Null
    $listing = Get-RemoteDirectoryListing `
        -SessionName $LabFtpSession `
        -RemoteDirectory $script:LogDirectory
    $downloaded = [System.Collections.Generic.List[string]]::new()
    $missing = [System.Collections.Generic.List[string]]::new()

    foreach ($name in $EvidenceName) {
        Assert-SafeEvidenceName -Name $name
        if (-not (Test-RemoteNameInListing -Listing $listing -Name $name)) {
            $missing.Add($name)
            continue
        }

        $remotePath = "$($script:LogDirectory)/$name"
        Assert-SafeLogPath -RemotePath $remotePath
        $localPath = Join-Path $script:EvidenceDirectory $name
        $null = Invoke-WinScpRetry -SessionName $LabFtpSession -Command @(
            "cd $($script:LogDirectory)",
            ('get -nopreservetime -transfer=binary {0} {1}' -f `
                (Quote-WinScpValue $name), `
                (Quote-WinScpValue $localPath))
        )
        $downloaded.Add($localPath)
    }

    $title = Get-NovaTitle
    $titlePath = Join-Path $script:EvidenceDirectory 'nova-title.json'
    $title | ConvertTo-Json -Depth 8 | Set-Content `
        -LiteralPath $titlePath `
        -Encoding UTF8
    $downloaded.Add($titlePath)

    $screenshotPath = $null
    if (-not $SkipScreenshot.IsPresent) {
        $screenshotPath = Join-Path $script:EvidenceDirectory 'screen.bmp'
        $captureScript = Join-Path $PSScriptRoot 'capture-nova.ps1'
        $null = & $captureScript `
            -BaseUri $NovaBaseUri `
            -OutputPath $screenshotPath `
            -Username $NovaUsername `
            -Password $NovaPassword `
            -TimeoutSec $NovaTimeoutSec
        $downloaded.Add($screenshotPath)
    }

    Write-Step "Evidence stored in $($script:EvidenceDirectory)."
    return [pscustomobject]@{
        Directory = $script:EvidenceDirectory
        Downloaded = @($downloaded)
        Missing = @($missing)
        Screenshot = $screenshotPath
    }
}

function Invoke-FtpSiteCommand {
    param(
        [Parameter(Mandatory = $true)][string]$SessionName,
        [Parameter(Mandatory = $true)][ValidateSet('REBOOT', 'NOVA LOAD')][string]$SiteCommand
    )

    $result = Invoke-WinScpCommands `
        -SessionName $SessionName `
        -Command @("call SITE $SiteCommand") `
        -AllowFailure
    if ($result.ExitCode -ne 0) {
        throw "SITE $SiteCommand failed."
    }
    return $result.ExitCode
}

function Wait-ProductionFtp {
    $deadline = [DateTime]::UtcNow.AddSeconds($RestoreTimeoutSec)
    do {
        $result = Invoke-WinScpCommands `
            -SessionName $ProductionFtpSession `
            -Command @('pwd') `
            -AllowFailure
        if ($result.ExitCode -eq 0) {
            return
        }
        Start-Sleep -Seconds 3
    } while ([DateTime]::UtcNow -lt $deadline)
    throw "Production FTP did not become available within $RestoreTimeoutSec seconds."
}

function Restore-ProductionAurora {
    try {
        return Invoke-NovaProductionLaunch
    }
    catch {
        # A successful title-launch request can briefly take NOVA down before
        # Wait-NovaTitle sees the new title. If production FTP is now present,
        # loading NOVA there is enough to confirm the non-reboot restore.
        Write-Warning ('Direct NOVA restore was not confirmed; checking ' +
            'production FTP without changing any file.')
    }

    try {
        Wait-ProductionFtp
        $null = Invoke-FtpSiteCommand `
            -SessionName $ProductionFtpSession `
            -SiteCommand 'NOVA LOAD'
        return Wait-NovaTitle `
            -ExpectedSuffix $script:ProductionTitleSuffix `
            -TimeoutSec $RestoreTimeoutSec
    }
    catch {
        if (-not $AllowSiteRebootFallback.IsPresent) {
            throw [InvalidOperationException]::new(
                'Production Aurora could not be restored without a reboot. ' +
                'The active lab plugin disable was attempted, but console ' +
                'state must be checked manually. Re-run with ' +
                '-AllowSiteRebootFallback only when SITE REBOOT is known to ' +
                'restart this console rather than power it off.'
            )
        }
    }

    Write-Warning ('Using the explicitly enabled SITE REBOOT fallback. ' +
        'This command can power off some consoles.')
    $rebootSent = $false
    foreach ($sessionName in @($LabFtpSession, $ProductionFtpSession) | Select-Object -Unique) {
        try {
            $null = Invoke-FtpSiteCommand `
                -SessionName $sessionName `
                -SiteCommand REBOOT
            $rebootSent = $true
            break
        }
        catch {
            # Try the credentials for the other Aurora instance.
        }
    }
    if (-not $rebootSent) {
        # A successful reboot can close the FTP control connection before
        # WinSCP records the server reply. Continue only if production FTP
        # actually returns; the subsequent title assertion is authoritative.
        Write-Warning 'SITE REBOOT was not acknowledged; waiting for production FTP to confirm the transition.'
    }

    Wait-ProductionFtp
    Write-Step 'Production FTP is available; loading NOVA with SITE NOVA LOAD.'
    $null = Invoke-FtpSiteCommand `
        -SessionName $ProductionFtpSession `
        -SiteCommand 'NOVA LOAD'
    return Wait-NovaTitle `
        -ExpectedSuffix $script:ProductionTitleSuffix `
        -TimeoutSec $RestoreTimeoutSec
}

function Write-EvidenceManifest {
    param([Parameter(Mandatory = $true)]$Value)

    New-Item -ItemType Directory -Force -Path $script:EvidenceDirectory | Out-Null
    $manifest = Join-Path $script:EvidenceDirectory 'harness-result.json'
    $Value | ConvertTo-Json -Depth 12 | Set-Content `
        -LiteralPath $manifest `
        -Encoding UTF8
    return $manifest
}

function Show-Plan {
    param($ArtifactInfo)

    [pscustomobject]@{
        NetworkOperations = $false
        RequestedAction = $Action
        Artifact = $(if ($null -eq $ArtifactInfo) { $null } else { $ArtifactInfo.Path })
        Sha256 = $(if ($null -eq $ArtifactInfo) { $null } else { $ArtifactInfo.Sha256 })
        OnlyActiveRemoteFile = $script:ActivePluginPath
        LabTitle = $script:LabDevicePath
        EvidenceDirectory = $script:EvidenceDirectory
        ProductionFtpSession = $ProductionFtpSession
        LabFtpSession = $LabFtpSession
        ProductionFileOperations = 'none'
        RestoreSequence = $(
            if ($AllowSiteRebootFallback.IsPresent) {
                'NOVA title launch; production NOVA LOAD recovery; opt-in SITE REBOOT fallback'
            }
            else {
                'NOVA title launch; production NOVA LOAD recovery; no reboot fallback'
            }
        )
    }
}

Assert-SafeSessionName -Name $ProductionFtpSession -ParameterName 'ProductionFtpSession'
Assert-SafeSessionName -Name $LabFtpSession -ParameterName 'LabFtpSession'
foreach ($name in $EvidenceName) {
    Assert-SafeEvidenceName -Name $name
}

$artifactInfo = $null
if ($Action -in @('Plan', 'Deploy', 'Test') -and
    -not [string]::IsNullOrWhiteSpace($Artifact)) {
    $artifactInfo = Get-ArtifactInfo -Path $Artifact
}
elseif ($Action -in @('Deploy', 'Test')) {
    throw "Action $Action requires -Artifact."
}

if ($Action -eq 'Plan' -or $DryRun.IsPresent) {
    Show-Plan -ArtifactInfo $artifactInfo
    return
}

$script:WinScpExecutable = Resolve-WinScpExecutable

switch ($Action) {
    'Deploy' {
        $result = Deploy-LabPlugin -ArtifactInfo $artifactInfo
        $manifest = Write-EvidenceManifest -Value $result
        $result | Add-Member -NotePropertyName Manifest -NotePropertyValue $manifest -PassThru
    }
    'Launch' {
        Invoke-NovaLabLaunch
    }
    'Collect' {
        $null = Assert-NovaTitle -ExpectedSuffix $script:LabTitleSuffix
        $result = Collect-LabEvidence
        $manifest = Write-EvidenceManifest -Value $result
        $result | Add-Member -NotePropertyName Manifest -NotePropertyValue $manifest -PassThru
    }
    'Disable' {
        Disable-LabPlugin
    }
    'Restore' {
        $disableResult = $null
        try {
            $disableResult = Disable-LabPlugin
        }
        catch {
            Write-Warning 'Could not disable before restore; retrying from production after restore.'
        }
        $title = Restore-ProductionAurora
        if ($null -eq $disableResult) {
            try {
                $disableResult = Disable-LabPluginWithSession `
                    -SessionName $ProductionFtpSession
            }
            catch {
                throw ('Production was restored, but the active lab plugin could ' +
                    'not be disabled. Do not launch AuroraAZLab until resolved.')
            }
        }
        [pscustomobject]@{
            ProductionTitle = $title.path
            Disabled = $disableResult
        }
    }
    'Test' {
        $deployment = $null
        $evidence = $null
        $disableResult = $null
        $productionTitle = $null
        $testError = $null
        try {
            $deployment = Deploy-LabPlugin -ArtifactInfo $artifactInfo
            $null = Invoke-NovaLabLaunch
            if ($TestDurationSec -gt 0) {
                Write-Step "Allowing the lab title to run for $TestDurationSec seconds."
                Start-Sleep -Seconds $TestDurationSec
            }
            $evidence = Collect-LabEvidence
        }
        catch {
            $testError = $_
        }
        finally {
            try {
                $disableResult = Disable-LabPlugin
            }
            catch {
                Write-Warning 'Pre-restore disable failed; the harness will retry after production is restored.'
            }

            try {
                $productionTitle = Restore-ProductionAurora
            }
            catch {
                if ($null -eq $testError) {
                    $testError = $_
                }
                else {
                    Write-Warning 'Production restore also failed; inspect console state before another lab launch.'
                }
            }

            if ($null -eq $disableResult -and $null -ne $productionTitle) {
                try {
                    $disableResult = Disable-LabPluginWithSession `
                        -SessionName $ProductionFtpSession
                }
                catch {
                    if ($null -eq $testError) {
                        $testError = [System.Management.Automation.ErrorRecord]::new(
                            [InvalidOperationException]::new(
                                'Production is restored, but the active lab plugin could not be disabled.'
                            ),
                            'AuroraAZLabDisableFailed',
                            [System.Management.Automation.ErrorCategory]::WriteError,
                            $script:ActivePluginPath
                        )
                    }
                }
            }
        }

        $result = [pscustomobject]@{
            Deployment = $deployment
            Evidence = $evidence
            Disabled = $disableResult
            ProductionTitle = $(
                if ($null -eq $productionTitle) { $null } else { $productionTitle.path }
            )
            Passed = ($null -eq $testError)
        }
        $manifest = Write-EvidenceManifest -Value $result
        $result | Add-Member -NotePropertyName Manifest -NotePropertyValue $manifest

        if ($null -ne $testError) {
            Write-Output $result
            throw $testError
        }
        Write-Output $result
    }
}
