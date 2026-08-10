[CmdletBinding()]
param(
    [string]$DownloaderPath = ''
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$repositoryRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
if ([string]::IsNullOrWhiteSpace($DownloaderPath)) {
    $DownloaderPath = Join-Path $repositoryRoot 'ocr-spike\download-tessdata.ps1'
}
if (-not (Test-Path -LiteralPath $DownloaderPath -PathType Leaf)) {
    throw "OCR language downloader was not found: $DownloaderPath"
}

$testRoot = Join-Path ([IO.Path]::GetTempPath()) (
    'FamilyPDF-OCR-Language-Download-Test-' + [Guid]::NewGuid().ToString('N')
)
New-Item -ItemType Directory -Path $testRoot | Out-Null

try {
    $commit = 'test-fixture-commit'
    $sourceRoot = Join-Path $testRoot 'source'
    New-Item -ItemType Directory -Path $sourceRoot | Out-Null
    $pinnedSource = Join-Path $sourceRoot 'eng.traineddata'
    $fixtureBytes = [byte[]]::new(2MB)
    [IO.File]::WriteAllBytes($pinnedSource, $fixtureBytes)
    $expectedHash = (
        Get-FileHash -LiteralPath $pinnedSource -Algorithm SHA256
    ).Hash
    $manifest = [ordered]@{
        schemaVersion = 1
        repository = 'https://github.com/tesseract-ocr/tessdata_fast'
        commit = $commit
        baseUrl = $sourceRoot
        languages = [ordered]@{
            eng = [ordered]@{
                bytes = $fixtureBytes.Length
                sha256 = $expectedHash
            }
        }
    }
    $validManifest = Join-Path $testRoot 'valid-manifest.json'
    $manifest | ConvertTo-Json -Depth 5 |
        Set-Content -LiteralPath $validManifest -Encoding UTF8

    $validData = Join-Path $testRoot 'valid'
    & $DownloaderPath `
        -DataDirectory $validData `
        -Languages eng `
        -ManifestPath $validManifest `
        -MaxAttempts 1
    $downloaded = Join-Path $validData 'eng.traineddata'
    if (-not (Test-Path -LiteralPath $downloaded -PathType Leaf) -or
        (Get-FileHash -LiteralPath $downloaded -Algorithm SHA256).Hash -ne
            $expectedHash) {
        throw 'Pinned OCR language download did not install the verified model.'
    }

    $manifest.languages.eng.sha256 = ('0' * 64)
    $invalidManifest = Join-Path $testRoot 'invalid-manifest.json'
    $manifest | ConvertTo-Json -Depth 5 |
        Set-Content -LiteralPath $invalidManifest -Encoding UTF8
    $invalidData = Join-Path $testRoot 'invalid'
    $rejected = $false
    try {
        & $DownloaderPath `
            -DataDirectory $invalidData `
            -Languages eng `
            -ManifestPath $invalidManifest `
            -MaxAttempts 1
    }
    catch {
        $rejected = $_.Exception.Message -match 'SHA-256'
    }
    if (-not $rejected) {
        throw 'OCR language downloader accepted a model with the wrong SHA-256.'
    }
    if (Test-Path -LiteralPath (Join-Path $invalidData 'eng.traineddata')) {
        throw 'Rejected OCR language download left an installed model behind.'
    }
    if (@(Get-ChildItem -LiteralPath $invalidData -Filter '*.download.*' -File).Count -ne 0) {
        throw 'Rejected OCR language download left a partial file behind.'
    }

    Write-Host 'Pinned OCR language download and tamper rejection test passed.'
}
finally {
    if (Test-Path -LiteralPath $testRoot -PathType Container) {
        $resolvedTestRoot = [IO.Path]::GetFullPath($testRoot)
        $systemTemp = [IO.Path]::GetFullPath([IO.Path]::GetTempPath())
        if ($resolvedTestRoot.StartsWith(
                $systemTemp,
                [StringComparison]::OrdinalIgnoreCase
            ) -and
            [IO.Path]::GetFileName($resolvedTestRoot).StartsWith(
                'FamilyPDF-OCR-Language-Download-Test-',
                [StringComparison]::Ordinal
            )) {
            Remove-Item -LiteralPath $resolvedTestRoot -Recurse -Force
        }
    }
}
