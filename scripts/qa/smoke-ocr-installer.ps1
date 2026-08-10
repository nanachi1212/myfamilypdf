[CmdletBinding()]
param(
    [switch]$SkipBuild
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$repositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$buildRoot = Join-Path $repositoryRoot 'build'
$setup = Join-Path $buildRoot 'FamilyPDF-OCR-Plugin-Smoke-Setup-x64.exe'
$testRoot = Join-Path $buildRoot 'ocr-installer-smoke'
$installRoot = Join-Path $testRoot 'installed'
$packageRoot = Join-Path $repositoryRoot 'dist\FamilyPDF-OCR-Plugin-windows-x64'
$corePackageRoot = Join-Path $repositoryRoot 'dist\FamilyPDF-windows-x64'

if (-not $SkipBuild) {
    & (Join-Path $repositoryRoot 'scripts\ocr\build-ocr-installer.ps1') `
        -SkipPackage `
        -StandaloneSmokeBuild
}
if (-not (Test-Path -LiteralPath $setup -PathType Leaf)) {
    throw "OCR smoke installer was not found: $setup"
}

$resolvedBuildRoot = [IO.Path]::GetFullPath($buildRoot).TrimEnd('\') + '\'
$resolvedTestRoot = [IO.Path]::GetFullPath($testRoot)
if (-not $resolvedTestRoot.StartsWith(
        $resolvedBuildRoot,
        [StringComparison]::OrdinalIgnoreCase
    )) {
    throw "Refusing to replace a test directory outside build: $resolvedTestRoot"
}
if (Test-Path -LiteralPath $resolvedTestRoot) {
    Remove-Item -LiteralPath $resolvedTestRoot -Recurse -Force
}
New-Item -ItemType Directory -Path $resolvedTestRoot | Out-Null
New-Item -ItemType Directory -Path $installRoot | Out-Null

# Seed the shared application directory with the real Core payload. This
# verifies that removing the optional OCR plugin cannot damage FamilyPDF.
Copy-Item -Path (Join-Path $corePackageRoot '*') `
    -Destination $installRoot -Recurse -Force
$corePrefix = [IO.Path]::GetFullPath($corePackageRoot).TrimEnd('\') + '\'
$coreFiles = @(Get-ChildItem -LiteralPath $corePackageRoot -File -Recurse)

$install = Start-Process -FilePath $setup -ArgumentList @(
    '/VERYSILENT',
    '/SUPPRESSMSGBOXES',
    '/NORESTART',
    '/SP-',
    "/DIR=$installRoot"
) -WindowStyle Hidden -Wait -PassThru
if ($install.ExitCode -ne 0) {
    throw "OCR smoke installer failed with exit code $($install.ExitCode)."
}

$packagePrefix = [IO.Path]::GetFullPath($packageRoot).TrimEnd('\') + '\'
$packageFiles = @(Get-ChildItem -LiteralPath $packageRoot -File -Recurse)
foreach ($sourceFile in $packageFiles) {
    $relativePath = $sourceFile.FullName.Substring($packagePrefix.Length)
    $installedFile = Join-Path $installRoot $relativePath
    if (-not (Test-Path -LiteralPath $installedFile -PathType Leaf)) {
        throw "Installed OCR package is missing: $relativePath"
    }
    $sourceHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $sourceFile.FullName).Hash
    $installedHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $installedFile).Hash
    if ($sourceHash -ne $installedHash) {
        throw "Installed OCR package differs from source: $relativePath"
    }
}

$uninstaller = Join-Path $installRoot 'ocr-plugin-uninstall\unins000.exe'
if (-not (Test-Path -LiteralPath $uninstaller -PathType Leaf)) {
    throw "OCR uninstaller was not created: $uninstaller"
}
$uninstall = Start-Process -FilePath $uninstaller -ArgumentList @(
    '/VERYSILENT',
    '/SUPPRESSMSGBOXES',
    '/NORESTART'
) -WindowStyle Hidden -Wait -PassThru
if ($uninstall.ExitCode -ne 0) {
    throw "OCR smoke uninstaller failed with exit code $($uninstall.ExitCode)."
}

foreach ($relativePath in @(
    'FamilyPDF-OCR.ps1',
    'FamilyPDF-OCR-Plugin.json',
    'ocr\tesseract.exe',
    'THIRD-PARTY-NOTICES-OCR.txt'
)) {
    if (Test-Path -LiteralPath (Join-Path $installRoot $relativePath)) {
        throw "OCR uninstall left a packaged file behind: $relativePath"
    }
}

foreach ($sourceFile in $coreFiles) {
    $relativePath = $sourceFile.FullName.Substring($corePrefix.Length)
    $installedFile = Join-Path $installRoot $relativePath
    if (-not (Test-Path -LiteralPath $installedFile -PathType Leaf)) {
        throw "OCR uninstall removed a Core file: $relativePath"
    }
    $sourceHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $sourceFile.FullName).Hash
    $installedHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $installedFile).Hash
    if ($sourceHash -ne $installedHash) {
        throw "OCR install/uninstall changed a Core file: $relativePath"
    }
}

$viewer = Start-Process -FilePath (Join-Path $installRoot 'Pdf4QtViewer.exe') `
    -PassThru
try {
    Start-Sleep -Seconds 5
    $viewer.Refresh()
    if ($viewer.HasExited -or -not $viewer.Responding) {
        throw 'Viewer did not remain responsive after OCR uninstall.'
    }
}
finally {
    if (-not $viewer.HasExited) {
        Stop-Process -Id $viewer.Id -Force
        $viewer.WaitForExit(5000) | Out-Null
    }
}

$summary = [ordered]@{
    recorded_at = [DateTimeOffset]::Now.ToString('o')
    installer = $setup
    isolated_app_id = '{CA617566-1078-46DE-A3CA-CC1B27E9A963}'
    package_files_verified = $packageFiles.Count
    core_files_preserved = $coreFiles.Count
    viewer_responding_after_uninstall = $true
    install_exit_code = $install.ExitCode
    uninstall_exit_code = $uninstall.ExitCode
    uninstall_verified = $true
}
$summaryPath = Join-Path $testRoot 'summary.json'
$summary | ConvertTo-Json -Depth 4 |
    Set-Content -LiteralPath $summaryPath -Encoding UTF8

Remove-Item -LiteralPath $setup -Force
Write-Host "OCR installer install/uninstall smoke passed: $summaryPath"
