[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$repositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$testRoot = Join-Path $repositoryRoot 'build\final-regression-retention-test'
$cleanupScript = Join-Path $PSScriptRoot `
    'cleanup-final-regression-results.ps1'

Remove-Item -LiteralPath $testRoot -Recurse -Force `
    -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Path $testRoot -Force | Out-Null

$oldest = Join-Path $testRoot 'final-regression-20260801-090000'
$previous = Join-Path $testRoot 'final-regression-20260802-090000'
$current = Join-Path $testRoot 'final-regression-20260803-090000'
$unrelated = Join-Path $testRoot 'keep-this-directory'
foreach ($directory in @($oldest, $previous, $current, $unrelated)) {
    New-Item -ItemType Directory -Path $directory -Force | Out-Null
    Set-Content -LiteralPath (Join-Path $directory 'marker.txt') `
        -Value $directory -Encoding UTF8
}

try {
    & $cleanupScript -BuildRoot $testRoot -Keep 1 `
        -CurrentResult $current

    foreach ($removed in @($oldest, $previous)) {
        if (Test-Path -LiteralPath $removed) {
            throw "Superseded result was not removed: $removed"
        }
    }
    foreach ($preserved in @($current, $unrelated)) {
        if (-not (Test-Path -LiteralPath $preserved -PathType Container)) {
            throw "Required directory was removed: $preserved"
        }
    }
}
finally {
    Remove-Item -LiteralPath $testRoot -Recurse -Force `
        -ErrorAction SilentlyContinue
}

Write-Host 'Final regression retention test passed.'
