[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidateSet('build', 'extract')]
    [string]$Action,

    [Parameter(Mandatory = $true)]
    [string]$InputPath,

    [Parameter(Mandatory = $true)]
    [string]$OutputPath,

    [ValidateSet(1)]
    [int]$Version = 1
)

$ErrorActionPreference = 'Stop'

function Write-BigEndianInt16 {
    param([System.IO.BinaryWriter]$Writer, [int]$Value)

    $bytes = [System.BitConverter]::GetBytes([int16]$Value)
    [array]::Reverse($bytes)
    $Writer.Write($bytes)
}

function Write-BigEndianInt32 {
    param([System.IO.BinaryWriter]$Writer, [int]$Value)

    $bytes = [System.BitConverter]::GetBytes([int32]$Value)
    [array]::Reverse($bytes)
    $Writer.Write($bytes)
}

function Read-BigEndianInt16 {
    param([System.IO.BinaryReader]$Reader)

    $bytes = $Reader.ReadBytes(2)
    if ($bytes.Length -ne 2) { throw 'Unexpected end of XZP header.' }
    [array]::Reverse($bytes)
    return [System.BitConverter]::ToInt16($bytes, 0)
}

function Read-BigEndianInt32 {
    param([System.IO.BinaryReader]$Reader)

    $bytes = $Reader.ReadBytes(4)
    if ($bytes.Length -ne 4) { throw 'Unexpected end of XZP file.' }
    [array]::Reverse($bytes)
    return [System.BitConverter]::ToInt32($bytes, 0)
}

function Get-XzpFiles {
    param([string]$Root, [string]$Directory)

    $result = New-Object System.Collections.Generic.List[object]

    foreach ($file in @(Get-ChildItem -LiteralPath $Directory -File | Sort-Object Name)) {
        $relative = $file.FullName.Substring($Root.Length).TrimStart('\').Replace('/', '\')
        $result.Add([pscustomobject]@{
            FullName = $file.FullName
            RelativeName = $relative
            Length = [int]$file.Length
        })
    }

    foreach ($child in @(Get-ChildItem -LiteralPath $Directory -Directory | Sort-Object Name)) {
        foreach ($file in @(Get-XzpFiles -Root $Root -Directory $child.FullName)) {
            $result.Add($file)
        }
    }

    return $result
}

function Build-Xzp {
    param([string]$SourceDirectory, [string]$DestinationFile)

    $root = (Resolve-Path -LiteralPath $SourceDirectory).Path.TrimEnd('\')
    $files = @(Get-XzpFiles -Root $root -Directory $root)
    if ($files.Count -gt [int16]::MaxValue) {
        throw "XZP contains too many entries: $($files.Count)"
    }

    $destinationDirectory = Split-Path -Parent $DestinationFile
    if (-not [string]::IsNullOrEmpty($destinationDirectory)) {
        New-Item -ItemType Directory -Force -Path $destinationDirectory | Out-Null
    }

    $stream = [System.IO.File]::Open(
        $DestinationFile,
        [System.IO.FileMode]::Create,
        [System.IO.FileAccess]::ReadWrite,
        [System.IO.FileShare]::None
    )
    $writer = [System.IO.BinaryWriter]::new($stream)

    try {
        $writer.Write([byte[]](0x58, 0x55, 0x49, 0x5A))
        Write-BigEndianInt32 -Writer $writer -Value $Version
        Write-BigEndianInt32 -Writer $writer -Value 0
        Write-BigEndianInt32 -Writer $writer -Value 0
        Write-BigEndianInt32 -Writer $writer -Value 0
        Write-BigEndianInt16 -Writer $writer -Value $files.Count

        $dataOffset = 0
        foreach ($file in $files) {
            if ($file.RelativeName.Length -gt 255) {
                throw "XZP path is too long: $($file.RelativeName)"
            }

            Write-BigEndianInt32 -Writer $writer -Value $file.Length
            Write-BigEndianInt32 -Writer $writer -Value $dataOffset
            $writer.Write([byte]$file.RelativeName.Length)
            $writer.Write([System.Text.Encoding]::BigEndianUnicode.GetBytes($file.RelativeName))
            $dataOffset += $file.Length
        }

        $resourceTableLength = [int]$writer.BaseStream.Position - 22

        foreach ($file in $files) {
            $bytes = [System.IO.File]::ReadAllBytes($file.FullName)
            $writer.Write($bytes)
        }

        $fileSize = [int]$writer.BaseStream.Position
        $writer.BaseStream.Seek(8, [System.IO.SeekOrigin]::Begin) | Out-Null
        Write-BigEndianInt32 -Writer $writer -Value $fileSize
        $writer.BaseStream.Seek(16, [System.IO.SeekOrigin]::Begin) | Out-Null
        Write-BigEndianInt32 -Writer $writer -Value $resourceTableLength
    }
    finally {
        $writer.Dispose()
        $stream.Dispose()
    }
}

function Extract-Xzp {
    param([string]$ArchiveFile, [string]$DestinationDirectory)

    $archive = (Resolve-Path -LiteralPath $ArchiveFile).Path
    New-Item -ItemType Directory -Force -Path $DestinationDirectory | Out-Null
    $destination = (Resolve-Path -LiteralPath $DestinationDirectory).Path.TrimEnd('\')

    $stream = [System.IO.File]::OpenRead($archive)
    $reader = [System.IO.BinaryReader]::new($stream)

    try {
        $magic = [System.Text.Encoding]::ASCII.GetString($reader.ReadBytes(4))
        if ($magic -ne 'XUIZ') { throw 'Input is not an XZP/XUIZ archive.' }

        $archiveVersion = Read-BigEndianInt32 -Reader $reader
        if ($archiveVersion -ne 1) { throw "Unsupported XZP version: $archiveVersion" }

        $null = Read-BigEndianInt32 -Reader $reader
        $reader.BaseStream.Seek(4, [System.IO.SeekOrigin]::Current) | Out-Null
        $resourceTableLength = Read-BigEndianInt32 -Reader $reader
        $entryCount = Read-BigEndianInt16 -Reader $reader

        $entries = New-Object System.Collections.Generic.List[object]
        for ($index = 0; $index -lt $entryCount; $index++) {
            $length = Read-BigEndianInt32 -Reader $reader
            $offset = Read-BigEndianInt32 -Reader $reader
            $nameLength = $reader.ReadByte()
            $nameBytes = $reader.ReadBytes($nameLength * 2)
            $relativeName = [System.Text.Encoding]::BigEndianUnicode.GetString($nameBytes)

            if ([System.IO.Path]::IsPathRooted($relativeName) -or $relativeName.Contains('..')) {
                throw "Unsafe XZP entry path: $relativeName"
            }

            $entries.Add([pscustomobject]@{
                Length = $length
                Offset = $offset
                RelativeName = $relativeName
            })
        }

        $dataStart = 22 + $resourceTableLength
        foreach ($entry in $entries) {
            $target = Join-Path $destination $entry.RelativeName
            $targetDirectory = Split-Path -Parent $target
            New-Item -ItemType Directory -Force -Path $targetDirectory | Out-Null
            $reader.BaseStream.Seek($dataStart + $entry.Offset, [System.IO.SeekOrigin]::Begin) | Out-Null
            [System.IO.File]::WriteAllBytes($target, $reader.ReadBytes($entry.Length))
        }
    }
    finally {
        $reader.Dispose()
        $stream.Dispose()
    }
}

if ($Action -eq 'build') {
    Build-Xzp -SourceDirectory $InputPath -DestinationFile $OutputPath
}
else {
    Extract-Xzp -ArchiveFile $InputPath -DestinationDirectory $OutputPath
}
