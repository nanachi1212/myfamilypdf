[CmdletBinding()]
param(
    [string]$BuildRoot = '',
    [ValidateRange(1, 20)]
    [int]$Keep = 1,
    [string]$CurrentResult = ''
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$repositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
if ([string]::IsNullOrWhiteSpace($BuildRoot)) {
    $BuildRoot = Join-Path $repositoryRoot 'build'
}
$BuildRoot = [IO.Path]::GetFullPath($BuildRoot).TrimEnd('\')
if (-not (Test-Path -LiteralPath $BuildRoot -PathType Container)) {
    throw "Build root was not found: $BuildRoot"
}

$resultNamePattern = '^final-regression-\d{8}-\d{6}$'
$currentPath = $null
if (-not [string]::IsNullOrWhiteSpace($CurrentResult)) {
    $currentPath = [IO.Path]::GetFullPath($CurrentResult).TrimEnd('\')
    if (
        -not [IO.Path]::GetDirectoryName($currentPath).Equals(
            $BuildRoot,
            [StringComparison]::OrdinalIgnoreCase
        ) -or
        [IO.Path]::GetFileName($currentPath) -notmatch $resultNamePattern
    ) {
        throw "Current result is outside the managed result family: $currentPath"
    }
    if (-not (Test-Path -LiteralPath $currentPath -PathType Container)) {
        throw "Current result was not found: $currentPath"
    }
}

$results = @(
    Get-ChildItem -LiteralPath $BuildRoot -Directory -Force |
        Where-Object { $_.Name -match $resultNamePattern } |
        Sort-Object Name -Descending
)
$preserved = [Collections.Generic.HashSet[string]]::new(
    [StringComparer]::OrdinalIgnoreCase
)
if ($null -ne $currentPath) {
    $preserved.Add($currentPath) | Out-Null
}
foreach ($result in $results) {
    if ($preserved.Count -ge $Keep) {
        break
    }
    $preserved.Add($result.FullName.TrimEnd('\')) | Out-Null
}

$removedCount = 0
[long]$removedBytes = 0
foreach ($result in $results) {
    $resultPath = [IO.Path]::GetFullPath($result.FullName).TrimEnd('\')
    if ($preserved.Contains($resultPath)) {
        continue
    }
    if (-not [IO.Path]::GetDirectoryName($resultPath).Equals(
            $BuildRoot,
            [StringComparison]::OrdinalIgnoreCase
        )) {
        throw "Refusing to remove a result outside the build root: $resultPath"
    }
    if (($result.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
        throw "Refusing to remove a reparse point: $resultPath"
    }

    $size = (
        Get-ChildItem -LiteralPath $resultPath -Recurse -File -Force |
            Measure-Object Length -Sum
    ).Sum
    if ($null -ne $size) {
        $removedBytes += [long]$size
    }
    Remove-Item -LiteralPath $resultPath -Recurse -Force
    $removedCount++
}

Write-Host (
    'Final regression retention: kept {0}, removed {1} ({2} bytes).' -f
    $preserved.Count,
    $removedCount,
    $removedBytes
)
