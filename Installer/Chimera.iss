; Build with Tools/Build-WindowsInstaller.ps1 after the Windows package stage.
#ifndef StageDir
  #error StageDir must point to the validated Windows package directory
#endif
#ifndef OutputPath
  #error OutputPath must be supplied by the build script
#endif
#define ProductName "Chimera Amp Matrix"
#define ProductVersion "1.0.0"

[Setup]
; Keep AppId stable across updates so Windows has one uninstall entry.
AppId=SpectralForge.ChimeraAmpMatrix
AppName={#ProductName}
AppVersion={#ProductVersion}
AppVerName={#ProductName} {#ProductVersion} Test
AppPublisher=SpectralForge
AppPublisherURL=https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix
DefaultDirName={autopf}\SpectralForge\{#ProductName}
DefaultGroupName=SpectralForge\{#ProductName}
DisableProgramGroupPage=yes
DisableDirPage=auto
PrivilegesRequired=admin
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0
OutputDir={#OutputPath}
OutputBaseFilename=Chimera-Amp-Matrix-{#ProductVersion}-test-win64-Setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
UninstallDisplayName={#ProductName}
UninstallDisplayIcon={uninstallexe}
UninstallFilesDir={app}\Uninstall
CloseApplications=no
RestartApplications=no
SetupLogging=yes
VersionInfoCompany=SpectralForge
VersionInfoDescription={#ProductName} Windows Installer
VersionInfoVersion={#ProductVersion}.0
VersionInfoProductName={#ProductName}
VersionInfoProductVersion={#ProductVersion}

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"
Name: "korean"; MessagesFile: "compiler:Languages\Korean.isl"

[Types]
Name: "full"; Description: "{cm:FullInstall}"
Name: "custom"; Description: "{cm:CustomInstall}"; Flags: iscustom

[Components]
Name: "vst3"; Description: "VST3 (64-bit)"; Types: full
Name: "standalone"; Description: "{cm:Standalone}"; Types: full
Name: "reference"; Description: "{cm:ReferenceTools}"; Types: full

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; Components: standalone; Flags: unchecked

[Files]
Source: "{#StageDir}\VST3\Chimera Amp Matrix.vst3\*"; DestDir: "{commoncf64}\VST3\Chimera Amp Matrix.vst3"; Components: vst3; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#StageDir}\Standalone\Chimera Amp Matrix.exe"; DestDir: "{app}"; Components: standalone; Flags: ignoreversion
Source: "{#StageDir}\*.txt"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#StageDir}\*.md"; DestDir: "{app}\Documentation"; Flags: ignoreversion
Source: "{#StageDir}\reference\*"; DestDir: "{app}\Documentation\reference"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#StageDir}\ReferenceTools\*"; DestDir: "{app}\ReferenceTools"; Components: reference; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#StageDir}\reference-audio\*"; DestDir: "{app}\reference-audio"; Components: reference; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\Chimera Amp Matrix"; Filename: "{app}\Chimera Amp Matrix.exe"; WorkingDir: "{app}"; Components: standalone
Name: "{group}\{cm:InstallGuide}"; Filename: "{app}\WINDOWS_INSTALL.txt"
Name: "{group}\{cm:UninstallProgram,Chimera Amp Matrix}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\Chimera Amp Matrix"; Filename: "{app}\Chimera Amp Matrix.exe"; WorkingDir: "{app}"; Tasks: desktopicon; Components: standalone

[Run]
Filename: "{app}\Chimera Amp Matrix.exe"; Description: "{cm:LaunchProgram,Chimera Amp Matrix}"; Components: standalone; Flags: nowait postinstall skipifsilent

; No wildcard deletion rules: user-created presets and IRs are never owned by Setup.
[CustomMessages]
english.FullInstall=VST3, standalone application and reference tools
korean.FullInstall=VST3, 단독 실행 앱 및 레퍼런스 도구
english.CustomInstall=Choose components
korean.CustomInstall=설치할 구성 요소 선택
english.Standalone=Standalone application
korean.Standalone=단독 실행 앱
english.ReferenceTools=Offline reference tools and example audio
korean.ReferenceTools=오프라인 레퍼런스 도구 및 예제 음원
english.InstallGuide=Installation guide
korean.InstallGuide=설치 안내
