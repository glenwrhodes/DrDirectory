; Inno Setup script for DrDirectory.
; Build: iscc /DAppVersion=1.0.0 /DSourceDir=<deployed app folder> packaging\windows\DrDirectory.iss

#ifndef AppVersion
  #define AppVersion "1.0.0"
#endif
#ifndef SourceDir
  #define SourceDir "..\..\deploy"
#endif

[Setup]
AppId={{8C1F2B7E-5D3A-4E9B-9A61-3D2C7B4F1E90}
AppName=DrDirectory
AppVersion={#AppVersion}
AppVerName=DrDirectory {#AppVersion}
AppPublisher=Tiny Moosh Games Inc.
AppPublisherURL=https://github.com/glenwrhodes/DrDirectory
AppSupportURL=https://github.com/glenwrhodes/DrDirectory/issues
AppUpdatesURL=https://github.com/glenwrhodes/DrDirectory/releases
AppCopyright=Copyright (c) 2026 Tiny Moosh Games Inc.
DefaultDirName={autopf}\DrDirectory
DefaultGroupName=DrDirectory
DisableProgramGroupPage=yes
LicenseFile=..\..\LICENSE
OutputDir=..\..\dist
OutputBaseFilename=DrDirectory-{#AppVersion}-windows-x64-setup
SetupIconFile=..\..\resources\DrDirectory.ico
UninstallDisplayIcon={app}\DrDirectory.exe
UninstallDisplayName=DrDirectory
VersionInfoVersion={#AppVersion}
VersionInfoCompany=Tiny Moosh Games Inc.
VersionInfoDescription=DrDirectory Setup
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
PrivilegesRequiredOverridesAllowed=dialog commandline
CloseApplications=yes

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
Source: "{#SourceDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{autoprograms}\DrDirectory"; Filename: "{app}\DrDirectory.exe"
Name: "{autodesktop}\DrDirectory"; Filename: "{app}\DrDirectory.exe"; Tasks: desktopicon

[Run]
Filename: "{app}\DrDirectory.exe"; Description: "{cm:LaunchProgram,DrDirectory}"; Flags: nowait postinstall skipifsilent
