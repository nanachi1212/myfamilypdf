[CmdletBinding()]
param(
    [string]$ManifestPath = ''
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$repositoryRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
if ([string]::IsNullOrWhiteSpace($ManifestPath)) {
    $ManifestPath = Join-Path $repositoryRoot 'ocr-spike\tessdata-manifest.json'
}
$manifest = Get-Content -LiteralPath $ManifestPath -Raw -Encoding UTF8 |
    ConvertFrom-Json
if ($manifest.schemaVersion -ne 1 -or
    [string]$manifest.commit -notmatch '^[0-9a-f]{40}$') {
    throw 'OCR language manifest must pin a full Git commit.'
}
if ([string]$manifest.baseUrl -notmatch [Regex]::Escape([string]$manifest.commit) -or
    [string]$manifest.baseUrl -match '/main(?:/|$)') {
    throw 'OCR language manifest URL is not pinned to its commit.'
}

$requiredLanguages = @('eng', 'chi_tra', 'chi_sim', 'chi_tra_vert', 'chi_sim_vert')
foreach ($language in $requiredLanguages) {
    $property = $manifest.languages.PSObject.Properties[$language]
    if ($null -eq $property) {
        throw "OCR language manifest is missing: $language"
    }
    $entry = $property.Value
    if ([long]$entry.bytes -le 1MB -or
        [string]$entry.sha256 -notmatch '^[0-9A-Fa-f]{64}$') {
        throw "OCR language manifest has invalid metadata for: $language"
    }
}

Write-Host "OCR language manifest is pinned: $($manifest.commit)"
