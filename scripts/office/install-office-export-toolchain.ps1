[CmdletBinding()]
param(
    [string]$VirtualEnvironment = '',
    [string]$PythonExecutable = '',
    [switch]$SkipDependencyInstall
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
$backupEnvironment = $null
$backupConfiguration = $null

function Get-PythonVersion {
    param([Parameter(Mandatory)][string]$LiteralPath)

    if (-not (Test-Path -LiteralPath $LiteralPath -PathType Leaf)) {
        return $null
    }
    try {
        $output = @(& $LiteralPath --version 2>&1)
        $versionMatch = [regex]::Match(
            ($output -join ' '),
            'Python\s+(\d+\.\d+(?:\.\d+)?)'
        )
        if ($LASTEXITCODE -ne 0 -or -not $versionMatch.Success) {
            return $null
        }
        return [Version]$versionMatch.Groups[1].Value
    }
    catch {
        return $null
    }
}

function Resolve-BootstrapPython {
    param([Version]$PreferredVersion)

    $candidates = @()
    if (-not [string]::IsNullOrWhiteSpace($PythonExecutable)) {
        $candidates += [IO.Path]::GetFullPath($PythonExecutable)
    }
    else {
        $pythonCommand = Get-Command python.exe -ErrorAction SilentlyContinue
        if ($pythonCommand) {
            $candidates += $pythonCommand.Source
        }
    }

    $toolsRoot = Join-Path (
        Split-Path $repositoryRoot -Parent
    ) 'FamilyPDF-tools'
    $aqtEnvironments = @(
        Get-ChildItem -LiteralPath $toolsRoot -Directory `
            -Filter 'aqt-*' -ErrorAction SilentlyContinue |
            Sort-Object Name -Descending
    )
    foreach ($environment in $aqtEnvironments) {
        $candidates += Join-Path $environment.FullName 'Scripts\python.exe'
    }

    $vcpkgPythonRoot = Join-Path $toolsRoot `
        'vcpkg\downloads\tools\python'
    $candidates += @(
        Get-ChildItem -LiteralPath $vcpkgPythonRoot -Recurse `
            -Filter 'python.exe' -File -ErrorAction SilentlyContinue |
            Select-Object -ExpandProperty FullName
    )

    $validCandidates = @()
    foreach ($candidate in $candidates | Select-Object -Unique) {
        $candidateVersion = Get-PythonVersion -LiteralPath $candidate
        if ($null -ne $candidateVersion -and $candidateVersion -ge [Version]'3.10') {
            $validCandidates += [pscustomobject]@{
                Path = $candidate
                Version = $candidateVersion
            }
        }
    }
    if ($null -ne $PreferredVersion) {
        $matchingCandidate = $validCandidates | Where-Object {
            $_.Version.Major -eq $PreferredVersion.Major -and
            $_.Version.Minor -eq $PreferredVersion.Minor
        } | Select-Object -First 1
        if ($matchingCandidate) {
            return $matchingCandidate.Path
        }
    }
    if ($validCandidates.Count -gt 0) {
        return $validCandidates[0].Path
    }
    throw (
        'Python 3.10 or newer was not found. Run ' +
        'scripts\phase0\install-build-toolchain.ps1 first.'
    )
}

$version = Get-PythonVersion -LiteralPath $venvPython
if ($null -eq $version) {
    $preferredVersion = $null
    $venvMarker = Join-Path $VirtualEnvironment 'pyvenv.cfg'
    if (Test-Path -LiteralPath $VirtualEnvironment -PathType Container) {
        if (-not (Test-Path -LiteralPath $venvMarker -PathType Leaf)) {
            throw (
                'Refusing to replace a directory that is not a Python ' +
                "virtual environment: $VirtualEnvironment"
            )
        }
        $configuration = Get-Content -LiteralPath $venvMarker `
            -Raw -Encoding UTF8
        $configuredVersion = [regex]::Match(
            $configuration,
            '(?m)^version\s*=\s*(\d+\.\d+(?:\.\d+)?)\s*$'
        )
        if ($configuredVersion.Success) {
            $preferredVersion = [Version]$configuredVersion.Groups[1].Value
        }
    }

    $PythonExecutable = Resolve-BootstrapPython `
        -PreferredVersion $preferredVersion
    $bootstrapVersion = Get-PythonVersion -LiteralPath $PythonExecutable
    $canRepairConfiguration = (
        $null -ne $preferredVersion -and
        $bootstrapVersion.Major -eq $preferredVersion.Major -and
        $bootstrapVersion.Minor -eq $preferredVersion.Minor
    )

    if ($canRepairConfiguration) {
        $backupConfiguration = (
            $venvMarker + '.repair-' + [Guid]::NewGuid().ToString('N')
        )
        Copy-Item -LiteralPath $venvMarker -Destination $backupConfiguration
        @(
            "home = $(Split-Path $PythonExecutable -Parent)",
            'include-system-site-packages = false',
            "version = $bootstrapVersion",
            "executable = $PythonExecutable",
            'command = FamilyPDF automatic environment repair'
        ) | Set-Content -LiteralPath $venvMarker -Encoding UTF8
        $version = Get-PythonVersion -LiteralPath $venvPython
        if ($null -eq $version) {
            Copy-Item -LiteralPath $backupConfiguration `
                -Destination $venvMarker -Force
            Remove-Item -LiteralPath $backupConfiguration -Force
            $backupConfiguration = $null
            throw "Repaired Python environment is not executable: $venvPython"
        }
    }
    else {
        if (Test-Path -LiteralPath $VirtualEnvironment -PathType Container) {
        $backupEnvironment = (
            $VirtualEnvironment + '.repair-' + [Guid]::NewGuid().ToString('N')
        )
        Move-Item -LiteralPath $VirtualEnvironment `
            -Destination $backupEnvironment
        }

        try {
            & $PythonExecutable -m venv $VirtualEnvironment
            if ($LASTEXITCODE -ne 0) {
                throw "Could not create Python environment: $VirtualEnvironment"
            }
            $version = Get-PythonVersion -LiteralPath $venvPython
            if ($null -eq $version) {
                throw "Recreated Python environment is not executable: $venvPython"
            }
        }
        catch {
            Remove-Item -LiteralPath $VirtualEnvironment -Recurse -Force `
                -ErrorAction SilentlyContinue
            if ($backupEnvironment -and (
                    Test-Path -LiteralPath $backupEnvironment -PathType Container
                )) {
                Move-Item -LiteralPath $backupEnvironment `
                    -Destination $VirtualEnvironment
            }
            throw
        }
    }
}

if ($version -lt [Version]'3.10') {
    throw "Python 3.10 or newer is required; found $version."
}

if (-not $SkipDependencyInstall) {
    try {
        & $venvPython -m pip install --require-hashes -r (
            Join-Path $officeRoot 'requirements.lock'
        )
        if ($LASTEXITCODE -ne 0) {
            throw 'Office export dependency installation failed.'
        }
    }
    catch {
        if ($backupConfiguration -and (
                Test-Path -LiteralPath $backupConfiguration -PathType Leaf
            )) {
            Copy-Item -LiteralPath $backupConfiguration `
                -Destination $venvMarker -Force
        }
        if ($backupEnvironment -and (
                Test-Path -LiteralPath $backupEnvironment -PathType Container
            )) {
            Remove-Item -LiteralPath $VirtualEnvironment -Recurse -Force `
                -ErrorAction SilentlyContinue
            Move-Item -LiteralPath $backupEnvironment `
                -Destination $VirtualEnvironment
        }
        throw
    }
}
if ($backupConfiguration -and (
        Test-Path -LiteralPath $backupConfiguration -PathType Leaf
    )) {
    Remove-Item -LiteralPath $backupConfiguration -Force
}
if ($backupEnvironment -and (
        Test-Path -LiteralPath $backupEnvironment -PathType Container
    )) {
    Remove-Item -LiteralPath $backupEnvironment -Recurse -Force
}

Write-Host "Office export Python: $venvPython"
Write-Host "Python version: $version"
