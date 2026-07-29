[CmdletBinding()]
param(
    [string]$DataDirectory = '',
    [string[]]$Languages = @(
        'eng',
        'chi_tra',
        'chi_sim',
        'chi_tra_vert',
        'chi_sim_vert'
    ),
    [string]$TesseractPath = '',
    [ValidateRange(1, 5)]
    [int]$MaxAttempts = 3
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

if ([string]::IsNullOrWhiteSpace($DataDirectory)) {
    $DataDirectory = Join-Path $PSScriptRoot 'tessdata'
}
$DataDirectory = [IO.Path]::GetFullPath($DataDirectory)
New-Item -ItemType Directory -Path $DataDirectory -Force | Out-Null

if ([string]::IsNullOrWhiteSpace($TesseractPath)) {
    $packagedTesseract = Join-Path ([IO.Path]::GetDirectoryName($DataDirectory)) 'tesseract.exe'
    if (Test-Path -LiteralPath $packagedTesseract -PathType Leaf) {
        $TesseractPath = $packagedTesseract
    }
}

$allowedLanguages = @('eng', 'chi_tra', 'chi_sim', 'chi_tra_vert', 'chi_sim_vert')
$requestedLanguages = @(
    $Languages |
        ForEach-Object { $_.Split(',', [StringSplitOptions]::RemoveEmptyEntries) } |
        ForEach-Object { $_.Trim() } |
        Where-Object { -not [string]::IsNullOrWhiteSpace($_) } |
        Select-Object -Unique
)
foreach ($language in $requestedLanguages) {
    if ($language -notin $allowedLanguages) {
        throw "Unsupported FamilyPDF OCR language: $language"
    }
}

function Test-LanguageFile {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path,
        [Parameter(Mandatory = $true)]
        [string]$Language
    )

    if (-not (Test-Path -LiteralPath $Path -PathType Leaf) -or
        (Get-Item -LiteralPath $Path).Length -le 1MB) {
        return $false
    }

    if (-not [string]::IsNullOrWhiteSpace($TesseractPath) -and
        (Test-Path -LiteralPath $TesseractPath -PathType Leaf)) {
        $availableLanguages = (& $TesseractPath --tessdata-dir $DataDirectory --list-langs 2>&1) -join "`n"
        if ($LASTEXITCODE -ne 0 -or
            $availableLanguages -notmatch "(?m)^$([Regex]::Escape($Language))$") {
            return $false
        }
    }

    return $true
}

$baseUrl = 'https://raw.githubusercontent.com/tesseract-ocr/tessdata_fast/main'
$failures = [Collections.Generic.List[string]]::new()

foreach ($language in $requestedLanguages) {
    $target = Join-Path $DataDirectory "$language.traineddata"
    if (Test-LanguageFile -Path $target -Language $language) {
        continue
    }

    $downloaded = $false
    for ($attempt = 1; $attempt -le $MaxAttempts; $attempt++) {
        $temporaryTarget = "$target.download.$([Guid]::NewGuid().ToString('N'))"
        try {
            Write-Host "Downloading official Tesseract language '$language' (attempt $attempt/$MaxAttempts)..."
            Invoke-WebRequest `
                -Uri "$baseUrl/$language.traineddata" `
                -OutFile $temporaryTarget `
                -UseBasicParsing `
                -TimeoutSec 120

            if ((Get-Item -LiteralPath $temporaryTarget).Length -le 1MB) {
                throw "Downloaded OCR language data is unexpectedly small: $language"
            }

            Move-Item -LiteralPath $temporaryTarget -Destination $target -Force
            if (-not (Test-LanguageFile -Path $target -Language $language)) {
                throw "Tesseract could not load the downloaded language: $language"
            }

            $hash = (Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash
            Write-Host "Installed $language ($hash)"
            $downloaded = $true
            break
        }
        catch {
            if ($attempt -eq $MaxAttempts) {
                $failures.Add("$language`: $($_.Exception.Message)")
            }
            else {
                Start-Sleep -Seconds ([Math]::Pow(2, $attempt))
            }
        }
        finally {
            if (Test-Path -LiteralPath $temporaryTarget -PathType Leaf) {
                Remove-Item -LiteralPath $temporaryTarget -Force
            }
        }
    }

    if (-not $downloaded -and (Test-Path -LiteralPath $target -PathType Leaf) -and
        -not (Test-LanguageFile -Path $target -Language $language)) {
        Remove-Item -LiteralPath $target -Force
    }
}

if ($failures.Count -gt 0) {
    throw "OCR language installation failed:`n$($failures -join "`n")"
}

Get-ChildItem -LiteralPath $DataDirectory -Filter '*.traineddata' -File |
    Sort-Object Name |
    Select-Object Name,Length,LastWriteTime |
    Format-Table -AutoSize
