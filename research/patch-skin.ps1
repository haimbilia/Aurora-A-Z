[CmdletBinding()]
param(
    # Any Aurora skin package. Default, Dark, Series, anything.
    [Parameter(Mandatory = $true)]
    [string]$SkinPackage,

    [Parameter(Mandatory = $true)]
    [string]$OutputPackage,

    [double]$Offset = 70.0,

    [string]$NameSuffix = ' + A-Z'
)

$ErrorActionPreference = 'Stop'

$projectRoot = Split-Path -Parent $PSScriptRoot
$patchScript = Join-Path $PSScriptRoot 'quickview-alphabet-row.ps1'
$xzpScript   = Join-Path $projectRoot 'scripts\xzp.ps1'
$xuiHelper   = Join-Path $projectRoot 'tools\XUIHelper\XUIHelper.CLI\bin\Release\net8.0\XUIHelper.CLI.exe'

foreach ($required in @($SkinPackage, $patchScript, $xzpScript, $xuiHelper)) {
    if (-not (Test-Path -LiteralPath $required)) {
        throw "Required input is missing: $required"
    }
}

$work = Join-Path ([System.IO.Path]::GetTempPath()) ("aurora-az-" + [System.Guid]::NewGuid().ToString('N'))
$stage = Join-Path $work 'skin'
New-Item -ItemType Directory -Force -Path $work | Out-Null

try {
    & $xzpScript -Action extract -InputPath $SkinPackage -OutputPath $stage

    $sceneXur = Join-Path $stage 'Aurora_QuickView.xur'
    if (-not (Test-Path -LiteralPath $sceneXur)) {
        throw "This skin has no Aurora_QuickView.xur; nothing to patch."
    }

    $stockXui   = Join-Path $work 'Aurora_QuickView.stock.xui'
    $patchedXui = Join-Path $work 'Aurora_QuickView.patched.xui'

    & $xuiHelper conv -s $sceneXur -f xuiv12 -o $stockXui -g AuroraV5 `
        -l (Join-Path $work 'decompile.log') -v info | Out-Null
    if (-not (Test-Path -LiteralPath $stockXui)) {
        Get-Content -LiteralPath (Join-Path $work 'decompile.log') -Tail 40
        throw 'Decompiling Aurora_QuickView.xur failed.'
    }

    & $patchScript -InputFile $stockXui -OutputFile $patchedXui -Offset $Offset

    & $xuiHelper conv -s $patchedXui -f xurv5 -o $sceneXur -g AuroraV5 `
        -l (Join-Path $work 'compile.log') -v info | Out-Null
    if ($LASTEXITCODE -ne 0) {
        Get-Content -LiteralPath (Join-Path $work 'compile.log') -Tail 60
        throw "XUIHelper failed with exit code $LASTEXITCODE."
    }

    # Rename via a targeted edit rather than a JSON round trip. Some skins ship
    # a skin.meta with trailing bytes after the closing brace (Series does).
    # Aurora stops at the brace and does not care, but ConvertFrom-Json throws,
    # and ConvertTo-Json would reformat every other skin's file for no reason.
    $metadataPath = Join-Path $stage 'skin.meta'
    if (Test-Path -LiteralPath $metadataPath) {
        $raw = [System.IO.File]::ReadAllText($metadataPath)
        $match = [regex]::Match($raw, '("skinname"\s*:\s*")((?:[^"\\]|\\.)*)(")')
        if (-not $match.Success) {
            throw 'skin.meta has no skinname field.'
        }

        $group = $match.Groups[2]
        $original = $group.Value
        $renamed = "$original$NameSuffix"
        $raw = $raw.Remove($group.Index, $group.Length).Insert($group.Index, $renamed)
        [System.IO.File]::WriteAllText(
            $metadataPath, $raw, (New-Object System.Text.UTF8Encoding($false)))
        Write-Host "Skin name: '$original' -> '$renamed'"
    }

    & $xzpScript -Action build -InputPath $stage -OutputPath $OutputPackage

    $hash = Get-FileHash -LiteralPath $OutputPackage -Algorithm SHA256
    Write-Host "Built $OutputPackage"
    Write-Host "SHA256 $($hash.Hash)"
}
finally {
    if (Test-Path -LiteralPath $work) {
        Remove-Item -LiteralPath $work -Recurse -Force -ErrorAction SilentlyContinue
    }
}
