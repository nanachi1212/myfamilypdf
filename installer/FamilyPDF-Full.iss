#define MyAppName "FamilyPDF"
#ifndef MyAppVersion
#define MyAppVersion "0.0.0-dev"
#endif
#define MyAppPublisher "FamilyPDF"

[Setup]
AppId={{3EE743F2-F10D-4D69-A4C3-01834462FBA6}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
DefaultDirName={localappdata}\Programs\FamilyPDF
DefaultGroupName=FamilyPDF
UsePreviousAppDir=yes
UsePreviousGroup=yes
UsePreviousLanguage=yes
UsePreviousTasks=yes
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
#ifdef VerificationBuild
OutputDir=..\build
OutputBaseFilename=FamilyPDF-Full-Verification-Setup-x64
Uninstallable=no
CreateUninstallRegKey=no
#else
OutputDir=..\dist
OutputBaseFilename=FamilyPDF-Full-Setup-x64
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
chinesetraditional.FullType=完整安裝（建議）
chinesetraditional.CompactType=僅安裝 PDF 閱讀與編輯
chinesetraditional.CustomType=自訂安裝
chinesetraditional.CoreComponent=FamilyPDF 閱讀、編輯、表單與 Office 匯出
chinesetraditional.OcrComponent=OCR 可搜尋文字（繁體、簡體、英文及直排模型）
chinesesimplified.FullType=完整安装（建议）
chinesesimplified.CompactType=仅安装 PDF 阅读与编辑
chinesesimplified.CustomType=自定义安装
chinesesimplified.CoreComponent=FamilyPDF 阅读、编辑、表单与 Office 导出
chinesesimplified.OcrComponent=OCR 可搜索文字（繁体、简体、英文及竖排模型）
english.FullType=Complete installation (recommended)
english.CompactType=PDF reading and editing only
english.CustomType=Custom installation
english.CoreComponent=FamilyPDF reading, editing, forms, and Office export
english.OcrComponent=Searchable OCR (Traditional Chinese, Simplified Chinese, English, and vertical models)
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

[Types]
Name: "full"; Description: "{cm:FullType}"
Name: "compact"; Description: "{cm:CompactType}"
Name: "custom"; Description: "{cm:CustomType}"; Flags: iscustom

[Components]
Name: "core"; Description: "{cm:CoreComponent}"; Types: full compact custom; Flags: fixed
Name: "ocr"; Description: "{cm:OcrComponent}"; Types: full

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked
Name: "pdfshell"; Description: "{cm:PdfShellTask}"; GroupDescription: "{cm:WindowsIntegration}"

[Files]
Source: "..\dist\FamilyPDF-windows-x64\*"; DestDir: "{app}"; Excludes: "portable.mode"; Flags: ignoreversion recursesubdirs createallsubdirs; Components: core
Source: "..\dist\FamilyPDF-OCR-Plugin-windows-x64\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs; Components: ocr

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

[Icons]
Name: "{group}\FamilyPDF 閱讀器"; Filename: "{app}\Pdf4QtViewer.exe"; WorkingDir: "{app}"
Name: "{group}\FamilyPDF 編輯器"; Filename: "{app}\Pdf4QtEditor.exe"; WorkingDir: "{app}"
Name: "{group}\FamilyPDF 頁面合併與拆分"; Filename: "{app}\Pdf4QtPageMaster.exe"; WorkingDir: "{app}"
Name: "{group}\{cm:CompareShortcut}"; Filename: "{app}\Pdf4QtDiff.exe"; WorkingDir: "{app}"
Name: "{group}\FamilyPDF OCR Language Repair"; Filename: "{app}\Install-FamilyPDF-OCR-Languages.cmd"; WorkingDir: "{app}"; Components: ocr
Name: "{autodesktop}\FamilyPDF"; Filename: "{app}\Pdf4QtViewer.exe"; WorkingDir: "{app}"; Tasks: desktopicon

[Run]
Filename: "{app}\Pdf4QtViewer.exe"; Description: "{cm:LaunchProgram,FamilyPDF}"; Flags: nowait postinstall skipifsilent
#endif
