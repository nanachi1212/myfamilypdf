[CmdletBinding()]
param(
    [string]$VirtualEnvironment = '',
    [string]$OutputDirectory = '',
    [switch]$SkipInstall,
    [switch]$SkipTests
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$repositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$officeRoot = Join-Path $repositoryRoot 'office-export'
if ([string]::IsNullOrWhiteSpace($VirtualEnvironment)) {
    $VirtualEnvironment = Join-Path (
        Split-Path $repositoryRoot -Parent
    ) 'FamilyPDF-tools\office-export-venv'
}
if ([string]::IsNullOrWhiteSpace($OutputDirectory)) {
    $OutputDirectory = Join-Path $repositoryRoot 'dist'
}
$VirtualEnvironment = [IO.Path]::GetFullPath($VirtualEnvironment)
$OutputDirectory = [IO.Path]::GetFullPath($OutputDirectory)
$venvPython = Join-Path $VirtualEnvironment 'Scripts\python.exe'

if (-not $SkipInstall) {
    & (Join-Path $PSScriptRoot 'install-office-export-toolchain.ps1') `
        -VirtualEnvironment $VirtualEnvironment
}
if (-not (Test-Path -LiteralPath $venvPython -PathType Leaf)) {
    throw "Office export Python was not found: $venvPython"
}

if (-not $SkipTests) {
    Push-Location $officeRoot
    try {
        & $venvPython -m unittest discover -s tests -v
        if ($LASTEXITCODE -ne 0) {
            throw "Office export tests failed with exit code $LASTEXITCODE."
        }
    }
    finally {
        Pop-Location
    }
}

$buildRoot = Join-Path (
    Split-Path $repositoryRoot -Parent
) 'FamilyPDF-tools\office-export-build'
$pyInstallerDist = Join-Path $buildRoot 'dist'
$pyInstallerWork = Join-Path $buildRoot 'work'
$specRoot = Join-Path $buildRoot 'spec'
foreach ($directory in @($pyInstallerDist, $pyInstallerWork, $specRoot)) {
    New-Item -ItemType Directory -Path $directory -Force | Out-Null
}

& $venvPython -m PyInstaller `
    --name FamilyPDFOfficeExport `
    --onedir `
    --console `
    --clean `
    --noconfirm `
    --paths $officeRoot `
    --collect-all pdfplumber `
    --distpath $pyInstallerDist `
    --workpath $pyInstallerWork `
    --specpath $specRoot `
    (Join-Path $officeRoot 'entrypoint.py')
if ($LASTEXITCODE -ne 0) {
    throw "Office export helper build failed with exit code $LASTEXITCODE."
}

New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null
$packageRoot = Join-Path $OutputDirectory 'FamilyPDF-Office-Export'
if (Test-Path -LiteralPath $packageRoot -PathType Container) {
    Remove-Item -LiteralPath $packageRoot -Recurse -Force
}
Copy-Item -LiteralPath (
    Join-Path $pyInstallerDist 'FamilyPDFOfficeExport'
) -Destination $packageRoot -Recurse

$helper = Join-Path $packageRoot 'FamilyPDFOfficeExport.exe'
if (-not (Test-Path -LiteralPath $helper -PathType Leaf)) {
    throw "Packaged Office export helper is missing: $helper"
}
& $helper --help | Out-Null
if ($LASTEXITCODE -ne 0) {
    throw 'Packaged Office export helper smoke test failed.'
}

Write-Host "Office export helper: $helper"
