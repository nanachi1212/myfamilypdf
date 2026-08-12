[CmdletBinding()]
param(
    [switch]$SkipPackage,
    [switch]$SkipDownload,
    [switch]$VerificationBuild,
    [switch]$StandaloneSmokeBuild
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$repositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$ocrVersion = (Get-Content -LiteralPath (Join-Path $repositoryRoot 'OCR_VERSION') -Raw -Encoding UTF8).Trim()
$iscc = 'E:\Codex project\FamilyPDF-tools\inno-7.0.2\ISCC.exe'
if (-not (Test-Path -LiteralPath $iscc -PathType Leaf)) {
    throw 'The verified Inno Setup compiler is missing. Run scripts\phase0\build-installer.ps1 once.'
}

if (-not $SkipPackage) {
    $arguments = @{}
    if ($SkipDownload) {
        $arguments.SkipDownload = $true
    }
    & (Join-Path $PSScriptRoot 'build-ocr-plugin.ps1') @arguments
}

$compilerArguments = @("/DMyAppVersion=$ocrVersion")
if ($VerificationBuild) {
    $compilerArguments += '/DVerificationBuild'
}
if ($StandaloneSmokeBuild) {
    $compilerArguments += '/DStandaloneSmokeBuild'
}
$compilerArguments += (Join-Path $repositoryRoot 'installer\FamilyPDF-OCR-Plugin.iss')
& $iscc $compilerArguments
if ($LASTEXITCODE -ne 0) {
    throw "OCR plugin installer compilation failed with exit code $LASTEXITCODE."
}

$setup = if ($StandaloneSmokeBuild) {
    Join-Path $repositoryRoot 'build\FamilyPDF-OCR-Plugin-Smoke-Setup-x64.exe'
} elseif ($VerificationBuild) {
    Join-Path $repositoryRoot 'build\FamilyPDF-OCR-Plugin-Verification-Setup-x64.exe'
} else {
    Join-Path $repositoryRoot 'dist\FamilyPDF-OCR-Plugin-Setup-x64.exe'
}
if (-not (Test-Path -LiteralPath $setup -PathType Leaf)) {
    throw "OCR plugin installer output was not found: $setup"
}
Write-Host "OCR plugin installer: $setup"
