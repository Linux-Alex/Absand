; SPDX-License-Identifier: MPL-2.0

#ifndef SourceDir
  #error SourceDir must point to the staged Absand bin directory
#endif
#ifndef AppVersion
  #error AppVersion must contain the semantic release version
#endif
#ifndef OutputDir
  #define OutputDir "."
#endif

[Setup]
AppId={{0FA93FD4-28DB-4D7D-BD35-EF9F4023A61B}
AppName=Absand
AppVersion={#AppVersion}
AppVerName=Absand {#AppVersion}
AppPublisher=Linux-Alex
AppPublisherURL=https://github.com/Linux-Alex/Absand
AppSupportURL=https://github.com/Linux-Alex/Absand/issues
AppUpdatesURL=https://github.com/Linux-Alex/Absand/releases
DefaultDirName={localappdata}\Programs\Absand
DefaultGroupName=Absand
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
OutputDir={#OutputDir}
OutputBaseFilename=Absand-{#AppVersion}-windows-x64-setup
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
LicenseFile={#SourceDir}\licenses\Absand-MPL-2.0.txt
UninstallDisplayName=Absand {#AppVersion}
ChangesAssociations=yes
VersionInfoVersion={#AppVersion}.0
VersionInfoCompany=Linux-Alex
VersionInfoDescription=Absand secure archive sender installer
VersionInfoProductName=Absand
VersionInfoProductVersion={#AppVersion}

[Files]
Source: "{#SourceDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{autoprograms}\Absand"; Filename: "{app}\Absand.exe"; WorkingDir: "{app}"

[Registry]
Root: HKCU; Subkey: "Software\Classes\*\shell\Absand"; ValueType: string; ValueName: ""; ValueData: "Send with Absand"; Flags: uninsdeletekey
Root: HKCU; Subkey: "Software\Classes\*\shell\Absand\command"; ValueType: string; ValueName: ""; ValueData: """{app}\Absand.exe"" ""%1"""; Flags: uninsdeletekey
Root: HKCU; Subkey: "Software\Classes\Folder\shell\Absand"; ValueType: string; ValueName: ""; ValueData: "Send with Absand"; Flags: uninsdeletekey
Root: HKCU; Subkey: "Software\Classes\Folder\shell\Absand\command"; ValueType: string; ValueName: ""; ValueData: """{app}\Absand.exe"" ""%1"""; Flags: uninsdeletekey

[Run]
Filename: "{app}\Absand.exe"; Description: "Launch Absand"; Flags: nowait postinstall skipifsilent
