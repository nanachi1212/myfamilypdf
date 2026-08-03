[CmdletBinding()]
param(
    [string]$PackageDirectory,
    [string]$OutputDirectory
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$repositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
if ([string]::IsNullOrWhiteSpace($PackageDirectory)) {
    $PackageDirectory = Join-Path $repositoryRoot `
        'dist\FamilyPDF-windows-x64'
}
if ([string]::IsNullOrWhiteSpace($OutputDirectory)) {
    $OutputDirectory = Join-Path $repositoryRoot 'build\pdf-diff-smoke'
}
$PackageDirectory = [IO.Path]::GetFullPath($PackageDirectory)
$OutputDirectory = [IO.Path]::GetFullPath($OutputDirectory)

function Assert-File {
    param([Parameter(Mandatory)][string]$LiteralPath)

    if (-not (Test-Path -LiteralPath $LiteralPath -PathType Leaf)) {
        throw "Required PDF comparison file was not found: $LiteralPath"
    }
}

$diffGui = Join-Path $PackageDirectory 'Pdf4QtDiff.exe'
$pdfTool = Join-Path $PackageDirectory 'PdfTool.exe'
$python = Join-Path (Split-Path $repositoryRoot -Parent) `
    'FamilyPDF-tools\office-export-venv\Scripts\python.exe'
Assert-File -LiteralPath $diffGui
Assert-File -LiteralPath $pdfTool
Assert-File -LiteralPath $python

New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null
$leftPdf = Join-Path $OutputDirectory 'left.pdf'
$rightPdf = Join-Path $OutputDirectory 'right.pdf'
$settingsDirectory = Join-Path $OutputDirectory 'settings'
New-Item -ItemType Directory -Path $settingsDirectory -Force | Out-Null

& $python -c @'
from pathlib import Path
import sys

from pypdf import PdfWriter
from pypdf.generic import DecodedStreamObject, DictionaryObject, NameObject


def write_pdf(path: Path, text: str) -> None:
    writer = PdfWriter()
    page = writer.add_blank_page(width=300, height=300)
    font = DictionaryObject(
        {
            NameObject('/Type'): NameObject('/Font'),
            NameObject('/Subtype'): NameObject('/Type1'),
            NameObject('/BaseFont'): NameObject('/Helvetica'),
        }
    )
    page[NameObject('/Resources')] = DictionaryObject(
        {
            NameObject('/Font'): DictionaryObject(
                {NameObject('/F1'): writer._add_object(font)}
            )
        }
    )
    stream = DecodedStreamObject()
    stream.set_data(
        f'BT /F1 20 Tf 40 200 Td ({text}) Tj ET'.encode('ascii')
    )
    page[NameObject('/Contents')] = writer._add_object(stream)
    with path.open('wb') as handle:
        writer.write(handle)


root = Path(sys.argv[1])
write_pdf(root / 'left.pdf', 'Version A')
write_pdf(root / 'right.pdf', 'Version B')
'@ $OutputDirectory
if ($LASTEXITCODE -ne 0) {
    throw 'Could not create the PDF comparison fixtures.'
}
Assert-File -LiteralPath $leftPdf
Assert-File -LiteralPath $rightPdf

$xmlText = (& $pdfTool diff --console-format xml $leftPdf $rightPdf 2>&1 |
    Out-String)
if ($LASTEXITCODE -ne 0) {
    throw "PdfTool diff failed.`n$xmlText"
}
[xml]$xml = $xmlText
$difference = $xml.'difference-report'.differences.difference
if ($difference.type -ne 'text-replaced' -or
    $difference.'text-added' -ne 'B' -or
    $difference.'text-removed' -ne 'A') {
    throw "Unexpected diff XML.`n$xmlText"
}

$quotedSettings = '"' + $settingsDirectory + '"'
$quotedLeft = '"' + $leftPdf + '"'
$quotedRight = '"' + $rightPdf + '"'
$process = Start-Process -FilePath $diffGui -ArgumentList @(
    '-c', $quotedSettings, $quotedLeft, $quotedRight
) -PassThru
$guiSummary = $null
try {
    $deadline = [DateTime]::UtcNow.AddSeconds(45)
    do {
        Start-Sleep -Milliseconds 500
        $process.Refresh()
    } while (-not $process.HasExited -and
        $process.MainWindowHandle -eq 0 -and
        [DateTime]::UtcNow -lt $deadline)

    if ($process.HasExited) {
        throw "Pdf4QtDiff exited unexpectedly with code $($process.ExitCode)."
    }
    if ($process.MainWindowHandle -eq 0) {
        throw 'Pdf4QtDiff did not create a main window within 45 seconds.'
    }

    Start-Sleep -Seconds 2
    $process.Refresh()
    if (-not $process.Responding) {
        throw 'Pdf4QtDiff stopped responding with two input PDFs.'
    }
    $guiSummary = [ordered]@{
        responding = $true
        main_window_title = $process.MainWindowTitle
        working_set_bytes = $process.WorkingSet64
    }
}
finally {
    if (-not $process.HasExited) {
        $process.Kill()
        $process.WaitForExit()
    }
}

$summary = [ordered]@{
    recorded_at = [DateTimeOffset]::Now.ToString('o')
    package = $PackageDirectory
    fixtures = [ordered]@{
        left = $leftPdf
        right = $rightPdf
    }
    cli = [ordered]@{
        difference_type = [string]$difference.type
        text_added = [string]$difference.'text-added'
        text_removed = [string]$difference.'text-removed'
    }
    gui = $guiSummary
}
$summaryPath = Join-Path $OutputDirectory 'summary.json'
$summary | ConvertTo-Json -Depth 5 |
    Set-Content -LiteralPath $summaryPath -Encoding UTF8

Write-Host "PDF document comparison smoke passed: $summaryPath"
