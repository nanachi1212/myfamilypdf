[CmdletBinding()]
param(
    [switch]$SkipBuild
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$repositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$buildRoot = Join-Path $repositoryRoot 'build'
$setup = Join-Path $buildRoot 'FamilyPDF-Shell-Verification-Setup-x64.exe'
$testRoot = Join-Path $buildRoot 'shell-installer-smoke'
$installRoot = Join-Path $testRoot 'app'
$summaryPath = Join-Path $buildRoot 'shell-installer-smoke-summary.json'
$uninstallKey = 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Uninstall\{D8942855-6D26-4801-908C-B8CD588A19C5}_is1'
$shellPaths = @(
    'HKCU:\Software\Classes\Applications\Pdf4QtViewer.exe',
    'HKCU:\Software\Classes\Applications\Pdf4QtEditor.exe',
    'HKCU:\Software\Classes\SystemFileAssociations\.pdf\shell\FamilyPDF.Open',
    'HKCU:\Software\Classes\SystemFileAssociations\.pdf\shell\FamilyPDF.Edit'
)

foreach ($path in $shellPaths) {
    if (Test-Path -LiteralPath $path) {
        throw "Refusing to overwrite an existing FamilyPDF shell key: $path"
    }
}
if (Test-Path -LiteralPath $uninstallKey) {
    throw "Refusing to overwrite an existing shell-verification uninstall entry: $uninstallKey"
}

$resolvedBuildRoot = [IO.Path]::GetFullPath($buildRoot).TrimEnd('\') + '\'
$resolvedTestRoot = [IO.Path]::GetFullPath($testRoot)
if (-not $resolvedTestRoot.StartsWith(
        $resolvedBuildRoot,
        [StringComparison]::OrdinalIgnoreCase
    )) {
    throw "Refusing to replace a test directory outside build: $resolvedTestRoot"
}
if (Test-Path -LiteralPath $resolvedTestRoot -PathType Container) {
    Remove-Item -LiteralPath $resolvedTestRoot -Recurse -Force
}

if (-not $SkipBuild) {
    & (Join-Path $repositoryRoot 'scripts\phase0\build-installer.ps1') `
        -SkipPackage `
        -SkipOcr `
        -ShellVerificationBuild
}
if (-not (Test-Path -LiteralPath $setup -PathType Leaf)) {
    throw "Shell verification installer was not found: $setup"
}

$installed = $false
$uninstalled = $false
$viewerResponsive = $false
$editorResponsive = $false
try {
    $installProcess = Start-Process -FilePath $setup -ArgumentList @(
        '/VERYSILENT',
        '/SUPPRESSMSGBOXES',
        '/NORESTART',
        '/SP-',
        '/LANG=english',
        "/DIR=$installRoot",
        '/TASKS=pdfshell'
    ) -WindowStyle Hidden -Wait -PassThru
    if ($installProcess.ExitCode -ne 0) {
        throw "Shell verification installation failed with exit code $($installProcess.ExitCode)."
    }
    $installed = $true

    $viewer = Join-Path $installRoot 'Pdf4QtViewer.exe'
    $editor = Join-Path $installRoot 'Pdf4QtEditor.exe'
    $uninstaller = Join-Path $installRoot 'unins000.exe'
    foreach ($required in @($viewer, $editor, $uninstaller)) {
        if (-not (Test-Path -LiteralPath $required -PathType Leaf)) {
            throw "Installed shell-verification file is missing: $required"
        }
    }

    $expectedCommands = [ordered]@{
        'HKCU:\Software\Classes\Applications\Pdf4QtViewer.exe\shell\open\command' =
            "`"$viewer`" `"%1`""
        'HKCU:\Software\Classes\Applications\Pdf4QtEditor.exe\shell\open\command' =
            "`"$editor`" `"%1`""
        'HKCU:\Software\Classes\SystemFileAssociations\.pdf\shell\FamilyPDF.Open\command' =
            "`"$viewer`" `"%1`""
        'HKCU:\Software\Classes\SystemFileAssociations\.pdf\shell\FamilyPDF.Edit\command' =
            "`"$editor`" `"%1`""
    }
    foreach ($entry in $expectedCommands.GetEnumerator()) {
        if (-not (Test-Path -LiteralPath $entry.Key)) {
            throw "Installed shell command key is missing: $($entry.Key)"
        }
        $actual = (Get-Item -LiteralPath $entry.Key).GetValue('')
        if ($actual -cne $entry.Value) {
            throw "Shell command mismatch at $($entry.Key): '$actual'"
        }
    }
    if (-not (Test-Path -LiteralPath $uninstallKey)) {
        throw 'Shell verification uninstall entry was not created.'
    }

    # Build the Unicode name from code points so Windows PowerShell 5.1 can
    # parse this UTF-8-without-BOM script without depending on its ANSI page.
    $fixtureName = ([char]0x542B) + ' ' +
        ([char]0x7A7A) + ([char]0x683C) + ' shell ' +
        ([char]0x6E2C) + ([char]0x8A66) + '.pdf'
    $fixture = Join-Path $testRoot $fixtureName
    Copy-Item -LiteralPath (Join-Path $repositoryRoot 'dist\qa\form-interop.pdf') `
        -Destination $fixture
    foreach ($application in @(
        [pscustomobject]@{ Path = $viewer; Name = 'viewer' },
        [pscustomobject]@{ Path = $editor; Name = 'editor' }
    )) {
        $process = Start-Process -FilePath $application.Path `
            -ArgumentList "`"$fixture`"" -PassThru
        try {
            Start-Sleep -Seconds 5
            $process.Refresh()
            if ($process.HasExited -or -not $process.Responding) {
                throw "$($application.Name) did not remain responsive for a quoted Chinese PDF path."
            }
            if ($application.Name -eq 'viewer') {
                $viewerResponsive = $true
            }
            else {
                $editorResponsive = $true
            }
        }
        finally {
            if (-not $process.HasExited) {
                Stop-Process -Id $process.Id -Force
                $process.WaitForExit(5000) | Out-Null
            }
        }
    }

    $uninstallProcess = Start-Process -FilePath $uninstaller -ArgumentList @(
        '/VERYSILENT',
        '/SUPPRESSMSGBOXES',
        '/NORESTART'
    ) -WindowStyle Hidden -Wait -PassThru
    if ($uninstallProcess.ExitCode -ne 0) {
        throw "Shell verification uninstall failed with exit code $($uninstallProcess.ExitCode)."
    }
    $uninstalled = $true

    foreach ($path in $shellPaths + @($uninstallKey)) {
        if (Test-Path -LiteralPath $path) {
            throw "Uninstall left a FamilyPDF Registry key behind: $path"
        }
    }
    if (Test-Path -LiteralPath $installRoot -PathType Container) {
        $remainingFiles = @(Get-ChildItem -LiteralPath $installRoot -Force)
        if ($remainingFiles.Count -gt 0) {
            throw "Uninstall left $($remainingFiles.Count) item(s) in $installRoot"
        }
    }

    $summary = [ordered]@{
        recorded_at = [DateTimeOffset]::Now.ToString('o')
        installer = $setup
        install_exit_code = $installProcess.ExitCode
        shell_commands = $expectedCommands
        quoted_chinese_pdf = $fixture
        viewer_responding = $viewerResponsive
        editor_responding = $editorResponsive
        uninstall_exit_code = $uninstallProcess.ExitCode
        registry_removed = $true
        installed_files_removed = $true
    }
    $summary | ConvertTo-Json -Depth 6 |
        Set-Content -LiteralPath $summaryPath -Encoding UTF8
}
finally {
    if ($installed -and -not $uninstalled) {
        $fallbackUninstaller = Join-Path $installRoot 'unins000.exe'
        if (Test-Path -LiteralPath $fallbackUninstaller -PathType Leaf) {
            Start-Process -FilePath $fallbackUninstaller -ArgumentList @(
                '/VERYSILENT',
                '/SUPPRESSMSGBOXES',
                '/NORESTART'
            ) -WindowStyle Hidden -Wait | Out-Null
        }
        foreach ($path in $shellPaths + @($uninstallKey)) {
            if (Test-Path -LiteralPath $path) {
                Remove-Item -LiteralPath $path -Recurse -Force
            }
        }
    }
}

Write-Host "Shell installation round-trip passed: $summaryPath"
