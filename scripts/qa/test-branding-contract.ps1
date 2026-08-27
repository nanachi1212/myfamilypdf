[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$repositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$applications = @(
    @{ Path = 'Pdf4QtEditor\main.cpp'; Internal = 'FamilyPDF Editor'; Display = 'FamilyPDF Editor' },
    @{ Path = 'Pdf4QtViewer\main.cpp'; Internal = 'FamilyPDF Reader'; Display = 'FamilyPDF Reader' },
    @{ Path = 'Pdf4QtPageMaster\main.cpp'; Internal = 'FamilyPDF PageMaster'; Display = 'FamilyPDF PageMaster' },
    @{ Path = 'Pdf4QtDiff\main.cpp'; Internal = 'FamilyPDF Diff'; Display = 'FamilyPDF Diff' },
    @{ Path = 'Pdf4QtLibGui\main.cpp'; Internal = 'FamilyPDF Editor'; Display = 'FamilyPDF Editor' }
)

foreach ($application in $applications) {
    $content = Get-Content -LiteralPath (Join-Path $repositoryRoot $application.Path) -Raw -Encoding UTF8
    $internalCall = [regex]::Escape("setApplicationName(`"$($application.Internal)`")")
    if ($content -notmatch $internalCall) {
        throw "$($application.Path) does not use the FamilyPDF settings identity '$($application.Internal)'."
    }
    if ($content -notmatch "setApplicationDisplayName\([^\r\n]*$([regex]::Escape($application.Display))") {
        throw "$($application.Path) does not expose the FamilyPDF display name '$($application.Display)'."
    }
}

$helpFiles = @(
    'Pdf4QtLibGui\pdfprogramcontroller.cpp',
    'Pdf4QtPageMaster\mainwindow.cpp',
    'Pdf4QtDiff\mainwindow.cpp'
)
foreach ($relativePath in $helpFiles) {
    $content = Get-Content -LiteralPath (Join-Path $repositoryRoot $relativePath) -Raw -Encoding UTF8
    if ($content -match 'github\.com/JakubMelka/PDF4QT') {
        throw "$relativePath still sends FamilyPDF users to the upstream issue tracker."
    }
    if ($content -notmatch 'github\.com/nanachi1212/myfamilypdf') {
        throw "$relativePath does not link to the FamilyPDF repository."
    }
}

Write-Host 'FamilyPDF branding and settings-identity contract passed.'
