[CmdletBinding()]
param(
    [switch]$SkipPackage,
    [switch]$SkipDownload,
    [switch]$VerificationBuild
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$repositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$iscc = 'E:\CodexProject\FamilyPDF-tools\inno-7.0.2\ISCC.exe'
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

$compilerArguments = @()
if ($VerificationBuild) {
    $compilerArguments += '/DVerificationBuild'
}
$compilerArguments += (Join-Path $repositoryRoot 'installer\FamilyPDF-OCR-Plugin.iss')
& $iscc $compilerArguments
if ($LASTEXITCODE -ne 0) {
    throw "OCR plugin installer compilation failed with exit code $LASTEXITCODE."
}

$setup = if ($VerificationBuild) {
    Join-Path $repositoryRoot 'build\FamilyPDF-OCR-Plugin-Verification-Setup-x64.exe'
} else {
    Join-Path $repositoryRoot 'dist\FamilyPDF-OCR-Plugin-Setup-x64.exe'
}
if (-not (Test-Path -LiteralPath $setup -PathType Leaf)) {
    throw "OCR plugin installer output was not found: $setup"
}
Write-Host "OCR plugin installer: $setup"
