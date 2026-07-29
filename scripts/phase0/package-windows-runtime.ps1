[CmdletBinding()]
param(
    [string]$BuildDirectory = '',
    [string]$OutputDirectory = '',
    [switch]$SkipOcr
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$RepositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
if ([string]::IsNullOrWhiteSpace($BuildDirectory)) {
    $BuildDirectory = Join-Path $RepositoryRoot 'build\phase0-upstream-release'
}
if ([string]::IsNullOrWhiteSpace($OutputDirectory)) {
    $OutputDirectory = Join-Path $RepositoryRoot 'dist'
}
$BuildDirectory = [IO.Path]::GetFullPath($BuildDirectory)
$OutputDirectory = [IO.Path]::GetFullPath($OutputDirectory)

$QtPrefix = 'E:\CodexProject\FamilyPDF-tools\qt\6.9.1\msvc2022_64'
$runtimeDirectory = Join-Path $BuildDirectory 'usr\bin'
$windeployqt = Join-Path $QtPrefix 'bin\windeployqt.exe'
$vcpkgBin = Join-Path $BuildDirectory 'vcpkg_installed\x64-windows\bin'
$targets = @(
    'PdfTool',
    'Pdf4QtViewer',
    'Pdf4QtEditor',
    'Pdf4QtPageMaster',
    'UnitTests',
    'UnitTestsImageOptimizer',
    'UnitTestsFontEncoding'
)
$ocrManifestRoot = Join-Path $RepositoryRoot 'ocr-spike'
$ocrInstallRoot = Join-Path $ocrManifestRoot 'vcpkg_installed'
$ocrTripletRoot = Join-Path $ocrInstallRoot 'x64-windows'
$ocrExecutable = Join-Path $ocrTripletRoot 'tools\tesseract\tesseract.exe'
$ocrTessdata = Join-Path $ocrManifestRoot 'tessdata'

foreach ($path in @($windeployqt, $runtimeDirectory)) {
    if (-not (Test-Path -LiteralPath $path)) {
        throw "Required path was not found: $path"
    }
}

New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null
$packageRoot = Join-Path $OutputDirectory 'FamilyPDF-windows-x64'
if (Test-Path -LiteralPath $packageRoot) {
    Remove-Item -LiteralPath $packageRoot -Recurse -Force
}
New-Item -ItemType Directory -Path $packageRoot -Force | Out-Null

# Start from a clean staging directory. Copy application and third-party files,
# then let windeployqt calculate the actually required release Qt runtime.
Get-ChildItem -LiteralPath $runtimeDirectory -File |
    Where-Object { $_.Name -notlike 'Qt6*.dll' } |
    Copy-Item -Destination $packageRoot -Force
foreach ($applicationDirectory in @('pdfplugins', 'translations')) {
    $sourceDirectory = Join-Path $runtimeDirectory $applicationDirectory
    if (Test-Path -LiteralPath $sourceDirectory -PathType Container) {
        Copy-Item -LiteralPath $sourceDirectory -Destination $packageRoot -Recurse -Force
    }
}
if (Test-Path -LiteralPath $vcpkgBin -PathType Container) {
    Get-ChildItem -LiteralPath $vcpkgBin -Filter '*.dll' -File |
        Copy-Item -Destination $packageRoot -Force
}

$qtDeploymentFailed = $false
foreach ($target in $targets) {
    $executable = Join-Path $runtimeDirectory "$target.exe"
    if (-not (Test-Path -LiteralPath $executable -PathType Leaf)) {
        Write-Warning "Skipping missing target: $target.exe"
        continue
    }
    & $windeployqt --release --dir $packageRoot --no-translations --no-system-d3d-compiler --no-opengl-sw $executable | Out-Null
    if ($LASTEXITCODE -ne 0) {
        $qtDeploymentFailed = $true
        Write-Warning "windeployqt failed for $target with exit code $LASTEXITCODE."
    }
}

if ($qtDeploymentFailed) {
    Write-Warning 'Using the explicit release Qt runtime fallback.'
    Get-ChildItem -LiteralPath (Join-Path $QtPrefix 'bin') -Filter 'Qt6*.dll' -File |
        Where-Object { $_.BaseName -cnotmatch 'd$' } |
        Copy-Item -Destination $packageRoot -Force
    foreach ($plugin in @('platforms\qwindows.dll', 'iconengines\qsvgicon.dll')) {
        $source = Join-Path $QtPrefix "plugins\$plugin"
        $destination = Join-Path $packageRoot $plugin
        New-Item -ItemType Directory -Path (Split-Path $destination) -Force | Out-Null
        if (Test-Path -LiteralPath $source -PathType Leaf) {
            Copy-Item -LiteralPath $source -Destination $destination -Force
        }
    }
    foreach ($pluginDirectory in @('imageformats', 'styles', 'texttospeech')) {
        $sourceDirectory = Join-Path $QtPrefix "plugins\$pluginDirectory"
        if (Test-Path -LiteralPath $sourceDirectory -PathType Container) {
            Copy-Item -LiteralPath $sourceDirectory -Destination $packageRoot -Recurse -Force
        }
    }
}

if (-not $SkipOcr) {
    $downloadTessdata = Join-Path $ocrManifestRoot 'download-tessdata.ps1'
    & $downloadTessdata

    if (-not (Test-Path -LiteralPath $ocrExecutable -PathType Leaf)) {
        $vcpkgExecutable = 'E:\CodexProject\FamilyPDF-tools\vcpkg\vcpkg.exe'
        if (-not (Test-Path -LiteralPath $vcpkgExecutable -PathType Leaf)) {
            throw "vcpkg was not found: $vcpkgExecutable"
        }
        & $vcpkgExecutable install `
            "--x-manifest-root=$ocrManifestRoot" `
            "--x-install-root=$ocrInstallRoot" `
            '--triplet=x64-windows' `
            '--clean-after-build'
        if ($LASTEXITCODE -ne 0) {
            throw "OCR dependency installation failed with exit code $LASTEXITCODE."
        }
    }

    $ocrPackageDirectory = Join-Path $packageRoot 'ocr'
    New-Item -ItemType Directory -Path $ocrPackageDirectory -Force | Out-Null
    Copy-Item -LiteralPath $ocrExecutable -Destination $ocrPackageDirectory -Force
    Get-ChildItem -LiteralPath (Join-Path $ocrTripletRoot 'bin') -Filter '*.dll' -File |
        Copy-Item -Destination $ocrPackageDirectory -Force
    Copy-Item -LiteralPath $ocrTessdata -Destination $ocrPackageDirectory -Recurse -Force
    Copy-Item -LiteralPath (Join-Path $RepositoryRoot 'scripts\ocr\FamilyPDF-OCR.ps1') -Destination $packageRoot -Force
    Copy-Item -LiteralPath (Join-Path $RepositoryRoot 'scripts\ocr\FamilyPDF-OCR.cmd') -Destination $packageRoot -Force
}

$zipPath = Join-Path $OutputDirectory 'FamilyPDF-windows-x64.zip'
if (Test-Path -LiteralPath $zipPath) {
    Remove-Item -LiteralPath $zipPath -Force
}
Compress-Archive -LiteralPath $packageRoot -DestinationPath $zipPath -CompressionLevel Optimal

Write-Host "Package directory: $packageRoot"
Write-Host "Package archive: $zipPath"
