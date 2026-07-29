#define MyAppName "FamilyPDF OCR Plugin"
#define MyAppVersion "0.2.0"
#define MyAppPublisher "FamilyPDF"

[Setup]
AppId={{B7902544-CF83-41A1-A7E5-04043DFE432F}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
DefaultDirName={localappdata}\Programs\FamilyPDF
DefaultGroupName=FamilyPDF
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
UninstallFilesDir={app}\ocr-plugin-uninstall
UninstallDisplayName=FamilyPDF OCR Plugin
UninstallDisplayIcon={app}\ocr\tesseract.exe
#ifdef VerificationBuild
OutputDir=..\build
OutputBaseFilename=FamilyPDF-OCR-Plugin-Verification-Setup-x64
Uninstallable=no
CreateUninstallRegKey=no
#else
OutputDir=..\dist
OutputBaseFilename=FamilyPDF-OCR-Plugin-Setup-x64
#endif
Compression=lzma2/ultra64
SolidCompression=yes
WizardStyle=modern
SetupLogging=yes
CloseApplications=yes
RestartApplications=no

[Languages]
Name: "chinesetraditional"; MessagesFile: "compiler:Languages\ChineseTraditional.isl"
Name: "chinesesimplified"; MessagesFile: "compiler:Languages\ChineseSimplified.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"

[Files]
Source: "..\dist\FamilyPDF-OCR-Plugin-windows-x64\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

#ifndef VerificationBuild
[Icons]
Name: "{group}\FamilyPDF OCR Language Repair"; Filename: "{app}\Install-FamilyPDF-OCR-Languages.cmd"; WorkingDir: "{app}"
#endif
