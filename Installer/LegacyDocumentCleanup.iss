// Compiled document identities only. There is no runtime manifest authority.
// All cleanup is nonrecursive and operates on an already verified file handle.
// Inno keeps old [Files] deletion records on upgrade. Preserved handles deny
// DELETE through the entire uninstall, including those historical records.
#ifndef DocumentRulesPath
  #error DocumentRulesPath must be generated from the reviewed inventory and current payload
#endif
#if VER < EncodeVer(6, 7, 1)
  #error Inno Setup 6.7.1 or newer is required to replace historical Chimera uninstall code safely
#endif

const
  DocRead = $80000000;
  DocWrite = $40000000;
  DocDelete = $00010000;
  DocListDirectory = $00000001;
  DocReadAttributes = $00000080;
  DocShareRead = $00000001;
  DocShareWrite = $00000002;
  DocOpenExisting = 3;
  DocCreateNew = 1;
  DocOpenReparsePoint = $00200000;
  DocBackupSemantics = $02000000;
  DocDeleteOnClose = $04000000;
  DocAttributeDirectory = $00000010;
  DocAttributeReparsePoint = $00000400;
  DocInvalidAttributes = $FFFFFFFF;
  DocDispositionInfo = 4;

type
  TDocumentFileInfo = record
    Attributes, CreationLow, CreationHigh, AccessLow, AccessHigh,
      WriteLow, WriteHigh, VolumeSerial, SizeHigh, SizeLow, Links,
      IndexHigh, IndexLow: Cardinal;
  end;
  TDocumentRule = record
    RelativePath: String;
    Bytes: Cardinal;
    SHA256: String;
    Retire, KeepAlways: Boolean;
  end;
  TDocumentHandleArray = array of THandle;
  TDocumentPending = record
    Handle: THandle;
    RelativePath: String;
    SHA256: String;
    Bytes: Cardinal;
    DeleteVerified: Boolean;
  end;

var
  DocumentRules: array of TDocumentRule;
  DocumentRulesLoaded: Boolean;
  DocumentParentLocks: TDocumentHandleArray;
  DocumentPending: array of TDocumentPending;
  DocumentUninstallActive: Boolean;
  DocumentCreatedDirectories: array of String;

function DocCreateFile(Name: String; Access, Share: Cardinal; Security: THandle;
  Creation, Flags: Cardinal; Template: THandle): THandle;
  external 'CreateFileW@kernel32.dll stdcall';
function DocCloseHandle(Handle: THandle): Boolean;
  external 'CloseHandle@kernel32.dll stdcall';
function DocGetFileInfo(Handle: THandle; var Info: TDocumentFileInfo): Boolean;
  external 'GetFileInformationByHandle@kernel32.dll stdcall';
function DocSetDisposition(Handle: THandle; InfoClass: Cardinal;
  var Disposition: Byte; Bytes: Cardinal): Boolean;
  external 'SetFileInformationByHandle@kernel32.dll stdcall';
function DocGetAttributes(Name: String): Cardinal;
  external 'GetFileAttributesW@kernel32.dll stdcall';
function DocLastError: Cardinal;
  external 'GetLastError@kernel32.dll stdcall';
function DocFlushFileBuffers(Handle: THandle): Boolean;
  external 'FlushFileBuffers@kernel32.dll stdcall';

function DocValidHandle(Handle: THandle): Boolean;
begin
  Result := (Handle <> 0) and (Handle <> THandle(-1));
end;

function OpenDocumentPreservationHandle(Filename: String): THandle;
begin
  // Attribute-only access bypasses Windows sharing checks and cannot be used
  // as a preservation lock. Request real data read access, denying deletion
  // even if another application's original handle closes during uninstall.
  Result := DocCreateFile(Filename, DocRead, DocShareRead, 0,
    DocOpenExisting, DocOpenReparsePoint or DocBackupSemantics, 0);
end;

procedure AddDocumentRule(RelativePath: String; Bytes: Cardinal; SHA256: String;
  Retire, KeepAlways: Boolean);
var
  I: Integer;
begin
  I := GetArrayLength(DocumentRules);
  SetArrayLength(DocumentRules, I + 1);
  DocumentRules[I].RelativePath := RelativePath;
  DocumentRules[I].Bytes := Bytes;
  DocumentRules[I].SHA256 := Lowercase(SHA256);
  DocumentRules[I].Retire := Retire;
  DocumentRules[I].KeepAlways := KeepAlways;
end;

#include DocumentRulesPath

procedure EnsureDocumentRules;
begin
  if not DocumentRulesLoaded then begin
    LoadDocumentRules;
    DocumentRulesLoaded := True;
  end;
end;

procedure CloseDocumentHandles(var Handles: TDocumentHandleArray);
var
  I: Integer;
begin
  for I := GetArrayLength(Handles) - 1 downto 0 do
    if DocValidHandle(Handles[I]) then DocCloseHandle(Handles[I]);
  SetArrayLength(Handles, 0);
end;

procedure RememberDocumentHandle(var Handles: TDocumentHandleArray; Handle: THandle);
var
  I: Integer;
begin
  I := GetArrayLength(Handles);
  SetArrayLength(Handles, I + 1);
  Handles[I] := Handle;
end;

procedure RememberCreatedDocumentDirectory(Directory: String);
var
  I: Integer;
begin
  if not DocumentUninstallActive then Exit;
  I := GetArrayLength(DocumentCreatedDirectories);
  SetArrayLength(DocumentCreatedDirectories, I + 1);
  // CreateDir receives a trailing slash; RemoveDir should not receive one.
  DocumentCreatedDirectories[I] := Copy(Directory, 1, Length(Directory) - 1);
end;

function DocumentRelativePath(Filename: String): String;
var
  Root: String;
begin
  Root := AddBackslash(ExpandFileName(ExpandConstant('{app}')));
  Filename := ExpandFileName(Filename);
  Result := '';
  if CompareText(Copy(Filename, 1, Length(Root)), Root) = 0 then
    Result := Copy(Filename, Length(Root) + 1, MaxInt);
end;

function LockDocumentParents(Filename: String; CreateMissing: Boolean;
  var Handles: TDocumentHandleArray): Boolean;
var
  I: Integer;
  Parent: String;
  Handle: THandle;
  Info: TDocumentFileInfo;
begin
  Result := False;
  Filename := ExpandFileName(Filename);
  // Network/reparse roots are not cleanup destinations. An unprovable path is
  // preserved; no attempt is made to discover or normalize a target elsewhere.
  if (Length(Filename) < 4) or (Filename[2] <> ':') or (Filename[3] <> '\') then Exit;
  for I := 3 to Length(Filename) do
    if Filename[I] = '\' then begin
      Parent := Copy(Filename, 1, I);
      if CreateMissing and not DirExists(Parent) then begin
        if not CreateDir(Parent) then Exit;
        RememberCreatedDocumentDirectory(Parent);
      end;
      // FILE_LIST_DIRECTORY participates in sharing checks. Attributes alone
      // do not stop a directory rename/reparse substitution after validation.
      // Share directory writes so Inno can rename child payload files into
      // place; denying DELETE still prevents this parent itself being moved.
      Handle := DocCreateFile(Parent, DocListDirectory or DocReadAttributes,
        DocShareRead or DocShareWrite, 0,
        DocOpenExisting, DocOpenReparsePoint or DocBackupSemantics, 0);
      if not DocValidHandle(Handle) then Exit;
      RememberDocumentHandle(Handles, Handle);
      if not DocGetFileInfo(Handle, Info) then Exit;
      if ((Info.Attributes and DocAttributeReparsePoint) <> 0) or
         ((Info.Attributes and DocAttributeDirectory) = 0) then Exit;
    end;
  Result := True;
end;

function HashDocumentHandle(Handle: THandle): String;
var
  Stream: THandleStream;
begin
  Stream := THandleStream.Create(Handle);
  try
    Stream.Position := 0;
    Result := Lowercase(GetSHA256OfStream(Stream));
  finally
    Stream.Free;
  end;
end;

function IsRegularSingleLinkDocument(Info: TDocumentFileInfo): Boolean;
begin
  Result := ((Info.Attributes and (DocAttributeDirectory or DocAttributeReparsePoint)) = 0)
    and (Info.Links = 1) and (Info.SizeHigh = 0) and (Info.SizeLow <= 67108864);
end;

function IsKnownDocument(RelativePath: String; Bytes: Cardinal; SHA256: String;
  RetiredOnly: Boolean): Boolean;
var
  I: Integer;
begin
  Result := False;
  EnsureDocumentRules;
  for I := 0 to GetArrayLength(DocumentRules) - 1 do
    if (CompareText(DocumentRules[I].RelativePath, RelativePath) = 0) and
       (DocumentRules[I].Bytes = Bytes) and (DocumentRules[I].SHA256 = SHA256) and
       not DocumentRules[I].KeepAlways and
       ((not RetiredOnly) or DocumentRules[I].Retire) then begin
      Result := True;
      Exit;
    end;
end;

function MustKeepDocument(RelativePath: String): Boolean;
var
  I: Integer;
begin
  Result := False;
  for I := 0 to GetArrayLength(DocumentRules) - 1 do
    if (CompareText(DocumentRules[I].RelativePath, RelativePath) = 0) and
       DocumentRules[I].KeepAlways then Result := True;
end;

function DeleteVerifiedDocumentHandle(Handle: THandle; Bytes: Cardinal; SHA256: String): Boolean;
var
  Info: TDocumentFileInfo;
  Disposition: Byte;
begin
  Result := False;
  if not DocGetFileInfo(Handle, Info) then Exit;
  if not IsRegularSingleLinkDocument(Info) or (Info.SizeLow <> Bytes) then Exit;
  if HashDocumentHandle(Handle) <> SHA256 then Exit;
  Disposition := 1;
  Result := DocSetDisposition(Handle, DocDispositionInfo, Disposition, 1);
end;

function PreserveModifiedDocument(Handle: THandle; RelativePath: String;
  Bytes: Cardinal; SHA256: String): Boolean;
var
  Destination: String;
  Target: THandle;
  Parents: TDocumentHandleArray;
  SourceStream, TargetStream: THandleStream;
  Buffer: AnsiString;
  Copied: Cardinal;
  ReadCount, WriteCount: Integer;
  Info: TDocumentFileInfo;
begin
  Result := False;
  Target := 0;
  Destination := ExpandConstant('{app}') + '\PreservedDocuments\' + SHA256 + '\' + RelativePath;
  try
    if not LockDocumentParents(Destination, True, Parents) then Exit;
    Target := DocCreateFile(Destination, DocRead or DocWrite, DocShareRead, 0,
      DocCreateNew, DocOpenReparsePoint, 0);
    if not DocValidHandle(Target) then begin
      // Never overwrite a previous snapshot, including a same-name user file.
      Target := DocCreateFile(Destination, DocRead, DocShareRead, 0,
        DocOpenExisting, DocOpenReparsePoint, 0);
      if not DocValidHandle(Target) then Exit;
      if not DocGetFileInfo(Target, Info) then Exit;
      if not IsRegularSingleLinkDocument(Info) or (Info.SizeLow <> Bytes) then Exit;
      Result := HashDocumentHandle(Target) = SHA256;
    end else begin
      SourceStream := THandleStream.Create(Handle);
      TargetStream := THandleStream.Create(Target);
      try
        SourceStream.Position := 0;
        SetLength(Buffer, 32768);
        Copied := 0;
        repeat
          ReadCount := SourceStream.Read(Buffer, 32768);
          if ReadCount < 0 then Exit;
          if ReadCount > 0 then begin
            WriteCount := TargetStream.Write(Buffer, ReadCount);
            if WriteCount <> ReadCount then Exit;
            Copied := Copied + ReadCount;
          end;
        until ReadCount = 0;
        Result := (Copied = Bytes) and DocFlushFileBuffers(Target) and (HashDocumentHandle(Target) = SHA256)
          and (HashDocumentHandle(Handle) = SHA256);
      finally
        TargetStream.Free;
        SourceStream.Free;
      end;
    end;
    if Result then Log('Chimera document preservation: verified copy ' + Destination);
  finally
    if DocValidHandle(Target) then DocCloseHandle(Target);
    CloseDocumentHandles(Parents);
  end;
end;

function ShouldInstallManagedDocument: Boolean;
var
  Filename, RelativePath, SHA256: String;
  Handle: THandle;
  Parents: TDocumentHandleArray;
  Info: TDocumentFileInfo;
  Attr, ErrorCode: Cardinal;
begin
  Result := False;
  EnsureDocumentRules;
  Filename := ExpandConstant(CurrentFilename);
  RelativePath := DocumentRelativePath(Filename);
  if RelativePath = '' then RaiseException('Cannot prove the documentation installation path.');
  Handle := 0;
  try
    // Lock/create each parent separately and reject directory junctions. These
    // handles live until Setup exits, covering the subsequent Inno file copy.
    if not LockDocumentParents(Filename, True, DocumentParentLocks) then
      RaiseException('Documentation path is linked or inaccessible; existing files were preserved.');
    Attr := DocGetAttributes(Filename);
    ErrorCode := DocLastError;
    if (Attr = DocInvalidAttributes) and ((ErrorCode = 2) or (ErrorCode = 3)) then begin
      Result := True;
      Exit;
    end;
    Handle := DocCreateFile(Filename, DocRead, DocShareRead, 0,
      DocOpenExisting, DocOpenReparsePoint or DocBackupSemantics, 0);
    if not DocValidHandle(Handle) or not DocGetFileInfo(Handle, Info) then
      RaiseException('Cannot preserve existing documentation; installation stopped before overwrite.');
    if not IsRegularSingleLinkDocument(Info) then
      RaiseException('Existing documentation is linked or not a regular file; installation stopped before overwrite.');
    SHA256 := HashDocumentHandle(Handle);
    if not IsKnownDocument(RelativePath, Info.SizeLow, SHA256, False) then
      if not PreserveModifiedDocument(Handle, RelativePath, Info.SizeLow, SHA256) then
        RaiseException('Cannot verify a copy of modified documentation; installation stopped before overwrite.');
    Result := True;
  finally
    if DocValidHandle(Handle) then DocCloseHandle(Handle);
    CloseDocumentHandles(Parents);
  end;
end;

procedure CleanupRetiredDocuments;
var
  I: Integer;
  Filename, SHA256: String;
  Handle: THandle;
  Parents: TDocumentHandleArray;
  Info: TDocumentFileInfo;
begin
  EnsureDocumentRules;
  for I := 0 to GetArrayLength(DocumentRules) - 1 do
    if DocumentRules[I].Retire and not DocumentRules[I].KeepAlways then begin
      Handle := 0;
      try
        Filename := ExpandConstant('{app}') + '\' + DocumentRules[I].RelativePath;
        if LockDocumentParents(Filename, False, Parents) then begin
          Handle := DocCreateFile(Filename, DocRead or DocDelete, DocShareRead, 0,
            DocOpenExisting, DocOpenReparsePoint or DocBackupSemantics, 0);
          if DocValidHandle(Handle) and DocGetFileInfo(Handle, Info) and
             IsRegularSingleLinkDocument(Info) then begin
            SHA256 := HashDocumentHandle(Handle);
            if IsKnownDocument(DocumentRules[I].RelativePath, Info.SizeLow, SHA256, True) then begin
              if DeleteVerifiedDocumentHandle(Handle, Info.SizeLow, SHA256) then
                Log('Chimera document cleanup: removed verified legacy file ' + Filename)
              else Log('Chimera document cleanup: preserved locked/read-only legacy file ' + Filename);
            end else Log('Chimera document cleanup: preserved modified/unknown legacy file ' + Filename);
          end;
        end;
      except
        Log('Chimera document cleanup: preserved unverified legacy file ' + Filename);
      end;
      if DocValidHandle(Handle) then DocCloseHandle(Handle);
      CloseDocumentHandles(Parents);
    end;
end;

procedure ReleaseDocumentPreservation;
var
  I: Integer;
  Attr: Cardinal;
begin
  for I := GetArrayLength(DocumentPending) - 1 downto 0 do
    if DocValidHandle(DocumentPending[I].Handle) then DocCloseHandle(DocumentPending[I].Handle);
  SetArrayLength(DocumentPending, 0);
  CloseDocumentHandles(DocumentParentLocks);
  // Only directories created for this uninstall's finite guard paths qualify.
  // RemoveDir is nonrecursive and fails if a user has put any files inside.
  for I := GetArrayLength(DocumentCreatedDirectories) - 1 downto 0 do begin
    Attr := DocGetAttributes(DocumentCreatedDirectories[I]);
    if (Attr <> DocInvalidAttributes) and ((Attr and DocAttributeDirectory) <> 0)
       and ((Attr and DocAttributeReparsePoint) = 0) then
      if not RemoveDir(DocumentCreatedDirectories[I]) then
        Log('Chimera document preservation: retained nonempty/unremovable guard directory ' + DocumentCreatedDirectories[I]);
  end;
  SetArrayLength(DocumentCreatedDirectories, 0);
  DocumentUninstallActive := False;
end;

function InitializeDocumentUninstall: Boolean;
var
  I, J, N: Integer;
  Filename, SHA256: String;
  Handle: THandle;
  Info: TDocumentFileInfo;
  AlreadySeen, CanDelete, CreatedGuard: Boolean;
  ErrorCode: Cardinal;
begin
  Result := False;
  EnsureDocumentRules;
  DocumentUninstallActive := True;
  try
    for I := 0 to GetArrayLength(DocumentRules) - 1 do begin
      AlreadySeen := False;
      for J := 0 to I - 1 do
        if CompareText(DocumentRules[I].RelativePath, DocumentRules[J].RelativePath) = 0 then AlreadySeen := True;
      if not AlreadySeen then begin
        Filename := ExpandConstant('{app}') + '\' + DocumentRules[I].RelativePath;
        // A missing pathname is also covered by old Inno deletion records.
        // Lock its parents before creating an exclusive, zero-byte guard so
        // a concurrent file creation cannot be claimed by those records.
        if not LockDocumentParents(Filename, True, DocumentParentLocks) then
          RaiseException('Cannot secure a documentation path against historical uninstall records.');
        CreatedGuard := False;
        CanDelete := True;
        Handle := DocCreateFile(Filename, DocRead or DocDelete, DocShareRead, 0,
          DocOpenExisting, DocOpenReparsePoint or DocBackupSemantics, 0);
        if not DocValidHandle(Handle) then begin
          ErrorCode := DocLastError;
          CanDelete := False;
          if (ErrorCode = 2) or (ErrorCode = 3) then begin
            // CREATE_NEW never overwrites a file that appeared meanwhile.
            // DELETE_ON_CLOSE removes only our own guard on cancellation or
            // process termination, without a deferred/reboot deletion queue.
            Handle := DocCreateFile(Filename, DocRead or DocDelete, DocShareRead, 0,
              DocCreateNew, DocOpenReparsePoint or DocDeleteOnClose, 0);
            if not DocValidHandle(Handle) then
              RaiseException('Cannot guard an absent historical document; uninstall stopped before deleting files.');
            CreatedGuard := True;
          end else Handle := OpenDocumentPreservationHandle(Filename);
        end;
        if not DocValidHandle(Handle) then
          RaiseException('Cannot lock a document for preservation; uninstall stopped before deleting files.');
        N := GetArrayLength(DocumentPending);
        SetArrayLength(DocumentPending, N + 1);
        DocumentPending[N].Handle := Handle;
        DocumentPending[N].RelativePath := DocumentRules[I].RelativePath;
        DocumentPending[N].DeleteVerified := False;
        if CreatedGuard then begin
          if not DocGetFileInfo(Handle, Info) or not IsRegularSingleLinkDocument(Info)
             or (Info.SizeLow <> 0) then
            RaiseException('Cannot verify an exclusive empty document guard.');
          Log('Chimera document preservation: guarded absent historical pathname ' + Filename);
        end else begin
          if DocGetFileInfo(Handle, Info) and IsRegularSingleLinkDocument(Info) and CanDelete
             and not MustKeepDocument(DocumentRules[I].RelativePath) then begin
            SHA256 := HashDocumentHandle(Handle);
            DocumentPending[N].SHA256 := SHA256;
            DocumentPending[N].Bytes := Info.SizeLow;
            DocumentPending[N].DeleteVerified := IsKnownDocument(DocumentRules[I].RelativePath,
              Info.SizeLow, SHA256, False);
          end;
          if not DocumentPending[N].DeleteVerified then
            Log('Chimera document preservation: locked in place through uninstall ' + Filename);
        end;
      end;
    end;
    Result := True;
  except
    Log('Chimera document preservation: uninstall refused: ' + GetExceptionMessage);
    ReleaseDocumentPreservation;
  end;
end;

procedure DeleteVerifiedUninstallDocuments;
var
  I: Integer;
begin
  // This is called only after the user has committed to uninstall. Do not
  // delete anything in InitializeUninstall, which precedes that confirmation.
  for I := 0 to GetArrayLength(DocumentPending) - 1 do
    if DocumentPending[I].DeleteVerified then begin
      if DeleteVerifiedDocumentHandle(DocumentPending[I].Handle,
        DocumentPending[I].Bytes, DocumentPending[I].SHA256) then begin
        // Keep the delete-pending object open until all historical Inno log
        // records have run. Closing now would allow a new user file at this
        // pathname to be deleted by a later old log record.
        Log('Chimera document cleanup: verified owned file pending removal at uninstall ' + DocumentPending[I].RelativePath);
      end else Log('Chimera document preservation: retained unverified/unremovable file ' + DocumentPending[I].RelativePath);
    end;
end;
