[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$InputPdf,

    [Parameter(Mandatory = $true)]
    [string]$PdfToolPath,

    [Parameter(Mandatory = $true)]
    [string]$TesseractPath,

    [Parameter(Mandatory = $true)]
    [string]$TessdataPath,

    [string]$OcrScriptPath = '',

    [string]$ExpectedText = 'FamilyPDF',

    [string]$RequiredLanguages = 'eng,chi_tra,chi_sim,chi_tra_vert,chi_sim_vert'
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

if ([string]::IsNullOrWhiteSpace($OcrScriptPath)) {
    $OcrScriptPath = Join-Path $PSScriptRoot 'FamilyPDF-OCR.ps1'
}
if (-not (Test-Path -LiteralPath $OcrScriptPath -PathType Leaf)) {
    throw "OCR script under test was not found: $OcrScriptPath"
}

$requiredLanguageList = @($RequiredLanguages.Split(',', [StringSplitOptions]::RemoveEmptyEntries))
foreach ($language in $requiredLanguageList) {
    $languageFile = Join-Path $TessdataPath "$language.traineddata"
    if (-not (Test-Path -LiteralPath $languageFile -PathType Leaf)) {
        throw "Required OCR test language is missing: $languageFile"
    }
}

$testRoot = Join-Path ([IO.Path]::GetTempPath()) ('FamilyPDF-OCR-Test-' + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $testRoot | Out-Null

try {
    $inputHash = (Get-FileHash -LiteralPath $InputPdf -Algorithm SHA256).Hash
    $outputPdf = Join-Path $testRoot 'searchable.pdf'
    $outputText = Join-Path $testRoot 'recognized.txt'

    & $OcrScriptPath `
        -InputPdf $InputPdf `
        -OutputPdf $outputPdf `
        -OutputText $outputText `
        -Languages 'chi_tra+chi_sim+eng' `
        -Dpi 200 `
        -PdfToolPath $PdfToolPath `
        -TesseractPath $TesseractPath `
        -TessdataPath $TessdataPath

    if (-not (Test-Path -LiteralPath $outputPdf -PathType Leaf)) {
        throw 'OCR did not create a searchable PDF.'
    }
    $signature = [Text.Encoding]::ASCII.GetString([IO.File]::ReadAllBytes($outputPdf), 0, 4)
    if ($signature -ne '%PDF') {
        throw "OCR output is not a PDF: $outputPdf"
    }
    if ((Get-FileHash -LiteralPath $InputPdf -Algorithm SHA256).Hash -ne $inputHash) {
        throw 'OCR changed the source PDF.'
    }

    $inputInfo = (& $PdfToolPath info $InputPdf 2>&1) -join "`n"
    $outputInfo = (& $PdfToolPath info $outputPdf 2>&1) -join "`n"
    $pagePattern = 'Page count\s+([0-9,]+)'
    if ($inputInfo -notmatch $pagePattern) {
        throw 'Could not read the source page count.'
    }
    $inputPages = $Matches[1].Replace(',', '')
    if ($outputInfo -notmatch $pagePattern) {
        throw 'Could not read the OCR output page count.'
    }
    $outputPages = $Matches[1].Replace(',', '')
    if ($inputPages -ne $outputPages) {
        throw "OCR page count changed: $inputPages -> $outputPages"
    }

    $fetchedText = (& $PdfToolPath fetch-text $outputPdf 2>&1) -join "`n"
    if ([string]::IsNullOrWhiteSpace($fetchedText)) {
        throw 'The OCR PDF does not contain an extractable text layer.'
    }
    if (-not [string]::IsNullOrWhiteSpace($ExpectedText) -and
        $fetchedText -notmatch [Regex]::Escape($ExpectedText)) {
        throw "Expected OCR text was not found: $ExpectedText"
    }
    if (-not (Test-Path -LiteralPath $outputText -PathType Leaf) -or
        (Get-Item -LiteralPath $outputText).Length -eq 0) {
        throw 'OCR did not create the optional UTF-8 text output.'
    }

    Write-Host "OCR searchable PDF test passed: $inputPages page(s), $($requiredLanguageList.Count) language files present."
}
finally {
    if (Test-Path -LiteralPath $testRoot -PathType Container) {
        Remove-Item -LiteralPath $testRoot -Recurse -Force
    }
}
