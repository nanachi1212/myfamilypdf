[CmdletBinding()]
param(
    [switch]$SkipBuild
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$repositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$buildRoot = Join-Path $repositoryRoot 'build'
$setup = Join-Path $buildRoot 'FamilyPDF-Full-Verification-Setup-x64.exe'
$testRoot = Join-Path $buildRoot 'full-installer-smoke'
$fullRoot = Join-Path $testRoot 'full'
$coreRoot = Join-Path $testRoot 'core-only'

if (-not $SkipBuild) {
    & (Join-Path $repositoryRoot 'scripts\phase0\build-full-installer.ps1') `
        -SkipBasePackage `
        -SkipOcrPackage `
        -VerificationBuild
}
if (-not (Test-Path -LiteralPath $setup -PathType Leaf)) {
    throw "Full verification installer was not found: $setup"
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
New-Item -ItemType Directory -Path $resolvedTestRoot | Out-Null

function Install-Isolated {
    param(
        [Parameter(Mandatory)][string]$Target,
        [Parameter(Mandatory)][string]$Components
    )

    $process = Start-Process -FilePath $setup -ArgumentList @(
        '/VERYSILENT',
        '/SUPPRESSMSGBOXES',
        '/NORESTART',
        '/SP-',
        "/DIR=$Target",
        "/COMPONENTS=$Components"
    ) -WindowStyle Hidden -Wait -PassThru
    if ($process.ExitCode -ne 0) {
        throw "Full installer failed for components '$Components' with exit code $($process.ExitCode)."
    }
}

function Assert-PayloadMatches {
    param(
        [Parameter(Mandatory)][string]$Source,
        [Parameter(Mandatory)][string]$Target,
        [Parameter(Mandatory)][string]$Label,
        [string[]]$Exclude = @()
    )

    if (-not (Test-Path -LiteralPath $Source -PathType Container)) {
        throw "Payload source was not found: $Source"
    }
    $sourceRoot = [IO.Path]::GetFullPath($Source).TrimEnd('\')
    $sourcePrefix = $sourceRoot + '\'
    $files = @(Get-ChildItem -LiteralPath $sourceRoot -File -Recurse)
    foreach ($sourceFile in $files) {
        if (-not $sourceFile.FullName.StartsWith(
                $sourcePrefix,
                [StringComparison]::OrdinalIgnoreCase
            )) {
            throw "Payload file escaped its source root: $($sourceFile.FullName)"
        }
        $relativePath = $sourceFile.FullName.Substring(
            $sourcePrefix.Length
        )
        if ($Exclude -contains $relativePath) {
            continue
        }
        $installedFile = Join-Path $Target $relativePath
        if (-not (Test-Path -LiteralPath $installedFile -PathType Leaf)) {
            throw "$Label is missing packaged file: $relativePath"
        }
        $sourceHash = (
            Get-FileHash -Algorithm SHA256 -LiteralPath $sourceFile.FullName
        ).Hash
        $installedHash = (
            Get-FileHash -Algorithm SHA256 -LiteralPath $installedFile
        ).Hash
        if ($sourceHash -ne $installedHash) {
            throw "$Label payload differs from package: $relativePath"
        }
    }
    return @(
        $files | Where-Object {
            $relative = $_.FullName.Substring($sourcePrefix.Length)
            $Exclude -notcontains $relative
        }
    ).Count
}

Install-Isolated -Target $fullRoot -Components 'core,ocr'
Install-Isolated -Target $coreRoot -Components 'core'

$fullRequired = @(
    'Pdf4QtViewer.exe',
    'Pdf4QtEditor.exe',
    'Pdf4QtPageMaster.exe',
    'Pdf4QtDiff.exe',
    'PdfTool.exe',
    'FamilyPDF-OCR.cmd',
    'FamilyPDF-OCR.ps1',
    'FamilyPDF-OCR-Plugin.json',
    'ocr\tesseract.exe',
    'ocr\tessdata\eng.traineddata',
    'ocr\tessdata\chi_tra.traineddata',
    'ocr\tessdata\chi_sim.traineddata',
    'ocr\tessdata\chi_tra_vert.traineddata',
    'ocr\tessdata\chi_sim_vert.traineddata'
)
foreach ($relativePath in $fullRequired) {
    $installedFile = Join-Path $fullRoot $relativePath
    if (-not (Test-Path -LiteralPath $installedFile -PathType Leaf)) {
        throw "Full installation is missing: $installedFile"
    }
}

foreach ($relativePath in @(
    'Pdf4QtViewer.exe',
    'Pdf4QtEditor.exe',
    'Pdf4QtPageMaster.exe',
    'Pdf4QtDiff.exe',
    'PdfTool.exe'
)) {
    $installedFile = Join-Path $coreRoot $relativePath
    if (-not (Test-Path -LiteralPath $installedFile -PathType Leaf)) {
        throw "Core-only installation is missing: $installedFile"
    }
}
foreach ($relativePath in @(
    'FamilyPDF-OCR.ps1',
    'ocr\tesseract.exe',
    'ocr\tessdata\chi_tra.traineddata'
)) {
    $unexpectedFile = Join-Path $coreRoot $relativePath
    if (Test-Path -LiteralPath $unexpectedFile) {
        throw "Core-only installation unexpectedly contains OCR: $unexpectedFile"
    }
}

$corePackage = Join-Path $repositoryRoot 'dist\FamilyPDF-windows-x64'
$ocrPackage = Join-Path $repositoryRoot `
    'dist\FamilyPDF-OCR-Plugin-windows-x64'
$fullCoreFileCount = Assert-PayloadMatches `
    -Source $corePackage -Target $fullRoot -Label 'Full installation core' `
    -Exclude @('portable.mode')
$coreOnlyFileCount = Assert-PayloadMatches `
    -Source $corePackage -Target $coreRoot -Label 'Core-only installation' `
    -Exclude @('portable.mode')
$fullOcrFileCount = Assert-PayloadMatches `
    -Source $ocrPackage -Target $fullRoot -Label 'Full installation OCR'

& (Join-Path $repositoryRoot 'scripts\ocr\Test-FamilyPDF-OCR-Horizontal.ps1') `
    -PdfToolPath (Join-Path $fullRoot 'PdfTool.exe') `
    -TesseractPath (Join-Path $fullRoot 'ocr\tesseract.exe') `
    -TessdataPath (Join-Path $fullRoot 'ocr\tessdata') `
    -OcrScriptPath (Join-Path $fullRoot 'FamilyPDF-OCR.ps1')
if ($LASTEXITCODE -ne 0) {
    throw 'Installed full-package OCR verification failed.'
}

& (Join-Path $repositoryRoot 'scripts\qa\smoke-pdf-diff.ps1') `
    -PackageDirectory $fullRoot `
    -OutputDirectory (Join-Path $testRoot 'pdf-diff')
if ($LASTEXITCODE -ne 0) {
    throw 'Installed full-package PDF comparison verification failed.'
}

$viewer = Start-Process -FilePath (Join-Path $fullRoot 'Pdf4QtViewer.exe') -PassThru
try {
    Start-Sleep -Seconds 5
    $viewer.Refresh()
    if ($viewer.HasExited -or -not $viewer.Responding) {
        throw 'Viewer from the full installation did not remain responsive.'
    }
}
finally {
    if (-not $viewer.HasExited) {
        Stop-Process -Id $viewer.Id -Force
        $viewer.WaitForExit(5000) | Out-Null
    }
}

$installerFile = Get-Item -LiteralPath $setup
$installerHash = (
    Get-FileHash -Algorithm SHA256 -LiteralPath $setup
).Hash
$summary = [ordered]@{
    recorded_at = [DateTimeOffset]::Now.ToString('o')
    installer = [ordered]@{
        path = $setup
        bytes = $installerFile.Length
        sha256 = $installerHash
        temporary_verification_build = $true
        removed_after_verification = $true
    }
    full_install = [ordered]@{
        path = $fullRoot
        base_application = $true
        ocr = $true
        languages = @('eng', 'chi_tra', 'chi_sim', 'chi_tra_vert', 'chi_sim_vert')
        horizontal_ocr = $true
        viewer_responding = $true
        core_payload_files_verified = $fullCoreFileCount
        ocr_payload_files_verified = $fullOcrFileCount
    }
    core_only_install = [ordered]@{
        path = $coreRoot
        base_application = $true
        ocr = $false
        core_payload_files_verified = $coreOnlyFileCount
    }
}
$summaryPath = Join-Path $testRoot 'summary.json'
$summary | ConvertTo-Json -Depth 6 |
    Set-Content -LiteralPath $summaryPath -Encoding UTF8

Remove-Item -LiteralPath $setup -Force
Write-Host "Full installer smoke passed: $summaryPath"
