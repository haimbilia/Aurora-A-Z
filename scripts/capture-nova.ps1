<#
.SYNOPSIS
Captures an Aurora screen through NOVA and downloads it as a BMP.

.DESCRIPTION
By default, this script asks NOVA to take a new screenshot, then downloads the
result. Use -LatestExisting to test the connection and download the newest
existing screenshot without changing console state.

NOVA credentials are optional because authentication can be disabled on the
console. When authentication is enabled, set AURORAAZ_NOVA_USERNAME and
AURORAAZ_NOVA_PASSWORD in the current process environment. The JWT remains in
memory and is never printed or written to disk.

.EXAMPLE
$env:AURORAAZ_NOVA_URL = 'http://CONSOLE_IP:9999'
.\scripts\capture-nova.ps1

.EXAMPLE
$env:AURORAAZ_NOVA_USERNAME = 'your-webui-user'
$env:AURORAAZ_NOVA_PASSWORD = 'your-webui-password'
.\scripts\capture-nova.ps1 -OutputPath .\build\nova\after-input.bmp

.EXAMPLE
.\scripts\capture-nova.ps1 -LatestExisting
#>
[CmdletBinding()]
param(
    [string]$BaseUri = $env:AURORAAZ_NOVA_URL,

    [string]$OutputPath,

    [string]$Username = $env:AURORAAZ_NOVA_USERNAME,

    [string]$Password = $env:AURORAAZ_NOVA_PASSWORD,

    [switch]$LatestExisting,

    [ValidateRange(5, 120)]
    [int]$TimeoutSec = 30
)

$ErrorActionPreference = 'Stop'

function Read-JsonResponse {
    param(
        [Parameter(Mandatory = $true)]
        [System.Net.Http.HttpResponseMessage]$Response,

        [Parameter(Mandatory = $true)]
        [string]$Operation
    )

    $body = $Response.Content.ReadAsStringAsync().GetAwaiter().GetResult()
    if (-not $Response.IsSuccessStatusCode) {
        $status = [int]$Response.StatusCode
        if ($status -eq 401) {
            throw "$Operation was rejected by NOVA (HTTP 401). Set AURORAAZ_NOVA_USERNAME and AURORAAZ_NOVA_PASSWORD."
        }

        throw "$Operation failed with HTTP $status ($($Response.ReasonPhrase))."
    }

    try {
        return $body | ConvertFrom-Json
    }
    catch {
        throw "$Operation returned invalid JSON."
    }
}

function Get-NovaJson {
    param(
        [Parameter(Mandatory = $true)]
        [System.Net.Http.HttpClient]$Client,

        [Parameter(Mandatory = $true)]
        [uri]$Uri,

        [Parameter(Mandatory = $true)]
        [string]$Operation
    )

    $response = $Client.GetAsync($Uri).GetAwaiter().GetResult()
    try {
        return Read-JsonResponse -Response $response -Operation $Operation
    }
    finally {
        $response.Dispose()
    }
}

Add-Type -AssemblyName System.Net.Http

if ([string]::IsNullOrWhiteSpace($BaseUri)) {
    throw 'Set AURORAAZ_NOVA_URL or pass -BaseUri (for example, http://CONSOLE_IP:9999).'
}

$novaUri = $null
if (-not [uri]::TryCreate($BaseUri.TrimEnd('/'), [System.UriKind]::Absolute, [ref]$novaUri)) {
    throw "BaseUri is not a valid absolute URI: $BaseUri"
}

if ($novaUri.Scheme -notin @('http', 'https')) {
    throw 'BaseUri must use HTTP or HTTPS.'
}

if ([string]::IsNullOrWhiteSpace($novaUri.Host)) {
    throw 'BaseUri must include a host.'
}

$hasUsername = -not [string]::IsNullOrWhiteSpace($Username)
$hasPassword = -not [string]::IsNullOrWhiteSpace($Password)
if ($hasUsername -xor $hasPassword) {
    throw 'Provide both NOVA username and password, or neither.'
}

$handler = [System.Net.Http.HttpClientHandler]::new()
$handler.AllowAutoRedirect = $false
$client = [System.Net.Http.HttpClient]::new($handler)
$client.Timeout = [TimeSpan]::FromSeconds($TimeoutSec)
$client.DefaultRequestHeaders.Accept.ParseAdd('application/json')

try {
    if ($hasUsername) {
        $authUri = [uri]::new($novaUri, '/authenticate')
        # NOVA's Angular client posts URLSearchParams. Its endpoint rejects
        # multipart form data on a clean Rev1655 installation.
        $formValues =
            [System.Collections.Generic.List[
                System.Collections.Generic.KeyValuePair[string, string]
            ]]::new()
        $formValues.Add(
            [System.Collections.Generic.KeyValuePair[string, string]]::new(
                'username', $Username
            )
        )
        $formValues.Add(
            [System.Collections.Generic.KeyValuePair[string, string]]::new(
                'password', $Password
            )
        )
        $form = [System.Net.Http.FormUrlEncodedContent]::new($formValues)
        try {
            $authResponse = $client.PostAsync($authUri, $form).GetAwaiter().GetResult()
            try {
                $authentication = Read-JsonResponse -Response $authResponse -Operation 'Authentication'
            }
            finally {
                $authResponse.Dispose()
            }
        }
        finally {
            $form.Dispose()
        }

        if ([string]::IsNullOrWhiteSpace($authentication.token)) {
            throw 'Authentication succeeded but NOVA did not return a JWT.'
        }

        $client.DefaultRequestHeaders.Authorization =
            [System.Net.Http.Headers.AuthenticationHeaderValue]::new('Bearer', [string]$authentication.token)
    }

    $client.DefaultRequestHeaders.Accept.Clear()
    $client.DefaultRequestHeaders.Accept.ParseAdd('application/json')

    if ($LatestExisting) {
        $listUri = [uri]::new($novaUri, '/screencapture/meta/list')
        $captures = @(Get-NovaJson -Client $client -Uri $listUri -Operation 'Screenshot listing')
        if ($captures.Count -eq 0) {
            throw 'NOVA has no existing screenshots for the running title.'
        }

        $metadata = $captures | Sort-Object -Property timestamp | Select-Object -Last 1
    }
    else {
        # NOVA documents this GET as "Take a screen capture". It creates a
        # capture entry on the console, which is deliberately not auto-deleted.
        $captureUri = [uri]::new($novaUri, '/screencapture/meta')
        $metadata = Get-NovaJson -Client $client -Uri $captureUri -Operation 'Screenshot capture'
    }

    if ([string]::IsNullOrWhiteSpace($metadata.filename)) {
        throw 'NOVA returned screenshot metadata without a filename.'
    }

    if ([string]::IsNullOrWhiteSpace($OutputPath)) {
        $outputDirectory = Join-Path (Split-Path -Parent $PSScriptRoot) 'build\nova'
        $outputName = 'aurora-{0}.bmp' -f [DateTime]::UtcNow.ToString('yyyyMMdd-HHmmssfff')
        $OutputPath = Join-Path $outputDirectory $outputName
    }

    $fullOutputPath = [System.IO.Path]::GetFullPath($OutputPath)
    if (Test-Path -LiteralPath $fullOutputPath) {
        throw "Output file already exists: $fullOutputPath"
    }

    $outputDirectory = Split-Path -Parent $fullOutputPath
    if (-not [string]::IsNullOrWhiteSpace($outputDirectory)) {
        New-Item -ItemType Directory -Force -Path $outputDirectory | Out-Null
    }

    $imageUri = [uri]::new(
        $novaUri,
        '/image/screencapture?uuid=' + [uri]::EscapeDataString([string]$metadata.filename)
    )
    $client.DefaultRequestHeaders.Accept.Clear()
    $client.DefaultRequestHeaders.Accept.ParseAdd('image/bmp')
    $imageResponse = $client.GetAsync($imageUri).GetAwaiter().GetResult()
    try {
        if (-not $imageResponse.IsSuccessStatusCode) {
            $status = [int]$imageResponse.StatusCode
            if ($status -eq 401) {
                throw 'Screenshot download was rejected by NOVA (HTTP 401). Set AURORAAZ_NOVA_USERNAME and AURORAAZ_NOVA_PASSWORD.'
            }

            throw "Screenshot download failed with HTTP $status ($($imageResponse.ReasonPhrase))."
        }

        $bytes = $imageResponse.Content.ReadAsByteArrayAsync().GetAwaiter().GetResult()
    }
    finally {
        $imageResponse.Dispose()
    }

    if ($bytes.Length -lt 54 -or $bytes[0] -ne 0x42 -or $bytes[1] -ne 0x4D) {
        throw 'NOVA returned data that is not a valid BMP file.'
    }

    $stream = [System.IO.File]::Open(
        $fullOutputPath,
        [System.IO.FileMode]::CreateNew,
        [System.IO.FileAccess]::Write,
        [System.IO.FileShare]::None
    )
    try {
        $stream.Write($bytes, 0, $bytes.Length)
    }
    finally {
        $stream.Dispose()
    }

    [pscustomobject]@{
        Path = $fullOutputPath
        Filename = [string]$metadata.filename
        Timestamp = [string]$metadata.timestamp
        Width = $metadata.info.width
        Height = $metadata.info.height
        Bytes = $bytes.Length
        CapturedNow = -not $LatestExisting.IsPresent
    }
}
finally {
    $client.Dispose()
    $handler.Dispose()
}
