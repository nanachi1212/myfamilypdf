[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$repositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path

$signaturePath = Join-Path $repositoryRoot 'Pdf4QtEditorPlugins\SignaturePlugin\signatureplugin.cpp'
$signature = Get-Content -LiteralPath $signaturePath -Raw -Encoding UTF8
if ($signature -notmatch '#include <QSaveFile>') {
    throw 'SignaturePlugin must include QSaveFile for signed-document output.'
}
if ($signature -notmatch 'QSaveFile\s+signedFile\(fileName\)') {
    throw 'SignaturePlugin signed-document output must use QSaveFile.'
}
if ($signature -match 'QFile\s+signedFile\(fileName\)') {
    throw 'SignaturePlugin still uses non-atomic QFile output for signed documents.'
}
if ($signature -notmatch 'signedFile\.commit\(\)') {
    throw 'SignaturePlugin must commit the signed document atomically.'
}

$sidebarPath = Join-Path $repositoryRoot 'Pdf4QtLibGui\pdfsidebarwidget.cpp'
$sidebar = Get-Content -LiteralPath $sidebarPath -Raw -Encoding UTF8
if ($sidebar -notmatch '#include <QSaveFile>') {
    throw 'PDFSidebarWidget must include QSaveFile for attachment output.'
}
if ($sidebar -notmatch 'QSaveFile\s+file\(fileName\)') {
    throw 'PDFSidebarWidget attachment output must use QSaveFile.'
}

Write-Host 'Atomic output and TTS proxy contracts passed.'
