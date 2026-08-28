$ErrorActionPreference = 'Stop'

$appx = Get-Content 'AppxManifest.xml.in' -Raw
$wix = Get-Content 'WixInstaller/Product.wxs.in' -Raw
$debian = Get-Content 'make-package.sh.in' -Raw
$flatpak = Get-Content 'Flatpak/io.github.JakubMelka.Pdf4qt.json' -Raw
$appdata = Get-Content 'Desktop/io.github.JakubMelka.Pdf4qt.appdata.xml' -Raw
$desktop = Get-ChildItem 'Desktop' -Filter '*.desktop' | ForEach-Object { Get-Content $_.FullName -Raw }

if ($appx -notmatch '<DisplayName>FamilyPDF</DisplayName>' -or
    $appx -notmatch 'DisplayName="FamilyPDF (Editor|Viewer|PageMaster|Diff)"' -or
    $appx -notmatch 'Version="\$\{FAMILYPDF_PACKAGE_VERSION\}"') {
    throw 'Appx metadata is not using FamilyPDF branding and the authoritative package version.'
}
if ($wix -notmatch '<Product Id="\*" Name="FamilyPDF"' -or
    $wix -notmatch 'Version="\$\{FAMILYPDF_PACKAGE_VERSION\}"' -or
    $wix -match 'Name="PDF4QT"') {
    throw 'WiX metadata is not using FamilyPDF branding and the authoritative package version.'
}
if ($debian -notmatch 'Package: familypdf' -or
    $debian -notmatch '\$\{FAMILYPDF_VERSION\}' -or
    $debian -match 'JakubMelka/PDF4QT') {
    throw 'Debian package metadata is not using FamilyPDF branding.'
}
if ($flatpak -notmatch 'nanachi1212/myfamilypdf\.git' -or
    $flatpak -match 'JakubMelka/PDF4QT\.git') {
    throw 'Flatpak source is still pointing at the upstream PDF4QT repository.'
}
if ($appdata -notmatch '<name>FamilyPDF</name>' -or
    $appdata -notmatch '<release version="0\.2\.3"') {
    throw 'AppStream metadata is not using the FamilyPDF release identity.'
}
if (($desktop -join "`n") -match '(?m)^Name=PDF4QT') {
    throw 'Desktop entries still expose the upstream PDF4QT product name.'
}

Write-Output 'Cross-platform branding and version contracts passed.'
