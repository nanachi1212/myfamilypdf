[CmdletBinding()]
param(
    [string]$PackageDirectory = '',
    [switch]$SkipBuild
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$repositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
if ([string]::IsNullOrWhiteSpace($PackageDirectory)) {
    $PackageDirectory = Join-Path $repositoryRoot 'dist\FamilyPDF-windows-x64'
}
$PackageDirectory = [IO.Path]::GetFullPath($PackageDirectory)

if (-not $SkipBuild) {
    & (Join-Path $repositoryRoot 'scripts\office\build-office-export-helper.ps1')
}

$helper = Join-Path $PackageDirectory 'office-export\FamilyPDFOfficeExport.exe'
if (-not (Test-Path -LiteralPath $helper -PathType Leaf)) {
    throw "Packaged Office export helper was not found: $helper"
}

$venvPython = Join-Path (
    Split-Path $repositoryRoot -Parent
) 'FamilyPDF-tools\office-export-venv\Scripts\python.exe'
& (Join-Path $repositoryRoot 'scripts\office\install-office-export-toolchain.ps1')

$smokeRoot = Join-Path $repositoryRoot 'build\office-export-smoke'
New-Item -ItemType Directory -Path $smokeRoot -Force | Out-Null
$inputPdf = Join-Path $smokeRoot 'office-input.pdf'
$outputDocx = Join-Path $smokeRoot 'office-output.docx'
$outputXlsx = Join-Path $smokeRoot 'office-output.xlsx'
$multiColumnPdf = Join-Path $smokeRoot 'two-column-input.pdf'
$multiColumnDocx = Join-Path $smokeRoot 'two-column-output.docx'
$tablePdf = Join-Path $smokeRoot 'table-input.pdf'
$tableDocx = Join-Path $smokeRoot 'table-output.docx'

Push-Location (Join-Path $repositoryRoot 'office-export')
try {
    & $venvPython -c @"
from pathlib import Path
from tests.test_cli import _write_two_page_pdf
from tests.test_extract import _write_text_and_table_pdf
from tests.test_multicolumn_export import _write_two_column_pdf
_write_two_page_pdf(Path(r'$inputPdf'))
_write_two_column_pdf(Path(r'$multiColumnPdf'))
_write_text_and_table_pdf(Path(r'$tablePdf'))
"@
}
finally {
    Pop-Location
}
if ($LASTEXITCODE -ne 0) {
    throw 'Could not create the Office export smoke fixture.'
}

$savedPath = $env:PATH
$env:PATH = "$env:WINDIR\System32;$env:WINDIR"
try {
    & $helper --input $inputPdf --output $outputDocx --format docx --pages 1-2
    if ($LASTEXITCODE -ne 0) {
        throw 'Portable DOCX export smoke test failed.'
    }
    & $helper --input $inputPdf --output $outputXlsx --format xlsx --pages 2
    if ($LASTEXITCODE -ne 0) {
        throw 'Portable XLSX export smoke test failed.'
    }
    & $helper --input $multiColumnPdf --output $multiColumnDocx --format docx
    if ($LASTEXITCODE -ne 0) {
        throw 'Portable two-column DOCX export smoke test failed.'
    }
    & $helper --input $tablePdf --output $tableDocx --format docx
    if ($LASTEXITCODE -ne 0) {
        throw 'Portable single-column table-page DOCX export smoke test failed.'
    }
}
finally {
    $env:PATH = $savedPath
}

& $venvPython -c @"
from docx import Document
from openpyxl import load_workbook
assert 'First page' in '\n'.join(p.text for p in Document(r'$outputDocx').paragraphs)
workbook = load_workbook(r'$outputXlsx', read_only=True)
assert workbook.sheetnames == ['Page 2']
assert workbook['Page 2']['A1'].value == 'Second page'
multi_column_page = Document(r'$multiColumnDocx')
assert [p.text for p in multi_column_page.paragraphs if p.text] == [
    'Full width heading'
]
columns = multi_column_page.tables[0].rows[0].cells
assert columns[0].text.splitlines() == ['Left top', 'Left bottom']
assert columns[1].text.splitlines() == ['Right top', 'Right bottom']
table_page = Document(r'$tableDocx')
assert len(table_page.tables) == 0
assert len([p for p in table_page.paragraphs if p.text]) == 3
"@
if ($LASTEXITCODE -ne 0) {
    throw 'Office output read-back verification failed.'
}

Write-Host "Office export smoke passed: $smokeRoot"
