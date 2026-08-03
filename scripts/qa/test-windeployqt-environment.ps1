[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$repositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$testRoot = Join-Path $repositoryRoot 'build\windeployqt-environment-test'
$packageScript = Join-Path $repositoryRoot `
    'scripts\phase0\package-windows-runtime.ps1'
$officeExportSource = Join-Path $repositoryRoot `
    'dist\FamilyPDF-Office-Export'
$officeExportTarget = Join-Path $testRoot 'FamilyPDF-Office-Export'

if (-not (Test-Path -LiteralPath $officeExportSource -PathType Container)) {
    throw "Office export helper was not found: $officeExportSource"
}

$packageScriptText = Get-Content -LiteralPath $packageScript -Raw
if ($packageScriptText -match '(?im)^\s*&\s+\$windeployqt\b') {
    throw 'Packaging must not execute the unstable Qt 6.9.1 windeployqt.'
}

Remove-Item -LiteralPath $testRoot -Recurse -Force `
    -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Path $testRoot -Force | Out-Null
Copy-Item -LiteralPath $officeExportSource -Destination $officeExportTarget `
    -Recurse -Force

try {
    $packageWarnings = @()
    $output = (& $packageScript -OutputDirectory $testRoot `
            -SkipOfficeBuild -WarningVariable packageWarnings `
            2>&1 | Out-String)
    $output += "`n" + ($packageWarnings | Out-String)

    foreach ($unexpected in @(
            'VCINSTALLDIR is not set',
            'Cannot find any version of the dxcompiler.dll and dxil.dll',
            'windeployqt failed with exit code',
            'Using the explicit release Qt runtime fallback'
        )) {
        if ($output -match [regex]::Escape($unexpected)) {
            throw "Packaging emitted an environment warning: $unexpected"
        }
    }

    foreach ($requiredFile in @(
            'FamilyPDF-windows-x64\Qt6Core.dll',
            'FamilyPDF-windows-x64\platforms\qwindows.dll',
            'FamilyPDF-windows-x64\dxcompiler.dll',
            'FamilyPDF-windows-x64\dxil.dll'
        )) {
        $path = Join-Path $testRoot $requiredFile
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
            throw "Packaged runtime file was not found: $path"
        }
    }
}
finally {
    Remove-Item -LiteralPath $testRoot -Recurse -Force `
        -ErrorAction SilentlyContinue
}

Write-Host 'windeployqt environment test passed.'
