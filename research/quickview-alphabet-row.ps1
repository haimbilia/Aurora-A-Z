param(
    [Parameter(Mandatory = $true)]
    [string]$InputFile,

    [Parameter(Mandatory = $true)]
    [string]$OutputFile,

    # How far to lift the letter strip from wherever this skin parks it.
    # Skins do not agree on a resting Y, so the shift is relative, never absolute.
    [double]$Offset = 70.0,

    [switch]$KeepIcons
)

$ErrorActionPreference = 'Stop'

$document = New-Object System.Xml.XmlDocument
$document.PreserveWhitespace = $false
$document.Load((Resolve-Path -LiteralPath $InputFile).Path)

$scene = $document.SelectSingleNode("/XuiCanvas/XuiScene[Properties/Id='ScnQuickView']")
if ($null -eq $scene) {
    throw 'ScnQuickView was not found. Is this Aurora_QuickView.xui?'
}

function Get-PositionTimeline {
    param([string]$ElementId)

    foreach ($timeline in $document.SelectNodes('//Timeline')) {
        $id = $timeline.SelectSingleNode('Id')
        $prop = $timeline.SelectSingleNode('TimelineProp')
        if ($null -eq $id -or $null -eq $prop) { continue }
        if ($id.InnerText -eq $ElementId -and $prop.InnerText -eq 'Position') {
            return $timeline
        }
    }
    return $null
}

# The strip slides on from offscreen, holds, then slides back off, so the
# timeline visits the offscreen Y both before and after the resting one.
# Y grows downward, so the position it comes to rest at is the smallest Y.
$animator = Get-PositionTimeline -ElementId 'Animator'
if ($null -eq $animator) {
    throw 'No Position timeline for Animator; this skin drives the QuickView strip differently.'
}

$restY = $null
foreach ($frame in $animator.SelectNodes('KeyFrame/Prop')) {
    $parts = $frame.InnerText.Split(',')
    if ($parts.Count -lt 2) { continue }
    $y = [double]$parts[1]
    if ($null -eq $restY -or $y -lt $restY) { $restY = $y }
}

if ($null -eq $restY) {
    throw 'Could not determine the resting Y of the QuickView strip.'
}

$targetY = $restY - $Offset
Write-Host "Skin parks the strip at Y=$restY; moving it to Y=$targetY."

$moved = 0
foreach ($elementId in @('Animator', 'ContentContainer')) {
    $timeline = Get-PositionTimeline -ElementId $elementId
    if ($null -eq $timeline) { continue }

    foreach ($frame in $timeline.SelectNodes('KeyFrame/Prop')) {
        $parts = $frame.InnerText.Split(',')
        if ($parts.Count -lt 2) { continue }
        if ([math]::Abs([double]$parts[1] - $restY) -lt 0.5) {
            $parts[1] = $targetY.ToString('F6', [System.Globalization.CultureInfo]::InvariantCulture)
            $frame.InnerText = ($parts -join ',')
            $moved++
        }
    }
}

if ($moved -eq 0) {
    throw "No keyframes at the resting Y=$restY were found."
}

# A-Z entries carry no artwork, so the icon slots would render as missing-icon
# boxes. Skins may name a different number of them, so discover rather than
# assume five.
$hidden = 0
if (-not $KeepIcons) {
    foreach ($image in $scene.SelectNodes(".//XuiImage/Properties[starts-with(Id,'QVImage')]")) {
        $show = $image.SelectSingleNode('Show')
        if ($null -eq $show) {
            $show = $document.CreateElement('Show')
            $null = $image.AppendChild($show)
        }
        $show.InnerText = 'false'
        $hidden++
    }
}

$settings = New-Object System.Xml.XmlWriterSettings
$settings.Indent = $true
$settings.IndentChars = "  "
$settings.NewLineChars = "`r`n"
$settings.NewLineHandling = [System.Xml.NewLineHandling]::Replace
$settings.OmitXmlDeclaration = $true
$settings.Encoding = New-Object System.Text.UTF8Encoding($false)

$writer = [System.Xml.XmlWriter]::Create($OutputFile, $settings)
try { $document.Save($writer) } finally { $writer.Dispose() }

Write-Host "Moved $moved keyframes; hid $hidden icon slots."
