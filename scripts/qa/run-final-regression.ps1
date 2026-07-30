[CmdletBinding()]
param(
    [string]$PackageDirectory = '',
    [string]$BuildDirectory = '',
    [string]$LargePdf = '',
    [switch]$SkipBuildTests
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$repositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
if ([string]::IsNullOrWhiteSpace($PackageDirectory)) {
    $PackageDirectory = Join-Path $repositoryRoot 'dist\FamilyPDF-windows-x64'
}
if ([string]::IsNullOrWhiteSpace($BuildDirectory)) {
    $BuildDirectory = Join-Path $repositoryRoot 'build\phase0-upstream-release'
}
if ([string]::IsNullOrWhiteSpace($LargePdf)) {
    $LargePdf = Join-Path $repositoryRoot 'build\large-1160-pages.pdf'
}

$PackageDirectory = [IO.Path]::GetFullPath($PackageDirectory)
$BuildDirectory = [IO.Path]::GetFullPath($BuildDirectory)
$LargePdf = [IO.Path]::GetFullPath($LargePdf)

function Assert-File {
    param([Parameter(Mandatory)][string]$LiteralPath)
    if (-not (Test-Path -LiteralPath $LiteralPath -PathType Leaf)) {
        throw "Required file was not found: $LiteralPath"
    }
}

function Stop-TestProcess {
    param([Diagnostics.Process]$Process)
    if ($null -ne $Process -and -not $Process.HasExited) {
        Stop-Process -Id $Process.Id -Force
        $Process.WaitForExit(5000) | Out-Null
    }
}

function Start-ResponsiveSmoke {
    param(
        [Parameter(Mandatory)][string]$Executable,
        [Parameter(Mandatory)][string[]]$Arguments,
        [int]$WaitSeconds = 10
    )

    $process = Start-Process -FilePath $Executable -ArgumentList $Arguments -PassThru
    try {
        Start-Sleep -Seconds $WaitSeconds
        $process.Refresh()
        if ($process.HasExited) {
            throw "$([IO.Path]::GetFileName($Executable)) exited during smoke testing."
        }
        if (-not $process.Responding) {
            throw "$([IO.Path]::GetFileName($Executable)) stopped responding."
        }
        return [pscustomobject]@{
            executable = [IO.Path]::GetFileName($Executable)
            responding = $true
            working_set_bytes = $process.WorkingSet64
        }
    }
    finally {
        Stop-TestProcess -Process $process
    }
}

foreach ($required in @(
    (Join-Path $PackageDirectory 'Pdf4QtViewer.exe'),
    (Join-Path $PackageDirectory 'Pdf4QtEditor.exe'),
    (Join-Path $PackageDirectory 'PdfTool.exe'),
    $LargePdf
)) {
    Assert-File -LiteralPath $required
}

$timestamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$qaRoot = Join-Path $repositoryRoot "build\final-regression-$timestamp"
$qaPackage = Join-Path $qaRoot 'portable'
New-Item -ItemType Directory -Path $qaRoot -Force | Out-Null
Copy-Item -LiteralPath $PackageDirectory -Destination $qaPackage -Recurse

& (Join-Path $repositoryRoot 'scripts\qa\verify-editor-plugins.ps1') `
    -PackageDirectory $qaPackage
& (Join-Path $repositoryRoot 'scripts\qa\smoke-office-export.ps1') `
    -PackageDirectory $qaPackage -SkipBuild

if (-not $SkipBuildTests) {
    & (Join-Path $repositoryRoot 'scripts\phase0\build-upstream-baseline.ps1') `
        -Stage Test -BuildDirectory $BuildDirectory
}

$venvPython = Join-Path (
    Split-Path $repositoryRoot -Parent
) 'FamilyPDF-tools\office-export-venv\Scripts\python.exe'
Assert-File -LiteralPath $venvPython
Push-Location (Join-Path $repositoryRoot 'office-export')
try {
    & $venvPython -m unittest discover -s tests -v
    if ($LASTEXITCODE -ne 0) {
        throw 'Office Export Python tests failed.'
    }
}
finally {
    Pop-Location
}

$pdfTool = Join-Path $qaPackage 'PdfTool.exe'
$infoOutput = & $pdfTool info $LargePdf 2>&1
if ($LASTEXITCODE -ne 0 -or ($infoOutput -join "`n") -notmatch '(?i)page count\s+1,?160') {
    throw "PdfTool did not report 1,160 pages.`n$($infoOutput -join "`n")"
}

$formPdf = Join-Path $repositoryRoot 'dist\qa\form-interop.pdf'
$documentEditPdf = Join-Path $repositoryRoot 'dist\qa\document-edit-interop.pdf'
Assert-File -LiteralPath $formPdf
Assert-File -LiteralPath $documentEditPdf

$settingsRoot = Join-Path $qaRoot 'settings'
$settingsFile = Join-Path $settingsRoot 'MelkaJ\PDF4QT Editor.ini'
New-Item -ItemType Directory -Path (Split-Path $settingsFile) -Force | Out-Null
@'
[Plugins]
EnabledPlugins=@Invalid()
'@ | Set-Content -LiteralPath $settingsFile -Encoding UTF8

$editor = Join-Path $qaPackage 'Pdf4QtEditor.exe'
$pluginSmoke = Start-ResponsiveSmoke -Executable $editor -Arguments @(
    '--config', $settingsRoot, $formPdf
) -WaitSeconds 8
$settingsText = Get-Content -LiteralPath $settingsFile -Raw -Encoding UTF8
foreach ($expected in @(
    'FamilyPDFDefaultsVersion=2',
    'Document Edit',
    'Editor',
    'Forms',
    'FamilyPDF Office Export',
    'Redact',
    'Signature'
)) {
    if (-not $settingsText.Contains($expected)) {
        throw "First-run plugin migration did not persist '$expected'."
    }
}

function Test-MultiDocumentSession {
    param([Parameter(Mandatory)][string]$Executable)

    $sessionPath = Join-Path $qaPackage 'data\session.json'
    if (Test-Path -LiteralPath $sessionPath -PathType Leaf) {
        Remove-Item -LiteralPath $sessionPath -Force
    }
    $process = Start-Process -FilePath $Executable -ArgumentList @(
        $formPdf, $documentEditPdf, $LargePdf
    ) -PassThru
    try {
        $deadline = [DateTime]::UtcNow.AddSeconds(45)
        $documentCount = 0
        while ([DateTime]::UtcNow -lt $deadline) {
            Start-Sleep -Seconds 1
            $process.Refresh()
            if ($process.HasExited) {
                throw "$([IO.Path]::GetFileName($Executable)) exited while opening multiple documents."
            }
            if (Test-Path -LiteralPath $sessionPath -PathType Leaf) {
                try {
                    $session = Get-Content -LiteralPath $sessionPath -Raw -Encoding UTF8 |
                        ConvertFrom-Json
                    $documentCount = @($session.documents).Count
                    if ($documentCount -eq 3) {
                        break
                    }
                }
                catch {
                    # QSaveFile replaces the file atomically. Retry if the
                    # reader happened to observe the replacement boundary.
                }
            }
        }
        $process.Refresh()
        if ($documentCount -ne 3) {
            throw "$([IO.Path]::GetFileName($Executable)) persisted $documentCount documents instead of 3."
        }
        if (-not $process.Responding) {
            throw "$([IO.Path]::GetFileName($Executable)) stopped responding after opening three documents."
        }
        return [pscustomobject]@{
            executable = [IO.Path]::GetFileName($Executable)
            responding = $true
            documents = $documentCount
            working_set_bytes = $process.WorkingSet64
        }
    }
    finally {
        Stop-TestProcess -Process $process
    }
}

$viewerMultiFile = Test-MultiDocumentSession -Executable (
    Join-Path $qaPackage 'Pdf4QtViewer.exe'
)
$editorMultiFile = Test-MultiDocumentSession -Executable $editor

$qtPrefix = 'E:\CodexProject\FamilyPDF-tools\qt\6.9.1\msvc2022_64'
$lconvert = Join-Path $qtPrefix 'bin\lconvert.exe'
Assert-File -LiteralPath $lconvert
foreach ($locale in @('zh_TW', 'zh_CN')) {
    $qm = Join-Path $qaPackage "translations\PDF4QT_$locale.qm"
    $ts = Join-Path $qaRoot "PDF4QT_$locale.ts"
    Assert-File -LiteralPath $qm
    & $lconvert -i $qm -o $ts
    if ($LASTEXITCODE -ne 0) {
        throw "Could not decode translation payload: $qm"
    }
    $translationText = Get-Content -LiteralPath $ts -Raw -Encoding UTF8
    if (-not $translationText.Contains('&amp;Office Export')) {
        throw "Office Export translation is missing from $qm"
    }
}

$summary = [ordered]@{
    recorded_at = [DateTimeOffset]::Now.ToString('o')
    package = $PackageDirectory
    large_pdf = [ordered]@{
        path = $LargePdf
        pages = 1160
    }
    plugin_defaults = [ordered]@{
        version = 2
        enabled = @(
            'Document Edit',
            'Editor',
            'Forms',
            'FamilyPDF Office Export',
            'Redact',
            'Signature'
        )
        responding = $pluginSmoke.responding
    }
    viewer_multi_file = $viewerMultiFile
    editor_multi_file = $editorMultiFile
    office_export = [ordered]@{
        python_tests = 7
        packaged_smoke = $true
        translations = @('zh_TW', 'zh_CN')
    }
}
$summaryPath = Join-Path $qaRoot 'summary.json'
$summary | ConvertTo-Json -Depth 6 |
    Set-Content -LiteralPath $summaryPath -Encoding UTF8

Write-Host "Final regression passed: $summaryPath"
