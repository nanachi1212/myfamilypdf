[CmdletBinding()]
param(
    [Parameter(Mandatory = $true, Position = 0)]
    [ValidateScript({ Test-Path -LiteralPath $_ -PathType Leaf })]
    [string]$InputPdf,

    [Parameter(Position = 1)]
    [string]$OutputPdf = '',

    [string]$OutputText = '',

    [ValidateSet('Auto', 'Traditional', 'Simplified', 'English', 'Custom')]
    [string]$Mode = 'Auto',

    [ValidatePattern('^[A-Za-z0-9_+.-]+$')]
    [string]$Languages = '',

    [string]$OutputReport = '',

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

function Get-OcrProbe {
    param(
        [string]$Tesseract,
        [string]$Image,
        [string]$OutputBase,
        [string]$Tessdata,
        [string]$Language,
        [int]$Psm
    )

    & $Tesseract $Image $OutputBase `
        --tessdata-dir $Tessdata `
        -l $Language `
        --psm $Psm `
        -c 'tessedit_create_tsv=1'
    if ($LASTEXITCODE -ne 0) {
        throw "Tesseract probe failed for $Language / PSM $Psm with exit code $LASTEXITCODE."
    }

    $tsvPath = "$OutputBase.tsv"
    if (-not (Test-Path -LiteralPath $tsvPath -PathType Leaf)) {
        throw "Tesseract probe did not create TSV output: $tsvPath"
    }
    $rows = @(
        ([IO.File]::ReadAllLines($tsvPath, [Text.Encoding]::UTF8) |
            ConvertFrom-Csv -Delimiter "`t") |
            Where-Object { [int]$_.conf -ge 0 }
    )
    $confidence = if ($rows.Count -gt 0) {
        [double](($rows | Measure-Object -Property conf -Average).Average)
    } else {
        0.0
    }

    $recognizedText = if ($rows.Count -gt 0) { $rows.text -join ' ' } else { '' }
    return [pscustomobject]@{
        languages = $Language
        psm = $Psm
        confidence = [math]::Round($confidence, 2)
        words = $rows.Count
        text = $recognizedText
    }
}

function Get-ScriptMarkerCount {
    param([string]$Text, [string]$Markers)

    $count = 0
    foreach ($character in $Text.ToCharArray()) {
        if ($Markers.Contains([string]$character)) {
            $count++
        }
    }
    return $count
}

function Select-AutomaticOcrProfile {
    param(
        [string]$Tesseract,
        [string]$Image,
        [string]$ProbeRoot,
        [string]$Tessdata,
        [int]$PageIndex,
        [bool]$LayoutIsForced,
        [int]$ForcedPsm
    )

    $languageCandidates = [Collections.Generic.List[object]]::new()
    foreach ($language in @('chi_tra+eng', 'chi_sim+eng')) {
        $probeBase = Join-Path $ProbeRoot (
            'probe-{0:D6}-{1}-psm3' -f $PageIndex, $language.Replace('+', '-')
        )
        $languageCandidates.Add((Get-OcrProbe `
            -Tesseract $Tesseract `
            -Image $Image `
            -OutputBase $probeBase `
            -Tessdata $Tessdata `
            -Language $language `
            -Psm 3))
    }

    $traditional = $languageCandidates | Where-Object languages -eq 'chi_tra+eng'
    $simplified = $languageCandidates | Where-Object languages -eq 'chi_sim+eng'
    $singleLanguageScores = @($traditional.confidence, $simplified.confidence) | Sort-Object -Descending
    $languageGap = [math]::Round(($singleLanguageScores[0] - $singleLanguageScores[1]), 2)
    $bestSingle = if ($traditional.confidence -ge $simplified.confidence) {
        $traditional
    } else {
        $simplified
    }

    $mixed = $null
    if ($languageGap -lt 12 -or $bestSingle.confidence -lt 82) {
        $mixedBase = Join-Path $ProbeRoot (
            'probe-{0:D6}-chi-tra-chi-sim-eng-psm3' -f $PageIndex
        )
        $mixed = Get-OcrProbe `
            -Tesseract $Tesseract `
            -Image $Image `
            -OutputBase $mixedBase `
            -Tessdata $Tessdata `
            -Language 'chi_tra+chi_sim+eng' `
            -Psm 3
        $languageCandidates.Add($mixed)
    }

    # Keep this script ASCII-only so Windows PowerShell 5.1 can parse it
    # correctly even when it is distributed as UTF-8 without a BOM.
    $traditionalMarkers = -join ([char[]]@(
        0x50B3, 0x7D71, 0x9AD4, 0x6E2C, 0x81FA, 0x7063, 0x570B, 0x865F,
        0x8B49, 0x64DA, 0x8ABF, 0x8A34, 0x8A1F, 0x95DC, 0x4FC2, 0x696D,
        0x8207, 0x70BA, 0x65BC, 0x5F8C, 0x767C, 0x73FE, 0x61C9, 0x8A72,
        0x9019, 0x500B, 0x88E1, 0x4F86, 0x8AAA, 0x8B93, 0x5C07, 0x7C21,
        0x6A94, 0x9801, 0x5716, 0x5C64, 0x9078, 0x64C7
    ))
    $simplifiedMarkers = -join ([char[]]@(
        0x4F20, 0x7EDF, 0x4F53, 0x6D4B, 0x53F0, 0x6E7E, 0x56FD, 0x53F7,
        0x8BC1, 0x636E, 0x8C03, 0x8BC9, 0x8BBC, 0x5173, 0x7CFB, 0x4E1A,
        0x4E0E, 0x4E3A, 0x4E8E, 0x540E, 0x53D1, 0x73B0, 0x5E94, 0x8BE5,
        0x8FD9, 0x4E2A, 0x91CC, 0x6765, 0x8BF4, 0x8BA9, 0x5C06, 0x7B80,
        0x6863, 0x9875, 0x56FE, 0x5C42, 0x9009, 0x62E9
    ))
    $traditionalCount = if ($null -ne $mixed) {
        Get-ScriptMarkerCount -Text $mixed.text -Markers $traditionalMarkers
    } else { 0 }
    $simplifiedCount = if ($null -ne $mixed) {
        Get-ScriptMarkerCount -Text $mixed.text -Markers $simplifiedMarkers
    } else { 0 }
    $isMixed = $null -ne $mixed -and
        $traditionalCount -ge 2 -and
        $simplifiedCount -ge 2 -and
        $mixed.confidence -ge ($bestSingle.confidence + 0.1)
    $useLowConfidenceMixedFallback = $null -ne $mixed -and
        $bestSingle.words -ge 100 -and
        $bestSingle.confidence -lt 82 -and
        $mixed.confidence -ge ($bestSingle.confidence - 2)

    if ($isMixed -or $useLowConfidenceMixedFallback) {
        $selectedLanguage = $mixed
    } else {
        $selectedLanguage = $bestSingle
    }

    $allCandidates = [Collections.Generic.List[object]]::new()
    foreach ($candidate in $languageCandidates) {
        $allCandidates.Add([pscustomobject]@{
            kind = 'language'
            languages = $candidate.languages
            psm = $candidate.psm
            confidence = $candidate.confidence
            words = $candidate.words
        })
    }

    $selectedLayout = $selectedLanguage
    if ($LayoutIsForced) {
        if ($ForcedPsm -ne 3) {
            $forcedBase = Join-Path $ProbeRoot (
                'probe-{0:D6}-{1}-psm{2}' -f $PageIndex, $selectedLanguage.languages.Replace('+', '-'), $ForcedPsm
            )
            $selectedLayout = Get-OcrProbe `
                -Tesseract $Tesseract `
                -Image $Image `
                -OutputBase $forcedBase `
                -Tessdata $Tessdata `
                -Language $selectedLanguage.languages `
                -Psm $ForcedPsm
        }
    } elseif ($selectedLanguage.confidence -lt 82 -or $languageGap -lt 5 -or $selectedLanguage.words -lt 20) {
        foreach ($psm in @(4, 6)) {
            $layoutBase = Join-Path $ProbeRoot (
                'probe-{0:D6}-{1}-psm{2}' -f $PageIndex, $selectedLanguage.languages.Replace('+', '-'), $psm
            )
            $layoutCandidate = Get-OcrProbe `
                -Tesseract $Tesseract `
                -Image $Image `
                -OutputBase $layoutBase `
                -Tessdata $Tessdata `
                -Language $selectedLanguage.languages `
                -Psm $psm
            $allCandidates.Add([pscustomobject]@{
                kind = 'layout'
                languages = $layoutCandidate.languages
                psm = $layoutCandidate.psm
                confidence = $layoutCandidate.confidence
                words = $layoutCandidate.words
            })
            if ($layoutCandidate.confidence -ge ($selectedLayout.confidence + 1.5)) {
                $selectedLayout = $layoutCandidate
            }
        }
    }

    $warnings = [Collections.Generic.List[string]]::new()
    if ($selectedLayout.confidence -lt 82) {
        $warnings.Add('low-confidence')
    }
    if ($languageGap -lt 5 -and -not $isMixed) {
        $warnings.Add('language-uncertain')
    }
    if ($isMixed) {
        $warnings.Add('mixed-traditional-simplified')
    }
    if ($useLowConfidenceMixedFallback -and -not $isMixed) {
        $warnings.Add('mixed-fallback-low-confidence')
    }
    $complexLayout = @($allCandidates | Where-Object {
        $_.kind -eq 'layout' -and
        $_.words -gt ($selectedLanguage.words * 1.05) -and
        $_.confidence -ge ($selectedLanguage.confidence - 2)
    }).Count -gt 0
    if ($complexLayout) {
        $warnings.Add('possible-complex-layout')
    }

    return [pscustomobject]@{
        languages = $selectedLanguage.languages
        psm = $selectedLayout.psm
        confidence = $selectedLayout.confidence
        languageGap = $languageGap
        needsReview = $warnings.Count -gt 0
        warnings = $warnings.ToArray()
        candidates = $allCandidates.ToArray()
    }
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

$reportOutputPath = ''
if (-not [string]::IsNullOrWhiteSpace($OutputReport)) {
    $reportOutputPath = [IO.Path]::GetFullPath($OutputReport)
}

$languagesWereSpecified = $PSBoundParameters.ContainsKey('Languages')
$layoutWasSpecified = $PSBoundParameters.ContainsKey('PageSegmentationMode')
$autoMode = $Mode -eq 'Auto' -and -not $languagesWereSpecified
if ($Mode -eq 'Custom' -and -not $languagesWereSpecified) {
    throw 'Custom OCR mode requires -Languages.'
}
if ($languagesWereSpecified) {
    $effectiveMode = 'Custom'
} else {
    $effectiveMode = $Mode
    $Languages = switch ($Mode) {
        'Traditional' { 'chi_tra+eng' }
        'Simplified' { 'chi_sim+eng' }
        'English' { 'eng' }
        'Auto' { 'chi_tra+chi_sim+eng' }
        default { throw "Unsupported OCR mode: $Mode" }
    }
}
if ($autoMode -and [string]::IsNullOrWhiteSpace($reportOutputPath)) {
    $reportOutputPath = [IO.Path]::ChangeExtension($outputPath, '.ocr-report.json')
}

$namedPaths = [ordered]@{
    InputPdf = $inputPath
    OutputPdf = $outputPath
}
if (-not [string]::IsNullOrWhiteSpace($textOutputPath)) {
    $namedPaths.OutputText = $textOutputPath
}
if (-not [string]::IsNullOrWhiteSpace($reportOutputPath)) {
    $namedPaths.OutputReport = $reportOutputPath
}
$imageOutputPath = ''
if ($KeepPageImages) {
    $imageOutputPath = "$outputPath.pages"
    $namedPaths.PageImages = $imageOutputPath
}
$pathNames = @($namedPaths.Keys)
for ($leftIndex = 0; $leftIndex -lt $pathNames.Count; $leftIndex++) {
    for ($rightIndex = $leftIndex + 1; $rightIndex -lt $pathNames.Count; $rightIndex++) {
        $leftName = $pathNames[$leftIndex]
        $rightName = $pathNames[$rightIndex]
        if ([string]::Equals(
                $namedPaths[$leftName],
                $namedPaths[$rightName],
                [StringComparison]::OrdinalIgnoreCase
            )) {
            throw "$leftName and $rightName must use different paths. No OCR output may overwrite another file."
        }
    }
}
foreach ($outputName in @($namedPaths.Keys | Where-Object { $_ -ne 'InputPdf' })) {
    $candidatePath = $namedPaths[$outputName]
    if (Test-Path -LiteralPath $candidatePath -PathType Container) {
        throw "$outputName points to an existing directory, not an output file: $candidatePath"
    }
}
if ($KeepPageImages -and (Test-Path -LiteralPath $imageOutputPath)) {
    throw "Cannot preserve page images because the target already exists: $imageOutputPath"
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

$languageRequest = if ($autoMode) { 'chi_tra+chi_sim+eng' } else { $Languages }
$requestedLanguages = @($languageRequest.Split('+', [StringSplitOptions]::RemoveEmptyEntries))
$languageManifestPath = Resolve-FirstExistingFile @(
    (Join-Path $PSScriptRoot 'ocr\tessdata-manifest.json'),
    (Join-Path ([IO.Path]::GetDirectoryName($tessdata)) 'tessdata-manifest.json'),
    (Join-Path $repositoryRoot 'ocr-spike\tessdata-manifest.json')
)
$languageManifest = if ($languageManifestPath) {
    Get-Content -LiteralPath $languageManifestPath -Raw -Encoding UTF8 |
        ConvertFrom-Json
} else {
    $null
}

function Test-OcrLanguageModel {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path,
        [object]$Expected
    )

    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        return $false
    }
    if ($null -eq $Expected) {
        return (Get-Item -LiteralPath $Path).Length -gt 1MB
    }
    if ((Get-Item -LiteralPath $Path).Length -ne [long]$Expected.bytes) {
        return $false
    }
    return (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash -eq
        [string]$Expected.sha256
}

$invalidLanguages = @(
    $requestedLanguages |
        Where-Object {
            $expected = if ($null -ne $languageManifest) {
                $property = $languageManifest.languages.PSObject.Properties[$_]
                if ($null -ne $property) { $property.Value } else { $null }
            } else {
                $null
            }
            -not (Test-OcrLanguageModel `
                -Path (Join-Path $tessdata "$_.traineddata") `
                -Expected $expected)
        }
)
if ($invalidLanguages.Count -gt 0) {
    $languageInstaller = Resolve-FirstExistingFile @(
        (Join-Path $PSScriptRoot 'ocr\Install-OCR-Languages.ps1'),
        (Join-Path $repositoryRoot 'ocr-spike\download-tessdata.ps1')
    )
    if (-not $languageInstaller) {
        throw "OCR language data is missing or corrupt and the automatic repair script was not found: $($invalidLanguages -join ', ')"
    }

    Write-Host "Repairing missing or corrupt OCR languages: $($invalidLanguages -join ', ')"
    & $languageInstaller `
        -DataDirectory $tessdata `
        -Languages $invalidLanguages `
        -TesseractPath $tesseract
    if ($LASTEXITCODE -ne 0) {
        throw "Automatic OCR language installation failed with exit code $LASTEXITCODE."
    }
}

foreach ($language in $requestedLanguages) {
    $languageFile = Join-Path $tessdata "$language.traineddata"
    $expected = if ($null -ne $languageManifest) {
        $property = $languageManifest.languages.PSObject.Properties[$language]
        if ($null -ne $property) { $property.Value } else { $null }
    } else {
        $null
    }
    if (-not (Test-OcrLanguageModel -Path $languageFile -Expected $expected)) {
        throw "OCR language data is missing or corrupt: $languageFile"
    }
}

$outputDirectories = [Collections.Generic.List[string]]::new()
$outputDirectories.Add([IO.Path]::GetDirectoryName($outputPath))
if (-not [string]::IsNullOrWhiteSpace($textOutputPath)) {
    $outputDirectories.Add([IO.Path]::GetDirectoryName($textOutputPath))
}
if (-not [string]::IsNullOrWhiteSpace($reportOutputPath)) {
    $outputDirectories.Add([IO.Path]::GetDirectoryName($reportOutputPath))
}
foreach ($directory in $outputDirectories) {
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
    $pageReports = [Collections.Generic.List[object]]::new()
    for ($index = 0; $index -lt $pageImages.Count; $index++) {
        $image = $pageImages[$index]
        $pageNumber = if ($image.BaseName -match '(\d+)$') { [int]$Matches[1] } else { $index + 1 }
        $percent = [int](($index / $pageImages.Count) * 100)
        Write-Progress -Activity 'FamilyPDF OCR' -Status "Page $pageNumber ($($index + 1)/$($pageImages.Count))" -PercentComplete $percent
        Write-Host "OCR page $pageNumber ($($index + 1)/$($pageImages.Count))..."

        if ($autoMode) {
            Write-Host "Analyzing page $pageNumber language and layout..."
            $profile = Select-AutomaticOcrProfile `
                -Tesseract $tesseract `
                -Image $image.FullName `
                -ProbeRoot $temporaryRoot `
                -Tessdata $tessdata `
                -PageIndex ($index + 1) `
                -LayoutIsForced $layoutWasSpecified `
                -ForcedPsm $PageSegmentationMode
            $pageLanguages = $profile.languages
            $pagePsm = $profile.psm
            $pageReports.Add([pscustomobject]@{
                page = $pageNumber
                languages = $profile.languages
                psm = $profile.psm
                confidence = $profile.confidence
                languageGap = $profile.languageGap
                needsReview = $profile.needsReview
                warnings = $profile.warnings
                candidates = $profile.candidates
            })
            Write-Host "Selected page ${pageNumber}: $pageLanguages / PSM $pagePsm / confidence $($profile.confidence)"
        } else {
            $pageLanguages = $Languages
            $pagePsm = $PageSegmentationMode
            $pageReports.Add([pscustomobject]@{
                page = $pageNumber
                languages = $pageLanguages
                psm = $pagePsm
                confidence = $null
                languageGap = $null
                needsReview = $false
                warnings = @()
                candidates = @()
            })
        }

        $ocrBase = Join-Path $temporaryRoot ('ocr-{0:D6}' -f ($index + 1))
        & $tesseract $image.FullName $ocrBase `
            --tessdata-dir $tessdata `
            -l $pageLanguages `
            --psm $pagePsm `
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

    if (-not [string]::IsNullOrWhiteSpace($reportOutputPath)) {
        $reviewPages = @($pageReports | Where-Object needsReview | ForEach-Object page)
        $report = [ordered]@{
            schemaVersion = 1
            mode = $effectiveMode
            generatedAt = [DateTimeOffset]::Now.ToString('o')
            summary = [ordered]@{
                pages = $pageImages.Count
                reviewPages = $reviewPages
            }
            pages = $pageReports.ToArray()
        }
        $utf8NoBom = [Text.UTF8Encoding]::new($false)
        [IO.File]::WriteAllText(
            $reportOutputPath,
            ($report | ConvertTo-Json -Depth 8),
            $utf8NoBom
        )
        Write-Host "OCR analysis report saved: $reportOutputPath"
    }

    if ($KeepPageImages) {
        New-Item -ItemType Directory -Path $imageOutputPath | Out-Null
        foreach ($image in $pageImages) {
            Copy-Item -LiteralPath $image.FullName -Destination $imageOutputPath
        }
        Write-Host "Rendered page images saved: $imageOutputPath"
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
