[CmdletBinding()]
param(
    [string]$BuildDirectory = '',
    [string]$OutputDirectory = ''
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
$targets = @('PdfTool', 'Pdf4QtViewer', 'Pdf4QtEditor', 'Pdf4QtPageMaster')

foreach ($path in @($windeployqt, $runtimeDirectory)) {
    if (-not (Test-Path -LiteralPath $path)) {
        throw "Required path was not found: $path"
    }
}

foreach ($target in $targets) {
    $executable = Join-Path $runtimeDirectory "$target.exe"
    if (-not (Test-Path -LiteralPath $executable)) {
        Write-Warning "Skipping missing target: $target.exe"
        continue
    }
    & $windeployqt --release --no-translations --no-system-d3d-compiler --no-opengl-sw $executable | Out-Null
    if ($LASTEXITCODE -ne 0) {
        Write-Warning "windeployqt failed for $target with exit code $LASTEXITCODE; using explicit Qt DLL deployment."
    }
}

Get-ChildItem -LiteralPath (Join-Path $QtPrefix 'bin') -Filter 'Qt6*.dll' -File |
    Copy-Item -Destination $runtimeDirectory -Force
foreach ($plugin in @('platforms\qwindows.dll', 'iconengines\qsvgicon.dll')) {
    $source = Join-Path $QtPrefix "plugins\$plugin"
    $destination = Join-Path $runtimeDirectory $plugin
    New-Item -ItemType Directory -Path (Split-Path $destination) -Force | Out-Null
    if (Test-Path -LiteralPath $source) { Copy-Item -LiteralPath $source -Destination $destination -Force }
}
foreach ($pluginDirectory in @('imageformats', 'styles', 'texttospeech')) {
    $sourceDirectory = Join-Path $QtPrefix "plugins\$pluginDirectory"
    if (Test-Path -LiteralPath $sourceDirectory) {
        $destinationDirectory = Join-Path $runtimeDirectory $pluginDirectory
        New-Item -ItemType Directory -Path $destinationDirectory -Force | Out-Null
        Get-ChildItem -LiteralPath $sourceDirectory -Filter '*.dll' -File |
            Copy-Item -Destination $destinationDirectory -Force
    }
}

if (Test-Path -LiteralPath $vcpkgBin -PathType Container) {
    Get-ChildItem -LiteralPath $vcpkgBin -Filter '*.dll' -File | Copy-Item -Destination $runtimeDirectory -Force
}

New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null
$packageRoot = Join-Path $OutputDirectory 'FamilyPDF-windows-x64'
if (Test-Path -LiteralPath $packageRoot) {
    Remove-Item -LiteralPath $packageRoot -Recurse -Force
}
New-Item -ItemType Directory -Path $packageRoot -Force | Out-Null
Get-ChildItem -LiteralPath $runtimeDirectory -Force |
    Copy-Item -Destination $packageRoot -Recurse -Force

$zipPath = Join-Path $OutputDirectory 'FamilyPDF-windows-x64.zip'
if (Test-Path -LiteralPath $zipPath) {
    Remove-Item -LiteralPath $zipPath -Force
}
Compress-Archive -LiteralPath $packageRoot -DestinationPath $zipPath -CompressionLevel Optimal

Write-Host "Package directory: $packageRoot"
Write-Host "Package archive: $zipPath"
