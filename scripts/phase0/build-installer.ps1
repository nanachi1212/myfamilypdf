[CmdletBinding()]
param(
    [switch]$SkipPackage,
    [switch]$SkipOcr,
    [switch]$VerificationBuild
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$repositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$toolsRoot = 'E:\CodexProject\FamilyPDF-tools'
$innoVersion = '7.0.2'
$innoRoot = Join-Path $toolsRoot "inno-$innoVersion"
$iscc = Join-Path $innoRoot 'ISCC.exe'
$innoInstaller = Join-Path $toolsRoot "innosetup-$innoVersion-x64.exe"
$innoUrl = "https://github.com/jrsoftware/issrc/releases/download/is-7_0_2/innosetup-$innoVersion-x64.exe"

New-Item -ItemType Directory -Path $toolsRoot -Force | Out-Null

if (-not (Test-Path -LiteralPath $iscc -PathType Leaf)) {
    if (-not (Test-Path -LiteralPath $innoInstaller -PathType Leaf)) {
        Write-Host "Downloading Inno Setup $innoVersion..."
        Invoke-WebRequest -Uri $innoUrl -OutFile $innoInstaller
    }

    $signature = Get-AuthenticodeSignature -LiteralPath $innoInstaller
    if ($signature.Status -ne 'Valid' -or
        $signature.SignerCertificate.Subject -notmatch 'Pyrsys B\.V\.') {
        throw "Inno Setup signature validation failed. Status: $($signature.Status); signer: $($signature.SignerCertificate.Subject)"
    }

    Write-Host "Installing the verified Inno Setup compiler into $innoRoot..."
    $installerProcess = Start-Process -FilePath $innoInstaller -ArgumentList @(
        '/PORTABLE=1',
        '/VERYSILENT',
        '/SUPPRESSMSGBOXES',
        '/NORESTART',
        '/SP-',
        "/DIR=$innoRoot"
    ) -WindowStyle Hidden -Wait -PassThru
    if ($installerProcess.ExitCode -ne 0) {
        throw "Inno Setup installation failed with exit code $($installerProcess.ExitCode)."
    }
}

if (-not $SkipPackage) {
    $packageArguments = @{}
    if ($SkipOcr) {
        $packageArguments.SkipOcr = $true
    }
    & (Join-Path $PSScriptRoot 'package-windows-runtime.ps1') @packageArguments
}

$packageRoot = Join-Path $repositoryRoot 'dist\FamilyPDF-windows-x64'
if (-not (Test-Path -LiteralPath (Join-Path $packageRoot 'Pdf4QtViewer.exe') -PathType Leaf)) {
    throw "FamilyPDF package was not found: $packageRoot"
}

$compilerArguments = @()
if ($VerificationBuild) {
    $compilerArguments += '/DVerificationBuild'
}
$compilerArguments += (Join-Path $repositoryRoot 'installer\FamilyPDF.iss')

& $iscc $compilerArguments
if ($LASTEXITCODE -ne 0) {
    throw "Inno Setup compiler failed with exit code $LASTEXITCODE."
}

$setup = if ($VerificationBuild) {
    Join-Path $repositoryRoot 'build\FamilyPDF-Verification-Setup-x64.exe'
} else {
    Join-Path $repositoryRoot 'dist\FamilyPDF-Setup-x64.exe'
}
if (-not (Test-Path -LiteralPath $setup -PathType Leaf)) {
    throw "Installer output was not found: $setup"
}

Write-Host "Installer: $setup"
