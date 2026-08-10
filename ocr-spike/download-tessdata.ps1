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
    [string]$ManifestPath = '',
    [ValidateRange(1, 5)]
    [int]$MaxAttempts = 3
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

if ([string]::IsNullOrWhiteSpace($DataDirectory)) {
    $DataDirectory = Join-Path $PSScriptRoot 'tessdata'
}

if ([string]::IsNullOrWhiteSpace($ManifestPath)) {
    $ManifestPath = Join-Path $PSScriptRoot 'tessdata-manifest.json'
}
$ManifestPath = [IO.Path]::GetFullPath($ManifestPath)
if (-not (Test-Path -LiteralPath $ManifestPath -PathType Leaf)) {
    throw "OCR language manifest was not found: $ManifestPath"
}
$languageManifest = Get-Content -LiteralPath $ManifestPath -Raw -Encoding UTF8 |
    ConvertFrom-Json
if ($languageManifest.schemaVersion -ne 1 -or
    [string]::IsNullOrWhiteSpace([string]$languageManifest.commit) -or
    [string]::IsNullOrWhiteSpace([string]$languageManifest.baseUrl)) {
    throw "OCR language manifest is invalid: $ManifestPath"
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
    if ($null -eq $languageManifest.languages.PSObject.Properties[$language]) {
        throw "OCR language manifest has no entry for: $language"
    }
}

function Test-LanguageFile {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path,
        [Parameter(Mandatory = $true)]
        [string]$Language,
        [Parameter(Mandatory = $true)]
        [object]$Expected
    )

    if (-not (Test-Path -LiteralPath $Path -PathType Leaf) -or
        (Get-Item -LiteralPath $Path).Length -ne [long]$Expected.bytes) {
        return $false
    }
    $actualHash = (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash
    if ($actualHash -ne [string]$Expected.sha256) {
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

$baseUrl = ([string]$languageManifest.baseUrl).TrimEnd('/', '\')
$failures = [Collections.Generic.List[string]]::new()

foreach ($language in $requestedLanguages) {
    $expected = $languageManifest.languages.PSObject.Properties[$language].Value
    $target = Join-Path $DataDirectory "$language.traineddata"
    if (Test-LanguageFile -Path $target -Language $language -Expected $expected) {
        continue
    }

    $downloaded = $false
    for ($attempt = 1; $attempt -le $MaxAttempts; $attempt++) {
        $temporaryTarget = "$target.download.$([Guid]::NewGuid().ToString('N'))"
        try {
            Write-Host "Downloading official Tesseract language '$language' (attempt $attempt/$MaxAttempts)..."
            if (Test-Path -LiteralPath $baseUrl -PathType Container) {
                Copy-Item -LiteralPath (Join-Path $baseUrl "$language.traineddata") `
                    -Destination $temporaryTarget
            }
            else {
                Invoke-WebRequest `
                    -Uri "$baseUrl/$language.traineddata" `
                    -OutFile $temporaryTarget `
                    -UseBasicParsing `
                    -TimeoutSec 120
            }

            $downloadedBytes = (Get-Item -LiteralPath $temporaryTarget).Length
            if ($downloadedBytes -ne [long]$expected.bytes) {
                throw "Downloaded OCR language size mismatch for $language`: expected $($expected.bytes), got $downloadedBytes."
            }
            $downloadedHash = (
                Get-FileHash -LiteralPath $temporaryTarget -Algorithm SHA256
            ).Hash
            if ($downloadedHash -ne [string]$expected.sha256) {
                throw "Downloaded OCR language SHA-256 mismatch for $language`: expected $($expected.sha256), got $downloadedHash."
            }

            Move-Item -LiteralPath $temporaryTarget -Destination $target -Force
            if (-not (Test-LanguageFile `
                    -Path $target `
                    -Language $language `
                    -Expected $expected)) {
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
        -not (Test-LanguageFile -Path $target -Language $language -Expected $expected)) {
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
