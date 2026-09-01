[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'

$projectRoot = Split-Path -Parent $PSScriptRoot
$stockSkin = Join-Path $projectRoot 'original\extracted'
$stockMainXui = Join-Path $projectRoot 'source\skin\stock\Aurora_Main.xui'
$patchScript = Join-Path $projectRoot 'source\skin\patches\add-alphabet-row.ps1'
$xzpScript = Join-Path $projectRoot 'scripts\xzp.ps1'
$xuiHelper = Join-Path $projectRoot 'tools\XUIHelper\XUIHelper.CLI\bin\Release\net8.0\XUIHelper.CLI.exe'
$buildRoot = Join-Path $projectRoot 'build\functional-test-r3'
$stageRoot = Join-Path $buildRoot 'skin'
$modifiedMainXui = Join-Path $buildRoot 'Aurora_Main.xui'
$outputSkin = Join-Path $projectRoot 'build\Aurora-A-Z-functional-test-r3.xzp'
$distributionRoot = Join-Path $buildRoot 'Aurora-A-Z'
$outputZip = Join-Path $projectRoot 'build\Aurora-A-Z-functional-test-r3.zip'
$compileLog = Join-Path $buildRoot 'xuihelper-compile.log'

$requiredPaths = @(
    $stockSkin,
    $stockMainXui,
    $patchScript,
    $xzpScript,
    $xuiHelper,
    (Join-Path $projectRoot 'source\content\Filters\AuroraAZ.lua'),
    (Join-Path $projectRoot 'source\content\Filters\AuroraAZ.ini'),
    (Join-Path $projectRoot 'source\utility\AuroraAZInstaller\Main.lua'),
    (Join-Path $projectRoot 'release\INSTALL.txt')
)

foreach ($requiredPath in $requiredPaths) {
    if (-not (Test-Path -LiteralPath $requiredPath)) {
        throw "Required build input is missing: $requiredPath"
    }
}

if (Test-Path -LiteralPath $buildRoot) {
    Remove-Item -LiteralPath $buildRoot -Recurse -Force
}

New-Item -ItemType Directory -Force -Path $stageRoot | Out-Null
Copy-Item -Path (Join-Path $stockSkin '*') -Destination $stageRoot -Recurse

& $patchScript -InputFile $stockMainXui -OutputFile $modifiedMainXui

& $xuiHelper conv `
    -s $modifiedMainXui `
    -f xurv5 `
    -o (Join-Path $stageRoot 'Aurora_Main.xur') `
    -g AuroraV5 `
    -l $compileLog `
    -v info

if ($LASTEXITCODE -ne 0) {
    Get-Content -LiteralPath $compileLog -Tail 100
    throw "XUIHelper failed with exit code $LASTEXITCODE."
}

$metadataPath = Join-Path $stageRoot 'skin.meta'
$metadata = Get-Content -LiteralPath $metadataPath -Raw | ConvertFrom-Json
$metadata.skinname = 'Aurora A-Z Functional Test r3'
$metadata.author = 'haimbilia'
$metadata.revision = 3
$metadata.description = 'QuickView-backed on-coverflow A-Z selector functional test.'
$metadata.creationdate = '2026-09-01'
$metadata.updateid = ''
$metadata | ConvertTo-Json | Set-Content -LiteralPath $metadataPath -Encoding utf8NoBOM

& $xzpScript -Action build -InputPath $stageRoot -OutputPath $outputSkin

$skinDestination = Join-Path $distributionRoot 'Skins'
$filterDestination = Join-Path $distributionRoot 'User\Scripts\Content\Filters'
$utilityDestination = Join-Path $distributionRoot 'User\Scripts\Utility\AuroraAZInstaller'
New-Item -ItemType Directory -Force -Path $skinDestination, $filterDestination, $utilityDestination | Out-Null

Copy-Item -LiteralPath $outputSkin -Destination $skinDestination
Copy-Item -LiteralPath (Join-Path $projectRoot 'source\content\Filters\AuroraAZ.lua') -Destination $filterDestination
Copy-Item -LiteralPath (Join-Path $projectRoot 'source\content\Filters\AuroraAZ.ini') -Destination $filterDestination
Copy-Item -LiteralPath (Join-Path $projectRoot 'source\utility\AuroraAZInstaller\Main.lua') -Destination $utilityDestination
Copy-Item -LiteralPath (Join-Path $projectRoot 'release\INSTALL.txt') -Destination $distributionRoot

Compress-Archive -Path (Join-Path $distributionRoot '*') -DestinationPath $outputZip -CompressionLevel Optimal -Force

$skinHash = Get-FileHash -LiteralPath $outputSkin -Algorithm SHA256
$zipHash = Get-FileHash -LiteralPath $outputZip -Algorithm SHA256
Write-Host "Built $outputSkin"
Write-Host "Skin SHA256 $($skinHash.Hash)"
Write-Host "Built $outputZip"
Write-Host "ZIP SHA256  $($zipHash.Hash)"
