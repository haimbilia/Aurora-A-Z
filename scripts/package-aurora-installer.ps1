param(
    [Parameter(Mandatory=$true)][string]$PluginPath,
    [Parameter(Mandatory=$true)][string]$ExpectedSha256,
    [ValidatePattern('^\d+\.\d+(\.\d+)?$')][string]$Version = '1.1',
    [string]$OutputDirectory = 'build/release-v1.1-installer'
)
$ErrorActionPreference = 'Stop'
if ((Get-FileHash -LiteralPath $PluginPath).Hash -ne $ExpectedSha256) {
    throw 'Plugin hash does not match the hardware-tested release'
}
$folder = Join-Path $OutputDirectory 'AuroraAZInstaller'
New-Item -ItemType Directory -Force $folder | Out-Null
Copy-Item -LiteralPath 'source/utility/AuroraAZInstaller/Main.lua' -Destination (Join-Path $folder 'Main.lua')
Copy-Item -LiteralPath $PluginPath -Destination (Join-Path $folder 'AuroraAZ.xex')
Copy-Item -LiteralPath 'icon.png' -Destination (Join-Path $folder 'icon.png')
$zipName = "AuroraAZ-v$Version-Installer.zip"
$zip = Join-Path $OutputDirectory $zipName
Compress-Archive -LiteralPath $folder -DestinationPath $zip -Force
$digest = (Get-FileHash -LiteralPath $zip).Hash.ToLowerInvariant()
[IO.File]::WriteAllText("$zip.sha256", "$digest  $zipName`n", [Text.UTF8Encoding]::new($false))
Write-Output "Created $zip"
Write-Output "SHA256 $digest"
