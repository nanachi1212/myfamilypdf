[CmdletBinding()]
param(
    [string]$OutputDirectory = '',
    [switch]$SkipDownload
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$repositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
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

Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'FamilyPDF-OCR.ps1') -Destination $packageRoot -Force
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'FamilyPDF-OCR.cmd') -Destination $packageRoot -Force
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'Install-FamilyPDF-OCR-Languages.cmd') -Destination $packageRoot -Force
Copy-Item -LiteralPath $downloader -Destination (Join-Path $packageOcr 'Install-OCR-Languages.ps1') -Force

$presentLanguages = @(Get-ChildItem -LiteralPath $packageTessdata -Filter '*.traineddata' -File |
    Sort-Object Name |
    ForEach-Object { $_.BaseName })
$manifest = [ordered]@{
    name = 'FamilyPDF OCR Plugin'
    version = '0.3.0'
    engine = 'Tesseract 5'
    languages = $presentLanguages
    searchablePdf = $true
    sourcePdfIsNeverOverwritten = $true
}
$manifest | ConvertTo-Json -Depth 4 |
    Set-Content -LiteralPath (Join-Path $packageRoot 'FamilyPDF-OCR-Plugin.json') -Encoding UTF8

$zipPath = Join-Path $OutputDirectory 'FamilyPDF-OCR-Plugin-windows-x64.zip'
if (Test-Path -LiteralPath $zipPath -PathType Leaf) {
    Remove-Item -LiteralPath $zipPath -Force
}
Compress-Archive -Path (Join-Path $packageRoot '*') -DestinationPath $zipPath -CompressionLevel Optimal

Write-Host "OCR plugin directory: $packageRoot"
Write-Host "OCR plugin archive: $zipPath"
Write-Host "Packaged languages: $($presentLanguages -join ', ')"
