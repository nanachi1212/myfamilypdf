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
$traditionalRange = $null
$simplifiedRange = $null
$secondPageRange = $null
$summaryPath = Join-Path $OutputDirectory 'summary.json'

function ConvertFrom-CodePoints {
    param([Parameter(Mandatory)][int[]]$Value)
    return -join @($Value | ForEach-Object { [char]$_ })
}

$traditionalChinese = ConvertFrom-CodePoints @(0x7E41, 0x9AD4, 0x4E2D, 0x6587)
$simplifiedChinese = ConvertFrom-CodePoints @(0x7B80, 0x4F53, 0x4E2D, 0x6587)
$secondPageTraditional = ConvertFrom-CodePoints @(0x7B2C, 0x4E8C, 0x9801)
$secondPageSimplified = ConvertFrom-CodePoints @(0x7B2C, 0x4E8C, 0x9875)
$itemHeader = ConvertFrom-CodePoints @(0x9805, 0x76EE)
$amountHeader = ConvertFrom-CodePoints @(0x91D1, 0x984D)
$familyTest = ConvertFrom-CodePoints @(0x5BB6, 0x5EAD, 0x6E2C, 0x8A66)
$fallbackTraditional = ConvertFrom-CodePoints @(
    0x7121, 0x8868, 0x683C, 0x7B2C, 0x4E00, 0x884C
)
$fallbackSimplified = ConvertFrom-CodePoints @(
    0x65E0, 0x8868, 0x683C, 0x7B2C, 0x4E8C, 0x884C
)

try {
    $word = New-Object -ComObject Word.Application
    $word.Visible = $false
    $word.DisplayAlerts = 0
    $document = $word.Documents.Open($docxPath, $false, $true, $false)
    $wordText = [string]$document.Content.Text
    foreach ($expected in @(
        $traditionalChinese,
        $simplifiedChinese,
        'FamilyPDF Office interoperability',
        $secondPageTraditional,
        $secondPageSimplified
    )) {
        if (-not $wordText.Contains($expected)) {
            throw "Microsoft Word did not read expected text: $expected"
        }
    }
    $wordPageCount = [int]$document.ComputeStatistics(2)
    if ($wordPageCount -lt 2) {
        throw "Microsoft Word reported $wordPageCount page instead of at least 2."
    }

    $traditionalRange = $document.Content.Duplicate
    if (-not $traditionalRange.Find.Execute($traditionalChinese) -or
        [int]$traditionalRange.Bold -ne -1) {
        throw 'Microsoft Word did not preserve the expected bold run.'
    }
    $simplifiedRange = $document.Content.Duplicate
    if (-not $simplifiedRange.Find.Execute($simplifiedChinese) -or
        [int]$simplifiedRange.Italic -ne -1) {
        throw 'Microsoft Word did not preserve the expected italic run.'
    }
    $secondPageRange = $document.Content.Duplicate
    if (-not $secondPageRange.Find.Execute($secondPageTraditional) -or
        [int]$secondPageRange.Information(3) -ne 2) {
        throw 'Microsoft Word did not lay out the second-page text on page 2.'
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
    if ($firstSheet.Range('A1').Text -ne $itemHeader -or
        $firstSheet.Range('C1').Text -ne $amountHeader -or
        $firstSheet.Range('A2').Text -ne $familyTest -or
        [int]$firstSheet.Range('B2').Value2 -ne 2 -or
        [int]$firstSheet.Range('C2').Value2 -ne 120) {
        throw 'Microsoft Excel did not read the expected table values.'
    }
    if (-not [bool]$firstSheet.Range('A1:B1').MergeCells) {
        throw 'Microsoft Excel did not preserve the expected merged cell range.'
    }
    if ($secondSheet.Range('A1').Text -ne $fallbackTraditional -or
        $secondSheet.Range('A2').Text -ne $fallbackSimplified) {
        throw 'Microsoft Excel did not read the expected fallback text.'
    }
    if ([int]$firstSheet.UsedRange.Rows.Count -ne 2 -or
        [int]$firstSheet.UsedRange.Columns.Count -ne 3 -or
        [int]$secondSheet.UsedRange.Rows.Count -ne 2 -or
        [int]$secondSheet.UsedRange.Columns.Count -ne 1) {
        throw 'Microsoft Excel reported an unexpected used layout range.'
    }
    $firstColumnWidth = [double]$firstSheet.Columns.Item(1).ColumnWidth
    $thirdColumnWidth = [double]$firstSheet.Columns.Item(3).ColumnWidth
    # Excel reports the openpyxl width of 8 as about 7.38 because its
    # displayed ColumnWidth unit depends on the Normal style font metrics.
    if (-not [bool]$firstSheet.Range('A1').Font.Bold -or
        $firstColumnWidth -lt 7 -or
        $thirdColumnWidth -lt 7) {
        throw 'Microsoft Excel did not preserve header style or fitted columns.'
    }

    $wordApplicationName = [string]$word.Name
    $wordVersion = [string]$word.Version
    $excelApplicationName = [string]$excel.Name
    $excelVersion = [string]$excel.Version
    $excelWorksheetCount = [int]$workbook.Worksheets.Count
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
        $secondPageRange,
        $simplifiedRange,
        $traditionalRange,
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

$summary = [ordered]@{
    recorded_at = [DateTimeOffset]::Now.ToString('o')
    word = [ordered]@{
        application = $wordApplicationName
        version = $wordVersion
        pages = $wordPageCount
        file = $docxPath
        multilingual_text = $true
        bold_style = $true
        italic_style = $true
        page_break_layout = $true
    }
    excel = [ordered]@{
        application = $excelApplicationName
        version = $excelVersion
        worksheets = $excelWorksheetCount
        file = $xlsxPath
        table_values = $true
        merged_cells = $true
        used_ranges = $true
        header_style = $true
        fitted_columns = $true
    }
}
$summary | ConvertTo-Json -Depth 5 |
    Set-Content -LiteralPath $summaryPath -Encoding UTF8

Write-Host "Microsoft Office smoke passed: $summaryPath"
