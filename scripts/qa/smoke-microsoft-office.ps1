[CmdletBinding()]
param(
    [string]$OutputDirectory = ''
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$repositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
if ([string]::IsNullOrWhiteSpace($OutputDirectory)) {
    $OutputDirectory = Join-Path $repositoryRoot 'build\microsoft-office-smoke'
}
$OutputDirectory = [IO.Path]::GetFullPath($OutputDirectory)
New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null

$venvPython = Join-Path (
    Split-Path $repositoryRoot -Parent
) 'FamilyPDF-tools\office-export-venv\Scripts\python.exe'
if (-not (Test-Path -LiteralPath $venvPython -PathType Leaf)) {
    & (Join-Path $repositoryRoot 'scripts\office\install-office-export-toolchain.ps1')
}
if (-not (Test-Path -LiteralPath $venvPython -PathType Leaf)) {
    throw "Office Export test Python was not found: $venvPython"
}

$officeExportRoot = Join-Path $repositoryRoot 'office-export'
$savedPythonPath = $env:PYTHONPATH
$env:PYTHONPATH = if ([string]::IsNullOrWhiteSpace($savedPythonPath)) {
    $officeExportRoot
}
else {
    "$officeExportRoot;$savedPythonPath"
}
Push-Location $officeExportRoot
try {
    & $venvPython 'tests\create_office_interop_fixtures.py' `
        --output-dir $OutputDirectory
    if ($LASTEXITCODE -ne 0) {
        throw 'Could not create Microsoft Office interoperability fixtures.'
    }
}
finally {
    Pop-Location
    $env:PYTHONPATH = $savedPythonPath
}

$docxPath = Join-Path $OutputDirectory 'office-interop.docx'
$xlsxPath = Join-Path $OutputDirectory 'office-interop.xlsx'
foreach ($path in @($docxPath, $xlsxPath)) {
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "Office interoperability fixture was not found: $path"
    }
}

$word = $null
$document = $null
$excel = $null
$workbook = $null
$firstSheet = $null
$secondSheet = $null
$summaryPath = Join-Path $OutputDirectory 'summary.json'
try {
    $word = New-Object -ComObject Word.Application
    $word.Visible = $false
    $word.DisplayAlerts = 0
    $document = $word.Documents.Open($docxPath, $false, $true, $false)
    $wordText = [string]$document.Content.Text
    foreach ($expected in @(
        '繁體中文',
        '简体中文',
        'FamilyPDF Office interoperability',
        '第二頁',
        '第二页'
    )) {
        if (-not $wordText.Contains($expected)) {
            throw "Microsoft Word did not read expected text: $expected"
        }
    }
    $wordPageCount = [int]$document.ComputeStatistics(2)
    if ($wordPageCount -lt 2) {
        throw "Microsoft Word reported $wordPageCount page instead of at least 2."
    }

    $excel = New-Object -ComObject Excel.Application
    $excel.Visible = $false
    $excel.DisplayAlerts = $false
    $workbook = $excel.Workbooks.Open($xlsxPath, 0, $true)
    if ($workbook.Worksheets.Count -ne 2) {
        throw "Microsoft Excel reported $($workbook.Worksheets.Count) worksheets instead of 2."
    }
    $firstSheet = $workbook.Worksheets.Item('Page 1')
    $secondSheet = $workbook.Worksheets.Item('Page 2')
    if ($firstSheet.Range('A1').Text -ne '項目' -or
        $firstSheet.Range('C1').Text -ne '金額' -or
        $firstSheet.Range('A2').Text -ne '家庭測試' -or
        [int]$firstSheet.Range('B2').Value2 -ne 2 -or
        [int]$firstSheet.Range('C2').Value2 -ne 120) {
        throw 'Microsoft Excel did not read the expected table values.'
    }
    if (-not [bool]$firstSheet.Range('A1:B1').MergeCells) {
        throw 'Microsoft Excel did not preserve the expected merged cell range.'
    }
    if ($secondSheet.Range('A1').Text -ne '無表格第一行' -or
        $secondSheet.Range('A2').Text -ne '无表格第二行') {
        throw 'Microsoft Excel did not read the expected fallback text.'
    }

    $summary = [ordered]@{
        recorded_at = [DateTimeOffset]::Now.ToString('o')
        word = [ordered]@{
            application = [string]$word.Name
            version = [string]$word.Version
            pages = $wordPageCount
            file = $docxPath
            multilingual_text = $true
        }
        excel = [ordered]@{
            application = [string]$excel.Name
            version = [string]$excel.Version
            worksheets = [int]$workbook.Worksheets.Count
            file = $xlsxPath
            table_values = $true
            merged_cells = $true
        }
    }
    $summary | ConvertTo-Json -Depth 5 |
        Set-Content -LiteralPath $summaryPath -Encoding UTF8
}
finally {
    if ($null -ne $workbook) {
        $workbook.Close($false)
    }
    if ($null -ne $excel) {
        $excel.Quit()
    }
    if ($null -ne $document) {
        $document.Close(0)
    }
    if ($null -ne $word) {
        $word.Quit()
    }
    foreach ($comObject in @(
        $secondSheet,
        $firstSheet,
        $workbook,
        $excel,
        $document,
        $word
    )) {
        if ($null -ne $comObject) {
            [void][Runtime.InteropServices.Marshal]::FinalReleaseComObject($comObject)
        }
    }
    [GC]::Collect()
    [GC]::WaitForPendingFinalizers()
}

Write-Host "Microsoft Office smoke passed: $summaryPath"
