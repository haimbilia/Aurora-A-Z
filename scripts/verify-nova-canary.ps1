[CmdletBinding()]
param(
    [string]$NovaBaseUri = 'http://192.168.1.103:9999',
    [string]$ExpectedTitlePathSuffix = '\Aurora\Aurora.xex',
    [string]$Username = $env:AURORAAZ_NOVA_USERNAME,
    [string]$Password = $env:AURORAAZ_NOVA_PASSWORD,
    [UInt64]$ModuleBase = 2446327808, # 0x91D00000
    [UInt64]$ModuleWindow = 1048576  # 0x00100000
)

$ErrorActionPreference = 'Stop'

$baseUri = $NovaBaseUri.TrimEnd('/')
$headers = @{ Accept = 'application/json' }

$hasUsername = -not [string]::IsNullOrWhiteSpace($Username)
$hasPassword = -not [string]::IsNullOrWhiteSpace($Password)
if ($hasUsername -xor $hasPassword) {
    throw 'Provide both NOVA username and password, or neither.'
}

if ($hasUsername) {
    try {
        $authRequest = @{
            Method = 'Post'
            Uri = "$baseUri/authenticate"
            ContentType = 'application/x-www-form-urlencoded'
            Body = @{ username = $Username; password = $Password }
        }
        $authentication = Invoke-RestMethod @authRequest
    }
    catch {
        throw "NOVA authentication failed: $($_.Exception.Message)"
    }

    if ([string]::IsNullOrWhiteSpace([string]$authentication.token)) {
        throw 'NOVA authentication succeeded without returning a JWT.'
    }

    $headers.Authorization = "Bearer $($authentication.token)"
}

$title = Invoke-RestMethod -Headers $headers -Uri "$baseUri/title"

if ([string]::IsNullOrWhiteSpace($ExpectedTitlePathSuffix)) {
    throw 'ExpectedTitlePathSuffix must not be empty.'
}

if ([string]::IsNullOrWhiteSpace([string]$title.path) -or
    -not ([string]$title.path).EndsWith($ExpectedTitlePathSuffix,
        [StringComparison]::OrdinalIgnoreCase)) {
    throw ('The expected Aurora title is not running: ' +
        "expected *$ExpectedTitlePathSuffix, got $($title.path)")
}

$threads = @(Invoke-RestMethod -Headers $headers -Uri "$baseUri/thread")
$moduleEnd = $ModuleBase + $ModuleWindow
$matches = @($threads | Where-Object {
    $addressText = [string]$_.address
    if (-not $addressText.StartsWith('0x', [StringComparison]::OrdinalIgnoreCase)) {
        return $false
    }

    $address = [Convert]::ToUInt64($addressText.Substring(2), 16)
    return $address -ge $ModuleBase -and $address -lt $moduleEnd
})

if ($matches.Count -eq 0) {
    throw ('No live thread starts in the AuroraAZ module window ' +
        ('0x{0:X8}-0x{1:X8}.' -f $ModuleBase, ($moduleEnd - 1)))
}

[pscustomobject]@{
    title = $title.path
    resolution = '{0}x{1}' -f $title.resolution.width, $title.resolution.height
    module_window = '0x{0:X8}-0x{1:X8}' -f $ModuleBase, ($moduleEnd - 1)
    matching_threads = @($matches | ForEach-Object {
        [pscustomobject]@{
            id = $_.id
            address = $_.address
            state = $_.state
            priority = $_.priority
        }
    })
} | ConvertTo-Json -Depth 4
