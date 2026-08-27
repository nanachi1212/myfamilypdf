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
. (Join-Path $repositoryRoot 'scripts\common\Resolve-FamilyPDFToolsRoot.ps1')
$toolsRoot = Resolve-FamilyPDFToolsRoot -RepositoryRoot $repositoryRoot
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
    (Join-Path $PackageDirectory 'Pdf4QtDiff.exe'),
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
$diffSmokeRoot = Join-Path $qaRoot 'pdf-diff'
& (Join-Path $repositoryRoot 'scripts\qa\smoke-pdf-diff.ps1') `
    -PackageDirectory $qaPackage -OutputDirectory $diffSmokeRoot
$diffSmokeSummary = Get-Content -LiteralPath (
    Join-Path $diffSmokeRoot 'summary.json'
) -Raw -Encoding UTF8 | ConvertFrom-Json
$securitySmokeRoot = Join-Path $qaRoot 'pdf-security'
& (Join-Path $repositoryRoot 'scripts\qa\smoke-pdf-security.ps1') `
    -PackageDirectory $qaPackage -OutputDirectory $securitySmokeRoot
$securitySmokeSummary = Get-Content -LiteralPath (
    Join-Path $securitySmokeRoot 'summary.json'
) -Raw -Encoding UTF8 | ConvertFrom-Json

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
    $officeTestStdout = Join-Path $qaRoot 'office-python-tests.stdout.log'
    $officeTestStderr = Join-Path $qaRoot 'office-python-tests.stderr.log'
    $officeTestProcess = Start-Process -FilePath $venvPython -ArgumentList @(
        '-m', 'unittest', 'discover', '-s', 'tests', '-v'
    ) -Wait -PassThru -NoNewWindow `
        -RedirectStandardOutput $officeTestStdout `
        -RedirectStandardError $officeTestStderr
    $officeTestOutput = @(
        Get-Content -LiteralPath $officeTestStdout -ErrorAction SilentlyContinue
        Get-Content -LiteralPath $officeTestStderr -ErrorAction SilentlyContinue
    )
    $officeTestOutput | ForEach-Object { Write-Host $_ }
    if ($officeTestProcess.ExitCode -ne 0) {
        throw 'Office Export Python tests failed.'
    }
    $officeTestSummary = $officeTestOutput |
        ForEach-Object { $_.ToString() } |
        Where-Object { $_ -match '^Ran (\d+) tests? in ' } |
        Select-Object -Last 1
    if (-not $officeTestSummary -or
        $officeTestSummary -notmatch '^Ran (\d+) tests? in ') {
        throw 'Office Export Python test count was not reported.'
    }
    $officeTestCount = [int]$Matches[1]
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

$qtPrefix = Join-Path $toolsRoot 'qt\6.9.1\msvc2022_64'
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
        python_tests = $officeTestCount
        packaged_smoke = $true
        translations = @('zh_TW', 'zh_CN')
    }
    document_compare = [ordered]@{
        gui_responding = [bool]$diffSmokeSummary.gui.responding
        cli_difference_type = [string](
            $diffSmokeSummary.cli.difference_type
        )
    }
    pdf_security = [ordered]@{
        algorithm = [string]$securitySmokeSummary.algorithm
        revision = [int]$securitySmokeSummary.revision
        pages = [int]$securitySmokeSummary.pages
        wrong_password_rejected = [bool]$securitySmokeSummary.wrong_password_rejected
        user_password_accepted = [bool]$securitySmokeSummary.user_password_accepted
        owner_password_accepted = [bool]$securitySmokeSummary.owner_password_accepted
        permissions_restricted = [bool]$securitySmokeSummary.permissions_restricted
        decrypted_text_preserved = [bool]$securitySmokeSummary.decrypted_text_preserved
        decrypted_render_preserved = [bool]$securitySmokeSummary.decrypted_render_preserved
        gui_responding = [bool]$securitySmokeSummary.gui_responding
    }
}
$summaryPath = Join-Path $qaRoot 'summary.json'
$summary | ConvertTo-Json -Depth 6 |
    Set-Content -LiteralPath $summaryPath -Encoding UTF8

& (Join-Path $PSScriptRoot 'cleanup-final-regression-results.ps1') `
    -BuildRoot (Join-Path $repositoryRoot 'build') `
    -Keep 1 `
    -CurrentResult $qaRoot

Write-Host "Final regression passed: $summaryPath"
