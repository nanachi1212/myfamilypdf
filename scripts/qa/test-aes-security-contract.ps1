[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$repositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$sourcePath = Join-Path $repositoryRoot 'Pdf4QtLibCore\sources\pdfsecurityhandler.cpp'
$content = Get-Content -LiteralPath $sourcePath -Raw -Encoding UTF8

$requiredPatterns = @(
    'data\.size\(\) < AES_BLOCK_SIZE \* 2',
    'data\.size\(\) % AES_BLOCK_SIZE != 0',
    'padding < 1 \|\| padding > AES_BLOCK_SIZE',
    'data\.mid\(AES_BLOCK_SIZE\)'
)

foreach ($pattern in $requiredPatterns) {
    if ($content -notmatch $pattern) {
        throw "AES security contract is missing: $pattern"
    }
}

if ($content -match 'qBound\(1,\s*padding,\s*AES_BLOCK_SIZE\)') {
    throw 'AES padding validation must reject invalid padding, not clamp it.'
}

Write-Output 'AES malformed-input and padding validation contract passed.'
