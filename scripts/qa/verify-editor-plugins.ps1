[CmdletBinding()]
param(
    [string]$PackageDirectory = ''
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$repositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
if ([string]::IsNullOrWhiteSpace($PackageDirectory)) {
    $PackageDirectory = Join-Path $repositoryRoot 'dist\FamilyPDF-windows-x64'
}
$PackageDirectory = [IO.Path]::GetFullPath($PackageDirectory)

$pluginDirectory = Join-Path $PackageDirectory 'pdfplugins'
$requiredPlugins = @(
    'EditorPlugin.dll',
    'RedactPlugin.dll',
    'SignaturePlugin.dll'
    'FormPlugin.dll'
    'DocumentEditPlugin.dll'
)

$missingPlugins = @(
    foreach ($plugin in $requiredPlugins) {
        $pluginPath = Join-Path $pluginDirectory $plugin
        if (-not (Test-Path -LiteralPath $pluginPath -PathType Leaf)) {
            $plugin
        }
    }
)

if ($missingPlugins.Count -gt 0) {
    Write-Error "FamilyPDF package is missing editor plugins: $($missingPlugins -join ', ')"
    exit 1
}

foreach ($plugin in $requiredPlugins) {
    $pluginPath = Join-Path $pluginDirectory $plugin
    $file = Get-Item -LiteralPath $pluginPath
    Write-Host ("OK {0} ({1} bytes)" -f $file.Name, $file.Length)
}

Write-Host "Editor plugin verification passed: $pluginDirectory"
