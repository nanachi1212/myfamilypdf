[CmdletBinding()]
param(
    [string]$FixtureImage = '',
    [string]$PdfToolPath = '',
    [string]$TesseractPath = '',
    [string]$TessdataPath = '',
    [string]$OcrScriptPath = ''
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$utf8 = New-Object Text.UTF8Encoding($false)
[Console]::OutputEncoding = $utf8
$OutputEncoding = $utf8

$repositoryRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
if ([string]::IsNullOrWhiteSpace($FixtureImage)) {
    $FixtureImage = Join-Path $PSScriptRoot 'testdata\horizontal-traditional-simplified.png'
}
if ([string]::IsNullOrWhiteSpace($PdfToolPath)) {
    $PdfToolPath = Join-Path $repositoryRoot 'build\phase0-upstream-release\usr\bin\PdfTool.exe'
}
if ([string]::IsNullOrWhiteSpace($TesseractPath)) {
    $TesseractPath = Join-Path $repositoryRoot 'ocr-spike\vcpkg_installed\x64-windows\tools\tesseract\tesseract.exe'
}
if ([string]::IsNullOrWhiteSpace($TessdataPath)) {
    $TessdataPath = Join-Path $repositoryRoot 'ocr-spike\tessdata'
}
if ([string]::IsNullOrWhiteSpace($OcrScriptPath)) {
    $OcrScriptPath = Join-Path $PSScriptRoot 'FamilyPDF-OCR.ps1'
}

$FixtureImage = [IO.Path]::GetFullPath($FixtureImage)
$PdfToolPath = [IO.Path]::GetFullPath($PdfToolPath)
$TesseractPath = [IO.Path]::GetFullPath($TesseractPath)
$TessdataPath = [IO.Path]::GetFullPath($TessdataPath)
$OcrScriptPath = [IO.Path]::GetFullPath($OcrScriptPath)

foreach ($requiredPath in @($FixtureImage, $PdfToolPath, $TesseractPath, $OcrScriptPath)) {
    if (-not (Test-Path -LiteralPath $requiredPath -PathType Leaf)) {
        throw "Required horizontal OCR test file was not found: $requiredPath"
    }
}
if (-not (Test-Path -LiteralPath $TessdataPath -PathType Container)) {
    throw "Tessdata directory was not found: $TessdataPath"
}

# Keep the script ASCII-only so Windows PowerShell 5.1 can parse it correctly
# even when the checkout uses UTF-8 without a BOM.
$traditionalExpected = -join ([char[]]@(
    0x50B3,
    0x7D71,
    0x4E2D,
    0x6587,
    0x6E2C,
    0x8A66
))
$simplifiedExpected = -join ([char[]]@(
    0x7B80,
    0x4F53,
    0x4E2D,
    0x6587,
    0x6D4B,
    0x8BD5
))
$profiles = @(
    [pscustomobject]@{
        Language = 'chi_tra'
        Expected = $traditionalExpected
    },
    [pscustomobject]@{
        Language = 'chi_sim'
        Expected = $simplifiedExpected
    },
    [pscustomobject]@{
        Language = 'eng'
        Expected = 'FamilyPDFOCR2026'
    }
)
foreach ($profile in $profiles) {
    $modelPath = Join-Path $TessdataPath "$($profile.Language).traineddata"
    if (-not (Test-Path -LiteralPath $modelPath -PathType Leaf) -or
        (Get-Item -LiteralPath $modelPath).Length -le 1MB) {
        throw "Required horizontal OCR model is missing or invalid: $modelPath"
    }
}

function Get-PdfPageCount {
    param([string]$Tool, [string]$Path)

    $information = (& $Tool info $Path 2>&1) -join "`n"
    if ($LASTEXITCODE -ne 0 -or $information -notmatch 'Page count\s+([0-9,]+)') {
        throw "Cannot read PDF page count: $Path"
    }
    return [int]$Matches[1].Replace(',', '')
}

function Get-NormalizedText {
    param([string]$Text)
    return [Regex]::Replace($Text, '\s+', '')
}

$testRoot = Join-Path ([IO.Path]::GetTempPath()) (
    'FamilyPDF-OCR-Horizontal-Test-' + [Guid]::NewGuid().ToString('N')
)
New-Item -ItemType Directory -Path $testRoot | Out-Null

try {
    foreach ($profile in $profiles) {
        $outputBase = Join-Path $testRoot $profile.Language
        & $TesseractPath `
            $FixtureImage `
            $outputBase `
            --tessdata-dir $TessdataPath `
            -l $profile.Language `
            --psm 6 `
            -c 'tessedit_create_txt=1'
        if ($LASTEXITCODE -ne 0) {
            throw "Direct horizontal OCR failed for $($profile.Language)."
        }

        $textPath = "$outputBase.txt"
        if (-not (Test-Path -LiteralPath $textPath -PathType Leaf)) {
            throw "Horizontal OCR did not create text for $($profile.Language)."
        }
        $recognized = Get-NormalizedText ([IO.File]::ReadAllText(
            $textPath,
            [Text.Encoding]::UTF8
        ))
        if (-not $recognized.Contains($profile.Expected)) {
            throw "Horizontal OCR text mismatch for $($profile.Language). Expected '$($profile.Expected)', got '$recognized'."
        }
        Write-Host "Direct horizontal OCR passed: $($profile.Language) -> $($profile.Expected)"
    }

    $sourceBase = Join-Path $testRoot 'horizontal-source'
    & $TesseractPath `
        $FixtureImage `
        $sourceBase `
        --tessdata-dir $TessdataPath `
        -l eng `
        --psm 6 `
        -c 'tessedit_create_pdf=1'
    if ($LASTEXITCODE -ne 0) {
        throw 'Could not create the horizontal OCR PDF fixture.'
    }

    $sourcePdf = "$sourceBase.pdf"
    $outputPdf = Join-Path $testRoot 'horizontal-searchable.pdf'
    $outputText = Join-Path $testRoot 'horizontal-searchable.txt'
    $sourceHash = (Get-FileHash -LiteralPath $sourcePdf -Algorithm SHA256).Hash

    & $OcrScriptPath `
        -InputPdf $sourcePdf `
        -OutputPdf $outputPdf `
        -OutputText $outputText `
        -Languages 'chi_tra+chi_sim+eng' `
        -PageSegmentationMode 6 `
        -Dpi 240 `
        -PdfToolPath $PdfToolPath `
        -TesseractPath $TesseractPath `
        -TessdataPath $TessdataPath
    if ($LASTEXITCODE -ne 0) {
        throw 'FamilyPDF horizontal OCR pipeline failed.'
    }

    if ((Get-FileHash -LiteralPath $sourcePdf -Algorithm SHA256).Hash -ne $sourceHash) {
        throw 'FamilyPDF horizontal OCR changed its source PDF.'
    }
    if (-not (Test-Path -LiteralPath $outputPdf -PathType Leaf) -or
        -not (Test-Path -LiteralPath $outputText -PathType Leaf)) {
        throw 'FamilyPDF horizontal OCR did not create both output files.'
    }
    if ((Get-PdfPageCount -Tool $PdfToolPath -Path $sourcePdf) -ne
        (Get-PdfPageCount -Tool $PdfToolPath -Path $outputPdf)) {
        throw 'FamilyPDF horizontal OCR changed the PDF page count.'
    }

    $fetchedText = Get-NormalizedText ((
        & $PdfToolPath fetch-text --text-codec utf8 $outputPdf 2>&1
    ) -join "`n")
    if ($LASTEXITCODE -ne 0) {
        throw 'Could not extract the FamilyPDF horizontal OCR text layer.'
    }
    if (-not $fetchedText.Contains('FamilyPDFOCR2026')) {
        throw "The horizontal OCR PDF is missing its searchable English marker. Got '$fetchedText'."
    }
    # PDF4QT's layout extractor can reorder adjacent positioned CJK glyphs.
    # Requiring every expected glyph proves the Chinese text layer survived
    # the page-PDF merge, while the UTF-8 sidecar below verifies exact order.
    foreach ($expected in @($traditionalExpected, $simplifiedExpected)) {
        foreach ($character in $expected.ToCharArray()) {
            if (-not $fetchedText.Contains([string]$character)) {
                throw "The horizontal OCR PDF is missing searchable glyph '$character'. Got '$fetchedText'."
            }
        }
    }

    $plainText = Get-NormalizedText ([IO.File]::ReadAllText(
        $outputText,
        [Text.Encoding]::UTF8
    ))
    foreach ($expected in @($traditionalExpected, $simplifiedExpected, 'FamilyPDFOCR2026')) {
        if (-not $plainText.Contains($expected)) {
            throw "The horizontal OCR text output is missing '$expected'."
        }
    }

    Write-Host 'FamilyPDF horizontal Traditional/Simplified searchable PDF test passed.'
}
finally {
    if (Test-Path -LiteralPath $testRoot -PathType Container) {
        $resolvedTestRoot = [IO.Path]::GetFullPath($testRoot)
        $systemTemp = [IO.Path]::GetFullPath([IO.Path]::GetTempPath())
        if ($resolvedTestRoot.StartsWith($systemTemp, [StringComparison]::OrdinalIgnoreCase) -and
            [IO.Path]::GetFileName($resolvedTestRoot).StartsWith(
                'FamilyPDF-OCR-Horizontal-Test-',
                [StringComparison]::Ordinal
            )) {
            Remove-Item -LiteralPath $resolvedTestRoot -Recurse -Force
        }
    }
}
