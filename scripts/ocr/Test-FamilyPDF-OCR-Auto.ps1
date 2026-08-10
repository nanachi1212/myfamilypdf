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

foreach ($requiredFile in @($FixtureImage, $PdfToolPath, $TesseractPath, $OcrScriptPath)) {
    if (-not (Test-Path -LiteralPath $requiredFile -PathType Leaf)) {
        throw "Required auto OCR test file was not found: $requiredFile"
    }
}
if (-not (Test-Path -LiteralPath $TessdataPath -PathType Container)) {
    throw "Tessdata directory was not found: $TessdataPath"
}

$testRoot = Join-Path ([IO.Path]::GetTempPath()) (
    'FamilyPDF-OCR-Auto-Test-' + [Guid]::NewGuid().ToString('N')
)
New-Item -ItemType Directory -Path $testRoot | Out-Null

try {
    Add-Type -AssemblyName System.Drawing
    $sourceImage = [Drawing.Bitmap]::FromFile((Resolve-Path -LiteralPath $FixtureImage).Path)
    try {
        $fixtures = @(
            [pscustomobject]@{
                Name = 'traditional'
                Rectangle = [Drawing.Rectangle]::new(0, 0, $sourceImage.Width, 300)
                ExpectedLanguages = 'chi_tra+eng'
                Blank = $false
            },
            [pscustomobject]@{
                Name = 'simplified'
                Rectangle = [Drawing.Rectangle]::new(0, 300, $sourceImage.Width, 310)
                ExpectedLanguages = 'chi_sim+eng'
                Blank = $false
            },
            [pscustomobject]@{
                Name = 'mixed'
                Rectangle = [Drawing.Rectangle]::new(0, 0, $sourceImage.Width, $sourceImage.Height)
                ExpectedLanguages = 'chi_tra+chi_sim+eng'
                Blank = $false
            },
            [pscustomobject]@{
                Name = 'blank'
                Rectangle = [Drawing.Rectangle]::new(0, 0, $sourceImage.Width, 300)
                ExpectedLanguages = 'chi_tra+eng'
                Blank = $true
            }
        )

        $sourcePages = [Collections.Generic.List[string]]::new()
        foreach ($fixture in $fixtures) {
            $fixturePng = Join-Path $testRoot "$($fixture.Name).png"
            if ($fixture.Blank) {
                $cropped = [Drawing.Bitmap]::new(
                    $fixture.Rectangle.Width,
                    $fixture.Rectangle.Height
                )
                $graphics = [Drawing.Graphics]::FromImage($cropped)
                try {
                    $graphics.Clear([Drawing.Color]::White)
                }
                finally {
                    $graphics.Dispose()
                }
            }
            else {
                $cropped = $sourceImage.Clone($fixture.Rectangle, $sourceImage.PixelFormat)
            }
            try {
                $cropped.Save($fixturePng, [Drawing.Imaging.ImageFormat]::Png)
            }
            finally {
                $cropped.Dispose()
            }

            $fixtureBase = Join-Path $testRoot $fixture.Name
            & $TesseractPath `
                $fixturePng `
                $fixtureBase `
                --tessdata-dir $TessdataPath `
                -l eng `
                --psm 6 `
                -c 'tessedit_create_pdf=1'
            if ($LASTEXITCODE -ne 0) {
                throw "Could not create the $($fixture.Name) PDF fixture."
            }
            $sourcePages.Add("$fixtureBase.pdf")
        }
    }
    finally {
        $sourceImage.Dispose()
    }

    $sourcePdf = Join-Path $testRoot 'auto-source.pdf'
    & $PdfToolPath unite @($sourcePages.ToArray()) $sourcePdf
    if ($LASTEXITCODE -ne 0) {
        throw 'Could not create the multi-page auto OCR fixture.'
    }

    $outputPdf = Join-Path $testRoot 'auto-searchable.pdf'
    $outputText = Join-Path $testRoot 'auto-searchable.txt'
    $outputReport = Join-Path $testRoot 'auto-searchable.ocr-report.json'
    $sourceHash = (Get-FileHash -LiteralPath $sourcePdf -Algorithm SHA256).Hash

    foreach ($collision in @(
        [pscustomobject]@{ Name = 'OutputText'; Parameters = @{ OutputText = $sourcePdf } },
        [pscustomobject]@{ Name = 'OutputReport'; Parameters = @{ OutputReport = $sourcePdf } },
        [pscustomobject]@{
            Name = 'OutputText/OutputReport'
            Parameters = @{
                OutputText = $outputText
                OutputReport = $outputText
            }
        },
        [pscustomobject]@{
            Name = 'OutputText/PageImages'
            Parameters = @{
                OutputText = "$outputPdf.pages"
                KeepPageImages = $true
            }
        }
    )) {
        $collisionRejected = $false
        $collisionParameters = @{
            InputPdf = $sourcePdf
            OutputPdf = $outputPdf
            Mode = 'Auto'
            PdfToolPath = $PdfToolPath
            TesseractPath = $TesseractPath
            TessdataPath = $TessdataPath
        }
        foreach ($parameterName in $collision.Parameters.Keys) {
            $collisionParameters[$parameterName] = $collision.Parameters[$parameterName]
        }
        try {
            & $OcrScriptPath @collisionParameters
        }
        catch {
            $collisionRejected = $_.Exception.Message.Contains('must use different paths')
        }
        if (-not $collisionRejected) {
            throw "FamilyPDF auto OCR did not reject the $($collision.Name) path collision."
        }
        if ((Get-FileHash -LiteralPath $sourcePdf -Algorithm SHA256).Hash -ne $sourceHash) {
            throw "FamilyPDF auto OCR changed its source while rejecting $($collision.Name)."
        }
    }

    & $OcrScriptPath `
        -InputPdf $sourcePdf `
        -OutputPdf $outputPdf `
        -OutputText $outputText `
        -Mode Auto `
        -Dpi 240 `
        -PdfToolPath $PdfToolPath `
        -TesseractPath $TesseractPath `
        -TessdataPath $TessdataPath
    if ($LASTEXITCODE -ne 0) {
        throw 'FamilyPDF auto OCR pipeline failed.'
    }

    if ((Get-FileHash -LiteralPath $sourcePdf -Algorithm SHA256).Hash -ne $sourceHash) {
        throw 'FamilyPDF auto OCR changed its source PDF.'
    }
    foreach ($output in @($outputPdf, $outputText, $outputReport)) {
        if (-not (Test-Path -LiteralPath $output -PathType Leaf)) {
            throw "FamilyPDF auto OCR did not create: $output"
        }
    }

    $report = Get-Content -LiteralPath $outputReport -Raw -Encoding UTF8 | ConvertFrom-Json
    if ($report.schemaVersion -ne 1 -or $report.mode -ne 'Auto') {
        throw 'FamilyPDF auto OCR report has an invalid schema or mode.'
    }
    if ($report.PSObject.Properties.Name -contains 'inputFile' -or
        $report.PSObject.Properties.Name -contains 'outputFile') {
        throw 'FamilyPDF auto OCR report disclosed input or output file names.'
    }
    if (@($report.pages).Count -ne 4) {
        throw "FamilyPDF auto OCR report expected 4 pages, got $(@($report.pages).Count)."
    }
    if ($report.pages[0].languages -ne 'chi_tra+eng') {
        throw "Traditional page selected '$($report.pages[0].languages)' instead of 'chi_tra+eng'."
    }
    if ($report.pages[1].languages -ne 'chi_sim+eng') {
        throw "Simplified page selected '$($report.pages[1].languages)' instead of 'chi_sim+eng'."
    }
    if ($report.pages[2].languages -ne 'chi_tra+chi_sim+eng') {
        throw "Mixed page selected '$($report.pages[2].languages)' instead of 'chi_tra+chi_sim+eng'."
    }
    if (-not $report.pages[3].needsReview -or
        @($report.pages[3].warnings) -notcontains 'low-confidence') {
        throw 'Blank page was not reported as low-confidence and needing review.'
    }
    if (@($report.summary.reviewPages) -notcontains 4) {
        throw 'Blank page was not included in the report reviewPages summary.'
    }
    foreach ($page in $report.pages) {
        if ($page.confidence -lt 0 -or $page.confidence -gt 100) {
            throw "Page $($page.page) reported invalid OCR confidence $($page.confidence)."
        }
        if (@($page.candidates).Count -lt 2) {
            throw "Page $($page.page) did not report both automatic language candidates."
        }
    }

    $partialPdf = Join-Path $testRoot 'partial-searchable.pdf'
    $partialReport = Join-Path $testRoot 'partial-searchable.ocr-report.json'
    & $OcrScriptPath `
        -InputPdf $sourcePdf `
        -OutputPdf $partialPdf `
        -OutputReport $partialReport `
        -Mode Auto `
        -Pages '1-2' `
        -KeepPageImages `
        -Dpi 240 `
        -PdfToolPath $PdfToolPath `
        -TesseractPath $TesseractPath `
        -TessdataPath $TessdataPath
    if ($LASTEXITCODE -ne 0) {
        throw 'FamilyPDF partial-page OCR pipeline failed.'
    }
    $partial = Get-Content -LiteralPath $partialReport -Raw -Encoding UTF8 |
        ConvertFrom-Json
    $pageImageDirectory = "$partialPdf.pages"
    if ($partial.summary.pages -ne 2 -or
        @($partial.pages).Count -ne 2 -or
        @(Get-ChildItem -LiteralPath $pageImageDirectory -Filter '*.png' -File).Count -ne 2) {
        throw 'Partial-page OCR did not preserve two report entries and two page images.'
    }

    Write-Host 'FamilyPDF automatic Traditional/Simplified selection test passed.'
}
finally {
    if (Test-Path -LiteralPath $testRoot -PathType Container) {
        $resolvedTestRoot = [IO.Path]::GetFullPath($testRoot)
        $systemTemp = [IO.Path]::GetFullPath([IO.Path]::GetTempPath())
        if ($resolvedTestRoot.StartsWith($systemTemp, [StringComparison]::OrdinalIgnoreCase) -and
            [IO.Path]::GetFileName($resolvedTestRoot).StartsWith(
                'FamilyPDF-OCR-Auto-Test-',
                [StringComparison]::Ordinal
            )) {
            Remove-Item -LiteralPath $resolvedTestRoot -Recurse -Force
        }
    }
}
