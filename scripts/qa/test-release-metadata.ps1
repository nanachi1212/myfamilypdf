[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$repositoryRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$versionFiles = [ordered]@{
    FamilyPDF = Join-Path $repositoryRoot 'VERSION'
    OCR = Join-Path $repositoryRoot 'OCR_VERSION'
}
$versions = [ordered]@{}
foreach ($entry in $versionFiles.GetEnumerator()) {
    if (-not (Test-Path -LiteralPath $entry.Value -PathType Leaf)) {
        throw "Release version file is missing: $($entry.Value)"
    }
    $version = (Get-Content -LiteralPath $entry.Value -Raw -Encoding UTF8).Trim()
    if ($version -notmatch '^\d+\.\d+\.\d+(?:-[0-9A-Za-z.-]+)?$') {
        throw "$($entry.Key) release version is invalid: $version"
    }
    $versions[$entry.Key] = $version
}

$cmakeContent = Get-Content -LiteralPath (Join-Path $repositoryRoot 'CMakeLists.txt') -Raw
if ($cmakeContent -notmatch 'set\(PDF4QT_VERSION\s+([0-9]+(?:\.[0-9]+){3})\)') {
    throw 'CMakeLists.txt does not define a four-part PDF4QT_VERSION.'
}
$pdf4qtVersion = $Matches[1]
foreach ($relativePath in @('vcpkg.json', 'vcpkg_with_qt.json')) {
    $manifest = Get-Content -LiteralPath (Join-Path $repositoryRoot $relativePath) -Raw |
        ConvertFrom-Json
    if ($manifest.'version-string' -ne $pdf4qtVersion) {
        throw "$relativePath version '$($manifest.'version-string')' does not match PDF4QT_VERSION '$pdf4qtVersion'."
    }
}

$installerFiles = @(
    'installer\FamilyPDF.iss',
    'installer\FamilyPDF-Full.iss',
    'installer\FamilyPDF-OCR-Plugin.iss'
)
foreach ($relativePath in $installerFiles) {
    $content = Get-Content -LiteralPath (Join-Path $repositoryRoot $relativePath) -Raw
    if ($content -notmatch '#ifndef MyAppVersion' -or
        $content -notmatch '#define MyAppVersion "0\.0\.0-dev"') {
        throw "$relativePath does not require its build script to supply the release version."
    }
}

$buildScripts = @(
    'scripts\phase0\build-installer.ps1',
    'scripts\phase0\build-full-installer.ps1',
    'scripts\ocr\build-ocr-installer.ps1'
)
foreach ($relativePath in $buildScripts) {
    $content = Get-Content -LiteralPath (Join-Path $repositoryRoot $relativePath) -Raw
    if ($content -notmatch '/DMyAppVersion=') {
        throw "$relativePath does not pass its authoritative version to Inno Setup."
    }
}

$ocrBuildScript = Get-Content -LiteralPath (
    Join-Path $repositoryRoot 'scripts\ocr\build-ocr-plugin.ps1'
) -Raw
if ($ocrBuildScript -notmatch "OCR_VERSION") {
    throw 'The OCR package manifest does not read the authoritative OCR_VERSION file.'
}

$builtManifest = Join-Path $repositoryRoot 'dist\FamilyPDF-OCR-Plugin-windows-x64\FamilyPDF-OCR-Plugin.json'
if (Test-Path -LiteralPath $builtManifest -PathType Leaf) {
    $manifest = Get-Content -LiteralPath $builtManifest -Raw -Encoding UTF8 |
        ConvertFrom-Json
    if ($manifest.version -ne $versions.OCR) {
        throw "Built OCR manifest version '$($manifest.version)' does not match OCR_VERSION '$($versions.OCR)'."
    }
}

Write-Host "Release metadata passed: FamilyPDF $($versions.FamilyPDF), PDF4QT $pdf4qtVersion, OCR $($versions.OCR)."
