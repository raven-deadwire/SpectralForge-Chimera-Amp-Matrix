; Build with Tools/Build-WindowsInstaller.ps1 after the Windows package stage.
#ifndef StageDir
  #error StageDir must point to the validated Windows package directory
#endif
#ifndef OutputPath
  #error OutputPath must be supplied by the build script
#endif
#define ProductName "SpectralForge Chimera"
#define ProductVersion "1.0.0"

[Setup]
; Keep AppId stable across updates so Windows has one uninstall entry.
AppId=SpectralForge.ChimeraAmpMatrix
AppName={#ProductName}
AppVersion={#ProductVersion}
AppVerName={#ProductName} Open Beta 1.0
AppPublisher=RavenForge Luthier Intelligence
AppPublisherURL=https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix
DefaultDirName={autopf}\SpectralForge\Chimera Amp Matrix
DefaultGroupName=SpectralForge\{#ProductName}
DisableProgramGroupPage=yes
DisableDirPage=auto
PrivilegesRequired=admin
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0
OutputDir={#OutputPath}
OutputBaseFilename=SpectralForge-Chimera-1.0.0-beta.1-win64-Setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
SetupIconFile=..\Assets\Artwork\spectralforge-emblem.ico
UninstallDisplayName={#ProductName}
UninstallDisplayIcon={uninstallexe}
UninstallFilesDir={app}\Uninstall
CloseApplications=no
RestartApplications=no
SetupLogging=yes
VersionInfoCompany=RavenForge Luthier Intelligence
VersionInfoDescription={#ProductName} Windows Installer
VersionInfoVersion={#ProductVersion}.0
VersionInfoProductName={#ProductName}
VersionInfoProductVersion={#ProductVersion}

#ifdef SignRelease
SignTool=ChimeraRelease
SignedUninstaller=yes
#endif

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
Source: "{#StageDir}\VST3\SpectralForge Chimera.vst3\*"; DestDir: "{commoncf64}\VST3\SpectralForge Chimera.vst3"; Components: vst3; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#StageDir}\Standalone\SpectralForge Chimera.exe"; DestDir: "{app}"; Components: standalone; Flags: ignoreversion
Source: "{#StageDir}\*.txt"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#StageDir}\ARTWORK_PROMPTS.json"; DestDir: "{app}\Documentation"; Flags: ignoreversion
Source: "{#StageDir}\*.html"; DestDir: "{app}\Documentation"; Flags: ignoreversion
Source: "{#StageDir}\payload-manifest.json"; DestDir: "{app}\Documentation"; Flags: ignoreversion
Source: "{#StageDir}\*.md"; DestDir: "{app}\Documentation"; Flags: ignoreversion
Source: "{#StageDir}\reference\*"; DestDir: "{app}\Documentation\reference"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#StageDir}\ReferenceTools\*"; DestDir: "{app}\ReferenceTools"; Components: reference; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#StageDir}\reference-audio\*"; DestDir: "{app}\reference-audio"; Components: reference; Flags: ignoreversion recursesubdirs createallsubdirs

; Optional user-owned companion pack, supplied separately from the public build.
; Install where both VST3 and standalone (and other Windows accounts) can discover it.
; Preserve these files across uninstall. Setup never bundles restricted T3K captures.
Source: "{src}\Chimera-Personal-IRs\*.wav"; DestDir: "{commonappdata}\SpectralForge\Chimera\IRs"; Flags: external skipifsourcedoesntexist recursesubdirs createallsubdirs onlyifdoesntexist uninsneveruninstall
Source: "{src}\Chimera-Personal-IRs\*.json"; DestDir: "{commonappdata}\SpectralForge\Chimera\IRs"; Flags: external skipifsourcedoesntexist recursesubdirs createallsubdirs onlyifdoesntexist uninsneveruninstall

[InstallDelete]
; Retire only exact legacy program binaries, never user presets or IR folders.
Type: files; Name: "{app}\Chimera Amp Matrix.exe"; Components: standalone
Type: files; Name: "{commoncf64}\VST3\Chimera Amp Matrix.vst3\Contents\x86_64-win\Chimera Amp Matrix.vst3"; Components: vst3
Type: files; Name: "{commonprograms}\SpectralForge\Chimera Amp Matrix\Chimera Amp Matrix.lnk"; Components: standalone

[Icons]
Name: "{group}\SpectralForge Chimera"; Filename: "{app}\SpectralForge Chimera.exe"; WorkingDir: "{app}"; Components: standalone
Name: "{group}\Manual"; Filename: "{app}\Documentation\MANUAL.html"
Name: "{group}\{cm:InstallGuide}"; Filename: "{app}\WINDOWS_INSTALL.txt"
Name: "{group}\{cm:UninstallProgram,SpectralForge Chimera}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\SpectralForge Chimera"; Filename: "{app}\SpectralForge Chimera.exe"; WorkingDir: "{app}"; Tasks: desktopicon; Components: standalone

[Run]
Filename: "{app}\SpectralForge Chimera.exe"; Description: "{cm:LaunchProgram,SpectralForge Chimera}"; Components: standalone; Flags: nowait postinstall skipifsilent

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
