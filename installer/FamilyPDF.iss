#define MyAppName "FamilyPDF"
#define MyAppVersion "0.1.0"
#define MyAppPublisher "FamilyPDF"
#define MyAppExeName "Pdf4QtViewer.exe"

[Setup]
AppId={{3EE743F2-F10D-4D69-A4C3-01834462FBA6}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
DefaultDirName={localappdata}\Programs\FamilyPDF
DefaultGroupName=FamilyPDF
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
OutputDir=..\dist
OutputBaseFilename=FamilyPDF-Setup-x64
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

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
Source: "..\dist\FamilyPDF-windows-x64\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\FamilyPDF 閱讀器"; Filename: "{app}\Pdf4QtViewer.exe"; WorkingDir: "{app}"
Name: "{group}\FamilyPDF 編輯器"; Filename: "{app}\Pdf4QtEditor.exe"; WorkingDir: "{app}"
Name: "{group}\FamilyPDF 頁面合併與拆分"; Filename: "{app}\Pdf4QtPageMaster.exe"; WorkingDir: "{app}"
Name: "{group}\FamilyPDF OCR"; Filename: "{app}\FamilyPDF-OCR.cmd"; WorkingDir: "{app}"
Name: "{autodesktop}\FamilyPDF"; Filename: "{app}\Pdf4QtViewer.exe"; WorkingDir: "{app}"; Tasks: desktopicon

[Run]
Filename: "{app}\Pdf4QtViewer.exe"; Description: "{cm:LaunchProgram,FamilyPDF}"; Flags: nowait postinstall skipifsilent
