[CmdletBinding()]
param(
    [switch]$SkipBasePackage,
    [switch]$SkipOcrPackage,
    [switch]$SkipDownload,
    [switch]$VerificationBuild
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$repositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$appVersion = (Get-Content -LiteralPath (Join-Path $repositoryRoot 'VERSION') -Raw -Encoding UTF8).Trim()
$iscc = 'E:\Codex project\FamilyPDF-tools\inno-7.0.2\ISCC.exe'

if (-not (Test-Path -LiteralPath $iscc -PathType Leaf)) {
    & (Join-Path $PSScriptRoot 'build-installer.ps1') -SkipPackage -SkipOcr
}
if (-not (Test-Path -LiteralPath $iscc -PathType Leaf)) {
    throw 'The verified Inno Setup compiler could not be installed.'
}

if (-not $SkipBasePackage) {
    & (Join-Path $PSScriptRoot 'package-windows-runtime.ps1') -SkipOcr
}
if (-not $SkipOcrPackage) {
    $ocrArguments = @{}
    if ($SkipDownload) {
        $ocrArguments.SkipDownload = $true
    }
    & (Join-Path $repositoryRoot 'scripts\ocr\build-ocr-plugin.ps1') @ocrArguments
}

$basePackage = Join-Path $repositoryRoot 'dist\FamilyPDF-windows-x64'
$ocrPackage = Join-Path $repositoryRoot 'dist\FamilyPDF-OCR-Plugin-windows-x64'
$requiredFiles = @(
    (Join-Path $basePackage 'Pdf4QtViewer.exe'),
    (Join-Path $basePackage 'Pdf4QtEditor.exe'),
    (Join-Path $basePackage 'Pdf4QtPageMaster.exe'),
    (Join-Path $basePackage 'Pdf4QtDiff.exe'),
    (Join-Path $basePackage 'PdfTool.exe'),
    (Join-Path $ocrPackage 'FamilyPDF-OCR.ps1'),
    (Join-Path $ocrPackage 'ocr\tesseract.exe'),
    (Join-Path $ocrPackage 'ocr\tessdata\chi_tra.traineddata'),
    (Join-Path $ocrPackage 'ocr\tessdata\chi_sim.traineddata')
)
foreach ($requiredFile in $requiredFiles) {
    if (-not (Test-Path -LiteralPath $requiredFile -PathType Leaf)) {
        throw "Full installer input is missing: $requiredFile"
    }
}

$compilerArguments = @("/DMyAppVersion=$appVersion")
if ($VerificationBuild) {
    $compilerArguments += '/DVerificationBuild'
}
$compilerArguments += (Join-Path $repositoryRoot 'installer\FamilyPDF-Full.iss')

& $iscc $compilerArguments
if ($LASTEXITCODE -ne 0) {
    throw "Full installer compilation failed with exit code $LASTEXITCODE."
}

$setup = if ($VerificationBuild) {
    Join-Path $repositoryRoot 'build\FamilyPDF-Full-Verification-Setup-x64.exe'
}
else {
    Join-Path $repositoryRoot 'dist\FamilyPDF-Full-Setup-x64.exe'
}
if (-not (Test-Path -LiteralPath $setup -PathType Leaf)) {
    throw "Full installer output was not found: $setup"
}

Write-Host "Full installer: $setup"
