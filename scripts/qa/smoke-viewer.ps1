[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [string]$RuntimeDirectory,
    [Parameter(Mandatory)]
    [string]$PdfFile,
    [string]$OutputDirectory = ''
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$RuntimeDirectory = [IO.Path]::GetFullPath($RuntimeDirectory)
$PdfFile = [IO.Path]::GetFullPath($PdfFile)
if ([string]::IsNullOrWhiteSpace($OutputDirectory)) {
    $OutputDirectory = Join-Path $env:TEMP 'FamilyPDF-viewer-smoke'
}
$OutputDirectory = [IO.Path]::GetFullPath($OutputDirectory)
$viewer = Join-Path $RuntimeDirectory 'Pdf4QtViewer.exe'
foreach ($requiredFile in @($viewer, $PdfFile)) {
    if (-not (Test-Path -LiteralPath $requiredFile -PathType Leaf)) {
        throw "Required file was not found: $requiredFile"
    }
}

New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null
$settingsRoot = Join-Path $OutputDirectory 'settings'
New-Item -ItemType Directory -Path $settingsRoot -Force | Out-Null
$arguments = @(
    '--config',
    ('"{0}"' -f $settingsRoot),
    '--theme-light',
    ('"{0}"' -f $PdfFile)
)
$process = Start-Process -FilePath $viewer -ArgumentList $arguments -PassThru -WindowStyle Hidden
$title = ''
try {
    $deadline = [DateTime]::UtcNow.AddSeconds(30)
    do {
        Start-Sleep -Milliseconds 500
        $process.Refresh()
        if ($process.HasExited) {
            throw "Viewer exited during startup with exit code $($process.ExitCode)."
        }
        $title = $process.MainWindowTitle
    } while ((-not $process.Responding -or [string]::IsNullOrWhiteSpace($title)) -and
        [DateTime]::UtcNow -lt $deadline)

    if (-not $process.Responding) {
        throw 'Viewer did not become responsive within 30 seconds.'
    }
    $expectedTitle = [IO.Path]::GetFileName($PdfFile)
    if ($title -notlike "*$expectedTitle*") {
        throw "Viewer did not report the opened PDF in its title. Actual title: $title"
    }

    $summary = [ordered]@{
        recorded_at = [DateTimeOffset]::Now.ToString('o')
        viewer = $viewer
        pdf_file = $PdfFile
        responding = $true
        main_window_title = $title
    }
    $summaryPath = Join-Path $OutputDirectory 'viewer-smoke.json'
    $summary | ConvertTo-Json -Depth 3 |
        Set-Content -LiteralPath $summaryPath -Encoding UTF8
    Write-Host "Viewer smoke test passed: $summaryPath"
}
finally {
    if (-not $process.HasExited) {
        Stop-Process -Id $process.Id -Force
        $process.WaitForExit(5000) | Out-Null
    }
}
