[CmdletBinding()]
param(
    [ValidateSet('All', 'Configure', 'Build', 'Test')]
    [string]$Stage = 'All',
    [string]$BuildDirectory
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$RepositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
if ([string]::IsNullOrWhiteSpace($BuildDirectory)) {
    $BuildDirectory = Join-Path $RepositoryRoot 'build\phase0-upstream-release'
}
$BuildDirectory = [IO.Path]::GetFullPath($BuildDirectory)

$QtPrefix = 'E:\CodexProject\FamilyPDF-tools\qt\6.9.1\msvc2022_64'
$VcpkgRoot = 'E:\CodexProject\FamilyPDF-tools\vcpkg'
$VcpkgToolchain = Join-Path $VcpkgRoot 'scripts\buildsystems\vcpkg.cmake'
$VsWhere = 'C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe'
$Targets = @(
    'Pdf4QtViewer',
    'Pdf4QtEditor',
    'Pdf4QtPageMaster',
    'UnitTests',
    'UnitTestsImageOptimizer',
    'UnitTestsFontEncoding'
)

function Assert-File {
    param([Parameter(Mandatory)][string]$LiteralPath)
    if (-not (Test-Path -LiteralPath $LiteralPath -PathType Leaf)) {
        throw "Required file was not found: $LiteralPath"
    }
}

function Import-MsvcEnvironment {
    Assert-File -LiteralPath $VsWhere
    $installationPath = (& $VsWhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath |
        Select-Object -First 1)
    if ([string]::IsNullOrWhiteSpace($installationPath)) {
        throw 'Visual Studio Build Tools with the x64 C++ toolchain was not found.'
    }

    $vsDevCmd = Join-Path $installationPath 'Common7\Tools\VsDevCmd.bat'
    Assert-File -LiteralPath $vsDevCmd
    $environmentLines = & $env:COMSPEC /d /s /c "`"$vsDevCmd`" -no_logo -arch=x64 -host_arch=x64 && set"
    if ($LASTEXITCODE -ne 0) {
        throw "VsDevCmd failed with exit code $LASTEXITCODE."
    }
    foreach ($line in $environmentLines) {
        if ($line -match '^([^=][^=]*)=(.*)$') {
            [Environment]::SetEnvironmentVariable($Matches[1], $Matches[2], 'Process')
        }
    }
    if ($env:VSCMD_ARG_TGT_ARCH -ne 'x64') {
        throw "VsDevCmd did not establish an x64 environment: VSCMD_ARG_TGT_ARCH=$env:VSCMD_ARG_TGT_ARCH"
    }

    return $installationPath
}

function Invoke-LoggedNative {
    param(
        [Parameter(Mandatory)][string]$FilePath,
        [Parameter(Mandatory)][string[]]$ArgumentList,
        [Parameter(Mandatory)][string]$LogPath
    )

    $displayCommand = "$FilePath $($ArgumentList -join ' ')"
    Write-Host "COMMAND: $displayCommand"
    "COMMAND: $displayCommand" | Set-Content -LiteralPath $LogPath -Encoding UTF8

    $previousErrorActionPreference = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try {
        & $FilePath @ArgumentList 2>&1 |
            Tee-Object -FilePath $LogPath -Append
        $exitCode = $LASTEXITCODE
    }
    finally {
        $ErrorActionPreference = $previousErrorActionPreference
    }
    if ($exitCode -ne 0) {
        throw "Command failed with exit code ${exitCode}. See $LogPath"
    }
}

function Get-DirectorySizeBytes {
    param([Parameter(Mandatory)][string]$LiteralPath)
    return [int64]((Get-ChildItem -LiteralPath $LiteralPath -Recurse -File -Force |
        Measure-Object -Property Length -Sum).Sum)
}

Assert-File -LiteralPath (Join-Path $QtPrefix 'lib\cmake\Qt6\Qt6Config.cmake')
Assert-File -LiteralPath $VcpkgToolchain
$visualStudioRoot = Import-MsvcEnvironment
$Cmake = Join-Path $visualStudioRoot 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
$Ninja = Join-Path $visualStudioRoot 'Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe'
Assert-File -LiteralPath $Cmake
Assert-File -LiteralPath $Ninja

New-Item -ItemType Directory -Path $BuildDirectory -Force | Out-Null
$metricsPath = Join-Path $BuildDirectory 'phase0-build-metrics.json'
$metrics = if (Test-Path -LiteralPath $metricsPath) {
    Get-Content -LiteralPath $metricsPath -Raw -Encoding UTF8 | ConvertFrom-Json
}
else {
    [pscustomobject]@{}
}

if ($Stage -in @('All', 'Configure')) {
    $stopwatch = [Diagnostics.Stopwatch]::StartNew()
    $configureArguments = @(
        '-S', $RepositoryRoot,
        '-B', $BuildDirectory,
        '-G', 'Ninja',
        "-DCMAKE_MAKE_PROGRAM=$Ninja",
        '-DCMAKE_BUILD_TYPE=Release',
        '-DCMAKE_VCPKG_BUILD_TYPE=Release',
        '-DVCPKG_TARGET_TRIPLET=x64-windows',
        "-DCMAKE_TOOLCHAIN_FILE=$VcpkgToolchain",
        "-DCMAKE_PREFIX_PATH=$QtPrefix",
        "-DPDF4QT_QT_ROOT=$QtPrefix",
        '-DPDF4QT_BUILD_TESTS=ON',
        '-DPDF4QT_INSTALL_PREPARE_WIX_INSTALLER=OFF',
        '-DPDF4QT_INSTALL_MSVC_REDISTRIBUTABLE=OFF',
        '-DPDF4QT_INSTALL_DEPENDENCIES=OFF',
        '-DPDF4QT_INSTALL_QT_DEPENDENCIES=OFF'
    )
    $configureLog = Join-Path $BuildDirectory 'configure.log'
    Invoke-LoggedNative -FilePath $Cmake -ArgumentList $configureArguments -LogPath $configureLog
    $stopwatch.Stop()
    $metrics | Add-Member -NotePropertyName configure_seconds -NotePropertyValue ([math]::Round($stopwatch.Elapsed.TotalSeconds, 3)) -Force
}

if ($Stage -in @('All', 'Build')) {
    Assert-File -LiteralPath (Join-Path $BuildDirectory 'CMakeCache.txt')
    $stopwatch = [Diagnostics.Stopwatch]::StartNew()
    $buildArguments = @(
        '--build', $BuildDirectory,
        '--target'
    ) + $Targets + @('--parallel')
    $buildLog = Join-Path $BuildDirectory 'build.log'
    Invoke-LoggedNative -FilePath $Cmake -ArgumentList $buildArguments -LogPath $buildLog
    $stopwatch.Stop()
    $metrics | Add-Member -NotePropertyName build_seconds -NotePropertyValue ([math]::Round($stopwatch.Elapsed.TotalSeconds, 3)) -Force
}

if ($Stage -in @('All', 'Test')) {
    Assert-File -LiteralPath (Join-Path $BuildDirectory 'CTestTestfile.cmake')
    $stopwatch = [Diagnostics.Stopwatch]::StartNew()
    $testArguments = @(
        '--build', $BuildDirectory,
        '--target', 'test'
    )
    $testLog = Join-Path $BuildDirectory 'ctest.log'
    Invoke-LoggedNative -FilePath $Cmake -ArgumentList $testArguments -LogPath $testLog
    $stopwatch.Stop()
    $metrics | Add-Member -NotePropertyName test_seconds -NotePropertyValue ([math]::Round($stopwatch.Elapsed.TotalSeconds, 3)) -Force
}

$metrics | Add-Member -NotePropertyName build_directory -NotePropertyValue $BuildDirectory -Force
$metrics | Add-Member -NotePropertyName build_size_bytes -NotePropertyValue (Get-DirectorySizeBytes -LiteralPath $BuildDirectory) -Force
$metrics | Add-Member -NotePropertyName recorded_at -NotePropertyValue ([DateTimeOffset]::Now.ToString('o')) -Force
$metrics | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath $metricsPath -Encoding UTF8

Write-Host ''
Write-Host "Stage completed: $Stage"
Write-Host "Build directory: $BuildDirectory"
Write-Host "Metrics: $metricsPath"
