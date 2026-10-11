; Build with Tools/Build-WindowsInstaller.ps1 after the Windows package stage.
#ifndef StageDir
  #error StageDir must point to the validated Windows package directory
#endif
#ifndef OutputPath
  #error OutputPath must be supplied by the build script
#endif
#ifndef ProductVersion
  #error ProductVersion must come from the validated source/payload contract
#endif
#ifndef PackageVersion
  #error PackageVersion must come from the validated source/payload contract
#endif
#define ProductName "SpectralForge Chimera"
#ifdef BuildId
  #define ProductRelease PackageVersion
  #define OutputName "SpectralForge-Chimera-update-" + BuildId + "-win64-Setup"
#else
  #define ProductRelease PackageVersion
  #define OutputName "SpectralForge-Chimera-" + PackageVersion + "-win64-Setup"
#endif

[Setup]
; Keep AppId stable across updates so Windows has one uninstall entry.
AppId=SpectralForge.ChimeraAmpMatrix
AppName={#ProductName}
AppVersion={#ProductVersion}
AppVerName={#ProductName} {#ProductRelease}
AppPublisher=RavenForge Luthier Intelligence
AppPublisherURL=https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix
DefaultDirName={autopf}\SpectralForge\Chimera Amp Matrix
DefaultGroupName=SpectralForge\{#ProductName}
DisableProgramGroupPage=yes
DisableDirPage=no
PrivilegesRequired=admin
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0
OutputDir={#OutputPath}
OutputBaseFilename={#OutputName}
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
VersionInfoCopyright=Copyright © 2026 RavenForge Luthier Intelligence. All rights reserved.
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
Source: "{#StageDir}\VST3\SpectralForge Chimera.vst3\*"; DestDir: "{code:GetVst3Dir}\SpectralForge Chimera.vst3"; Components: vst3; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#StageDir}\Standalone\SpectralForge Chimera.exe"; DestDir: "{app}"; Components: standalone; Flags: ignoreversion
Source: "{#StageDir}\*.txt"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#StageDir}\ARTWORK_PROMPTS.json"; DestDir: "{app}\Documentation"; Flags: ignoreversion
Source: "{#StageDir}\*.html"; DestDir: "{app}\Documentation"; Flags: ignoreversion
Source: "{#StageDir}\payload-manifest.json"; DestDir: "{app}\Documentation"; Flags: ignoreversion
Source: "{#StageDir}\*.md"; DestDir: "{app}\Documentation"; Flags: ignoreversion
Source: "{#StageDir}\ReferenceTools\*"; DestDir: "{app}\ReferenceTools"; Components: reference; Flags: ignoreversion recursesubdirs createallsubdirs

; Optional user-owned companion pack, supplied separately from the public build.
; Install where both VST3 and standalone (and other Windows accounts) can discover it.
; Preserve these files across uninstall. Setup never bundles restricted T3K captures.
Source: "{src}\Chimera-Personal-IRs\*.wav"; DestDir: "{commonappdata}\SpectralForge\Chimera\IRs"; Flags: external skipifsourcedoesntexist recursesubdirs createallsubdirs onlyifdoesntexist uninsneveruninstall
Source: "{src}\Chimera-Personal-IRs\*.json"; DestDir: "{commonappdata}\SpectralForge\Chimera\IRs"; Flags: external skipifsourcedoesntexist recursesubdirs createallsubdirs onlyifdoesntexist uninsneveruninstall

[InstallDelete]
; Retire only exact legacy program binaries, never user presets or IR folders.
Type: files; Name: "{app}\Chimera Amp Matrix.exe"; Components: standalone
Type: files; Name: "{commoncf64}\VST3\Chimera Amp Matrix.vst3\Contents\x86_64-win\Chimera Amp Matrix.vst3"; Components: vst3
Type: files; Name: "{code:GetPreviousVst3Dir}\SpectralForge Chimera.vst3\Contents\x86_64-win\SpectralForge Chimera.vst3"; Components: vst3; Check: Vst3PathChanged
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
english.ReferenceTools=Offline reference tools
korean.ReferenceTools=오프라인 레퍼런스 도구
english.InstallGuide=Installation guide
korean.InstallGuide=설치 안내
english.Vst3FolderTitle=VST3 installation folder
korean.Vst3FolderTitle=VST3 설치 경로
english.Vst3FolderDescription=Choose the plugin folder scanned by your DAW.
korean.Vst3FolderDescription=DAW에서 검색할 VST3 플러그인 폴더를 선택하세요.
english.Vst3FolderInfo=The standard folder works with most DAWs. If you select a custom folder, add it to your DAW's plugin search paths.
korean.Vst3FolderInfo=기본 경로는 대부분의 DAW에서 자동 검색됩니다. 다른 경로를 선택하면 DAW의 플러그인 검색 경로에도 추가하세요.
english.Vst3FolderLabel=VST3 folder:
korean.Vst3FolderLabel=VST3 폴더:
english.Vst3FolderInvalid=Choose an absolute VST3 folder path.
korean.Vst3FolderInvalid=VST3 폴더의 전체 경로를 선택하세요.

[Code]
var
  Vst3Page: TInputDirWizardPage;
  PreviousVst3Dir: String;

function GetVst3Dir(Param: String): String;
begin
  Result := Vst3Page.Values[0];
end;

function GetPreviousVst3Dir(Param: String): String;
begin
  Result := PreviousVst3Dir;
end;

function Vst3PathChanged: Boolean;
begin
  Result := CompareText(AddBackslash(PreviousVst3Dir), AddBackslash(Vst3Page.Values[0])) <> 0;
end;

procedure InitializeWizard;
var
  RequestedVst3Dir: String;
begin
  PreviousVst3Dir := GetPreviousData('Vst3Dir', ExpandConstant('{commoncf64}\VST3'));
  Vst3Page := CreateInputDirPage(wpSelectComponents,
    CustomMessage('Vst3FolderTitle'), CustomMessage('Vst3FolderDescription'),
    CustomMessage('Vst3FolderInfo'), False, '');
  Vst3Page.Add(CustomMessage('Vst3FolderLabel'));
  RequestedVst3Dir := ExpandConstant('{param:VST3DIR|}');
  if RequestedVst3Dir = '' then RequestedVst3Dir := PreviousVst3Dir;
  Vst3Page.Values[0] := RequestedVst3Dir;
end;

function ShouldSkipPage(PageID: Integer): Boolean;
begin
  Result := (PageID = Vst3Page.ID) and not WizardIsComponentSelected('vst3');
end;

function PrepareToInstall(var NeedsRestart: Boolean): String;
var
  Folder: String;
begin
  Result := '';
  if not WizardIsComponentSelected('vst3') then Exit;
  Folder := Vst3Page.Values[0];
  if not (((Length(Folder) >= 3) and (Folder[2] = ':') and (Folder[3] = '\')) or
          ((Length(Folder) > 2) and (Copy(Folder, 1, 2) = '\\'))) then
    Result := CustomMessage('Vst3FolderInvalid');
end;

procedure RegisterPreviousData(PreviousDataKey: Integer);
begin
  if WizardIsComponentSelected('vst3') then
    SetPreviousData(PreviousDataKey, 'Vst3Dir', Vst3Page.Values[0])
  else
    SetPreviousData(PreviousDataKey, 'Vst3Dir', PreviousVst3Dir);
end;

function UpdateReadyMemo(Space, NewLine, MemoUserInfoInfo, MemoDirInfo, MemoTypeInfo,
  MemoComponentsInfo, MemoGroupInfo, MemoTasksInfo: String): String;
begin
  Result := MemoDirInfo;
  if WizardIsComponentSelected('vst3') then
    Result := Result + NewLine + NewLine + CustomMessage('Vst3FolderLabel') +
      NewLine + Space + Vst3Page.Values[0];
  Result := Result + NewLine + NewLine + MemoComponentsInfo;
  if MemoTasksInfo <> '' then Result := Result + NewLine + NewLine + MemoTasksInfo;
end;
