; MakineAI Inno Setup Installer Script
; Requires Inno Setup 6.2+ (Unicode)
;
; Usage (local):   iscc makineai.iss /DAppVersion=0.1.0 /DDistDir=..\dist
; Usage (CI):      iscc makineai.iss /DAppVersion=$VERSION /DDistDir=dist

#ifndef AppVersion
  #define AppVersion "0.1.0"
#endif

#ifndef DistDir
  #define DistDir "..\dist"
#endif

[Setup]
AppId={{A2E9F3C1-4B67-4D8E-9F2A-1C3D5E7F9B01}
AppName=MakineAI
AppVersion={#AppVersion}
AppVerName=MakineAI {#AppVersion}
AppPublisher=MakineAI
AppPublisherURL=https://github.com/jlceaser/MakineAI
AppSupportURL=https://github.com/jlceaser/MakineAI/issues
AppUpdatesURL=https://github.com/jlceaser/MakineAI/releases
DefaultDirName={autopf}\MakineAI
DefaultGroupName=MakineAI
AllowNoIcons=yes
OutputDir=output
OutputBaseFilename=MakineAI-{#AppVersion}-setup
Compression=lzma2/ultra64
SolidCompression=yes
WizardStyle=modern
PrivilegesRequired=admin
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0
VersionInfoVersion={#AppVersion}.0
DisableProgramGroupPage=yes

[Languages]
Name: "turkish"; MessagesFile: "compiler:Languages\Turkish.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked
Name: "startupicon"; Description: "Start with Windows"; GroupDescription: "Additional options:"

[Files]
Source: "{#DistDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\MakineAI"; Filename: "{app}\MakineAI.exe"
Name: "{group}\Uninstall MakineAI"; Filename: "{uninstallexe}"
Name: "{autodesktop}\MakineAI"; Filename: "{app}\MakineAI.exe"; Tasks: desktopicon

[Registry]
; Startup entry (only if task selected)
Root: HKCU; Subkey: "SOFTWARE\Microsoft\Windows\CurrentVersion\Run"; ValueType: string; ValueName: "MakineAI"; ValueData: """{app}\MakineAI.exe"" --minimized"; Flags: uninsdeletevalue; Tasks: startupicon

[Run]
Filename: "{app}\MakineAI.exe"; Description: "Launch MakineAI"; Flags: nowait postinstall skipifsilent

[UninstallDelete]
Type: dirifempty; Name: "{app}"

[Code]
// Check for Visual C++ Redistributable (2015-2022)
function VCRedistInstalled(): Boolean;
var
  RegKey: String;
begin
  RegKey := 'SOFTWARE\Microsoft\VisualStudio\14.0\VC\Runtimes\X64';
  Result := RegKeyExists(HKLM, RegKey);
  if not Result then
  begin
    RegKey := 'SOFTWARE\WOW6432Node\Microsoft\VisualStudio\14.0\VC\Runtimes\X64';
    Result := RegKeyExists(HKLM, RegKey);
  end;
end;

function InitializeSetup(): Boolean;
begin
  Result := True;
  if not VCRedistInstalled() then
  begin
    if MsgBox('Visual C++ Redistributable was not found.' + #13#10 +
              'MakineAI requires it to run.' + #13#10#13#10 +
              'Do you want to continue anyway?',
              mbConfirmation, MB_YESNO) = IDNO then
    begin
      Result := False;
    end;
  end;
end;
