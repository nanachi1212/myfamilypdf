[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$repositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$checks = @(
    @{
        Path = 'Pdf4QtEditorPlugins\OfficeExportPlugin\officeexportplugin.cpp'
        TimeoutVariable = 'FAMILYPDF_OFFICE_EXPORT_TIMEOUT_MS'
    },
    @{
        Path = 'Pdf4QtEditorPlugins\ScannerPlugin\wiascannerbackend.cpp'
        TimeoutVariable = 'FAMILYPDF_SCANNER_TIMEOUT_MS'
    }
)

foreach ($check in $checks) {
    $path = Join-Path $repositoryRoot $check.Path
    $content = Get-Content -LiteralPath $path -Raw -Encoding UTF8
    if ($content -notmatch [regex]::Escape($check.TimeoutVariable)) {
        throw "$($check.Path) does not expose the bounded timeout $($check.TimeoutVariable)."
    }
    if ($content -match 'waitForFinished\s*\(\s*-1\s*\)') {
        throw "$($check.Path) still contains an unbounded waitForFinished(-1)."
    }
    if ($content -notmatch 'waitForFinished\s*\(\s*5000\s*\)') {
        throw "$($check.Path) does not bound process cleanup to 5000 ms."
    }
}

Write-Host 'External process timeout contract passed.'
