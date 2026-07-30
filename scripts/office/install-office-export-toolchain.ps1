[CmdletBinding()]
param(
    [string]$VirtualEnvironment = '',
    [string]$PythonExecutable = ''
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$repositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$officeRoot = Join-Path $repositoryRoot 'office-export'
if ([string]::IsNullOrWhiteSpace($VirtualEnvironment)) {
    $VirtualEnvironment = Join-Path (
        Split-Path $repositoryRoot -Parent
    ) 'FamilyPDF-tools\office-export-venv'
}
$VirtualEnvironment = [IO.Path]::GetFullPath($VirtualEnvironment)
$venvPython = Join-Path $VirtualEnvironment 'Scripts\python.exe'

if (-not (Test-Path -LiteralPath $venvPython -PathType Leaf)) {
    if ([string]::IsNullOrWhiteSpace($PythonExecutable)) {
        $pythonCommand = Get-Command python.exe -ErrorAction SilentlyContinue
        if (-not $pythonCommand) {
            throw 'Python 3.10 or newer was not found on PATH.'
        }
        $PythonExecutable = $pythonCommand.Source
    }
    & $PythonExecutable -m venv $VirtualEnvironment
    if ($LASTEXITCODE -ne 0) {
        throw "Could not create Python environment: $VirtualEnvironment"
    }
}

$versionText = & $venvPython -c 'import sys; print(f"{sys.version_info.major}.{sys.version_info.minor}")'
if ($LASTEXITCODE -ne 0) {
    throw 'Could not determine the Python version.'
}
$versionParts = $versionText.Trim().Split('.')
if (
    [int]$versionParts[0] -lt 3 -or
    ([int]$versionParts[0] -eq 3 -and [int]$versionParts[1] -lt 10)
) {
    throw "Python 3.10 or newer is required; found $versionText."
}

& $venvPython -m pip install --require-hashes -r (
    Join-Path $officeRoot 'requirements.lock'
)
if ($LASTEXITCODE -ne 0) {
    throw 'Office export dependency installation failed.'
}

Write-Host "Office export Python: $venvPython"
Write-Host "Python version: $versionText"
