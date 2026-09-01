[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'

$projectRoot = Split-Path -Parent $PSScriptRoot
$stockSkin = Join-Path $projectRoot 'original\extracted'
$stockMainXui = Join-Path $projectRoot 'source\skin\stock\Aurora_Main.xui'
$patchScript = Join-Path $projectRoot 'source\skin\patches\add-alphabet-row.ps1'
$xzpScript = Join-Path $projectRoot 'scripts\xzp.ps1'
$xuiHelper = Join-Path $projectRoot 'tools\XUIHelper\XUIHelper.CLI\bin\Release\net8.0\XUIHelper.CLI.exe'
$buildRoot = Join-Path $projectRoot 'build\visual-test'
$stageRoot = Join-Path $buildRoot 'skin'
$modifiedMainXui = Join-Path $buildRoot 'Aurora_Main.xui'
$outputSkin = Join-Path $projectRoot 'build\Aurora-A-Z-visual-test.xzp'
$compileLog = Join-Path $buildRoot 'xuihelper-compile.log'

foreach ($requiredPath in @($stockSkin, $stockMainXui, $patchScript, $xzpScript, $xuiHelper)) {
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
$metadata.skinname = 'Aurora A-Z Visual Test'
$metadata.author = 'haimbilia'
$metadata.revision = 1
$metadata.description = 'Aurora A-Z on-coverflow selector visual smoke test.'
$metadata.creationdate = '2026-09-01'
$metadata.updateid = ''
$metadata | ConvertTo-Json | Set-Content -LiteralPath $metadataPath -Encoding utf8NoBOM

& $xzpScript -Action build -InputPath $stageRoot -OutputPath $outputSkin

$hash = Get-FileHash -LiteralPath $outputSkin -Algorithm SHA256
Write-Host "Built $outputSkin"
Write-Host "SHA256 $($hash.Hash)"
