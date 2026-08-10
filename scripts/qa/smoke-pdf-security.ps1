[CmdletBinding()]
param(
    [string]$PackageDirectory = '',
    [string]$OutputDirectory = ''
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$repositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
if ([string]::IsNullOrWhiteSpace($PackageDirectory)) {
    $PackageDirectory = Join-Path $repositoryRoot 'dist\FamilyPDF-windows-x64'
}
if ([string]::IsNullOrWhiteSpace($OutputDirectory)) {
    $OutputDirectory = Join-Path $repositoryRoot 'build\pdf-security-smoke'
}
$PackageDirectory = [IO.Path]::GetFullPath($PackageDirectory)
$OutputDirectory = [IO.Path]::GetFullPath($OutputDirectory)

$pdfTool = Join-Path $PackageDirectory 'PdfTool.exe'
$editor = Join-Path $PackageDirectory 'Pdf4QtEditor.exe'
foreach ($requiredFile in @($pdfTool, $editor)) {
    if (-not (Test-Path -LiteralPath $requiredFile -PathType Leaf)) {
        throw "Required PDF security executable was not found: $requiredFile"
    }
}

$venvPython = Join-Path (
    Split-Path $repositoryRoot -Parent
) 'FamilyPDF-tools\office-export-venv\Scripts\python.exe'
& (Join-Path $repositoryRoot 'scripts\office\install-office-export-toolchain.ps1')
if (-not (Test-Path -LiteralPath $venvPython -PathType Leaf)) {
    throw "Office verification Python was not found: $venvPython"
}

New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null
$plainPdf = Join-Path $OutputDirectory 'plain-source.pdf'
$encryptedPdf = Join-Path $OutputDirectory 'aes256-encrypted.pdf'
$decryptedPdf = Join-Path $OutputDirectory 'owner-decrypted.pdf'
$summaryPath = Join-Path $OutputDirectory 'summary.json'
$userPassword = 'FamilyPDF-Test-User-2026!'
$ownerPassword = 'FamilyPDF-Test-Owner-2026!'

Push-Location (Join-Path $repositoryRoot 'office-export')
try {
    & $venvPython -c @"
from pathlib import Path
from tests.test_cli import _write_two_page_pdf
_write_two_page_pdf(Path(r'$plainPdf'))
"@
}
finally {
    Pop-Location
}
if ($LASTEXITCODE -ne 0) {
    throw 'Could not create the PDF security fixture.'
}

$sourceHashBefore = (Get-FileHash -LiteralPath $plainPdf -Algorithm SHA256).Hash
Copy-Item -LiteralPath $plainPdf -Destination $encryptedPdf -Force
& $pdfTool encrypt --enc-algorithm aes-256 --enc-contents all `
    --enc-user-password $userPassword `
    --enc-owner-password $ownerPassword `
    --enc-permissions 0 $encryptedPdf
if ($LASTEXITCODE -ne 0) {
    throw 'AES-256 PDF encryption failed.'
}
$sourceHashAfter = (Get-FileHash -LiteralPath $plainPdf -Algorithm SHA256).Hash
$encryptedHash = (Get-FileHash -LiteralPath $encryptedPdf -Algorithm SHA256).Hash
if ($sourceHashBefore -ne $sourceHashAfter) {
    throw 'PDF encryption modified the source fixture.'
}
if ($sourceHashBefore -eq $encryptedHash) {
    throw 'Encrypted PDF did not differ from the plain source.'
}

$decryptionSucceeded = $false
for ($attempt = 1; $attempt -le 3; $attempt++) {
    Copy-Item -LiteralPath $encryptedPdf -Destination $decryptedPdf -Force
    & $pdfTool decrypt --pswd $ownerPassword $decryptedPdf
    if ($LASTEXITCODE -eq 0) {
        $decryptionSucceeded = $true
        break
    }
    if ($attempt -lt 3) {
        Write-Warning "PDF decryption write failed on attempt $attempt; retrying."
        Start-Sleep -Seconds ([Math]::Pow(2, $attempt))
    }
}
if (-not $decryptionSucceeded) {
    throw 'Owner-authorized PDF decryption failed.'
}

& $venvPython -c @"
import hashlib
import json
from pathlib import Path

import pypdfium2 as pdfium
from pypdf import PasswordType, PdfReader

plain_path = Path(r'$plainPdf')
encrypted_path = Path(r'$encryptedPdf')
decrypted_path = Path(r'$decryptedPdf')

wrong_reader = PdfReader(encrypted_path)
assert wrong_reader.is_encrypted
wrong_result = wrong_reader.decrypt('FamilyPDF-Definitely-Wrong')
assert wrong_result == PasswordType.NOT_DECRYPTED

user_reader = PdfReader(encrypted_path)
user_result = user_reader.decrypt(r'$userPassword')
assert user_result == PasswordType.USER_PASSWORD
assert len(user_reader.pages) == 2
user_text = '\n'.join(page.extract_text() or '' for page in user_reader.pages)
assert 'First page' in user_text and 'Second page' in user_text
encrypt_dictionary = user_reader.trailer['/Encrypt']
assert int(encrypt_dictionary['/V']) == 5
assert int(encrypt_dictionary['/R']) == 6
permission_value = int(encrypt_dictionary['/P'])
restricted_mask = 4 | 8 | 16 | 32 | 256 | 1024 | 2048
assert permission_value & restricted_mask == 0

owner_reader = PdfReader(encrypted_path)
owner_result = owner_reader.decrypt(r'$ownerPassword')
assert owner_result == PasswordType.OWNER_PASSWORD
assert len(owner_reader.pages) == 2

plain_reader = PdfReader(plain_path)
decrypted_reader = PdfReader(decrypted_path)
assert not decrypted_reader.is_encrypted
assert len(decrypted_reader.pages) == len(plain_reader.pages) == 2
plain_text = '\n'.join(page.extract_text() or '' for page in plain_reader.pages)
decrypted_text = '\n'.join(
    page.extract_text() or '' for page in decrypted_reader.pages
)
assert decrypted_text == plain_text

def render_hashes(path: Path) -> list[str]:
    document = pdfium.PdfDocument(str(path))
    hashes = []
    for page_index in range(len(document)):
        bitmap = document[page_index].render(scale=2.0)
        image = bitmap.to_pil().convert('RGB')
        hashes.append(hashlib.sha256(image.tobytes()).hexdigest())
    return hashes

plain_render_hashes = render_hashes(plain_path)
decrypted_render_hashes = render_hashes(decrypted_path)
assert decrypted_render_hashes == plain_render_hashes

summary = {
    'algorithm': 'AES-256',
    'revision': 6,
    'pages': 2,
    'wrong_password_rejected': True,
    'user_password_accepted': True,
    'owner_password_accepted': True,
    'permissions_restricted': True,
    'source_hash_preserved': True,
    'decrypted_text_preserved': True,
    'decrypted_render_preserved': True,
    'plain_sha256': r'$sourceHashBefore',
    'encrypted_sha256': r'$encryptedHash',
}
Path(r'$summaryPath').write_text(
    json.dumps(summary, ensure_ascii=False, indent=2),
    encoding='utf-8-sig',
)
"@
if ($LASTEXITCODE -ne 0) {
    throw 'Independent PDF security verification failed.'
}

$editorProcess = $null
$guiResponding = $false
try {
    $editorProcess = Start-Process -FilePath $editor `
        -ArgumentList @($encryptedPdf) -PassThru -WindowStyle Hidden
    $deadline = [DateTime]::UtcNow.AddSeconds(15)
    do {
        Start-Sleep -Milliseconds 250
        $editorProcess.Refresh()
        if ($editorProcess.HasExited) {
            break
        }
        if ($editorProcess.Responding) {
            $guiResponding = $true
            break
        }
    } while ([DateTime]::UtcNow -lt $deadline)
}
finally {
    if ($editorProcess -and -not $editorProcess.HasExited) {
        Stop-Process -Id $editorProcess.Id -Force -ErrorAction SilentlyContinue
        $editorProcess.WaitForExit(5000) | Out-Null
    }
}
if (-not $guiResponding) {
    throw 'Editor did not remain responsive while opening an encrypted PDF.'
}

$summary = Get-Content -LiteralPath $summaryPath -Raw | ConvertFrom-Json
$summary | Add-Member -NotePropertyName gui_responding `
    -NotePropertyValue $guiResponding
$summary | Add-Member -NotePropertyName editor_executable `
    -NotePropertyValue $editor
$summary | ConvertTo-Json -Depth 6 |
    Set-Content -LiteralPath $summaryPath -Encoding UTF8

Write-Host "PDF password security smoke passed: $summaryPath"
