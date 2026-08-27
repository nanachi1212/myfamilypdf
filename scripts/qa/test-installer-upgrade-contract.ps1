[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$repositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$productVersionText = (Get-Content -LiteralPath (Join-Path $repositoryRoot 'VERSION') -Raw -Encoding UTF8).Trim()
$productVersion = [version]$productVersionText
$productionAppId = '3EE743F2-F10D-4D69-A4C3-01834462FBA6'
$installerPaths = @(
    'installer\FamilyPDF.iss',
    'installer\FamilyPDF-Full.iss'
)

foreach ($relativePath in $installerPaths) {
    $content = Get-Content -LiteralPath (Join-Path $repositoryRoot $relativePath) -Raw -Encoding UTF8
    if ($content -notmatch "AppId=\{\{$([regex]::Escape($productionAppId))\}") {
        throw "$relativePath does not retain the production upgrade AppId $productionAppId."
    }
    foreach ($directive in @(
        'UsePreviousAppDir=yes',
        'UsePreviousGroup=yes',
        'UsePreviousLanguage=yes',
        'UsePreviousTasks=yes'
    )) {
        if ($content -notmatch "(?m)^$([regex]::Escape($directive))\s*$") {
            throw "$relativePath does not declare '$directive'."
        }
    }
    if ($content -match '(?im)^\s*(UninstallDelete|InstallDelete).*\{(userappdata|localappdata|appdata)\}') {
        throw "$relativePath deletes user data during install or uninstall."
    }
}

$installedVersions = foreach ($root in @(
    'HKCU:\Software\Microsoft\Windows\CurrentVersion\Uninstall\*',
    'HKLM:\Software\Microsoft\Windows\CurrentVersion\Uninstall\*',
    'HKLM:\Software\WOW6432Node\Microsoft\Windows\CurrentVersion\Uninstall\*'
)) {
    Get-ItemProperty -Path $root -ErrorAction SilentlyContinue |
        Where-Object { $_.PSChildName -eq "{$productionAppId}_is1" } |
        ForEach-Object { [version]$_.DisplayVersion }
}

foreach ($installedVersion in $installedVersions) {
    if ($productVersion -lt $installedVersion) {
        throw "VERSION $productVersionText must not be older than installed FamilyPDF $installedVersion."
    }
}

Write-Host "Installer upgrade contract passed for FamilyPDF $productVersionText."
