#define MyAppName "gsshvnc"
#define MyAppVersion "0.92"
#define MyAppPublisher "Michael Hansen"
#define MyAppURL "http://www.github.com/zrax/gsshvnc"
#define MyAppExeName "gsshvnc.exe"

[Setup]
AppId={{D7C0AA2E-5A7B-4BE0-B6E7-8F0F88E86057}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
;AppVerName={#MyAppName} {#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
AppUpdatesURL={#MyAppURL}
DefaultDirName={pf}\{#MyAppName}
DefaultGroupName={#MyAppName}
AllowNoIcons=yes
LicenseFile=COPYING
OutputDir=.
OutputBaseFilename={#MyAppName}-setup-{#MyAppVersion}
Compression=lzma
SolidCompression=yes
ArchitecturesInstallIn64BitMode=x64
ArchitecturesAllowed=x64

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
#include "filelist.iss"

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\bin\gsshvnc.exe"
Name: "{commondesktop}\{#MyAppName}"; Filename: "{app}\bin\gsshvnc.exe"; Tasks: desktopicon

[Run]
Filename: "{app}\bin\gsshvnc.exe"; Description: "{cm:LaunchProgram,{#StringChange(MyAppName, '&', '&&')}}"; Flags: nowait postinstall skipifsilent

[InstallDelete]
; Files removed from the distribution which may have been installed in previous
; releases (usually due to msys2 package upgrades)
Type: files; Name: "{app}\share\icons\Adwaita\512x512\emblems\emblem-synchronizing.png"
