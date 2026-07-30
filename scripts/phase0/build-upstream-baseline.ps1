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
    'PdfTool',
    'Pdf4QtViewer',
    'Pdf4QtEditor',
    'Pdf4QtPageMaster',
    'EditorPlugin',
    'RedactPlugin',
    'SignaturePlugin',
    'FormPlugin',
    'UnitTests',
    'UnitTestsImageOptimizer',
    'UnitTestsFontEncoding',
    'UnitTestsBookmarks',
    'UnitTestsForms',
    'release_translations'
)

$toolchainBootstrap = Join-Path $PSScriptRoot 'install-build-toolchain.ps1'
$requiredToolchainFiles = @(
    (Join-Path $QtPrefix 'lib\cmake\Qt6\Qt6Config.cmake'),
    $VcpkgToolchain
)
if ($requiredToolchainFiles |
        Where-Object { -not (Test-Path -LiteralPath $_ -PathType Leaf) }) {
    if (-not (Test-Path -LiteralPath $toolchainBootstrap -PathType Leaf)) {
        throw "Toolchain files are missing and the bootstrap script was not found: $toolchainBootstrap"
    }
    Write-Host 'Required Qt/vcpkg files are missing. Running the verified local toolchain bootstrap...'
    & $toolchainBootstrap
    if ($LASTEXITCODE -ne 0) {
        throw "Toolchain bootstrap failed with exit code $LASTEXITCODE."
    }
}

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
            $variableName = $Matches[1]
            if ($variableName -ieq 'Path') {
                # Windows environment variables are case-insensitive, but the
                # process block can still contain both PATH and Path. .NET's
                # Start-Process converts that block to a case-insensitive
                # dictionary and fails when both spellings are present.
                foreach ($existingName in [Environment]::GetEnvironmentVariables('Process').Keys) {
                    if ([string]$existingName -ieq 'Path') {
                        [Environment]::SetEnvironmentVariable([string]$existingName, $null, 'Process')
                    }
                }
                [Environment]::SetEnvironmentVariable('Path', $Matches[2], 'Process')
            }
            else {
                [Environment]::SetEnvironmentVariable($variableName, $Matches[2], 'Process')
            }
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

    $stdoutPath = "$LogPath.stdout"
    $stderrPath = "$LogPath.stderr"
    Remove-Item -LiteralPath $stdoutPath, $stderrPath -Force -ErrorAction SilentlyContinue
    $startArguments = @($ArgumentList | ForEach-Object {
        '"' + $_.Replace('"', '\"') + '"'
    })
    $process = Start-Process -FilePath $FilePath -ArgumentList $startArguments -NoNewWindow -Wait -PassThru `
        -RedirectStandardOutput $stdoutPath -RedirectStandardError $stderrPath
    $exitCode = $process.ExitCode
    $output = @(
        if (Test-Path -LiteralPath $stdoutPath) { Get-Content -LiteralPath $stdoutPath }
        if (Test-Path -LiteralPath $stderrPath) { Get-Content -LiteralPath $stderrPath }
    )
    $output | Set-Content -LiteralPath $LogPath -Encoding UTF8
    $output | Write-Host
    Remove-Item -LiteralPath $stdoutPath, $stderrPath -Force -ErrorAction SilentlyContinue
    if ($exitCode -ne 0) {
        throw "Command failed with exit code ${exitCode}. See $LogPath"
    }
}

function Get-DirectorySizeBytes {
    param([Parameter(Mandatory)][string]$LiteralPath)
    return [int64]((Get-ChildItem -LiteralPath $LiteralPath -Recurse -File -Force |
        Measure-Object -Property Length -Sum).Sum)
}

function Prepare-TestRuntime {
    $runtimeDirectory = Join-Path $BuildDirectory 'usr\bin'
    $windeployqt = Join-Path $QtPrefix 'bin\windeployqt.exe'
    Assert-File -LiteralPath $windeployqt
    New-Item -ItemType Directory -Path $runtimeDirectory -Force | Out-Null

    foreach ($target in $Targets | Where-Object { $_ -like 'UnitTests*' }) {
        $executable = Join-Path $runtimeDirectory "$target.exe"
        Assert-File -LiteralPath $executable
        & $windeployqt --release --no-translations --no-system-d3d-compiler --no-opengl-sw $executable 2>&1 |
            Set-Content -LiteralPath (Join-Path $BuildDirectory "windeployqt-$target.log") -Encoding UTF8
        if ($LASTEXITCODE -ne 0) {
            $qtCoreRuntime = Join-Path $runtimeDirectory 'Qt6Core.dll'
            if (Test-Path -LiteralPath $qtCoreRuntime -PathType Leaf) {
                Write-Warning "windeployqt failed for $target with exit code $LASTEXITCODE; using the already deployed Qt runtime."
            }
            else {
                throw "windeployqt failed for $target with exit code $LASTEXITCODE and Qt6Core.dll is missing."
            }
        }
    }

    $vcpkgBin = Join-Path $BuildDirectory 'vcpkg_installed\x64-windows\bin'
    if (Test-Path -LiteralPath $vcpkgBin -PathType Container) {
        Get-ChildItem -LiteralPath $vcpkgBin -Filter '*.dll' -File | Copy-Item -Destination $runtimeDirectory -Force
    }
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
    Prepare-TestRuntime
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
$runtimeOutputDirectory = Join-Path $BuildDirectory 'usr'
$runtimeOutputSize = if (Test-Path -LiteralPath $runtimeOutputDirectory -PathType Container) {
    Get-DirectorySizeBytes -LiteralPath $runtimeOutputDirectory
}
else {
    0
}
$metrics | Add-Member -NotePropertyName runtime_output_size_bytes -NotePropertyValue $runtimeOutputSize -Force
$metrics | Add-Member -NotePropertyName recorded_at -NotePropertyValue ([DateTimeOffset]::Now.ToString('o')) -Force
$metrics | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath $metricsPath -Encoding UTF8

Write-Host ''
Write-Host "Stage completed: $Stage"
Write-Host "Build directory: $BuildDirectory"
Write-Host "Metrics: $metricsPath"
