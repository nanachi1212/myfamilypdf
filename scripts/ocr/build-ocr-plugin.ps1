[CmdletBinding()]
param(
    [string]$OutputDirectory = '',
    [switch]$SkipDownload,
    [switch]$SkipVerification
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$repositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$ocrVersion = (Get-Content -LiteralPath (Join-Path $repositoryRoot 'OCR_VERSION') -Raw -Encoding UTF8).Trim()
if ([string]::IsNullOrWhiteSpace($OutputDirectory)) {
    $OutputDirectory = Join-Path $repositoryRoot 'dist'
}
$OutputDirectory = [IO.Path]::GetFullPath($OutputDirectory)

$ocrRoot = Join-Path $repositoryRoot 'ocr-spike'
$installRoot = Join-Path $ocrRoot 'vcpkg_installed'
$tripletRoot = Join-Path $installRoot 'x64-windows'
$tesseractTools = Join-Path $tripletRoot 'tools\tesseract'
$tesseractExe = Join-Path $tesseractTools 'tesseract.exe'
$tessdata = Join-Path $ocrRoot 'tessdata'
$downloader = Join-Path $ocrRoot 'download-tessdata.ps1'
$vcpkg = 'E:\CodexProject\FamilyPDF-tools\vcpkg\vcpkg.exe'

if (-not $SkipDownload) {
    try {
        & $downloader -TesseractPath $tesseractExe
    }
    catch {
        Write-Warning "Some OCR languages could not be downloaded in this environment: $($_.Exception.Message)"
        Write-Warning 'The packaged language repair script will retry automatically on a normal internet connection.'
    }
}

if (-not (Test-Path -LiteralPath $tesseractExe -PathType Leaf)) {
    if (-not (Test-Path -LiteralPath $vcpkg -PathType Leaf)) {
        throw "vcpkg was not found: $vcpkg"
    }
    & $vcpkg install `
        "--x-manifest-root=$ocrRoot" `
        "--x-install-root=$installRoot" `
        '--triplet=x64-windows' `
        '--clean-after-build'
    if ($LASTEXITCODE -ne 0) {
        throw "OCR dependency installation failed with exit code $LASTEXITCODE."
    }
}

$coreLanguages = @('eng', 'chi_tra', 'chi_sim')
foreach ($language in $coreLanguages) {
    $languageFile = Join-Path $tessdata "$language.traineddata"
    if (-not (Test-Path -LiteralPath $languageFile -PathType Leaf)) {
        throw "Required OCR language data is missing: $languageFile"
    }
}

New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null
$packageRoot = Join-Path $OutputDirectory 'FamilyPDF-OCR-Plugin-windows-x64'
if (Test-Path -LiteralPath $packageRoot -PathType Container) {
    Remove-Item -LiteralPath $packageRoot -Recurse -Force
}
$packageOcr = Join-Path $packageRoot 'ocr'
$packageTessdata = Join-Path $packageOcr 'tessdata'
New-Item -ItemType Directory -Path $packageTessdata -Force | Out-Null

Get-ChildItem -LiteralPath $tesseractTools -File |
    Where-Object { $_.Extension -in @('.exe', '.dll') } |
    Copy-Item -Destination $packageOcr -Force
Get-ChildItem -LiteralPath $tessdata -Filter '*.traineddata' -File |
    Copy-Item -Destination $packageTessdata -Force

$licenseRoot = Join-Path $packageRoot 'THIRD-PARTY-LICENSES\OCR'
New-Item -ItemType Directory -Path $licenseRoot -Force | Out-Null
$licensePackages = @(
    'tesseract',
    'tessdata',
    'leptonica',
    'libarchive',
    'bzip2',
    'giflib',
    'libjpeg-turbo',
    'liblzma',
    'libpng',
    'libwebp',
    'lz4',
    'openjpeg',
    'openssl',
    'curl',
    'tiff',
    'zlib',
    'zstd'
)
$shareRoot = Join-Path $tripletRoot 'share'
foreach ($package in $licensePackages) {
    $copyright = Join-Path (Join-Path $shareRoot $package) 'copyright'
    if ($package -eq 'tessdata' -and
        -not (Test-Path -LiteralPath $copyright -PathType Leaf)) {
        # Tesseract and the official tessdata models are both distributed
        # under Apache-2.0. vcpkg installs the shared notice with Tesseract.
        $copyright = Join-Path (Join-Path $shareRoot 'tesseract') 'copyright'
    }
    if (-not (Test-Path -LiteralPath $copyright -PathType Leaf)) {
        throw "Required third-party license was not found: $copyright"
    }
    Copy-Item -LiteralPath $copyright `
        -Destination (Join-Path $licenseRoot "$package.txt") -Force
}
$noticeLines = @(
    'FamilyPDF OCR Plugin third-party notices',
    '',
    'This package redistributes Tesseract OCR, its language models, and runtime dependencies.',
    'The corresponding license and copyright texts are included in THIRD-PARTY-LICENSES.',
    '',
    'Included components:'
) + @($licensePackages | ForEach-Object { "- $_" })
$noticeLines | Set-Content `
    -LiteralPath (Join-Path $packageRoot 'THIRD-PARTY-NOTICES-OCR.txt') `
    -Encoding UTF8

Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'FamilyPDF-OCR.ps1') -Destination $packageRoot -Force
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'FamilyPDF-OCR.cmd') -Destination $packageRoot -Force
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'Install-FamilyPDF-OCR-Languages.cmd') -Destination $packageRoot -Force
Copy-Item -LiteralPath $downloader -Destination (Join-Path $packageOcr 'Install-OCR-Languages.ps1') -Force
Copy-Item -LiteralPath (Join-Path $ocrRoot 'tessdata-manifest.json') `
    -Destination (Join-Path $packageOcr 'tessdata-manifest.json') -Force

$presentLanguages = @(Get-ChildItem -LiteralPath $packageTessdata -Filter '*.traineddata' -File |
    Sort-Object Name |
    ForEach-Object { $_.BaseName })
$manifest = [ordered]@{
    name = 'FamilyPDF OCR Plugin'
    version = $ocrVersion
    engine = 'Tesseract 5'
    languages = $presentLanguages
    searchablePdf = $true
    sourcePdfIsNeverOverwritten = $true
    thirdPartyNotices = 'THIRD-PARTY-NOTICES-OCR.txt'
}
$manifest | ConvertTo-Json -Depth 4 |
    Set-Content -LiteralPath (Join-Path $packageRoot 'FamilyPDF-OCR-Plugin.json') -Encoding UTF8

if (-not $SkipVerification) {
    $pdfTool = Join-Path $repositoryRoot 'build\phase0-upstream-release\usr\bin\PdfTool.exe'
    if (-not (Test-Path -LiteralPath $pdfTool -PathType Leaf)) {
        throw "PdfTool is required for OCR verification: $pdfTool"
    }

    & (Join-Path $PSScriptRoot 'Test-FamilyPDF-OCR-Horizontal.ps1') `
        -PdfToolPath $pdfTool `
        -TesseractPath (Join-Path $packageOcr 'tesseract.exe') `
        -TessdataPath $packageTessdata `
        -OcrScriptPath (Join-Path $packageRoot 'FamilyPDF-OCR.ps1')

    & (Join-Path $PSScriptRoot 'Test-FamilyPDF-OCR-Auto.ps1') `
        -PdfToolPath $pdfTool `
        -TesseractPath (Join-Path $packageOcr 'tesseract.exe') `
        -TessdataPath $packageTessdata `
        -OcrScriptPath (Join-Path $packageRoot 'FamilyPDF-OCR.ps1')

    & (Join-Path $PSScriptRoot 'Test-OCR-Language-Download.ps1') `
        -DownloaderPath (Join-Path $packageOcr 'Install-OCR-Languages.ps1')
    & (Join-Path $PSScriptRoot 'Test-OCR-Language-Manifest.ps1') `
        -ManifestPath (Join-Path $packageOcr 'tessdata-manifest.json')

    $requiredVerticalLanguages = @('chi_tra_vert', 'chi_sim_vert')
    $missingVerticalLanguages = @(
        $requiredVerticalLanguages |
            Where-Object {
                -not (Test-Path -LiteralPath (
                    Join-Path $packageTessdata "$_.traineddata"
                ) -PathType Leaf)
            }
    )
    if ($missingVerticalLanguages.Count -eq 0) {
        & (Join-Path $PSScriptRoot 'Test-FamilyPDF-OCR-Vertical.ps1') `
            -PdfToolPath $pdfTool `
            -TesseractPath (Join-Path $packageOcr 'tesseract.exe') `
            -TessdataPath $packageTessdata `
            -OcrScriptPath (Join-Path $packageRoot 'FamilyPDF-OCR.ps1')
    }
    else {
        Write-Warning "Skipping vertical OCR verification because models are missing: $($missingVerticalLanguages -join ', ')"
    }
}

$zipPath = Join-Path $OutputDirectory 'FamilyPDF-OCR-Plugin-windows-x64.zip'
if (Test-Path -LiteralPath $zipPath -PathType Leaf) {
    Remove-Item -LiteralPath $zipPath -Force
}
Compress-Archive -Path (Join-Path $packageRoot '*') -DestinationPath $zipPath -CompressionLevel Optimal

Write-Host "OCR plugin directory: $packageRoot"
Write-Host "OCR plugin archive: $zipPath"
Write-Host "Packaged languages: $($presentLanguages -join ', ')"
