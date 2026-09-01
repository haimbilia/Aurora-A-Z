param(
    [Parameter(Mandatory = $true)]
    [string]$InputFile,

    [Parameter(Mandatory = $true)]
    [string]$OutputFile
)

$ErrorActionPreference = 'Stop'

$document = New-Object System.Xml.XmlDocument
$document.PreserveWhitespace = $false
$document.Load((Resolve-Path -LiteralPath $InputFile).Path)

$applicationLayer = $document.SelectSingleNode(
    "/XuiCanvas/XuiScene/XuiScene[Properties/Id='ApplicationLayer']"
)

if ($null -eq $applicationLayer) {
    throw 'ApplicationLayer was not found in Aurora_Main.xui.'
}

$coverflowWrapper = $applicationLayer.SelectSingleNode(
    "XuiGroup[Properties/Id='CoverflowWrapper']"
)

if ($null -eq $coverflowWrapper) {
    throw 'CoverflowWrapper was not found in Aurora_Main.xui.'
}

if ($null -ne $applicationLayer.SelectSingleNode("XuiText[Properties/Id='AlphabetSelector']")) {
    throw 'AlphabetSelector already exists in Aurora_Main.xui.'
}

$selectorXml = @'
<XuiText>
  <Properties>
    <Id>AlphabetSelector</Id>
    <Width>2360.000000</Width>
    <Height>90.000000</Height>
    <Position>50.000000,532.000000,0.000000</Position>
    <Scale>0.500000,0.500000,1.000000</Scale>
    <Text>#  A B C D E F G H I J K L M N O P Q R S T U V W X Y Z</Text>
    <TextColor>0xffffffff</TextColor>
    <DropShadowColor>0xc0000000</DropShadowColor>
    <PointSize>46.000000</PointSize>
    <Font>Segoe UI Regular</Font>
    <TextStyle>16</TextStyle>
  </Properties>
</XuiText>
'@

$fragment = $document.CreateDocumentFragment()
$fragment.InnerXml = $selectorXml
$null = $applicationLayer.InsertAfter($fragment.FirstChild, $coverflowWrapper)

$settings = New-Object System.Xml.XmlWriterSettings
$settings.Indent = $true
$settings.IndentChars = "  "
$settings.NewLineChars = "`r`n"
$settings.NewLineHandling = [System.Xml.NewLineHandling]::Replace
$settings.OmitXmlDeclaration = $true
$settings.Encoding = New-Object System.Text.UTF8Encoding($false)

$writer = [System.Xml.XmlWriter]::Create($OutputFile, $settings)
try {
    $document.Save($writer)
}
finally {
    $writer.Dispose()
}
