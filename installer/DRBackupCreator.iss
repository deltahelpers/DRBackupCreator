; ---------------------------------------------------------------------------
; DRBackupCreator - Inno Setup script
;
; Build the app first (build.ps1 or the CI workflow), then compile:
;   iscc /DMyAppVersion=1.2.3 installer\DRBackupCreator.iss
;
; Output: dist\drbcWindowsInstaller.exe (stable asset name)
; ---------------------------------------------------------------------------

#define MyAppName "DRBackupCreator"
#ifndef MyAppVersion
  #define MyAppVersion "2.1.0"
#endif
#define MyAppPublisher "DeltaHelpers"
#define MyAppURL "https://github.com/deltahelpers/DRBackupCreator"
#define MyAppExeName "drbc.exe"

[Setup]
AppId={{B7C3D2E1-5A44-4F0B-9C1D-2E8F6A7B3C90}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppVerName={#MyAppName} {#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}/issues
AppUpdatesURL={#MyAppURL}/releases/latest
AppCopyright=Copyright (C) 2026 {#MyAppPublisher}
LicenseFile=..\LICENSE
DefaultDirName={autopf}\{#MyAppName}
DefaultGroupName={#MyAppName}
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog
OutputDir=..\dist
; Stable asset name so latest/download/drbcWindowsInstaller.exe never changes.
OutputBaseFilename=drbcWindowsInstaller
SetupIconFile=..\assets\app.ico
UninstallDisplayIcon={app}\{#MyAppExeName}
UninstallDisplayName={#MyAppName}
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0
CloseApplications=yes
RestartApplications=no
ChangesAssociations=yes
ChangesEnvironment=no
AllowNoIcons=yes
VersionInfoVersion={#MyAppVersion}

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"
Name: "spanish"; MessagesFile: "compiler:Languages\Spanish.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
Source: "..\build\bin\{#MyAppExeName}"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\build\bin\DRBackupCCore.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\README.md"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\LICENSE"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{group}\{cm:UninstallProgram,{#MyAppName}}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchProgram,{#StringChange(MyAppName, '&', '&&')}}"; Flags: nowait postinstall skipifsilent

; ---------------------------------------------------------------------------
; File associations for the two document types.
;
; HKA maps to HKCU for this per-user installer, so no administrator rights are
; needed and nothing is written to HKLM. Windows 10/11 still asks the user to
; confirm the default handler the first time one of these files is opened;
; after that double-clicking a .bctheme or .bcconfig opens DRBackupCreator.
; ---------------------------------------------------------------------------
[Registry]
Root: HKA; Subkey: "Software\Classes\.bctheme"; ValueType: string; ValueName: ""; ValueData: "DRBackupCreator.Theme"; Flags: uninsdeletevalue
Root: HKA; Subkey: "Software\Classes\DRBackupCreator.Theme"; ValueType: string; ValueName: ""; ValueData: "DRBackupCreator theme"; Flags: uninsdeletekey
Root: HKA; Subkey: "Software\Classes\DRBackupCreator.Theme\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\{#MyAppExeName},0"; Flags: uninsdeletekey
Root: HKA; Subkey: "Software\Classes\DRBackupCreator.Theme\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""; Flags: uninsdeletekey

Root: HKA; Subkey: "Software\Classes\.bcconfig"; ValueType: string; ValueName: ""; ValueData: "DRBackupCreator.Config"; Flags: uninsdeletevalue
Root: HKA; Subkey: "Software\Classes\DRBackupCreator.Config"; ValueType: string; ValueName: ""; ValueData: "DRBackupCreator configuration"; Flags: uninsdeletekey
Root: HKA; Subkey: "Software\Classes\DRBackupCreator.Config\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\{#MyAppExeName},0"; Flags: uninsdeletekey
Root: HKA; Subkey: "Software\Classes\DRBackupCreator.Config\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""; Flags: uninsdeletekey

; Offer both types in the "Open with" menu even before the user picks a default.
Root: HKA; Subkey: "Software\Classes\Applications\{#MyAppExeName}\SupportedTypes"; ValueType: string; ValueName: ".bctheme"; ValueData: ""; Flags: uninsdeletekey
Root: HKA; Subkey: "Software\Classes\Applications\{#MyAppExeName}\SupportedTypes"; ValueType: string; ValueName: ".bcconfig"; ValueData: ""; Flags: uninsdeletekey
Root: HKA; Subkey: "Software\Classes\Applications\{#MyAppExeName}\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""; Flags: uninsdeletekey

; User data (backups and settings in %LOCALAPPDATA%\DRBackupCreator) is
; intentionally left untouched on uninstall.
