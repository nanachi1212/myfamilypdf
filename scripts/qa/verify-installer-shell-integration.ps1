[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$repositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$installers = @(
    (Join-Path $repositoryRoot 'installer\FamilyPDF.iss'),
    (Join-Path $repositoryRoot 'installer\FamilyPDF-Full.iss')
)

$requiredMessages = @(
    'chinesetraditional.PdfShellTask=',
    'chinesesimplified.PdfShellTask=',
    'english.PdfShellTask=',
    'chinesetraditional.OpenWithFamilyPDF=',
    'chinesesimplified.OpenWithFamilyPDF=',
    'english.OpenWithFamilyPDF='
)
$requiredRegistryFragments = @(
    'Software\Classes\Applications\Pdf4QtViewer.exe',
    'Software\Classes\Applications\Pdf4QtEditor.exe',
    'Software\Classes\SystemFileAssociations\.pdf\shell\FamilyPDF.Open',
    'Software\Classes\SystemFileAssociations\.pdf\shell\FamilyPDF.Edit',
    'ValueData: """{app}\Pdf4QtViewer.exe"" ""%1"""',
    'ValueData: """{app}\Pdf4QtEditor.exe"" ""%1"""'
)

foreach ($installer in $installers) {
    if (-not (Test-Path -LiteralPath $installer -PathType Leaf)) {
        throw "Installer source was not found: $installer"
    }
    $text = Get-Content -LiteralPath $installer -Raw -Encoding UTF8

    foreach ($required in $requiredMessages + $requiredRegistryFragments) {
        if (-not $text.Contains($required)) {
            throw "Shell integration is missing '$required' in $installer"
        }
    }
    if ($text -notmatch 'Name:\s*"pdfshell".*Description:\s*"\{cm:PdfShellTask\}"') {
        throw "The optional pdfshell task is missing in $installer"
    }

    $registryLines = @(
        $text -split "`r?`n" |
            Where-Object { $_ -match '^Root:\s' }
    )
    if ($registryLines.Count -ne 12) {
        throw "Expected 12 shell registry entries in $installer, found $($registryLines.Count)."
    }
    if (@($registryLines | Where-Object { $_ -notmatch '^Root:\s+HKCU;' }).Count -gt 0) {
        throw "Shell integration must remain user-scoped (HKCU): $installer"
    }
    if (@($registryLines | Where-Object { $_ -notmatch 'Tasks:\s+pdfshell' }).Count -gt 0) {
        throw "Every shell registry entry must be controlled by the pdfshell task: $installer"
    }
    if ($text -match 'Software\\Classes\\\.pdf";\s*ValueType:\s*string;\s*ValueName:\s*""') {
        throw "Installer must not force FamilyPDF as the default PDF handler: $installer"
    }
    foreach ($ownedKey in @(
        'Applications\Pdf4QtViewer.exe',
        'Applications\Pdf4QtEditor.exe',
        'FamilyPDF.Open',
        'FamilyPDF.Edit'
    )) {
        $ownedLine = @(
            $registryLines |
                Where-Object { $_.Contains($ownedKey) -and $_ -match 'Flags:\s+uninsdeletekey' }
        )
        if ($ownedLine.Count -eq 0) {
            throw "Uninstall cleanup is missing for '$ownedKey' in $installer"
        }
    }
}

Write-Host 'Installer shell integration verification passed.'
