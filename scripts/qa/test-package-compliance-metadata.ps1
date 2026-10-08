[CmdletBinding()]
param(
    [string]$PackageRoot = ''
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$repositoryRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
if ([string]::IsNullOrWhiteSpace($PackageRoot)) {
    $PackageRoot = Join-Path $repositoryRoot 'dist\FamilyPDF-windows-x64'
}
$PackageRoot = [IO.Path]::GetFullPath($PackageRoot)

$requiredFiles = @(
    'THIRD-PARTY-NOTICES.txt',
    'THIRD-PARTY-SBOM\Qt\qtbase-6.9.1.spdx',
    'THIRD-PARTY-SBOM\Qt\qtsvg-6.9.1.spdx',
    'THIRD-PARTY-SBOM\Qt\qttranslations-6.9.1.spdx',
    'office-export\requirements.lock',
    'office-export\THIRD-PARTY-NOTICES.md'
)
foreach ($relativePath in $requiredFiles) {
    $path = Join-Path $PackageRoot $relativePath
    if (-not (Test-Path -LiteralPath $path -PathType Leaf) -or
        (Get-Item -LiteralPath $path).Length -eq 0) {
        throw "Required package compliance metadata is missing or empty: $path"
    }
}

Write-Host 'Package compliance metadata passed.'
