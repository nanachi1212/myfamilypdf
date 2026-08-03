[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [string]$PythonExecutable
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$repositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$testEnvironment = Join-Path $repositoryRoot 'build\office-toolchain-repair-test'
$buildRoot = [IO.Path]::GetFullPath(
    (Join-Path $repositoryRoot 'build')
).TrimEnd('\') + '\'
$testEnvironment = [IO.Path]::GetFullPath($testEnvironment)
if (-not $testEnvironment.StartsWith(
        $buildRoot,
        [StringComparison]::OrdinalIgnoreCase
    )) {
    throw "Unsafe toolchain repair test path: $testEnvironment"
}

if (-not (Test-Path -LiteralPath $PythonExecutable -PathType Leaf)) {
    throw "Test Python was not found: $PythonExecutable"
}

Remove-Item -LiteralPath $testEnvironment -Recurse -Force `
    -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Path (
    Join-Path $testEnvironment 'Scripts'
) -Force | Out-Null
Copy-Item -LiteralPath (
    Join-Path (Split-Path $repositoryRoot -Parent) `
        'FamilyPDF-tools\office-export-venv\Scripts\python.exe'
) -Destination (Join-Path $testEnvironment 'Scripts\python.exe')
Set-Content -LiteralPath (
    Join-Path $testEnvironment 'pyvenv.cfg'
) -Value @(
    'home = C:\missing-python',
    'include-system-site-packages = false',
    'version = 3.14.6',
    'executable = C:\missing-python\python.exe'
) -Encoding ASCII

try {
    & (Join-Path $repositoryRoot `
        'scripts\office\install-office-export-toolchain.ps1') `
        -VirtualEnvironment $testEnvironment `
        -PythonExecutable $PythonExecutable `
        -SkipDependencyInstall
    if ($LASTEXITCODE -ne 0) {
        throw 'Office toolchain repair command failed.'
    }

    $venvPython = Join-Path $testEnvironment 'Scripts\python.exe'
    & $venvPython --version
    if ($LASTEXITCODE -ne 0) {
        throw 'Repaired Office toolchain Python could not start.'
    }
}
finally {
    Remove-Item -LiteralPath $testEnvironment -Recurse -Force `
        -ErrorAction SilentlyContinue
}
