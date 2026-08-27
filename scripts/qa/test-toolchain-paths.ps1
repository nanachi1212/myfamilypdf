[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$repositoryRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$resolverPath = Join-Path $repositoryRoot 'scripts\common\Resolve-FamilyPDFToolsRoot.ps1'
if (-not (Test-Path -LiteralPath $resolverPath -PathType Leaf)) {
    throw "Toolchain root resolver is missing: $resolverPath"
}
. $resolverPath

$expectedDefault = [IO.Path]::GetFullPath(
    (Join-Path (Split-Path $repositoryRoot -Parent) 'FamilyPDF-tools')
)
$actualDefault = Resolve-FamilyPDFToolsRoot -RepositoryRoot $repositoryRoot
if ($actualDefault -ne $expectedDefault) {
    throw "Default tools root '$actualDefault' does not match '$expectedDefault'."
}

$savedOverride = $env:FAMILYPDF_TOOLS_ROOT
try {
    $override = Join-Path $env:TEMP 'FamilyPDF custom tools'
    $env:FAMILYPDF_TOOLS_ROOT = $override
    $actualOverride = Resolve-FamilyPDFToolsRoot -RepositoryRoot $repositoryRoot
    if ($actualOverride -ne [IO.Path]::GetFullPath($override)) {
        throw "Environment override was ignored: $actualOverride"
    }
}
finally {
    $env:FAMILYPDF_TOOLS_ROOT = $savedOverride
}

$activeScripts = @(
    'scripts\phase0\build-upstream-baseline.ps1',
    'scripts\phase0\package-windows-runtime.ps1',
    'scripts\phase0\build-installer.ps1',
    'scripts\phase0\build-full-installer.ps1',
    'scripts\phase0\install-build-toolchain.ps1',
    'scripts\ocr\build-ocr-plugin.ps1',
    'scripts\ocr\build-ocr-installer.ps1',
    'scripts\qa\run-final-regression.ps1'
)
foreach ($relativePath in $activeScripts) {
    $content = Get-Content -LiteralPath (Join-Path $repositoryRoot $relativePath) -Raw
    if ($content -match '[A-Za-z]:\\[^''"\r\n]*FamilyPDF-tools') {
        throw "$relativePath contains a fixed FamilyPDF-tools path: $($Matches[0])"
    }
}

Write-Host "Toolchain path contract passed: $actualDefault"
