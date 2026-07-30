[CmdletBinding()]
param(
    [string]$InputPdf = '',
    [string]$OutputDirectory = ''
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$repositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$buildRoot = [IO.Path]::GetFullPath(
    (Join-Path $repositoryRoot 'build')
).TrimEnd('\') + '\'

if ([string]::IsNullOrWhiteSpace($InputPdf)) {
    $InputPdf = Join-Path $repositoryRoot 'dist\qa\form-interop.pdf'
}
$InputPdf = [IO.Path]::GetFullPath($InputPdf)
if (-not (Test-Path -LiteralPath $InputPdf -PathType Leaf)) {
    throw "AcroForm fixture was not found: $InputPdf"
}

if ([string]::IsNullOrWhiteSpace($OutputDirectory)) {
    $OutputDirectory = Join-Path $repositoryRoot 'build\acrobat-form-interop'
}
$OutputDirectory = [IO.Path]::GetFullPath($OutputDirectory)
if (-not $OutputDirectory.StartsWith(
        $buildRoot,
        [StringComparison]::OrdinalIgnoreCase
    )) {
    throw "Acrobat smoke output must remain under the repository build directory: $OutputDirectory"
}
New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null

$acrobat = 'C:\Program Files\Adobe\Acrobat DC\Acrobat\Acrobat.exe'
if (-not (Test-Path -LiteralPath $acrobat -PathType Leaf)) {
    throw 'Adobe Acrobat was not found; this optional cross-product smoke test requires Acrobat.'
}
if (Get-Process -Name Acrobat -ErrorAction SilentlyContinue) {
    throw 'Adobe Acrobat is already running. Close it before this isolated smoke test.'
}

function Stop-IsolatedAcrobat {
    $processes = @(Get-Process -Name Acrobat -ErrorAction SilentlyContinue)
    if ($processes.Count -eq 0) {
        return
    }
    $processes | Wait-Process -Timeout 5 -ErrorAction SilentlyContinue
    Get-Process -Name Acrobat -ErrorAction SilentlyContinue |
        Stop-Process -Force
}

$venvPython = Join-Path (
    Split-Path $repositoryRoot -Parent
) 'FamilyPDF-tools\office-export-venv\Scripts\python.exe'
if (-not (Test-Path -LiteralPath $venvPython -PathType Leaf)) {
    & (Join-Path $repositoryRoot 'scripts\office\install-office-export-toolchain.ps1')
}
if (-not (Test-Path -LiteralPath $venvPython -PathType Leaf)) {
    throw "Office verification Python was not prepared: $venvPython"
}

$stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$outputPdf = Join-Path $OutputDirectory "form-acrobat-filled-$stamp.pdf"
$driver = Join-Path $PSScriptRoot 'acrobat-form-interop.vbs'
$driverSucceeded = $false
for ($attempt = 1; $attempt -le 3; $attempt++) {
    $savedErrorActionPreference = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try {
        $driverOutput = @(
            & "$env:SystemRoot\System32\cscript.exe" //nologo `
                $driver $InputPdf $outputPdf 2>&1
        )
        $driverExitCode = $LASTEXITCODE
    }
    finally {
        $ErrorActionPreference = $savedErrorActionPreference
    }
    $driverOutput | Write-Host
    if (
        $driverExitCode -eq 0 -and
        (Test-Path -LiteralPath $outputPdf -PathType Leaf)
    ) {
        $driverSucceeded = $true
        Stop-IsolatedAcrobat
        break
    }

    # This script refuses to start when Acrobat was already running, so any
    # Acrobat process here belongs to the failed isolated attempt.
    Stop-IsolatedAcrobat
    if ($attempt -lt 3) {
        Write-Warning (
            "Acrobat COM attempt $attempt failed with exit code " +
            "$driverExitCode; retrying."
        )
        Start-Sleep -Seconds 3
    }
}
if (-not $driverSucceeded) {
    throw 'Adobe Acrobat form write failed after three attempts.'
}

& $venvPython -c @"
from pathlib import Path
from pypdf import PdfReader

path = Path(r'$outputPdf')
reader = PdfReader(str(path))
fields = reader.get_fields() or {}
text_name = chr(0x59D3) + chr(0x540D)
check_name = chr(0x540C) + chr(0x610F)
default_text = ''.join(chr(value) for value in (0x7E41, 0x9AD4, 0x6E2C, 0x8A66))

assert len(reader.pages) == 1, len(reader.pages)
assert set(fields) == {text_name, check_name}, [name.encode('unicode_escape').decode() for name in fields]
assert str(fields[text_name].get('/FT')) == '/Tx', fields[text_name]
assert str(fields[text_name].get('/V')) == 'AdobeInterop2026', fields[text_name]
assert str(fields[text_name].get('/DV')) == default_text, fields[text_name]
assert str(fields[check_name].get('/FT')) == '/Btn', fields[check_name]
assert str(fields[check_name].get('/V')) in {'None', '/Off'}, fields[check_name]
assert str(fields[check_name].get('/DV')) == '/Yes', fields[check_name]
"@
if ($LASTEXITCODE -ne 0) {
    throw 'Independent pypdf verification of the Acrobat output failed.'
}

$inputHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $InputPdf).Hash
$outputHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $outputPdf).Hash
if ($inputHash -eq $outputHash) {
    throw 'Acrobat output is byte-identical to the input; the form was not saved.'
}

$summary = [ordered]@{
    recorded_at = [DateTimeOffset]::Now.ToString('o')
    acrobat = $acrobat
    input = $InputPdf
    input_sha256 = $inputHash
    output = $outputPdf
    output_sha256 = $outputHash
    text_field_name_preserved = $true
    text_value = 'AdobeInterop2026'
    checkbox_field_name_preserved = $true
    checkbox_value = 'Off'
    independent_parser = 'pypdf'
}
$summaryPath = Join-Path $OutputDirectory 'summary.json'
$summary | ConvertTo-Json -Depth 4 |
    Set-Content -LiteralPath $summaryPath -Encoding UTF8

Write-Host "Adobe Acrobat AcroForm round-trip passed: $summaryPath"
