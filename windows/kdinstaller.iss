; Inno Setup 6 or later is required for this script to work.

[Setup]
AppID=Koord
AppName=Koord
AppVerName=Koord
AppVersion={#ApplicationVersion}
VersionInfoVersion={#ApplicationVersion}
AppPublisher=Koord.Live
AppPublisherURL=https://koord.live
AppSupportURL=https://github.com/koord-live/koord-realtime/issues
AppUpdatesURL=https://github.com/koord-live/koord-realtime/releases
AppContact=contact@koord.live
WizardStyle=modern

DefaultDirName={autopf}\Koord
AppendDefaultDirName=no
ArchitecturesInstallIn64BitMode=x64

; for 100% dpi setting should be 164x314 - https://jrsoftware.org/ishelp/
WizardImageFile=windows\koord-rt.bmp
; for 100% dpi setting should be 55x55 
WizardSmallImageFile=windows\koord-rt-small.bmp

[Files]
Source:"deploy\x86_64\KoordASIO.dll"; DestDir: "{app}"; Flags: ignoreversion regserver 64bit; Check: Is64BitInstallMode
; install everything else in deploy dir, including portaudio.dll, KoordASIOControl.exe and all Qt dll deps
Source:"deploy\x86_64\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs 64bit; Check: Is64BitInstallMode

[Icons]
Name: "{group}\Koord"; Filename: "{app}\Koord.exe"; WorkingDir: "{app}"
Name: "{group}\KoordASIO Control"; Filename: "{app}\KoordASIOControl.exe"; WorkingDir: "{app}"

[Run]
Filename: "{app}\KoordASIOControl.exe"; Description: "Run KoordASIO Control (set up sound devices)"; Flags: postinstall nowait skipifsilent
; Filename: "{app}\Koord.exe"; Description: "Launch Koord"; Flags: postinstall nowait skipifsilent unchecked

; install reg key to locate KoordASIOControl.exe at runtime
[Registry]
Root: HKLM64; Subkey: "Software\Koord"; Flags: uninsdeletekeyifempty
Root: HKLM64; Subkey: "Software\Koord\KoordASIO"; Flags: uninsdeletekey
Root: HKLM64; Subkey: "Software\Koord\KoordASIO\Install"; ValueType: string; ValueName: "InstallPath"; ValueData: "{app}"