# OBS 32.2.2 startup deadlock: dump evidence provenance

This analysis concerns **newly reproduced case `09-absent`**, not a retrospective dump of PR #248. It uses thread contexts and actual stack unwinding from the live process minidumps captured before forced termination. No raw stack-address scan is used.

## Evidence identity

- [Run 41377205562, attempt 1](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/41377205562)
- [Windows startup-control job 125059928546](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/41377205562/job/125059928546)
- [Artifact 13111019789](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/41377205562/artifacts/13111019789), ZIP SHA-256 `b2cda823744cd68bd35336a9f289d1ac478da4784664c9a7f3f1475159e0776f` verified locally against GitHub's digest.
- Case: `obs-startup-results-32/09-absent`, PID 6264; no Chimera module appears in the minidump module list.
- Pre-close dump: 2026-11-02 02:26:31.010 UTC, 234,766 bytes, SHA-256 `4ce609808ba372e2350a15a1234c1fda705aa7832a1a9c86ae3216e9a50fd033`.
- Shutdown-timeout dump: 2026-11-02 02:27:36.529 UTC, 234,606 bytes, SHA-256 `3e58fc402ddd499a57a62dba267984bbb032a2c11a857483ea634657afee06bd`.
- The corresponding event trace identifies the failed RPC as `CreateScene`, request id `3`, phase `probe-scene`.

The selected main and request-worker threads have identical complete unwound PC sequences in both dumps, 65.519 seconds apart.

## Binary and PDB identity

Official OBS assets were downloaded from the `obsproject/obs-studio` **32.2.2** release:

- `OBS-Studio-32.2.2-Windows-x64.zip`: SHA-256 `4d6e40e3ab155f56b30de517380566a206d74b63cdf5ad49aa596924768f97e1`.
- `OBS-Studio-32.2.2-Windows-x64-PDBs.zip`: SHA-256 `a2d2c16859ea3c2508f49c2b9a48b8ec630578334d301fbdf42718f6e473662a`.

`provenance.json` records individual binary/PDB SHA-256 values and each module's minidump base, image timestamp, image size, and CodeView GUID/age. The selected eight PE files match the minidump's timestamp, image size, and CodeView identity. The available PDBs also match their GUID and DBI stream age.

| OBS module | Matching CodeView/PDB GUID | Age |
|---|---|---:|
| obs64.exe | DEE6C0AE-F8AD-32D6-AEB4-2E2740C7BABD | 1 |
| obs.dll | A929BD4E-6352-24B6-CB56-CC45C91E8911 | 1 |
| obs-websocket.dll | 7794C3A0-19AB-47B9-C7AA-BE9C0529BC90 | 1 |
| w32-pthreads.dll | 4FBDC571-6BA6-E9A7-BEC6-1C6475495A3F | 1 |

Exact matching Windows system DLLs/public symbols were fetched from Microsoft's symbol server using the dump's image timestamp/size and CodeView GUID/age. Public-system-PDB info-stream ages can differ from the PE age after stripping (e.g. ntdll info age 4); the DBI age is 1 and matches. Both ages are explicitly retained in JSON. **Qt6Core has no PDB in this analysis**; its displayed labels are nearest PE exports, with offsets, rather than claims of exact internal function/source locations.

## Tools and reproducible procedure

Ubuntu LLVM/LLDB **19.1.1** (`19.1.1-1ubuntu1~24.04.2`) packages were downloaded from official Ubuntu repositories, checked against the package metadata's SHA-256, and extracted into scratch without global installation.

1. Extract the verified OBS binary and PDB archives.
2. Obtain matching `ntdll.dll`, `KERNELBASE.dll`, `kernel32.dll` and their PDBs from Microsoft's symbol server. The download key for a PE is `<timestamp as 8 hexadecimal digits><SizeOfImage as hexadecimal>`; for a PDB it is `<CodeView GUID without punctuation><age>`.
3. Put matching DLL/EXE files in a separate directory **without PDB files**, then perform an LLDB unwind:

```text
lldb -b
settings set target.exec-search-paths <binaries-only-directory>
target create --core <obs-run-1.test-failure.dmp> <binaries-only-directory>/obs64.exe
thread backtrace all
```

4. Symbolize the resulting module-relative frame PCs separately against the binaries alongside their matching PDBs. Keep raw PC in the evidence; use PC-1 for callers to identify the calling source line, and unadjusted PC for current frame 0. For example:

```text
llvm-symbolizer --obj=<OBS-with-PDB>/obs64.exe --relative-address --no-inlines 0x1d31ea 0x1f6e10 0x2344ba
llvm-symbolizer --obj=<OBS-with-PDB>/obs.dll --relative-address --no-inlines 0x9cbce 0x9d0d5
llvm-pdbutil dump -summary <module.pdb>
llvm-pdbutil pdb2yaml --dbi-stream <module.pdb>
```

The first command resolves `AudioMixer::AudioMixer` at `AudioMixer.cpp:208`, `OBSBasic::OBSInit` at `OBSBasic.cpp:1212`, and `OBSBasic::SourceCreated` at `OBSBasic_SceneItems.cpp:205`. Using the unadjusted return PC for the first frame would map to the following source line 209; this distinction matters because line 208 connects `source_create`.

5. Repeat the unwind with the shutdown dump and compare raw frame-PC lists. The selected threads' lists are identical.

### Tool workaround and limits

Direct LLDB PDB type parsing crashes in `llvm::codeview::LazyRandomTypeCollection::getType` on these symbols. Some `llvm-symbolizer --inlines` lookups hit the same type-parser bug. PE-only LLDB unwinding plus a separate `--no-inlines` PDB pass avoids that debugger fault. The crash is in the analysis tool, not evidence of an OBS crash. The raw PE-only unwind and the exact separate PDB lookup results are retained locally; JSON retains the selected frames and lookup values.

The dump does not independently identify Windows kernel mutex ownership. The circular-wait conclusion is supported by two contemporaneous blocked stacks and the exact matching OBS source's lock/callback order:

- [AudioMixer.cpp: source_create registration](https://github.com/obsproject/obs-studio/blob/32.2.2/frontend/widgets/AudioMixer.cpp#L208)
- [signal.c: signal mutex acquisition while connecting](https://github.com/obsproject/obs-studio/blob/32.2.2/libobs/callback/signal.c#L215)
- [signal.c: callback invocation while holding the signal mutex](https://github.com/obsproject/obs-studio/blob/32.2.2/libobs/callback/signal.c#L298)
- [OBSBasic_SceneItems.cpp: synchronous scene callback to UI](https://github.com/obsproject/obs-studio/blob/32.2.2/frontend/widgets/OBSBasic_SceneItems.cpp#L205)

The UI thread is still constructing AudioMixer and waits to connect the `source_create` signal. The WebSocket worker holds the same signal-dispatch path during `CreateScene` and waits for the UI's `AddScene` call. This reproduced host startup deadlock requires no Chimera module. There was no dump from the original #248 failure, so the original incident remains strongly consistent with this mechanism rather than independently stack-proven.

## Archive integrity recheck

A later audit found the first retained PDB ZIP truncated to 61,300,736 bytes (SHA-256 `7a4decaf260d6ad9d2063a3b4c349b7bfb49c4fe12229ac4146d0b805bbbb1f6`), with its central directory missing. The reason for this local truncation is unknown; no local repack was performed. That damaged file is not accepted as proof of the official PDB archive.

A fresh download to a different path, `verified-assets/OBS-Studio-32.2.2-Windows-x64-PDBs-verified.zip`, is 64,011,684 bytes, hashes to the official release digest `a2d2c16859ea3c2508f49c2b9a48b8ec630578334d301fbdf42718f6e473662a`, and passes the complete ZIP CRC check.

Separately, every OBS PDB actually used for this stack analysis was compared against the already-verified official **binary ZIP** (`4d6e40...`), which also includes PDBs. All four analysis PDBs match its entries byte for byte by SHA-256. `obs64.pdb` and `obs.pdb` are identical in both official archives. The `w32-pthreads.pdb` and `obs-websocket.pdb` used here are the smaller stripped PDBs from the binary ZIP; the dedicated PDB ZIP contains larger versions with the same matching CodeView identity. Their exact analysis hashes and source archive are recorded per module in JSON. The stack interpretation therefore does not depend on the damaged local ZIP or on substituting unmatched PDBs.
