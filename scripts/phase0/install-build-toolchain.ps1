[CmdletBinding()]
param(
    [string]$ToolsRoot,
    [string]$BootstrapPython
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$QtVersion = '6.9.1'
$QtHost = 'windows'
$QtTarget = 'desktop'
$QtArchitecture = 'win64_msvc2022_64'
$QtInstallDirectoryName = 'msvc2022_64'
$QtModules = @('qtmultimedia', 'qtspeech')
$AqtVersion = '3.3.0'
$VcpkgCommit = '6d9d7df564a1ccdaa994e4ad39ccd4a32360867b'
$VcpkgRepository = 'https://github.com/microsoft/vcpkg.git'
$PyPiIndex = 'https://pypi.org/simple'

$RepositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
. (Join-Path $RepositoryRoot 'scripts\common\Resolve-FamilyPDFToolsRoot.ps1')
$ToolsRoot = Resolve-FamilyPDFToolsRoot -RepositoryRoot $RepositoryRoot `
    -ExplicitRoot $ToolsRoot
$QtRoot = Join-Path $ToolsRoot 'qt'
$QtPrefix = Join-Path $QtRoot "$QtVersion\$QtInstallDirectoryName"
$AqtVenv = Join-Path $ToolsRoot "aqt-$AqtVersion"
$AqtPython = Join-Path $AqtVenv 'Scripts\python.exe'
$VcpkgRoot = Join-Path $ToolsRoot 'vcpkg'
$VcpkgExecutable = Join-Path $VcpkgRoot 'vcpkg.exe'

function Invoke-Native {
    param(
        [Parameter(Mandatory)]
        [string]$FilePath,
        [Parameter(Mandatory)]
        [string[]]$ArgumentList
    )

    & $FilePath @ArgumentList
    if ($LASTEXITCODE -ne 0) {
        throw "Command failed with exit code ${LASTEXITCODE}: $FilePath $($ArgumentList -join ' ')"
    }
}

function Invoke-Git {
    param(
        [Parameter(Mandatory)]
        [string[]]$ArgumentList
    )

    $safeRepository = $RepositoryRoot.Replace('\', '/')
    $safeVcpkg = $VcpkgRoot.Replace('\', '/')
    $gitArguments = @(
        '-c', "safe.directory=$safeRepository",
        '-c', "safe.directory=$safeVcpkg",
        '-c', 'core.excludesFile='
    ) + $ArgumentList
    Invoke-Native -FilePath 'git.exe' -ArgumentList $gitArguments
}

function Get-DirectorySizeBytes {
    param([Parameter(Mandatory)][string]$LiteralPath)

    if (-not (Test-Path -LiteralPath $LiteralPath -PathType Container)) {
        return [int64]0
    }

    return [int64]((Get-ChildItem -LiteralPath $LiteralPath -Recurse -File -Force |
        Measure-Object -Property Length -Sum).Sum)
}

function Get-PythonVersion {
    param([Parameter(Mandatory)][string]$PythonExecutable)

    $versionText = & $PythonExecutable --version
    if ($LASTEXITCODE -ne 0) {
        throw "Python version check failed: $PythonExecutable"
    }
    if ($versionText -notmatch 'Python\s+(\d+\.\d+\.\d+)') {
        throw "Could not parse Python version: $versionText"
    }
    return [version]$Matches[1]
}

New-Item -ItemType Directory -Path $ToolsRoot -Force | Out-Null

if (-not (Test-Path -LiteralPath $AqtPython -PathType Leaf)) {
    if (Test-Path -LiteralPath $AqtVenv) {
        throw "aqt target path exists but is not a complete virtual environment. Remove only this path and rerun: $AqtVenv"
    }

    if ([string]::IsNullOrWhiteSpace($BootstrapPython)) {
        $pythonCommand = Get-Command python.exe -ErrorAction SilentlyContinue | Select-Object -First 1
        if (-not $pythonCommand) {
            throw 'Python was not found. Pass -BootstrapPython with a Python 3.9+ executable. The script will only use it to create the local aqt virtual environment.'
        }
        $BootstrapPython = $pythonCommand.Source
    }

    $BootstrapPython = [System.IO.Path]::GetFullPath($BootstrapPython)
    if (-not (Test-Path -LiteralPath $BootstrapPython -PathType Leaf)) {
        throw "Bootstrap Python does not exist: $BootstrapPython"
    }
    if ((Get-PythonVersion -PythonExecutable $BootstrapPython) -lt [version]'3.9') {
        throw "Python 3.9+ is required to create the aqt environment: $BootstrapPython"
    }

    Write-Host "Creating local aqt virtual environment: $AqtVenv"
    Invoke-Native -FilePath $BootstrapPython -ArgumentList @('-m', 'venv', $AqtVenv)
}

Invoke-Native -FilePath $AqtPython -ArgumentList @('-m', 'ensurepip', '--upgrade')
Invoke-Native -FilePath $AqtPython -ArgumentList @('-m', 'pip', '--version')

$installedAqtPackages = (& $AqtPython -m pip list --format=json | ConvertFrom-Json)
$installedAqtPackage = ($installedAqtPackages |
    Where-Object { $_.name -eq 'aqtinstall' } |
    Select-Object -First 1)
$installedAqtVersion = if ($installedAqtPackage) { $installedAqtPackage.version } else { '' }
if ($installedAqtVersion -ne $AqtVersion) {
    Write-Host "Installing aqtinstall $AqtVersion from $PyPiIndex"
    Invoke-Native -FilePath $AqtPython -ArgumentList @(
        '-m', 'pip', 'install',
        '--disable-pip-version-check',
        '--no-input',
        '--index-url', $PyPiIndex,
        "aqtinstall==$AqtVersion"
    )
}

$qmake = Join-Path $QtPrefix 'bin\qmake.exe'
$qtpaths = Join-Path $QtPrefix 'bin\qtpaths.exe'
$qtIsComplete = (Test-Path -LiteralPath $qmake -PathType Leaf) -and
    (Test-Path -LiteralPath $qtpaths -PathType Leaf)
if ($qtIsComplete) {
    $installedQtVersion = (& $qmake -query QT_VERSION).Trim()
    $qtIsComplete = ($LASTEXITCODE -eq 0 -and $installedQtVersion -eq $QtVersion)
}

if (-not $qtIsComplete) {
    if (Test-Path -LiteralPath $QtPrefix) {
        throw "Qt target path exists but is incomplete or has the wrong version. Remove only this path and rerun: $QtPrefix"
    }

    Write-Host "Installing Qt $QtVersion ($QtArchitecture), modules: $($QtModules -join ', ')"
    $aqtArguments = @(
        '-m', 'aqt', 'install-qt',
        $QtHost, $QtTarget, $QtVersion, $QtArchitecture,
        '-O', $QtRoot,
        '-m'
    ) + $QtModules
    Push-Location $ToolsRoot
    try {
        Invoke-Native -FilePath $AqtPython -ArgumentList $aqtArguments
    }
    finally {
        Pop-Location
    }
}

$requiredQtPackageFiles = @(
    'lib\cmake\Qt6\Qt6Config.cmake',
    'lib\cmake\Qt6Core\Qt6CoreConfig.cmake',
    'lib\cmake\Qt6Gui\Qt6GuiConfig.cmake',
    'lib\cmake\Qt6Widgets\Qt6WidgetsConfig.cmake',
    'lib\cmake\Qt6Svg\Qt6SvgConfig.cmake',
    'lib\cmake\Qt6Xml\Qt6XmlConfig.cmake',
    'lib\cmake\Qt6PrintSupport\Qt6PrintSupportConfig.cmake',
    'lib\cmake\Qt6Multimedia\Qt6MultimediaConfig.cmake',
    'lib\cmake\Qt6TextToSpeech\Qt6TextToSpeechConfig.cmake',
    'lib\cmake\Qt6Concurrent\Qt6ConcurrentConfig.cmake',
    'lib\cmake\Qt6Test\Qt6TestConfig.cmake',
    'lib\cmake\Qt6LinguistTools\Qt6LinguistToolsConfig.cmake'
)
foreach ($relativePackageFile in $requiredQtPackageFiles) {
    $packageFile = Join-Path $QtPrefix $relativePackageFile
    if (-not (Test-Path -LiteralPath $packageFile -PathType Leaf)) {
        throw "Required Qt CMake package file is missing: $packageFile"
    }
}

if (-not (Test-Path -LiteralPath (Join-Path $VcpkgRoot '.git') -PathType Container)) {
    if (Test-Path -LiteralPath $VcpkgRoot) {
        throw "vcpkg target exists but is not a Git checkout. Remove only this path and rerun: $VcpkgRoot"
    }

    Write-Host "Cloning official vcpkg repository: $VcpkgRepository"
    Invoke-Git -ArgumentList @('clone', $VcpkgRepository, $VcpkgRoot)
}

$vcpkgRemote = (Invoke-Git -ArgumentList @('-C', $VcpkgRoot, 'remote', 'get-url', 'origin') | Out-String).Trim()
if ($vcpkgRemote -notin @($VcpkgRepository, 'https://github.com/Microsoft/vcpkg.git')) {
    throw "Unexpected vcpkg origin URL: $vcpkgRemote"
}

$vcpkgChanges = (Invoke-Git -ArgumentList @('-C', $VcpkgRoot, 'status', '--porcelain') | Out-String).Trim()
if ($vcpkgChanges) {
    throw "The vcpkg checkout has local changes. Refusing to overwrite them: $VcpkgRoot"
}

try {
    $currentVcpkgCommit = (Invoke-Git -ArgumentList @('-C', $VcpkgRoot, 'rev-parse', 'HEAD') 2>$null | Out-String).Trim()
}
catch {
    $currentVcpkgCommit = ''
}
$vcpkgCommitChanged = ($LASTEXITCODE -ne 0 -or $currentVcpkgCommit -ne $VcpkgCommit)
if ($vcpkgCommitChanged) {
    Write-Host "Checking out pinned vcpkg commit: $VcpkgCommit"
    Invoke-Git -ArgumentList @('-C', $VcpkgRoot, 'fetch', '--no-tags', 'origin', $VcpkgCommit)
    Invoke-Git -ArgumentList @('-C', $VcpkgRoot, 'checkout', '--detach', $VcpkgCommit)
}

$vcpkgMetadataFile = Join-Path $VcpkgRoot 'scripts\vcpkg-tool-metadata.txt'
if (-not (Test-Path -LiteralPath $vcpkgMetadataFile -PathType Leaf)) {
    throw "Pinned vcpkg tool metadata is missing: $vcpkgMetadataFile"
}
$vcpkgReleaseMatch = Select-String -LiteralPath $vcpkgMetadataFile -Pattern '^VCPKG_TOOL_RELEASE_TAG=(.+)$' |
    Select-Object -First 1
if (-not $vcpkgReleaseMatch) {
    throw "VCPKG_TOOL_RELEASE_TAG is missing from: $vcpkgMetadataFile"
}
$expectedVcpkgRelease = $vcpkgReleaseMatch.Matches[0].Groups[1].Value.Trim()

$vcpkgExecutableMatches = $false
if (Test-Path -LiteralPath $VcpkgExecutable -PathType Leaf) {
    try {
        $existingVcpkgVersionOutput = & $VcpkgExecutable version 2>$null
        $existingVcpkgVersionExitCode = $LASTEXITCODE
        if ($existingVcpkgVersionExitCode -eq 0) {
            $existingVcpkgVersion = ($existingVcpkgVersionOutput | Select-Object -First 1).Trim()
            $vcpkgExecutableMatches = (
                $existingVcpkgVersion -match "version\s+$([regex]::Escape($expectedVcpkgRelease))(?:-|$)"
            )
        }
    }
    catch {
        $vcpkgExecutableMatches = $false
    }
}

if ($vcpkgCommitChanged -or -not $vcpkgExecutableMatches) {
    Write-Host 'Bootstrapping vcpkg with telemetry disabled'
    Invoke-Native -FilePath (Join-Path $VcpkgRoot 'bootstrap-vcpkg.bat') -ArgumentList @('-disableMetrics')
}

$actualQtVersion = (& $qmake -query QT_VERSION).Trim()
if ($LASTEXITCODE -ne 0 -or $actualQtVersion -ne $QtVersion) {
    throw "Qt verification failed. Expected $QtVersion, got '$actualQtVersion'."
}
$qtpathsVersion = (& $qtpaths --qt-version).Trim()
if ($LASTEXITCODE -ne 0 -or $qtpathsVersion -ne $QtVersion) {
    throw "qtpaths verification failed. Expected $QtVersion, got '$qtpathsVersion'."
}
$actualVcpkgCommit = (Invoke-Git -ArgumentList @('-C', $VcpkgRoot, 'rev-parse', 'HEAD') | Out-String).Trim()
if ($actualVcpkgCommit -ne $VcpkgCommit) {
    throw "vcpkg commit verification failed. Expected $VcpkgCommit, got $actualVcpkgCommit."
}
$vcpkgVersionOutput = & $VcpkgExecutable version
$vcpkgVersionExitCode = $LASTEXITCODE
if ($vcpkgVersionExitCode -ne 0) {
    throw 'vcpkg version verification failed.'
}
$vcpkgVersion = ($vcpkgVersionOutput | Select-Object -First 1).Trim()
if ($vcpkgVersion -notmatch "version\s+$([regex]::Escape($expectedVcpkgRelease))(?:-|$)") {
    throw "vcpkg executable does not match pinned tool release $expectedVcpkgRelease`: $vcpkgVersion"
}

$qtSize = Get-DirectorySizeBytes -LiteralPath $QtRoot
$vcpkgSize = Get-DirectorySizeBytes -LiteralPath $VcpkgRoot
$aqtSize = Get-DirectorySizeBytes -LiteralPath $AqtVenv

Write-Host ''
Write-Host 'Phase 0 build toolchain is installed and verified.'
Write-Host "Qt version:        $actualQtVersion"
Write-Host "Qt prefix:         $QtPrefix"
Write-Host "Qt CMake package:  $(Join-Path $QtPrefix 'lib\cmake\Qt6\Qt6Config.cmake')"
Write-Host "Qt size (bytes):   $qtSize"
Write-Host "aqtinstall:        $AqtVersion"
Write-Host "aqt venv:          $AqtVenv"
Write-Host "aqt size (bytes):  $aqtSize"
Write-Host "vcpkg version:     $vcpkgVersion"
Write-Host "vcpkg commit:      $actualVcpkgCommit"
Write-Host "vcpkg root:        $VcpkgRoot"
Write-Host "vcpkg size (bytes): $vcpkgSize"
Write-Host ''
Write-Host 'No global PATH, registry, or system Python changes were made.'
