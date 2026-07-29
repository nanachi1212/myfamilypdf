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
    'Pdf4QtPageMaster'
)
# SkipOcr is retained for compatibility with older build commands. OCR is now
# always packaged separately by scripts\ocr\build-ocr-plugin.ps1.

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
Set-Content -LiteralPath (Join-Path $packageRoot 'portable.mode') -Value 'FamilyPDF portable data mode' -Encoding ASCII

# Start from a clean staging directory. Copy application and third-party files,
# then let windeployqt calculate the actually required release Qt runtime.
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
}

$zipPath = Join-Path $OutputDirectory 'FamilyPDF-windows-x64.zip'
if (Test-Path -LiteralPath $zipPath) {
    Remove-Item -LiteralPath $zipPath -Force
}
Compress-Archive -LiteralPath $packageRoot -DestinationPath $zipPath -CompressionLevel Optimal

Write-Host "Package directory: $packageRoot"
Write-Host "Package archive: $zipPath"
