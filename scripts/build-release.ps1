[CmdletBinding()]
param(
    [string]$Version
)

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot

if ([string]::IsNullOrWhiteSpace($Version)) {
    $Version = (Get-Content -LiteralPath (Join-Path $projectRoot 'VERSION') -Raw).Trim()
}

if ($Version -notmatch '^\d+\.\d+\.\d+([-.][0-9A-Za-z.-]+)?$') {
    throw "Invalid release version: $Version"
}

$releaseName = "Aurora-A-Z-v$Version"
$buildRoot = Join-Path $projectRoot 'build'
$stageRoot = Join-Path $buildRoot $releaseName
$filterRoot = Join-Path $stageRoot 'User\Scripts\Content\Filters'
$archivePath = Join-Path $buildRoot "$releaseName.zip"

if (Test-Path -LiteralPath $stageRoot) {
    Remove-Item -LiteralPath $stageRoot -Recurse -Force
}

if (Test-Path -LiteralPath $archivePath) {
    Remove-Item -LiteralPath $archivePath -Force
}

New-Item -ItemType Directory -Path $filterRoot -Force | Out-Null

Copy-Item -LiteralPath (Join-Path $projectRoot 'source\filter\AuroraAZ.lua') -Destination $filterRoot
Copy-Item -LiteralPath (Join-Path $projectRoot 'source\filter\AuroraAZ.ini') -Destination $filterRoot
Copy-Item -LiteralPath (Join-Path $projectRoot 'README.md') -Destination (Join-Path $stageRoot 'README.md')
Copy-Item -LiteralPath (Join-Path $projectRoot 'CHANGELOG.md') -Destination (Join-Path $stageRoot 'CHANGELOG.md')

Compress-Archive -LiteralPath $stageRoot -DestinationPath $archivePath -CompressionLevel Optimal

$hash = Get-FileHash -LiteralPath $archivePath -Algorithm SHA256
Write-Host "Built $archivePath"
Write-Host "SHA256 $($hash.Hash)"
