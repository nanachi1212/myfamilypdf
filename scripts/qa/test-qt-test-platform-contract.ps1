[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$repositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$cmake = Get-Content -LiteralPath (Join-Path $repositoryRoot 'UnitTests\CMakeLists.txt') -Raw -Encoding UTF8

if ($cmake -notmatch 'set_tests_properties\(UnitTestsBookmarks\s+PROPERTIES\s+ENVIRONMENT\s+"QT_QPA_PLATFORM=offscreen"') {
    throw 'UnitTestsBookmarks must run with QT_QPA_PLATFORM=offscreen.'
}
if ($cmake -notmatch 'QOffscreenIntegrationPlugin') {
    throw 'UnitTestsBookmarks must deploy Qt offscreen integration plugin.'
}

Write-Host 'Qt test platform contract passed.'
