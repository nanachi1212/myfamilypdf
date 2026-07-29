[CmdletBinding()]
param(
    [Parameter(Mandatory = $true, Position = 0)]
    [ValidateScript({ Test-Path -LiteralPath $_ -PathType Leaf })]
    [string]$InputPdf,

    [Parameter(Position = 1)]
    [string]$OutputPdf = '',

    [string]$OutputText = '',

    [ValidatePattern('^[A-Za-z0-9_+.-]+$')]
    [string]$Languages = 'chi_tra+chi_sim+eng',

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

function Get-PdfPageCount {
    param([string]$Tool, [string]$Path)

    $information = (& $Tool info $Path 2>&1) -join "`n"
    if ($LASTEXITCODE -ne 0 -or $information -notmatch 'Page count\s+([0-9,]+)') {
        throw "Cannot validate PDF page count: $Path"
    }
    return [int]$Matches[1].Replace(',', '')
}

$inputPath = (Resolve-Path -LiteralPath $InputPdf).Path
if ([IO.Path]::GetExtension($inputPath) -ine '.pdf') {
    throw "Input file must be a PDF: $inputPath"
}

if ([string]::IsNullOrWhiteSpace($OutputPdf)) {
    $OutputPdf = [IO.Path]::Combine(
        [IO.Path]::GetDirectoryName($inputPath),
        ([IO.Path]::GetFileNameWithoutExtension($inputPath) + '.ocr.pdf')
    )
}
$outputPath = [IO.Path]::GetFullPath($OutputPdf)
if ([string]::Equals($inputPath, $outputPath, [StringComparison]::OrdinalIgnoreCase)) {
    throw 'OCR output must be a new PDF. The source PDF is never overwritten.'
}
if ([IO.Path]::GetExtension($outputPath) -ine '.pdf') {
    throw "OCR output must use the .pdf extension: $outputPath"
}

$textOutputPath = ''
if (-not [string]::IsNullOrWhiteSpace($OutputText)) {
    $textOutputPath = [IO.Path]::GetFullPath($OutputText)
}

$repositoryRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$pdfTool = Resolve-FirstExistingFile @(
    $PdfToolPath,
    (Join-Path $PSScriptRoot 'PdfTool.exe'),
    (Join-Path $PSScriptRoot '..\PdfTool.exe'),
    (Join-Path $repositoryRoot 'dist\FamilyPDF-windows-x64\PdfTool.exe'),
    (Join-Path $repositoryRoot 'build\phase0-upstream-release\usr\bin\PdfTool.exe')
)
$tesseract = Resolve-FirstExistingFile @(
    $TesseractPath,
    (Join-Path $PSScriptRoot 'ocr\tesseract.exe'),
    (Join-Path $PSScriptRoot 'tesseract.exe'),
    (Join-Path $repositoryRoot 'ocr-spike\vcpkg_installed\x64-windows\tools\tesseract\tesseract.exe')
)
$tessdata = Resolve-FirstExistingDirectory @(
    $TessdataPath,
    (Join-Path $PSScriptRoot 'ocr\tessdata'),
    (Join-Path $PSScriptRoot 'tessdata'),
    (Join-Path $repositoryRoot 'ocr-spike\tessdata')
)

if (-not $pdfTool) {
    throw 'PdfTool.exe was not found. Install the FamilyPDF base application first.'
}
if (-not $tesseract) {
    throw 'tesseract.exe was not found. Install the FamilyPDF OCR plugin.'
}
if (-not $tessdata) {
    throw 'OCR language data was not found. Reinstall the FamilyPDF OCR plugin.'
}

$requestedLanguages = @($Languages.Split('+', [StringSplitOptions]::RemoveEmptyEntries))
$missingLanguages = @(
    $requestedLanguages |
        Where-Object {
            -not (Test-Path -LiteralPath (Join-Path $tessdata "$_.traineddata") -PathType Leaf)
        }
)
if ($missingLanguages.Count -gt 0) {
    $languageInstaller = Resolve-FirstExistingFile @(
        (Join-Path $PSScriptRoot 'ocr\Install-OCR-Languages.ps1'),
        (Join-Path $repositoryRoot 'ocr-spike\download-tessdata.ps1')
    )
    if (-not $languageInstaller) {
        throw "OCR language data is missing and the automatic repair script was not found: $($missingLanguages -join ', ')"
    }

    Write-Host "Installing missing OCR languages: $($missingLanguages -join ', ')"
    & $languageInstaller `
        -DataDirectory $tessdata `
        -Languages $missingLanguages `
        -TesseractPath $tesseract
    if ($LASTEXITCODE -ne 0) {
        throw "Automatic OCR language installation failed with exit code $LASTEXITCODE."
    }
}

foreach ($language in $requestedLanguages) {
    $languageFile = Join-Path $tessdata "$language.traineddata"
    if (-not (Test-Path -LiteralPath $languageFile -PathType Leaf) -or
        (Get-Item -LiteralPath $languageFile).Length -le 1MB) {
        throw "OCR language data is missing: $languageFile"
    }
}

foreach ($directory in @([IO.Path]::GetDirectoryName($outputPath), [IO.Path]::GetDirectoryName($textOutputPath))) {
    if (-not [string]::IsNullOrWhiteSpace($directory) -and
        -not (Test-Path -LiteralPath $directory -PathType Container)) {
        New-Item -ItemType Directory -Path $directory -Force | Out-Null
    }
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

    $pagePdfs = [Collections.Generic.List[string]]::new()
    $pageTexts = [Collections.Generic.List[string]]::new()
    for ($index = 0; $index -lt $pageImages.Count; $index++) {
        $image = $pageImages[$index]
        $pageNumber = if ($image.BaseName -match '(\d+)$') { [int]$Matches[1] } else { $index + 1 }
        $percent = [int](($index / $pageImages.Count) * 100)
        Write-Progress -Activity 'FamilyPDF OCR' -Status "Page $pageNumber ($($index + 1)/$($pageImages.Count))" -PercentComplete $percent
        Write-Host "OCR page $pageNumber ($($index + 1)/$($pageImages.Count))..."

        $ocrBase = Join-Path $temporaryRoot ('ocr-{0:D6}' -f ($index + 1))
        & $tesseract $image.FullName $ocrBase `
            --tessdata-dir $tessdata `
            -l $Languages `
            --psm $PageSegmentationMode `
            -c 'tessedit_create_pdf=1' `
            -c 'tessedit_create_txt=1'
        if ($LASTEXITCODE -ne 0) {
            throw "Tesseract failed on page $pageNumber with exit code $LASTEXITCODE."
        }

        $pagePdf = "$ocrBase.pdf"
        if (-not (Test-Path -LiteralPath $pagePdf -PathType Leaf)) {
            throw "Tesseract did not create a PDF for page $pageNumber."
        }
        $pagePdfs.Add($pagePdf)

        $pageTextFile = "$ocrBase.txt"
        $recognizedText = if (Test-Path -LiteralPath $pageTextFile -PathType Leaf) {
            [IO.File]::ReadAllText($pageTextFile, [Text.Encoding]::UTF8).TrimEnd()
        } else {
            ''
        }
        $pageTexts.Add("===== Page $pageNumber =====`r`n$recognizedText")
    }
    Write-Progress -Activity 'FamilyPDF OCR' -Completed

    $candidatePdf = Join-Path $temporaryRoot 'FamilyPDF-searchable-candidate.pdf'
    if ($pagePdfs.Count -eq 1) {
        Copy-Item -LiteralPath $pagePdfs[0] -Destination $candidatePdf
    } else {
        $uniteArguments = @('unite') + $pagePdfs.ToArray() + @($candidatePdf)
        & $pdfTool @uniteArguments
        if ($LASTEXITCODE -ne 0) {
            throw "PdfTool could not combine OCR pages (exit code $LASTEXITCODE)."
        }
    }
    if (-not (Test-Path -LiteralPath $candidatePdf -PathType Leaf)) {
        throw 'OCR did not create a combined PDF.'
    }

    $candidatePages = Get-PdfPageCount -Tool $pdfTool -Path $candidatePdf
    if ($candidatePages -ne $pageImages.Count) {
        throw "OCR output validation failed: expected $($pageImages.Count) pages, found $candidatePages."
    }
    $signatureBytes = [IO.File]::ReadAllBytes($candidatePdf)
    if ($signatureBytes.Length -lt 4 -or
        [Text.Encoding]::ASCII.GetString($signatureBytes, 0, 4) -ne '%PDF') {
        throw 'OCR output validation failed: candidate is not a PDF.'
    }

    Move-Item -LiteralPath $candidatePdf -Destination $outputPath -Force
    Write-Host "Searchable OCR PDF saved: $outputPath"

    if (-not [string]::IsNullOrWhiteSpace($textOutputPath)) {
        $utf8NoBom = [Text.UTF8Encoding]::new($false)
        [IO.File]::WriteAllText($textOutputPath, ($pageTexts -join "`r`n`r`n"), $utf8NoBom)
        Write-Host "OCR text saved: $textOutputPath"
    }

    if ($KeepPageImages) {
        $imageOutput = "$outputPath.pages"
        if (Test-Path -LiteralPath $imageOutput) {
            throw "Cannot preserve page images because the target already exists: $imageOutput"
        }
        New-Item -ItemType Directory -Path $imageOutput | Out-Null
        foreach ($image in $pageImages) {
            Copy-Item -LiteralPath $image.FullName -Destination $imageOutput
        }
        Write-Host "Rendered page images saved: $imageOutput"
    }
}
finally {
    Write-Progress -Activity 'FamilyPDF OCR' -Completed
    if (Test-Path -LiteralPath $temporaryRoot -PathType Container) {
        $resolvedTemp = [IO.Path]::GetFullPath($temporaryRoot)
        $systemTemp = [IO.Path]::GetFullPath([IO.Path]::GetTempPath())
        if ($resolvedTemp.StartsWith($systemTemp, [StringComparison]::OrdinalIgnoreCase) -and
            [IO.Path]::GetFileName($resolvedTemp).StartsWith('FamilyPDF-OCR-', [StringComparison]::Ordinal)) {
            Remove-Item -LiteralPath $resolvedTemp -Recurse -Force
        }
    }
}
