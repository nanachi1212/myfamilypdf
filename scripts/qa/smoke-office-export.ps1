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
$unequalColumnPdf = Join-Path $smokeRoot 'unequal-column-input.pdf'
$unequalColumnDocx = Join-Path $smokeRoot 'unequal-column-output.docx'
$threeColumnPdf = Join-Path $smokeRoot 'three-column-input.pdf'
$threeColumnDocx = Join-Path $smokeRoot 'three-column-output.docx'
$tablePdf = Join-Path $smokeRoot 'table-input.pdf'
$tableDocx = Join-Path $smokeRoot 'table-output.docx'

Push-Location (Join-Path $repositoryRoot 'office-export')
try {
    & $venvPython -c @"
from pathlib import Path
from tests.test_cli import _write_two_page_pdf
from tests.test_extract import _write_text_and_table_pdf
from tests.test_multicolumn_export import (
    _write_three_column_pdf,
    _write_two_column_pdf,
    _write_unequal_two_column_pdf,
)
_write_two_page_pdf(Path(r'$inputPdf'))
_write_two_column_pdf(Path(r'$multiColumnPdf'))
_write_unequal_two_column_pdf(Path(r'$unequalColumnPdf'))
_write_three_column_pdf(Path(r'$threeColumnPdf'))
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
    $multiColumnReportJson = (& $helper --input $multiColumnPdf `
            --output $multiColumnDocx --format docx | Out-String).Trim()
    if ($LASTEXITCODE -ne 0) {
        throw 'Portable two-column DOCX export smoke test failed.'
    }
    $multiColumnReport = $multiColumnReportJson | ConvertFrom-Json
    if (-not ($multiColumnReport.PSObject.Properties.Name -contains `
            'images_exported') -or $multiColumnReport.images_exported -ne 2) {
        throw 'Portable DOCX raster image export smoke test failed.'
    }
    Write-Host $multiColumnReportJson
    $unequalColumnReportJson = (& $helper --input $unequalColumnPdf `
            --output $unequalColumnDocx --format docx | Out-String).Trim()
    if ($LASTEXITCODE -ne 0) {
        throw 'Portable unequal-column DOCX export smoke test failed.'
    }
    $unequalColumnReport = $unequalColumnReportJson | ConvertFrom-Json
    if ($unequalColumnReport.images_exported -ne 2) {
        throw 'Portable unequal-column image export smoke test failed.'
    }
    Write-Host $unequalColumnReportJson
    $threeColumnReportJson = (& $helper --input $threeColumnPdf `
            --output $threeColumnDocx --format docx | Out-String).Trim()
    if ($LASTEXITCODE -ne 0) {
        throw 'Portable three-column DOCX export smoke test failed.'
    }
    $threeColumnReport = $threeColumnReportJson | ConvertFrom-Json
    if ($threeColumnReport.images_exported -ne 3) {
        throw 'Portable three-column image export smoke test failed.'
    }
    Write-Host $threeColumnReportJson
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
    'Full width heading',
    'Middle heading',
]
body_order = [
    child.tag.rsplit('}', 1)[-1]
    for child in multi_column_page.element.body.iterchildren()
    if child.tag.rsplit('}', 1)[-1] in {'p', 'tbl'}
]
assert body_order == ['p', 'tbl', 'p', 'tbl', 'p']
assert len(multi_column_page.inline_shapes) == 2
assert len(multi_column_page.tables) == 2
first_columns = multi_column_page.tables[0].rows[0].cells
assert first_columns[0].text.splitlines() == ['Left top', '', 'Left bottom']
assert [p.text for p in first_columns[0].paragraphs] == [
    'Left top', '', 'Left bottom'
]
assert first_columns[1].text.splitlines() == ['Right top', 'Right bottom']
second_columns = multi_column_page.tables[1].rows[0].cells
assert second_columns[0].text.splitlines() == [
    'Left second top', 'Left second bottom'
]
assert second_columns[1].text.splitlines() == [
    'Right second top', 'Right second bottom'
]
unequal_column_page = Document(r'$unequalColumnDocx')
assert [p.text for p in unequal_column_page.paragraphs if p.text] == [
    'Unequal columns heading across page'
]
assert len(unequal_column_page.inline_shapes) == 2
assert len(unequal_column_page.tables) == 1
unequal_cells = unequal_column_page.tables[0].rows[0].cells
assert int(unequal_cells[0].width) > int(unequal_cells[1].width) * 2
assert unequal_cells[0].text.splitlines() == [
    'Wide left top', 'Wide left bottom with extended editable text'
]
assert unequal_cells[1].text.splitlines() == [
    'Narrow right top', 'Narrow right bottom'
]
three_column_page = Document(r'$threeColumnDocx')
assert [p.text for p in three_column_page.paragraphs if p.text] == [
    'Three column heading across the full page width'
]
assert len(three_column_page.inline_shapes) == 3
assert len(three_column_page.tables) == 1
three_cells = three_column_page.tables[0].rows[0].cells
assert len(three_cells) == 3
assert three_cells[0].text.splitlines() == [
    'Column one top', 'Column one bottom'
]
assert three_cells[1].text.splitlines() == [
    'Column two top', 'Column two bottom'
]
assert three_cells[2].text.splitlines() == [
    'Column three top', 'Column three bottom'
]
table_page = Document(r'$tableDocx')
assert len(table_page.tables) == 0
assert len([p for p in table_page.paragraphs if p.text]) == 3
"@
if ($LASTEXITCODE -ne 0) {
    throw 'Office output read-back verification failed.'
}

Write-Host "Office export smoke passed: $smokeRoot"
