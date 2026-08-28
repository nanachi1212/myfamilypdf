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

$ttsPath = Join-Path $repositoryRoot 'Pdf4QtLibGui\pdftexttospeech.cpp'
$tts = Get-Content -LiteralPath $ttsPath -Raw -Encoding UTF8
if ($tts -notmatch '(?s)void PDFTextToSpeech::setProxy\(pdf::PDFDrawWidgetProxy\* proxy\).*?m_proxy = proxy;\s*if \(!m_proxy\)') {
    throw 'PDFTextToSpeech::setProxy must reject a null proxy before dereferencing it.'
}
if ($tts -notmatch '(?s)void PDFTextToSpeech::updatePlay\(\).*?if \(!m_proxy \|\| !m_document \|\| !m_textToSpeech\)') {
    throw 'PDFTextToSpeech::updatePlay must guard proxy and document in release builds.'
}
if ($tts -notmatch '(?s)void PDFTextToSpeech::updateVoices\(\).*?if \(!m_textToSpeech \|\| !m_speechVoiceComboBox\)') {
    throw 'PDFTextToSpeech::updateVoices must guard the speech engine and voice control.'
}
if ($tts -notmatch '(?s)void PDFTextToSpeech::setSettings\(.*?if \(!viewerSettings \|\| !m_initialized\)') {
    throw 'PDFTextToSpeech::setSettings must guard null settings and uninitialized UI.'
}
if ($tts -notmatch '(?s)void PDFTextToSpeech::updateUI\(\).*?if \(!m_initialized \|\|') {
    throw 'PDFTextToSpeech::updateUI must be safe before UI initialization.'
}
if ($tts -notmatch '(?s)void PDFTextToSpeech::updateToNextPage\(pdf::PDFInteger pageIndex\).*?if \(!m_document \|\| !m_proxy \|\| !m_speechSynchronizeButton') {
    throw 'PDFTextToSpeech::updateToNextPage must guard document, proxy, and synchronize control.'
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
