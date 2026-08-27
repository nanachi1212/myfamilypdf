[CmdletBinding()]
param(
    [string]$BuildDirectory = '',
    [string]$OutputDirectory = '',
    [switch]$SkipOcr,
    [switch]$SkipOfficeBuild
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$RepositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
. (Join-Path $RepositoryRoot 'scripts\common\Resolve-FamilyPDFToolsRoot.ps1')
$ToolsRoot = Resolve-FamilyPDFToolsRoot -RepositoryRoot $RepositoryRoot
if ([string]::IsNullOrWhiteSpace($BuildDirectory)) {
    $BuildDirectory = Join-Path $RepositoryRoot 'build\phase0-upstream-release'
}
if ([string]::IsNullOrWhiteSpace($OutputDirectory)) {
    $OutputDirectory = Join-Path $RepositoryRoot 'dist'
}
$BuildDirectory = [IO.Path]::GetFullPath($BuildDirectory)
$OutputDirectory = [IO.Path]::GetFullPath($OutputDirectory)

$officeExportSource = Join-Path $OutputDirectory 'FamilyPDF-Office-Export'
if (-not $SkipOfficeBuild) {
    & (Join-Path $RepositoryRoot 'scripts\office\build-office-export-helper.ps1') `
        -OutputDirectory $OutputDirectory
}
if (-not (Test-Path -LiteralPath (
    Join-Path $officeExportSource 'FamilyPDFOfficeExport.exe'
) -PathType Leaf)) {
    throw "Office export helper was not found: $officeExportSource"
}

$QtPrefix = Join-Path $ToolsRoot 'qt\6.9.1\msvc2022_64'
$runtimeDirectory = Join-Path $BuildDirectory 'usr\bin'
$vcpkgBin = Join-Path $BuildDirectory 'vcpkg_installed\x64-windows\bin'
$targets = @(
    'PdfTool',
    'Pdf4QtViewer',
    'Pdf4QtEditor',
    'Pdf4QtPageMaster',
    'Pdf4QtDiff'
)
# SkipOcr is retained for compatibility with older build commands. OCR is now
# always packaged separately by scripts\ocr\build-ocr-plugin.ps1.

if (-not (Test-Path -LiteralPath $runtimeDirectory -PathType Container)) {
    throw "Required path was not found: $runtimeDirectory"
}
foreach ($target in $targets) {
    $targetExecutable = Join-Path $runtimeDirectory "$target.exe"
    if (-not (Test-Path -LiteralPath $targetExecutable -PathType Leaf)) {
        throw "Required application was not found: $targetExecutable"
    }
}

New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null
$packageRoot = Join-Path $OutputDirectory 'FamilyPDF-windows-x64'
if (Test-Path -LiteralPath $packageRoot) {
    Remove-Item -LiteralPath $packageRoot -Recurse -Force
}
New-Item -ItemType Directory -Path $packageRoot -Force | Out-Null
Set-Content -LiteralPath (Join-Path $packageRoot 'portable.mode') -Value 'FamilyPDF portable data mode' -Encoding ASCII

Copy-Item -LiteralPath (Join-Path $RepositoryRoot 'LICENSE') `
    -Destination (Join-Path $packageRoot 'LICENSE-PDF4QT.txt') -Force
$baseLicenseRoot = Join-Path $packageRoot 'THIRD-PARTY-LICENSES\Base'
New-Item -ItemType Directory -Path $baseLicenseRoot -Force | Out-Null
$qtSbomRoot = Join-Path $packageRoot 'THIRD-PARTY-SBOM\Qt'
New-Item -ItemType Directory -Path $qtSbomRoot -Force | Out-Null
$qtSourceSbomRoot = Join-Path $QtPrefix 'sbom'
$qtSbomModules = @('qtbase', 'qtmultimedia', 'qtspeech', 'qtsvg', 'qttranslations')
foreach ($module in $qtSbomModules) {
    $sbom = Join-Path $qtSourceSbomRoot "$module-6.9.1.spdx"
    if (-not (Test-Path -LiteralPath $sbom -PathType Leaf)) {
        throw "Required Qt SBOM was not found: $sbom"
    }
    Copy-Item -LiteralPath $sbom -Destination $qtSbomRoot -Force
}
$vcpkgShareRoot = Join-Path $BuildDirectory 'vcpkg_installed\x64-windows\share'
$copiedLicensePackages = [Collections.Generic.List[string]]::new()
if (Test-Path -LiteralPath $vcpkgShareRoot -PathType Container) {
    foreach ($shareDirectory in Get-ChildItem -LiteralPath $vcpkgShareRoot -Directory) {
        $copyright = Join-Path $shareDirectory.FullName 'copyright'
        if (Test-Path -LiteralPath $copyright -PathType Leaf) {
            Copy-Item -LiteralPath $copyright `
                -Destination (Join-Path $baseLicenseRoot "$($shareDirectory.Name).txt") `
                -Force
            $copiedLicensePackages.Add($shareDirectory.Name)
        }
    }
}
if ($copiedLicensePackages.Count -eq 0) {
    throw "No third-party license files were found under $vcpkgShareRoot"
}
$baseNotice = @(
    'FamilyPDF third-party notices',
    '',
    'FamilyPDF is based on PDF4QT and dynamically links Qt 6 libraries.',
    'PDF4QT license: LICENSE-PDF4QT.txt',
    'Qt licensing information and corresponding source: https://www.qt.io/licensing/open-source-lgpl-obligations',
    'Qt source code: https://code.qt.io/cgit/qt/',
    'The multimedia runtime includes FFmpeg libraries distributed with Qt.',
    'FFmpeg licensing and source: https://ffmpeg.org/legal.html and https://ffmpeg.org/download.html',
    'Qt 6.9.1 package SBOMs, including the FFmpeg runtime relationship, are included in THIRD-PARTY-SBOM\Qt.',
    'Office export dependency hashes and notices are included under office-export.',
    'Microsoft Visual C++ and DirectX runtime files remain subject to Microsoft license terms.',
    '',
    'Bundled vcpkg dependency notices are included in THIRD-PARTY-LICENSES\Base:',
    ($copiedLicensePackages | Sort-Object | ForEach-Object { "- $_" })
)
$baseNotice | Set-Content `
    -LiteralPath (Join-Path $packageRoot 'THIRD-PARTY-NOTICES.txt') `
    -Encoding UTF8

# Start from a clean staging directory, then copy application, third-party and
# explicitly verified release Qt runtime files.
Get-ChildItem -LiteralPath $runtimeDirectory -File |
    Where-Object {
        $_.Name -notlike 'Qt6*.dll' -and
        $_.Name -notlike 'UnitTests*.exe' -and
        $_.Name -ne 'vc_redist.x64.exe'
    } |
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

$officeExportTarget = Join-Path $packageRoot 'office-export'
Copy-Item -LiteralPath $officeExportSource -Destination $officeExportTarget -Recurse -Force
Copy-Item -LiteralPath (Join-Path $RepositoryRoot 'office-export\requirements.lock') `
    -Destination $officeExportTarget -Force
Copy-Item -LiteralPath (Join-Path $RepositoryRoot 'office-export\THIRD-PARTY-NOTICES.md') `
    -Destination $officeExportTarget -Force

# Keep the portable package runnable on clean Windows installations without
# requiring an administrator-level VC++ Redistributable install.
$vsWhere = 'C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe'
if (-not (Test-Path -LiteralPath $vsWhere -PathType Leaf)) {
    throw "vswhere was not found: $vsWhere"
}
$visualStudioRoot = (& $vsWhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Redist.14.Latest -property installationPath |
    Select-Object -First 1)
if ([string]::IsNullOrWhiteSpace($visualStudioRoot)) {
    throw 'Visual C++ redistributable files were not found.'
}
$redistRoot = Join-Path $visualStudioRoot 'VC\Redist\MSVC'
$crtDirectory = Get-ChildItem -LiteralPath $redistRoot -Filter 'Microsoft.VC*.CRT' -Directory -Recurse |
    Where-Object { $_.FullName -match '\\x64\\Microsoft\.VC\d+\.CRT$' } |
    Sort-Object FullName -Descending |
    Select-Object -First 1
if (-not $crtDirectory) {
    throw "The x64 Visual C++ CRT directory was not found under $redistRoot"
}
Get-ChildItem -LiteralPath $crtDirectory.FullName -Filter '*.dll' -File |
    Copy-Item -Destination $packageRoot -Force

# Copy the paired Windows SDK DirectX Shader Compiler runtime explicitly.
$directXRuntimeDirectory =
    'C:\Program Files (x86)\Windows Kits\10\Redist\D3D\x64'
foreach ($directXRuntime in @('dxcompiler.dll', 'dxil.dll')) {
    $source = Join-Path $directXRuntimeDirectory $directXRuntime
    if (-not (Test-Path -LiteralPath $source -PathType Leaf)) {
        throw "DirectX Shader Compiler runtime was not found: $source"
    }
    Copy-Item -LiteralPath $source -Destination $packageRoot -Force
}

$requiredQtModules = @(
    'Qt6Concurrent.dll',
    'Qt6Core.dll',
    'Qt6Gui.dll',
    'Qt6Multimedia.dll',
    'Qt6Network.dll',
    'Qt6PrintSupport.dll',
    'Qt6Svg.dll',
    'Qt6TextToSpeech.dll',
    'Qt6Widgets.dll',
    'Qt6Xml.dll'
)
foreach ($module in $requiredQtModules) {
    $source = Join-Path $QtPrefix "bin\$module"
    if (-not (Test-Path -LiteralPath $source -PathType Leaf)) {
        throw "Required release Qt module was not found: $source"
    }
    Copy-Item -LiteralPath $source -Destination $packageRoot -Force
}

$requiredPlugins = @(
    'platforms\qwindows.dll',
    'iconengines\qsvgicon.dll',
    'imageformats\qgif.dll',
    'imageformats\qico.dll',
    'imageformats\qjpeg.dll',
    'imageformats\qsvg.dll',
    'styles\qmodernwindowsstyle.dll',
    'texttospeech\qtexttospeech_mock.dll',
    'texttospeech\qtexttospeech_sapi.dll',
    'texttospeech\qtexttospeech_winrt.dll',
    'multimedia\ffmpegmediaplugin.dll',
    'multimedia\windowsmediaplugin.dll',
    'networkinformation\qnetworklistmanager.dll',
    'tls\qcertonlybackend.dll',
    'tls\qschannelbackend.dll'
)
foreach ($plugin in $requiredPlugins) {
    $source = Join-Path $QtPrefix "plugins\$plugin"
    $destination = Join-Path $packageRoot $plugin
    if (-not (Test-Path -LiteralPath $source -PathType Leaf)) {
        throw "Required release Qt plugin was not found: $source"
    }
    New-Item -ItemType Directory -Path (Split-Path $destination) -Force | Out-Null
    Copy-Item -LiteralPath $source -Destination $destination -Force
}

$zipPath = Join-Path $OutputDirectory 'FamilyPDF-windows-x64.zip'
if (Test-Path -LiteralPath $zipPath) {
    Remove-Item -LiteralPath $zipPath -Force
}
Compress-Archive -LiteralPath $packageRoot -DestinationPath $zipPath -CompressionLevel Optimal

Write-Host "Package directory: $packageRoot"
Write-Host "Package archive: $zipPath"
