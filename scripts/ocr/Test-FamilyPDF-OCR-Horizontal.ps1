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
    $outputReport = Join-Path $testRoot 'horizontal-searchable.ocr-report.json'
    $sourceHash = (Get-FileHash -LiteralPath $sourcePdf -Algorithm SHA256).Hash

    $failedOutputPdf = Join-Path $testRoot 'must-not-exist.pdf'
    $invalidTextTarget = Join-Path $testRoot 'text-target-is-a-directory'
    New-Item -ItemType Directory -Path $invalidTextTarget | Out-Null
    $invalidTargetRejected = $false
    try {
        & $OcrScriptPath `
            -InputPdf $sourcePdf `
            -OutputPdf $failedOutputPdf `
            -OutputText $invalidTextTarget `
            -Mode Traditional `
            -PdfToolPath $PdfToolPath `
            -TesseractPath $TesseractPath `
            -TessdataPath $TessdataPath
    }
    catch {
        $invalidTargetRejected = $_.Exception.Message -match 'directory'
    }
    if (-not $invalidTargetRejected) {
        throw 'FamilyPDF OCR did not reject an output path that is a directory.'
    }
    if (Test-Path -LiteralPath $failedOutputPdf) {
        throw 'FamilyPDF OCR left a PDF after rejecting an invalid sidecar target.'
    }
    if ((Get-FileHash -LiteralPath $sourcePdf -Algorithm SHA256).Hash -ne $sourceHash) {
        throw 'FamilyPDF OCR changed its source while rejecting an invalid target.'
    }

    & $OcrScriptPath `
        -InputPdf $sourcePdf `
        -OutputPdf $outputPdf `
        -OutputText $outputText `
        -OutputReport $outputReport `
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
        -not (Test-Path -LiteralPath $outputText -PathType Leaf) -or
        -not (Test-Path -LiteralPath $outputReport -PathType Leaf)) {
        throw 'FamilyPDF horizontal OCR did not create every requested output file.'
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

    $report = Get-Content -LiteralPath $outputReport -Raw -Encoding UTF8 |
        ConvertFrom-Json
    if (@($report.pages).Count -ne 1) {
        throw 'Fixed-language OCR report did not include its processed page.'
    }
    if ($report.pages[0].languages -ne 'chi_tra+chi_sim+eng' -or
        $report.pages[0].psm -ne 6) {
        throw 'Fixed-language OCR report did not record its language and PSM.'
    }
    if ($report.PSObject.Properties.Name -contains 'inputFile' -or
        $report.PSObject.Properties.Name -contains 'outputFile') {
        throw 'OCR report disclosed input or output file names.'
    }

    $repairRoot = Join-Path $testRoot 'corrupt-model-repair'
    $repairOcrRoot = Join-Path $repairRoot 'ocr'
    $repairData = Join-Path $repairOcrRoot 'tessdata'
    $repairSource = Join-Path $repairRoot 'verified-source'
    New-Item -ItemType Directory -Path $repairData -Force | Out-Null
    New-Item -ItemType Directory -Path $repairSource -Force | Out-Null
    Copy-Item -LiteralPath $OcrScriptPath `
        -Destination (Join-Path $repairRoot 'FamilyPDF-OCR.ps1')
    Copy-Item -LiteralPath (Join-Path $repositoryRoot 'ocr-spike\download-tessdata.ps1') `
        -Destination (Join-Path $repairOcrRoot 'Install-OCR-Languages.ps1')
    foreach ($language in @('eng', 'chi_tra')) {
        Copy-Item -LiteralPath (Join-Path $TessdataPath "$language.traineddata") `
            -Destination $repairSource
        Copy-Item -LiteralPath (Join-Path $TessdataPath "$language.traineddata") `
            -Destination $repairData
    }
    $corruptTraditional = Join-Path $repairData 'chi_tra.traineddata'
    $corruptStream = [IO.File]::Open($corruptTraditional, [IO.FileMode]::Open)
    try {
        $corruptStream.SetLength($corruptStream.Length - 1)
    }
    finally {
        $corruptStream.Dispose()
    }
    $repairLanguages = [ordered]@{}
    foreach ($language in @('eng', 'chi_tra')) {
        $model = Get-Item -LiteralPath (Join-Path $repairSource "$language.traineddata")
        $repairLanguages[$language] = [ordered]@{
            bytes = $model.Length
            sha256 = (Get-FileHash -LiteralPath $model.FullName -Algorithm SHA256).Hash
        }
    }
    $repairManifest = [ordered]@{
        schemaVersion = 1
        repository = 'local-test-fixture'
        commit = 'test-fixture-commit'
        baseUrl = $repairSource
        languages = $repairLanguages
    }
    $repairManifest | ConvertTo-Json -Depth 5 |
        Set-Content -LiteralPath (Join-Path $repairOcrRoot 'tessdata-manifest.json') `
            -Encoding UTF8
    $repairOutput = Join-Path $testRoot 'repaired-model-output.pdf'
    & (Join-Path $repairRoot 'FamilyPDF-OCR.ps1') `
        -InputPdf $sourcePdf `
        -OutputPdf $repairOutput `
        -Mode Traditional `
        -Dpi 240 `
        -PdfToolPath $PdfToolPath `
        -TesseractPath $TesseractPath `
        -TessdataPath $repairData
    if ($LASTEXITCODE -ne 0 -or
        -not (Test-Path -LiteralPath $repairOutput -PathType Leaf)) {
        throw 'FamilyPDF OCR did not repair a corrupt language model.'
    }
    $verifiedTraditional = Join-Path $repairSource 'chi_tra.traineddata'
    if ((Get-FileHash -LiteralPath $corruptTraditional -Algorithm SHA256).Hash -ne
        (Get-FileHash -LiteralPath $verifiedTraditional -Algorithm SHA256).Hash) {
        throw 'FamilyPDF OCR did not replace the corrupt language model.'
    }

    $customData = Join-Path $testRoot 'custom-tessdata'
    New-Item -ItemType Directory -Path $customData | Out-Null
    Copy-Item -LiteralPath (Join-Path $TessdataPath 'eng.traineddata') `
        -Destination (Join-Path $customData 'family_eng.traineddata')
    $customOutput = Join-Path $testRoot 'custom-language-output.pdf'
    & $OcrScriptPath `
        -InputPdf $sourcePdf `
        -OutputPdf $customOutput `
        -Languages family_eng `
        -PageSegmentationMode 6 `
        -Dpi 240 `
        -PdfToolPath $PdfToolPath `
        -TesseractPath $TesseractPath `
        -TessdataPath $customData
    if ($LASTEXITCODE -ne 0 -or
        -not (Test-Path -LiteralPath $customOutput -PathType Leaf)) {
        throw 'FamilyPDF OCR rejected an installed custom language model.'
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
