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
$languages = @('eng', 'chi_tra', 'chi_sim', 'chi_tra_vert', 'chi_sim_vert')
foreach ($language in $languages) {
    $target = Join-Path $DataDirectory "$language.traineddata"
    $isValidExistingFile = (Test-Path -LiteralPath $target -PathType Leaf) -and
        (Get-Item -LiteralPath $target).Length -gt 1MB
    if (-not $isValidExistingFile) {
        $temporaryTarget = "$target.download"
        try {
            if (Test-Path -LiteralPath $temporaryTarget -PathType Leaf) {
                Remove-Item -LiteralPath $temporaryTarget -Force
            }
            Invoke-WebRequest -Uri "$baseUrl/$language.traineddata" -OutFile $temporaryTarget
            if ((Get-Item -LiteralPath $temporaryTarget).Length -le 1MB) {
                throw "Downloaded OCR language data is unexpectedly small: $language"
            }
            Move-Item -LiteralPath $temporaryTarget -Destination $target -Force
        }
        finally {
            if (Test-Path -LiteralPath $temporaryTarget -PathType Leaf) {
                Remove-Item -LiteralPath $temporaryTarget -Force
            }
        }
    }
}

Get-ChildItem -LiteralPath $DataDirectory -Filter '*.traineddata' |
    Select-Object Name,Length,LastWriteTime | Format-Table -AutoSize
