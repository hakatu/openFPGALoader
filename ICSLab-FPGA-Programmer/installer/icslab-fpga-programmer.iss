; ICSLab FPGA Programmer - optional installer (Inno Setup 6)
;
; Build the portable release first (scripts/build-release.sh), then:
;   iscc /DAppVersion=1.0.0 /DSourceDir=..\release\ICSLab-FPGA-Programmer installer\icslab-fpga-programmer.iss
;
; Installs per user (no administrator rights needed). The portable ZIP
; remains the primary distribution.

#ifndef AppVersion
  #define AppVersion "1.0.0"
#endif
#ifndef SourceDir
  #define SourceDir "..\release\ICSLab-FPGA-Programmer"
#endif

#define AppName "ICSLab FPGA Programmer"
#define AppExe  "ICSLab FPGA Programmer.exe"

[Setup]
AppId={{6E0C7B2A-58E4-4C55-9A57-3F1A0D7C1C11}
AppName={#AppName}
AppVersion={#AppVersion}
AppVerName={#AppName} {#AppVersion}
AppPublisher=ICSLab
AppCopyright=Copyright (C) ICSLab. Uses openFPGALoader (Apache-2.0), an independent open-source project.
AppComments=Simple FPGA programming utility powered by openFPGALoader
DefaultDirName={autopf}\{#AppName}
DefaultGroupName={#AppName}
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
OutputDir=..\release
OutputBaseFilename=ICSLab-FPGA-Programmer-Setup
SetupIconFile=..\resources\icslab.ico
UninstallDisplayIcon={app}\{#AppExe}
InfoBeforeFile={#SourceDir}\README-licenses.txt
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
VersionInfoCompany=ICSLab
VersionInfoProductName={#AppName}
VersionInfoDescription={#AppName} Setup
VersionInfoVersion={#AppVersion}

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"

[Files]
Source: "{#SourceDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{autoprograms}\{#AppName}"; Filename: "{app}\{#AppExe}"
Name: "{autodesktop}\{#AppName}"; Filename: "{app}\{#AppExe}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#AppExe}"; Description: "{cm:LaunchProgram,{#AppName}}"; Flags: nowait postinstall skipifsilent
