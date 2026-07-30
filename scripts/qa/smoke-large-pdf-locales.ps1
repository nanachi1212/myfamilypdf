[CmdletBinding()]
param(
    [string]$PackageDirectory = '',
    [string]$LargePdf = '',
    [string]$OutputDirectory = '',
    [ValidateRange(10, 3600)]
    [int]$DurationSeconds = 30
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$repositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
if ([string]::IsNullOrWhiteSpace($PackageDirectory)) {
    $PackageDirectory = Join-Path $repositoryRoot 'dist\FamilyPDF-windows-x64'
}
if ([string]::IsNullOrWhiteSpace($LargePdf)) {
    $LargePdf = Join-Path $repositoryRoot 'build\large-1160-pages.pdf'
}
if ([string]::IsNullOrWhiteSpace($OutputDirectory)) {
    $OutputDirectory = Join-Path $repositoryRoot 'build\large-pdf-locale-smoke'
}

$PackageDirectory = [IO.Path]::GetFullPath($PackageDirectory)
$LargePdf = [IO.Path]::GetFullPath($LargePdf)
$OutputDirectory = [IO.Path]::GetFullPath($OutputDirectory)
$viewer = Join-Path $PackageDirectory 'Pdf4QtViewer.exe'
foreach ($required in @($viewer, $LargePdf)) {
    if (-not (Test-Path -LiteralPath $required -PathType Leaf)) {
        throw "Required file was not found: $required"
    }
}

New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null
$locales = [ordered]@{
    zh_TW = 'E_LANGUAGE_CHINESE_TRADITIONAL'
    zh_CN = 'E_LANGUAGE_CHINESE_SIMPLIFIED'
}
$results = @()

foreach ($locale in $locales.GetEnumerator()) {
    $settingsRoot = Join-Path $OutputDirectory "settings\$($locale.Key)"
    $settingsFile = Join-Path $settingsRoot 'MelkaJ\PDF4QT Viewer.ini'
    New-Item -ItemType Directory -Path (Split-Path $settingsFile) -Force | Out-Null
    @"
[Language]
language=$($locale.Value)
"@ | Set-Content -LiteralPath $settingsFile -Encoding UTF8

    $arguments = @(
        '--config',
        "`"$settingsRoot`"",
        "`"$LargePdf`""
    )
    $process = Start-Process -FilePath $viewer -ArgumentList $arguments -PassThru
    try {
        $startupDeadline = [DateTime]::UtcNow.AddSeconds(30)
        do {
            Start-Sleep -Seconds 1
            $process.Refresh()
            if ($process.HasExited) {
                throw "Viewer exited while loading the large PDF for $($locale.Key)."
            }
        } while (-not $process.Responding -and [DateTime]::UtcNow -lt $startupDeadline)
        if (-not $process.Responding) {
            throw "Viewer did not become responsive for $($locale.Key)."
        }

        $maxWorkingSet = [int64]0
        $maxPrivateMemory = [int64]0
        $samples = 0
        $nonRespondingSamples = 0
        $stopwatch = [Diagnostics.Stopwatch]::StartNew()
        while ($stopwatch.Elapsed.TotalSeconds -lt $DurationSeconds) {
            Start-Sleep -Seconds 1
            $process.Refresh()
            if ($process.HasExited) {
                throw "Viewer exited during the $($locale.Key) large-PDF smoke test."
            }
            $samples++
            if (-not $process.Responding) {
                $nonRespondingSamples++
            }
            $maxWorkingSet = [Math]::Max($maxWorkingSet, $process.WorkingSet64)
            $maxPrivateMemory = [Math]::Max(
                $maxPrivateMemory,
                $process.PrivateMemorySize64
            )
        }
        $stopwatch.Stop()
        $process.Refresh()
        if (-not $process.Responding -or $nonRespondingSamples -gt 0) {
            throw "Viewer stopped responding during the $($locale.Key) smoke test."
        }

        $results += [ordered]@{
            locale = $locale.Key
            language_setting = $locale.Value
            duration_seconds = [Math]::Round($stopwatch.Elapsed.TotalSeconds, 3)
            samples = $samples
            responding = $true
            non_responding_samples = $nonRespondingSamples
            max_working_set_bytes = $maxWorkingSet
            max_private_memory_bytes = $maxPrivateMemory
        }
    }
    finally {
        if (-not $process.HasExited) {
            Stop-Process -Id $process.Id -Force
            $process.WaitForExit(5000) | Out-Null
        }
    }
}

$summary = [ordered]@{
    recorded_at = [DateTimeOffset]::Now.ToString('o')
    viewer = $viewer
    large_pdf = $LargePdf
    page_count = 1160
    locales = $results
}
$summaryPath = Join-Path $OutputDirectory 'summary.json'
$summary | ConvertTo-Json -Depth 6 |
    Set-Content -LiteralPath $summaryPath -Encoding UTF8

Write-Host "Large PDF locale smoke passed: $summaryPath"
