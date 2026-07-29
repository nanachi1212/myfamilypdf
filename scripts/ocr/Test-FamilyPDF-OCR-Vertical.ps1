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
    $FixtureImage = Join-Path $PSScriptRoot 'testdata\vertical-traditional-simplified.png'
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
        throw "Required vertical OCR test file was not found: $requiredPath"
    }
}
if (-not (Test-Path -LiteralPath $TessdataPath -PathType Container)) {
    throw "Tessdata directory was not found: $TessdataPath"
}

$profiles = @(
    [pscustomobject]@{
        Language = 'chi_tra_vert'
        Expected = -join ([char[]]@(
            0x7E41,
            0x9AD4,
            0x4E2D,
            0x6587,
            0x6E2C,
            0x8A66
        ))
    },
    [pscustomobject]@{
        Language = 'chi_sim_vert'
        Expected = -join ([char[]]@(
            0x7B80,
            0x4F53,
            0x4E2D,
            0x6587,
            0x6D4B,
            0x8BD5
        ))
    }
)
foreach ($profile in $profiles) {
    $modelPath = Join-Path $TessdataPath "$($profile.Language).traineddata"
    if (-not (Test-Path -LiteralPath $modelPath -PathType Leaf) -or
        (Get-Item -LiteralPath $modelPath).Length -le 1MB) {
        throw "Required vertical OCR model is missing or invalid: $modelPath"
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
    'FamilyPDF-OCR-Vertical-Test-' + [Guid]::NewGuid().ToString('N')
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
            --psm 5 `
            -c 'tessedit_create_txt=1'
        if ($LASTEXITCODE -ne 0) {
            throw "Direct vertical OCR failed for $($profile.Language)."
        }

        $textPath = "$outputBase.txt"
        if (-not (Test-Path -LiteralPath $textPath -PathType Leaf)) {
            throw "Vertical OCR did not create text for $($profile.Language)."
        }
        $recognized = Get-NormalizedText ([IO.File]::ReadAllText(
            $textPath,
            [Text.Encoding]::UTF8
        ))
        if (-not $recognized.Contains($profile.Expected)) {
            throw "Vertical OCR text mismatch for $($profile.Language). Expected '$($profile.Expected)', got '$recognized'."
        }
        Write-Host "Direct vertical OCR passed: $($profile.Language) -> $($profile.Expected)"
    }

    # Tesseract wraps the inspected PNG in a one-page PDF. The existing text
    # layer is irrelevant because FamilyPDF renders the page before OCR.
    $sourceBase = Join-Path $testRoot 'vertical-source'
    & $TesseractPath `
        $FixtureImage `
        $sourceBase `
        --tessdata-dir $TessdataPath `
        -l eng `
        --psm 6 `
        -c 'tessedit_create_pdf=1'
    if ($LASTEXITCODE -ne 0) {
        throw 'Could not create the vertical OCR PDF fixture.'
    }

    $sourcePdf = "$sourceBase.pdf"
    $outputPdf = Join-Path $testRoot 'vertical-searchable.pdf'
    $outputText = Join-Path $testRoot 'vertical-searchable.txt'
    $sourceHash = (Get-FileHash -LiteralPath $sourcePdf -Algorithm SHA256).Hash

    & $OcrScriptPath `
        -InputPdf $sourcePdf `
        -OutputPdf $outputPdf `
        -OutputText $outputText `
        -Languages 'chi_tra_vert+chi_sim_vert+eng' `
        -PageSegmentationMode 5 `
        -Dpi 240 `
        -PdfToolPath $PdfToolPath `
        -TesseractPath $TesseractPath `
        -TessdataPath $TessdataPath
    if ($LASTEXITCODE -ne 0) {
        throw 'FamilyPDF vertical OCR pipeline failed.'
    }

    if ((Get-FileHash -LiteralPath $sourcePdf -Algorithm SHA256).Hash -ne $sourceHash) {
        throw 'FamilyPDF vertical OCR changed its source PDF.'
    }
    if (-not (Test-Path -LiteralPath $outputPdf -PathType Leaf) -or
        -not (Test-Path -LiteralPath $outputText -PathType Leaf)) {
        throw 'FamilyPDF vertical OCR did not create both output files.'
    }
    if ((Get-PdfPageCount -Tool $PdfToolPath -Path $sourcePdf) -ne
        (Get-PdfPageCount -Tool $PdfToolPath -Path $outputPdf)) {
        throw 'FamilyPDF vertical OCR changed the PDF page count.'
    }

    $fetchedText = Get-NormalizedText ((
        & $PdfToolPath fetch-text --text-codec utf8 $outputPdf 2>&1
    ) -join "`n")
    if ($LASTEXITCODE -ne 0 -or -not $fetchedText.Contains('FamilyPDF')) {
        throw "The vertical OCR PDF does not contain the searchable English marker: $fetchedText"
    }
    foreach ($profile in $profiles) {
        foreach ($character in $profile.Expected.ToCharArray()) {
            if (-not $fetchedText.Contains([string]$character)) {
                throw "The vertical OCR PDF is missing searchable glyph '$character'."
            }
        }
    }

    $plainText = Get-NormalizedText ([IO.File]::ReadAllText(
        $outputText,
        [Text.Encoding]::UTF8
    ))
    foreach ($profile in $profiles) {
        if (-not $plainText.Contains($profile.Expected)) {
            throw "The vertical OCR text output is missing '$($profile.Expected)'."
        }
    }

    Write-Host 'FamilyPDF vertical Traditional/Simplified searchable PDF test passed.'
}
finally {
    if (Test-Path -LiteralPath $testRoot -PathType Container) {
        $resolvedTestRoot = [IO.Path]::GetFullPath($testRoot)
        $systemTemp = [IO.Path]::GetFullPath([IO.Path]::GetTempPath())
        if ($resolvedTestRoot.StartsWith($systemTemp, [StringComparison]::OrdinalIgnoreCase) -and
            [IO.Path]::GetFileName($resolvedTestRoot).StartsWith(
                'FamilyPDF-OCR-Vertical-Test-',
                [StringComparison]::Ordinal
            )) {
            Remove-Item -LiteralPath $resolvedTestRoot -Recurse -Force
        }
    }
}
