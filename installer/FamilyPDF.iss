#define MyAppName "FamilyPDF"
#define MyAppVersion "0.2.0"
#define MyAppPublisher "FamilyPDF"
#define MyAppExeName "Pdf4QtViewer.exe"

[Setup]
#ifdef ShellVerificationBuild
AppId={{D8942855-6D26-4801-908C-B8CD588A19C5}
#else
AppId={{3EE743F2-F10D-4D69-A4C3-01834462FBA6}
#endif
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
DefaultDirName={localappdata}\Programs\FamilyPDF
DefaultGroupName=FamilyPDF
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
#ifdef ShellVerificationBuild
OutputDir=..\build
OutputBaseFilename=FamilyPDF-Shell-Verification-Setup-x64
#else
#ifdef VerificationBuild
OutputDir=..\build
OutputBaseFilename=FamilyPDF-Verification-Setup-x64
Uninstallable=no
CreateUninstallRegKey=no
#else
OutputDir=..\dist
OutputBaseFilename=FamilyPDF-Setup-x64
#endif
#endif
Compression=lzma2/ultra64
SolidCompression=yes
WizardStyle=modern
SetupLogging=yes
CloseApplications=yes
RestartApplications=no
UninstallDisplayIcon={app}\Pdf4QtViewer.exe

[Languages]
Name: "chinesetraditional"; MessagesFile: "compiler:Languages\ChineseTraditional.isl"
Name: "chinesesimplified"; MessagesFile: "compiler:Languages\ChineseSimplified.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"

[CustomMessages]
chinesetraditional.WindowsIntegration=Windows 整合
chinesetraditional.PdfShellTask=加入 PDF 右鍵「使用 FamilyPDF 開啟／編輯」及 Windows「開啟方式」
chinesetraditional.OpenWithFamilyPDF=使用 FamilyPDF 開啟
chinesetraditional.EditWithFamilyPDF=使用 FamilyPDF 編輯
chinesetraditional.CompareShortcut=FamilyPDF 文件比較
chinesesimplified.WindowsIntegration=Windows 集成
chinesesimplified.PdfShellTask=添加 PDF 右键“使用 FamilyPDF 打开／编辑”及 Windows“打开方式”
chinesesimplified.OpenWithFamilyPDF=使用 FamilyPDF 打开
chinesesimplified.EditWithFamilyPDF=使用 FamilyPDF 编辑
chinesesimplified.CompareShortcut=FamilyPDF 文档比较
english.WindowsIntegration=Windows integration
english.PdfShellTask=Add Open/Edit with FamilyPDF to PDF context menus and Open with
english.OpenWithFamilyPDF=Open with FamilyPDF
english.EditWithFamilyPDF=Edit with FamilyPDF
english.CompareShortcut=FamilyPDF Document Compare

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked
Name: "pdfshell"; Description: "{cm:PdfShellTask}"; GroupDescription: "{cm:WindowsIntegration}"

[Files]
Source: "..\dist\FamilyPDF-windows-x64\*"; DestDir: "{app}"; Excludes: "portable.mode"; Flags: ignoreversion recursesubdirs createallsubdirs

#ifndef VerificationBuild
[Registry]
Root: HKCU; Subkey: "Software\Classes\Applications\Pdf4QtViewer.exe"; ValueType: string; ValueName: "FriendlyAppName"; ValueData: "FamilyPDF Reader"; Flags: uninsdeletekey; Tasks: pdfshell
Root: HKCU; Subkey: "Software\Classes\Applications\Pdf4QtViewer.exe\SupportedTypes"; ValueType: none; ValueName: ".pdf"; Tasks: pdfshell
Root: HKCU; Subkey: "Software\Classes\Applications\Pdf4QtViewer.exe\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\Pdf4QtViewer.exe"" ""%1"""; Tasks: pdfshell
Root: HKCU; Subkey: "Software\Classes\Applications\Pdf4QtEditor.exe"; ValueType: string; ValueName: "FriendlyAppName"; ValueData: "FamilyPDF Editor"; Flags: uninsdeletekey; Tasks: pdfshell
Root: HKCU; Subkey: "Software\Classes\Applications\Pdf4QtEditor.exe\SupportedTypes"; ValueType: none; ValueName: ".pdf"; Tasks: pdfshell
Root: HKCU; Subkey: "Software\Classes\Applications\Pdf4QtEditor.exe\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\Pdf4QtEditor.exe"" ""%1"""; Tasks: pdfshell
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.pdf\shell\FamilyPDF.Open"; ValueType: string; ValueName: ""; ValueData: "{cm:OpenWithFamilyPDF}"; Flags: uninsdeletekey; Tasks: pdfshell
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.pdf\shell\FamilyPDF.Open"; ValueType: string; ValueName: "Icon"; ValueData: "{app}\Pdf4QtViewer.exe"; Tasks: pdfshell
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.pdf\shell\FamilyPDF.Open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\Pdf4QtViewer.exe"" ""%1"""; Tasks: pdfshell
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.pdf\shell\FamilyPDF.Edit"; ValueType: string; ValueName: ""; ValueData: "{cm:EditWithFamilyPDF}"; Flags: uninsdeletekey; Tasks: pdfshell
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.pdf\shell\FamilyPDF.Edit"; ValueType: string; ValueName: "Icon"; ValueData: "{app}\Pdf4QtEditor.exe"; Tasks: pdfshell
Root: HKCU; Subkey: "Software\Classes\SystemFileAssociations\.pdf\shell\FamilyPDF.Edit\command"; ValueType: string; ValueName: ""; ValueData: """{app}\Pdf4QtEditor.exe"" ""%1"""; Tasks: pdfshell
#endif

#ifndef VerificationBuild
#ifndef ShellVerificationBuild
[Icons]
Name: "{group}\FamilyPDF 閱讀器"; Filename: "{app}\Pdf4QtViewer.exe"; WorkingDir: "{app}"
Name: "{group}\FamilyPDF 編輯器"; Filename: "{app}\Pdf4QtEditor.exe"; WorkingDir: "{app}"
Name: "{group}\FamilyPDF 頁面合併與拆分"; Filename: "{app}\Pdf4QtPageMaster.exe"; WorkingDir: "{app}"
Name: "{group}\{cm:CompareShortcut}"; Filename: "{app}\Pdf4QtDiff.exe"; WorkingDir: "{app}"
Name: "{autodesktop}\FamilyPDF"; Filename: "{app}\Pdf4QtViewer.exe"; WorkingDir: "{app}"; Tasks: desktopicon

[Run]
Filename: "{app}\Pdf4QtViewer.exe"; Description: "{cm:LaunchProgram,FamilyPDF}"; Flags: nowait postinstall skipifsilent
#endif
#endif
