[CmdletBinding()]
param(
    [string]$DataDirectory = ''
)

$ErrorActionPreference = 'Stop'
if ([string]::IsNullOrWhiteSpace($DataDirectory)) {
    $DataDirectory = Join-Path $PSScriptRoot 'tessdata'
}
New-Item -ItemType Directory -Path $DataDirectory -Force | Out-Null

$baseUrl = 'https://github.com/tesseract-ocr/tessdata_fast/raw/main'
$languages = @('eng', 'chi_tra', 'chi_sim')
foreach ($language in $languages) {
    $target = Join-Path $DataDirectory "$language.traineddata"
    if (-not (Test-Path -LiteralPath $target)) {
        Invoke-WebRequest -Uri "$baseUrl/$language.traineddata" -OutFile $target
    }
}

Get-ChildItem -LiteralPath $DataDirectory -Filter '*.traineddata' |
    Select-Object Name,Length,LastWriteTime | Format-Table -AutoSize
