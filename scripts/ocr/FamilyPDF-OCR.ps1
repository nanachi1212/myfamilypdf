[CmdletBinding()]
param(
    [Parameter(Mandatory = $true, Position = 0)]
    [ValidateScript({ Test-Path -LiteralPath $_ -PathType Leaf })]
    [string]$InputPdf,

    [Parameter(Position = 1)]
    [string]$OutputText = '',

    [ValidatePattern('^[A-Za-z0-9_+.-]+$')]
    [string]$Languages = 'chi_tra+eng',

    [ValidatePattern('^[0-9,.-]+$')]
    [string]$Pages = '',

    [ValidateRange(72, 600)]
    [int]$Dpi = 300,

    [ValidateRange(1, 13)]
    [int]$PageSegmentationMode = 3,

    [switch]$KeepPageImages,

    [string]$PdfToolPath = '',

    [string]$TesseractPath = '',

    [string]$TessdataPath = ''
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

function Resolve-FirstExistingFile {
    param([string[]]$Candidates)

    foreach ($candidate in $Candidates) {
        if (-not [string]::IsNullOrWhiteSpace($candidate) -and
            (Test-Path -LiteralPath $candidate -PathType Leaf)) {
            return (Resolve-Path -LiteralPath $candidate).Path
        }
    }

    return $null
}

function Resolve-FirstExistingDirectory {
    param([string[]]$Candidates)

    foreach ($candidate in $Candidates) {
        if (-not [string]::IsNullOrWhiteSpace($candidate) -and
            (Test-Path -LiteralPath $candidate -PathType Container)) {
            return (Resolve-Path -LiteralPath $candidate).Path
        }
    }

    return $null
}

$inputPath = (Resolve-Path -LiteralPath $InputPdf).Path
if ([IO.Path]::GetExtension($inputPath) -ine '.pdf') {
    throw "Input file must be a PDF: $inputPath"
}

if ([string]::IsNullOrWhiteSpace($OutputText)) {
    $OutputText = [IO.Path]::Combine(
        [IO.Path]::GetDirectoryName($inputPath),
        ([IO.Path]::GetFileNameWithoutExtension($inputPath) + '.ocr.txt')
    )
}
$outputPath = [IO.Path]::GetFullPath($OutputText)

$repositoryRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$pdfTool = Resolve-FirstExistingFile @(
    $PdfToolPath,
    (Join-Path $PSScriptRoot 'PdfTool.exe'),
    (Join-Path $repositoryRoot 'dist\FamilyPDF-windows-x64\PdfTool.exe'),
    (Join-Path $repositoryRoot 'build\phase0-upstream-release\usr\bin\PdfTool.exe')
)
$tesseract = Resolve-FirstExistingFile @(
    $TesseractPath,
    (Join-Path $PSScriptRoot 'ocr\tesseract.exe'),
    (Join-Path $repositoryRoot 'ocr-spike\vcpkg_installed\x64-windows\tools\tesseract\tesseract.exe')
)
$tessdata = Resolve-FirstExistingDirectory @(
    $TessdataPath,
    (Join-Path $PSScriptRoot 'ocr\tessdata'),
    (Join-Path $repositoryRoot 'ocr-spike\tessdata')
)

if (-not $pdfTool) {
    throw 'PdfTool.exe was not found. Rebuild or unpack the complete FamilyPDF package.'
}
if (-not $tesseract) {
    throw 'tesseract.exe was not found. Run scripts\phase0\package-windows-runtime.ps1 to download and package OCR dependencies.'
}
if (-not $tessdata) {
    throw 'OCR language data was not found. Run ocr-spike\download-tessdata.ps1.'
}

foreach ($language in $Languages.Split('+', [StringSplitOptions]::RemoveEmptyEntries)) {
    $languageFile = Join-Path $tessdata "$language.traineddata"
    if (-not (Test-Path -LiteralPath $languageFile -PathType Leaf)) {
        throw "OCR language data is missing: $languageFile"
    }
}

$outputDirectory = [IO.Path]::GetDirectoryName($outputPath)
if (-not (Test-Path -LiteralPath $outputDirectory -PathType Container)) {
    New-Item -ItemType Directory -Path $outputDirectory -Force | Out-Null
}

$temporaryRoot = [IO.Path]::Combine(
    [IO.Path]::GetTempPath(),
    ('FamilyPDF-OCR-' + [Guid]::NewGuid().ToString('N'))
)
New-Item -ItemType Directory -Path $temporaryRoot | Out-Null

try {
    Write-Host "Rendering PDF pages at $Dpi DPI..."
    $renderArguments = @(
        'render',
        '--image-output-dir', $temporaryRoot,
        '--image-template-fn', 'page-%',
        '--image-format', 'png',
        '--image-res-mode', 'dpi',
        '--image-res-dpi', $Dpi,
        '--render-hw-accel', '0'
    )
    if (-not [string]::IsNullOrWhiteSpace($Pages)) {
        $renderArguments += @('--page-select', $Pages)
    }
    $renderArguments += $inputPath

    & $pdfTool @renderArguments
    if ($LASTEXITCODE -ne 0) {
        throw "PdfTool rendering failed with exit code $LASTEXITCODE."
    }

    $pageImages = @(
        Get-ChildItem -LiteralPath $temporaryRoot -Filter 'page-*.png' -File |
            Sort-Object {
                if ($_.BaseName -match '(\d+)$') { [int]$Matches[1] } else { [int]::MaxValue }
            }
    )
    if ($pageImages.Count -eq 0) {
        throw 'PdfTool did not render any page images.'
    }

    $utf8NoBom = [Text.UTF8Encoding]::new($false)
    $pageTexts = [Collections.Generic.List[string]]::new()
    for ($index = 0; $index -lt $pageImages.Count; $index++) {
        $image = $pageImages[$index]
        $pageNumber = if ($image.BaseName -match '(\d+)$') { [int]$Matches[1] } else { $index + 1 }
        $percent = [int](($index / $pageImages.Count) * 100)
        Write-Progress -Activity 'FamilyPDF OCR' -Status "Page $pageNumber ($($index + 1)/$($pageImages.Count))" -PercentComplete $percent

        $ocrBase = Join-Path $temporaryRoot ("ocr-$pageNumber")
        & $tesseract $image.FullName $ocrBase --tessdata-dir $tessdata -l $Languages --psm $PageSegmentationMode
        if ($LASTEXITCODE -ne 0) {
            throw "Tesseract failed on page $pageNumber with exit code $LASTEXITCODE."
        }

        $pageTextFile = "$ocrBase.txt"
        $recognizedText = if (Test-Path -LiteralPath $pageTextFile) {
            [IO.File]::ReadAllText($pageTextFile, [Text.Encoding]::UTF8).TrimEnd()
        } else {
            ''
        }
        $pageTexts.Add("===== Page $pageNumber =====`r`n$recognizedText")
    }
    Write-Progress -Activity 'FamilyPDF OCR' -Completed

    [IO.File]::WriteAllText($outputPath, ($pageTexts -join "`r`n`r`n"), $utf8NoBom)
    Write-Host "OCR text saved: $outputPath"

    if ($KeepPageImages) {
        $imageOutput = "$outputPath.pages"
        if (Test-Path -LiteralPath $imageOutput) {
            throw "Cannot preserve page images because the target already exists: $imageOutput"
        }
        Move-Item -LiteralPath $temporaryRoot -Destination $imageOutput
        $temporaryRoot = ''
        Write-Host "Rendered page images saved: $imageOutput"
    }
}
finally {
    if (-not [string]::IsNullOrWhiteSpace($temporaryRoot) -and
        (Test-Path -LiteralPath $temporaryRoot -PathType Container)) {
        $resolvedTemp = [IO.Path]::GetFullPath($temporaryRoot)
        $systemTemp = [IO.Path]::GetFullPath([IO.Path]::GetTempPath())
        if ($resolvedTemp.StartsWith($systemTemp, [StringComparison]::OrdinalIgnoreCase) -and
            [IO.Path]::GetFileName($resolvedTemp).StartsWith('FamilyPDF-OCR-', [StringComparison]::Ordinal)) {
            Remove-Item -LiteralPath $resolvedTemp -Recurse -Force
        }
    }
}
