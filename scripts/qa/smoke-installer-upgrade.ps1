[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$repositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$currentVersion = (Get-Content -LiteralPath (Join-Path $repositoryRoot 'VERSION') -Raw -Encoding UTF8).Trim()
$testAppId = 'A31480A6-4CB4-4CBF-B797-A971C4C2A2C7'
$uninstallKey = "HKCU:\Software\Microsoft\Windows\CurrentVersion\Uninstall\{$testAppId}_is1"
$testRoot = Join-Path $env:TEMP "FamilyPDF-upgrade-smoke-$PID"
$installRoot = Join-Path $testRoot 'installed'
$oldSetup = Join-Path $testRoot 'FamilyPDF-0.2.2-Setup-x64.exe'
$newSetup = Join-Path $repositoryRoot 'build\FamilyPDF-Upgrade-Verification-Setup-x64.exe'
$tempRoot = [IO.Path]::GetFullPath($env:TEMP).TrimEnd('\') + '\'
$resolvedTestRoot = [IO.Path]::GetFullPath($testRoot)

if (-not $resolvedTestRoot.StartsWith($tempRoot, [StringComparison]::OrdinalIgnoreCase)) {
    throw "Refusing to use a test directory outside TEMP: $resolvedTestRoot"
}

function Invoke-Setup {
    param(
        [Parameter(Mandatory)]
        [string]$Path,
        [string[]]$AdditionalArguments = @()
    )

    $arguments = @('/VERYSILENT', '/SUPPRESSMSGBOXES', '/NORESTART', '/SP-') +
        $AdditionalArguments
    $process = Start-Process -FilePath $Path -ArgumentList $arguments `
        -WindowStyle Hidden -Wait -PassThru
    if ($process.ExitCode -ne 0) {
        throw "Installer failed with exit code $($process.ExitCode): $Path"
    }
}

try {
    New-Item -ItemType Directory -Path $testRoot -Force | Out-Null

    & (Join-Path $repositoryRoot 'scripts\phase0\build-installer.ps1') `
        -SkipPackage -SkipOcr -UpgradeVerificationBuild -VersionOverride '0.2.2'
    Copy-Item -LiteralPath $newSetup -Destination $oldSetup

    Invoke-Setup -Path $oldSetup -AdditionalArguments @("/DIR=$installRoot")
    $oldRegistration = Get-ItemProperty -LiteralPath $uninstallKey
    if ([version]$oldRegistration.DisplayVersion -ne [version]'0.2.2') {
        throw "Old fixture registered version $($oldRegistration.DisplayVersion), expected 0.2.2."
    }
    if ([IO.Path]::GetFullPath($oldRegistration.InstallLocation).TrimEnd('\') -ne
        [IO.Path]::GetFullPath($installRoot).TrimEnd('\')) {
        throw 'Old fixture did not register the requested isolated install directory.'
    }

    $sentinelPath = Join-Path $installRoot 'upgrade-preserve.sentinel'
    [IO.File]::WriteAllText($sentinelPath, 'preserve-across-upgrade')

    & (Join-Path $repositoryRoot 'scripts\phase0\build-installer.ps1') `
        -SkipPackage -SkipOcr -UpgradeVerificationBuild -VersionOverride $currentVersion
    Invoke-Setup -Path $newSetup

    $newRegistration = Get-ItemProperty -LiteralPath $uninstallKey
    if ([version]$newRegistration.DisplayVersion -ne [version]$currentVersion) {
        throw "Upgrade registered version $($newRegistration.DisplayVersion), expected $currentVersion."
    }
    if ([IO.Path]::GetFullPath($newRegistration.InstallLocation).TrimEnd('\') -ne
        [IO.Path]::GetFullPath($installRoot).TrimEnd('\')) {
        throw 'Upgrade did not reuse the previous installation directory.'
    }
    if ([IO.File]::ReadAllText($sentinelPath) -ne 'preserve-across-upgrade') {
        throw 'Upgrade did not preserve an existing installation-side file.'
    }
    if (-not (Test-Path -LiteralPath (Join-Path $installRoot 'Pdf4QtViewer.exe') -PathType Leaf)) {
        throw 'Upgraded installation is missing Pdf4QtViewer.exe.'
    }

    Write-Host "Installer upgrade smoke passed: 0.2.2 -> $currentVersion in $installRoot"
}
finally {
    if (Test-Path -LiteralPath $uninstallKey) {
        $registration = Get-ItemProperty -LiteralPath $uninstallKey
        $uninstaller = Join-Path $registration.InstallLocation 'unins000.exe'
        if (Test-Path -LiteralPath $uninstaller -PathType Leaf) {
            $process = Start-Process -FilePath $uninstaller -ArgumentList @(
                '/VERYSILENT', '/SUPPRESSMSGBOXES', '/NORESTART'
            ) -WindowStyle Hidden -Wait -PassThru
            if ($process.ExitCode -ne 0) {
                Write-Warning "Test uninstaller exited with $($process.ExitCode)."
            }
        }
    }
    if (Test-Path -LiteralPath $uninstallKey) {
        Remove-Item -LiteralPath $uninstallKey -Recurse -Force
    }
    if (Test-Path -LiteralPath $resolvedTestRoot) {
        Remove-Item -LiteralPath $resolvedTestRoot -Recurse -Force
    }
}
