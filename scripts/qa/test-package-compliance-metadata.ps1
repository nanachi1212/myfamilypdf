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
    'THIRD-PARTY-SBOM\Qt\qtmultimedia-6.9.1.spdx',
    'THIRD-PARTY-SBOM\Qt\qtspeech-6.9.1.spdx',
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

$multimediaSbom = Get-Content -LiteralPath (
    Join-Path $PackageRoot 'THIRD-PARTY-SBOM\Qt\qtmultimedia-6.9.1.spdx'
) -Raw -Encoding UTF8
if ($multimediaSbom -notmatch '(?m)^PackageName: FFmpeg\r?$') {
    throw 'The packaged Qt Multimedia SBOM does not identify the bundled FFmpeg runtime.'
}

Write-Host 'Package compliance metadata passed.'
