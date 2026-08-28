$ErrorActionPreference = 'Stop'

$safeSave = Get-Content 'Pdf4QtLibGui/pdfsafesaveservice.h' -Raw
$controller = Get-Content 'Pdf4QtLibGui/pdfprogramcontroller.cpp' -Raw

if ($safeSave -notmatch '(?s)#else\s+.*?std::rename\(nativeCandidate\.constData\(\), nativeSource\.constData\(\)\)') {
    throw 'PDFSafeSaveService POSIX commit path must use std::rename for atomic replacement.'
}
if ($safeSave -match '(?s)#else\s+.*?QFile::remove\(sourcePath\).*?QFile::rename\(candidatePath, sourcePath\)') {
    throw 'PDFSafeSaveService still deletes the source before renaming the candidate.'
}
if ($controller -notmatch '(?s)#else\s+.*?std::rename\(nativeTemporary\.constData\(\), nativeFinal\.constData\(\)\)') {
    throw 'Recovery snapshot POSIX commit path must use std::rename for atomic replacement.'
}
if ($controller -match '(?s)#else\s+.*?QFile::remove\(finalSnapshot\).*?QFile::rename\(temporarySnapshot, finalSnapshot\)') {
    throw 'Recovery snapshot still deletes the final snapshot before renaming the temporary file.'
}

Write-Output 'POSIX atomic replacement contracts passed.'
