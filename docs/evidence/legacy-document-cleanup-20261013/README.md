# Verified legacy-document upgrade maintenance

Investigation: 2026-10-12 UTC / 2026-10-13 KST. Target: `1.3.1-beta.2`.
Naming baseline: [Draft PR #56](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/pull/56)
at `d7450883ddbeba58e44cf217072f325654c16128`.
Proposed compatibility change: [Draft PR #58](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/pull/58),
stacked on #56. Neither PR authorizes publication.

## Finding

The old-document upgrade gap is real. A clean new package establishes what it
ships; it does not establish the contents of an existing installation after
upgrade. There is a second preservation problem: some installer engines can
delete an obsolete document even when the user has edited it.

The seven public releases from `v1.0.0-beta.1` through `v1.3.1-beta.1` were
compared with #56. All 21 public Windows ZIP, macOS PKG and Linux TAR archives
were downloaded, their complete SHA-256 digests matched release metadata, and
their document bytes were inspected. The latest Linux DEB was verified
separately. The resulting inventory covers 1,057 document occurrences,
79 distinct paths and 259 exact byte variants. Of these paths, **29 are
retirement candidates, 38 are always protected license/attribution records,
and 12 are historical public-document paths used to recognize user edits**.

[HISTORICAL_INVENTORY.md](HISTORICAL_INVENTORY.md) contains every release,
platform count, retired path, mapping and evidence limitation.
[archive-audit.json](archive-audit.json) records an independent archive audit.
The build input is
[legacy-document-inventory.json](../../../Validation/legacy-document-inventory.json).
Installed copies of that data or `payload-manifest.json` cannot expand the
compiled deletion rules.

## New-package document list compared with beta.1

The exact #56 [packaging source](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/blob/d7450883ddbeba58e44cf217072f325654c16128/Tools/package_release.py)
defines these 11 shared public documents. This compatibility proposal keeps
that list unchanged:

- `MANUAL.html`
- `OPEN_BETA_RELEASE_NOTES.md`
- `THIRD_PARTY_NOTICES.md`
- `PRESETS.md`
- `EXTERNAL_BASS_IRS.md`
- `IR_DISTRIBUTION.md`
- `UPDATES.md`
- `WINDOWS_INSTALL.txt`
- `INSTALLATION.md`
- `STUDIO_ONE_TEARDOWN.md`
- `PUBLIC_PRODUCT_NAMING.md`

The document payload also includes `COPYRIGHT.txt`, the generated
`Verification.txt`, and `payload-manifest.json`. Windows additionally generates
`README.txt`. The new maintenance executables, application/plugin binaries and
optional diagnostic tools are outside the document counts below.

| Fresh document payload | Public beta.1 archive: observed | #56 source: expected | Direct comparison |
| --- | ---: | ---: | --- |
| Windows Setup / current portable ZIP layout | 24 | 15 | 10 retired documents omitted; `PUBLIC_PRODUCT_NAMING.md` added |
| macOS PKG | 20 | 14 | 7 retired documents omitted; `PUBLIC_PRODUCT_NAMING.md` added |
| Linux TAR / latest DEB | 20 | 14 | 7 retired documents omitted; `PUBLIC_PRODUCT_NAMING.md` added |

These are source-defined expectations for a newly built payload, not results
of a completed beta.2 installation. Protected old notices, modified documents
and user files can legitimately make an upgraded directory contain more files.
Windows Setup and portable ZIP route their documents differently, as detailed
below. Older portable ZIPs also have the additional evidence subtree recorded
in [HISTORICAL_INVENTORY.md](HISTORICAL_INVENTORY.md).

## Installation paths and engine behavior

| Installation | Actual document destination | Upgrade/removal behavior that needs protection |
| --- | --- | --- |
| Windows Setup | Default `C:\Program Files\SpectralForge\Chimera Amp Matrix\Documentation`; use the actual `{app}` if `/DIR` changed it | New file omission leaves old files. Inno's appended uninstall log retains earlier pathname-based deletion records. |
| Windows text documents | `{app}\*.txt`, including `COPYRIGHT.txt`, `README.txt`, `WINDOWS_INSTALL.txt` and `Verification.txt` | Repair can overwrite edited current text. Old uninstall records can delete it later. |
| Windows portable ZIP | The user's extraction directory; documents are at the extracted stage root rather than Setup's `Documentation` mapping | There is no installer registration or fixed destination. Overlay extraction can leave old files, and Setup cleanup must not search for or claim ownership of arbitrary portable directories. |
| macOS PKG | `/Library/Application Support/SpectralForge/Chimera/Documentation` | This is separate from the app, VST3 and AU bundles. Bundle upgrade settings do not prove removal of obsolete files here. The previous package has no document-maintenance hooks. |
| Linux TAR `install.sh` | `$HOME/.local/share/doc/chimera-amp-matrix` | The previous `cp -R` merges files and leaves absent old files. A separate prepare/check does not make a later pathname-based copy safe against path replacement. |
| Linux DEB | `/usr/share/doc/chimera-amp-matrix` | Ordinary package documents are not conffiles. dpkg can remove obsolete modified documents before `postinst`, so preservation must happen in `preinst`. |

These are document roots, not search starting points. Maintenance does not
walk user preset, IR, application state or project directories. Windows's
personal IR location `%ProgramData%\SpectralForge\Chimera\IRs` remains outside
the rules. So do `/Applications/SpectralForge Chimera.app`, the macOS VST3/AU
bundles, Linux application/VST3 paths, and Windows application/plugin files.

The seven public releases contain no MSIX, APPX, MSIX bundle or MSI asset.
MSIX output is an internal review package, not an established public-upgrade
path. Portable Windows ZIP users should extract into a new directory; this
proposal does not automatically erase their previous extraction directory.

For a direct upgrade from `v1.3.1-beta.1`, the shared flat retired files are:

- `AMP_NATIVE_DSP.md`
- `AMP_VALIDATION.md`
- `AMP_VOICES_OPEN_BETA.md`
- `FACTORY_CAB_VOICING_1_3.md`
- `PEDAL_BOARD_DSP.md`
- `POST_NATIVE_DSP.md`
- `VALIDATION_STATUS.md`

Windows adds `ARTWORK_PROMPTS.json`, `GRAPHICS_DSP_UPDATE.md` and
`WINDOWS_SIGNING.md`: **10 files instead of 7**. Earlier upgrade chains can
also retain mapping/design documents, two non-license reference manifests
and 12 macOS/Linux research-evidence files. The complete platform scope is
17 Windows paths and 26 macOS/Linux paths, with 29 distinct paths overall.
Policy-only names such as `NEW_AMP_DSP.md` and `UPDATE_TEST_BUILD.md` were not
observed in those seven public releases and receive no deletion authority.

The 36 historical `reference/*.json` records with licensing or attribution
fields are protected along with `COPYRIGHT.txt` and
`THIRD_PARTY_NOTICES.md`. Deleting the entire `reference` or Documentation
directory would violate this boundary.

## Maintenance contract

A historical research file is eligible for retirement only when all of the
following agree: its exact relative path, platform, regular-file status,
supported size, and SHA-256 of the raw bytes actually shipped in a verified
archive. Names and textual equipment-name matches never authorize deletion.
The inventory contains measured Windows CRLF variants; it does not add
theoretical LF/CRLF conversions, trimmed text or decoded/normalized hashes.

The native POSIX helper and generated Windows installer rules are built from
this finite input. They check links and ancestors and recheck the file used
for the operation. Preparatory preservation failure blocks DEB/PKG unpack.
A Windows per-document preservation failure stops that file's normal copy;
earlier installer operations may already have run. A TAR failure after an
atomic exchange can likewise leave some current files installed. It stops
further work and retains any captured original for recovery rather than
claiming that no changes occurred. Unsafe managed links can require the user
to repair the installation path before the installer can proceed; following
a link is not a cleanup fallback. The engine and concurrency limits below
remain part of this contract.

| File state | Upgrade/repair | Removal |
| --- | --- | --- |
| Verified, unmodified retired research bytes | Retire the exact file | May retire the exact file |
| Modified or unrecognized bytes at a historical retired path | Keep the file; restore it if a package engine removed it after preservation | Keep or restore the file |
| Edited current manual/support document | Preserve an exact checked copy before installing current package bytes | Preserve/restore the edited bytes |
| Required copyright, notice or licensed metadata | Preserve prior bytes even if current notice text changes | Preserve the file |
| Unknown filename, personal preset, IR or project | Outside cleanup scope | Outside cleanup scope |
| Linked, changed-during-check or otherwise unsafe managed path | Refuse unsafe handling; do not follow the path to another file | Refuse unsafe handling or retain the link/file |

Preserved copies are create-new, content-addressed records; they never replace
an existing different file. A pending operation is identified by its compiled
rule digest and operation type. A damaged or incompatible journal blocks
automatic continuation instead of silently discarding preservation evidence.
Recovery restores missing documents and leaves newer edits in place. It does
not claim to roll back all application binaries or survive every possible
power-loss/filesystem failure.

### Windows

`Installer/LegacyDocumentCleanup.iss` keeps directory and file handles,
computes SHA-256 through the inspected file handle, and uses handle-based
deletion for verified bytes. Current edited documents receive a checked copy
under `{app}\PreservedDocuments\<SHA-256>\<relative-path>` before the existing
repair behavior refreshes the packaged file.

The follow-up implementation holds parent directories with actual directory
read access and read/write sharing, while denying delete sharing. Read/write
sharing is needed for Inno's child-file replacement operations; denying delete
sharing guards against renaming or deleting the held parent. This does not
claim to block every possible in-place directory change.

On removal, protected files are held without delete sharing so that earlier
Inno uninstall records cannot delete them by pathname. Verified deletion
handles remain open until uninstall finishes: keeping their objects in the
delete-pending state prevents a later-created user file from occupying a
pathname still mentioned by an old uninstall record. For a missing path in
the finite compiled document list, the follow-up code creates only an
exclusive zero-byte `CREATE_NEW` guard with `DELETE_ON_CLOSE`. It never
replaces a file that appeared concurrently. Guards precede the uninstall
file phase and are closed on normal cancellation or completion. Directories
created for guards are removed only when empty, without recursive deletion.
Forced process termination closes the guard handles through the OS but can
leave empty guard directories. The native Inno fixture at `0bf06fe` passed
cancellation, forced process termination, blocked concurrent path creation,
and later pathname reuse. The earlier `50e40f9` failure remains recorded below.

There is a separate remaining upgrade/repair limit: `ShouldInstallManagedDocument`
checks and snapshots the existing file, then releases its file handle before
Inno performs the ordinary copy. A new user edit or file replacement during
that Check-to-copy interval is not guaranteed to be preserved. Directory
handles and the uninstall guards do not close this interval. The tests do
not establish general safety for actively editing managed documents during
Setup's copy phase.

The prior public releases all used Inno Setup 6.7.1; the new build enforces at
least that version, supported by the exact historical build-log and Setup-asset joins in
[windows-compiler-history.json](windows-compiler-history.json).

### macOS and Linux DEB

Native pre/post hooks preserve modified and licensed documents before the
package manager changes files, then restore obsolete protected bytes and
retire known original research documents. The helper has no Python runtime
dependency. The macOS helper is universal, with native binaries used as PKG
scripts. Linux derives the helper's `Pre-Depends` from `dpkg-shlibdeps` so the
preinstall phase does not assume an unavailable C runtime.

For these system installations, copies live in the sibling
`Documentation.preserved/<SHA-256>/<relative-path>` on macOS and
`/usr/share/doc/chimera-amp-matrix.preserved/<SHA-256>/<relative-path>` on Linux.
The preservation directories follow the privileged installation context.
When a missing original is restored, its recorded owner, group and ordinary
permission bits are restored as well. Preinstall snapshots do not create a
lock spanning the package engine's separate unpack process; preservation of
active edits made after that snapshot is outside the demonstrated scenarios.

### Linux TAR

TAR installation preserves documents in
`$HOME/.local/share/doc/chimera-amp-matrix.preserved/<SHA-256>/<relative-path>`.
Independent review reproduced a destination-directory replacement between
prepare and the original `cp -R`. The revised `install-documents SOURCE
DOCROOT` command keeps the source and destination directory descriptors open
through validation, preservation, replacement and retirement. Current source
documents must match their separately compiled current size and SHA-256;
matching a historical variant alone is insufficient.

Existing targets are exchanged atomically with a checked replacement, then
the captured previous inode is inspected. Modified bytes receive a snapshot
and the original inode can also be retained under
`<SHA-256>/.open-inode-<pid>-<index>/<relative-path>` so an already open editor
can finish writing to it. Files created concurrently are not overwritten by
the missing-target path. A raced link is not followed. Unsupported atomic
exchange, unsafe paths and unstable source bytes produce failure, and a
captured original can remain beside its document in a
`.chimera-install-*/replacement` recovery entry. Recovery entries are not
automatically discarded. The command can fail after exchanging some files;
it does not undo all earlier document replacements or imply a complete
application rollback. Installer success must not be inferred from a partial
directory or a retained recovery entry.

`payload-manifest.json` needs special handling because it contains the helper
binary's own hash and is generated after the helper. Only this fixed metadata
filename has a copy exception: an open source descriptor supplies the stable
size and SHA-256 for copy verification, and previous target bytes are
preserved. Its content is never parsed as deletion authority. The existing
package payload hash inventory is not weakened to break the circularity.

### Document-only removal

The macOS helper is installed at
`/Library/Application Support/SpectralForge/Chimera/Tools/document-maintenance`.
The Linux per-user helper is installed at
`$HOME/.local/libexec/chimera-document-maintenance`; DEB uses
`/usr/libexec/chimera-document-maintenance` and maintainer scripts.

The explicit `remove-documents` operation removes only recognized, unmodified
ordinary document bytes and keeps protected/user files. It is not a complete
macOS/TAR application uninstaller. User instructions are in
[INSTALLATION.md](../../INSTALLATION.md); they do not recommend deleting the
whole documentation directory.

## Validation status and limits

Results are attached to their tested code checkpoints. The local full Python
run uses GitHub checkpoint
`50e40f906843d5bc264774ec27c9a81edddd4884`, Git tree
`eb168ebe3b9bedf6e467675c9d04b451a4572e6c`. The locally tested commit
`76889b0a6cba12248c283b930a548ce48805579b` has that same exact tree.
The final Windows, macOS and Ubuntu native fixture passes are at
`0bf06fe2a04f7e64b2870dee7d618eb6dad54e1e`, Git tree
`705ad0f50cd03cab351dcceafd6975b6c47bff5f`. That follow-up changes only the
Windows cleanup include and its native fixture; it leaves the tested POSIX
implementation and original #56 product/contract inputs unchanged. Separate
Linux product-baseline probes record every source input's SHA256 and Git blob.
Later evidence or standalone validation-tool commits do not convert these
results into a new exact-SHA release-gate pass.

The full three-platform document workflow completed successfully at this
code checkpoint. The [workflow observation](native-ci-0bf06fe/workflow-observation.json)
also records the separate product build: its validation-contract job passed,
but its three native product builds were still queued at the observation.

| Evidence | Recorded result | Scope and source boundary |
| --- | --- | --- |
| Local Python regressions | **262 PASS, 4 explicit SKIP / 266 cases** | Exact local/GitHub trees match. Includes #56's 210 passing cases and 3 existing Windows-only skips, plus 52 passing new cases and 1 local UID-mapping skip. [Result](local-regressions.json), [raw log](local-regressions.log). |
| Ubuntu native helper CI | **42 / 42 PASS; no skips** | Exact `0bf06fe` source. Synthetic helper and real-dpkg fixtures; deterministic Linux pathname races; actual UID/GID and mode restoration. [Job 114915651265](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38280283924/job/114915651265), [final evidence](native-ci-0bf06fe/ubuntu/). The previous `50e40f9` pass and raw log also remain in [native-ci-50e40f9](native-ci-50e40f9/). |
| Actual public beta.1 DEB plus successor document fixture | **7 / 7 native dpkg calls and 73 byte-preservation assertions PASS** | Exact baseline archive and recorded working-tree SHA256/Git blob IDs. Old app/plugin bytes remain fixed; no rebuilt beta.2 product acceptance. [Result](linux-native-dpkg/LinuxDocumentUpgradeVerification.json). |
| Windows Inno document fixtures | **12 native invocations; 8 check groups PASS at `0bf06fe`** | [Job 114915651402](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38280283924/job/114915651402) compiled and ran actual Inno 6.7.1 fixtures: fresh, upgrade, twice-repair, historical-log removal, lock/link safety, guard cancellation and forced termination. [Result and artifact provenance](native-ci-0bf06fe/windows-ci-observation.json). This uses synthetic historical documents, not the public beta.1 product Setup. |
| Earlier Windows Inno checkpoint | **FAILED at `50e40f9`; corrected in the later native pass** | Parent sharing blocked Inno's child-file replacement. [Failed job](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38279252685/job/114912219710), [failure observation](native-ci-50e40f9/later-ci-observation.json), [decoded job log](native-ci-50e40f9/windows-job-114912219710.log). |
| macOS native helper / Apple PKG fixtures | **4 / 4 actual Apple installer calls and 4 lifecycle groups PASS at `0bf06fe`** | [Job 114915650894](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38280283924/job/114915650894): synthetic PKG fresh/upgrade/repair plus document-only removal. [Native PKG result](native-ci-0bf06fe/macos/artifact/macos-pkg-fixture/MacOSDocumentUpgradeVerification.json). Helper methods: 33 PASS, 6 method SKIP, plus one skipped dpkg class containing 3 unexecuted methods. This is not a public historical PKG to rebuilt product PKG test. |
| Actual public beta.1 Setup to current product Setup | **Implemented; execution pending** | Added to the existing product installer verification flow. Synthetic Inno fixture results cannot certify this path. |
| Complete Linux TAR `install.sh` lifecycle | **4 / 4 shell installations; 80 byte assertions PASS** | The actual public beta.1 TAR and current full wrapper were executed in the unchanged real account HOME, after proving all eight product targets absent. Current-fixture fresh install, real baseline install, upgrade, repair and installed-helper document removal passed. Old app/plugin bytes stayed fixed; app/VST3/icon/desktop/helper installation bytes were checked. [Result](linux-native-tar/final/LinuxTarDocumentUpgradeVerification.json), [provenance and prior probe assertion correction](linux-native-tar/verification.json). |

The local helper subset passed **41 cases and skipped 1 across 42 cases**.
That skip was the different-UID/GID restoration case, which this container's
single-UID mapping cannot execute. The Ubuntu run subsequently executed and
passed it, so its 42/42 result has no skip. The 16 cases added after the first
26 cover native document installation, deterministic source/target/directory
races and ownership restoration. The separate 11 Windows Python tests verify
rule generation, not execution of the Windows installer.

The packaging subset's first local attempt lacked CMake. The final full run
used the installed CMake runtime and passed without suppressing checks. Local
tracked sources were clean before and after that run; the logs were saved
afterward. Auxiliary CI uses explicitly labeled synthetic documents and
records the actual installer invocations.

The separate Linux probe installs the actual public beta.1 DEB into an
isolated dpkg root, verifies its 20 document files and original runtime bytes,
then upgrades to an internal document-lifecycle fixture. The fixture retains
the old application/plugin bytes and replaces only documentation and hooks.
It uses `--force-depends` and `--force-script-chrootless`, so it does not verify
dependency installation, the rebuilt beta.2 product, DAW operation or audio.
The report's `next_release_product_installer_verified` remains `false`.

The actual Linux DEB probe also verified unchanged source inputs and an
unchanged host dpkg status database. Its source input hashes, Git blob IDs,
native helper/fixture hashes, and every byte-preservation assertion are
recorded in the linked JSON result. Archive inspection and this isolated
native lifecycle remain different evidence categories.

The separate TAR probe closes the full shell-wrapper gap for the tested Linux
environment. It does not change `HOME`, create an account, install dependencies
or launch the application. The new-package document fixture contains all 14
expected document files, including copy-only payload metadata generated after
the native helper. Its seven check groups cover both a fresh current fixture
and an actual beta.1 installation followed by upgrade, repair and document
removal. All 21 source-input hashes were unchanged during execution.

Both the fresh-fixture cycle and final test housekeeping restored the eight
initially absent Chimera targets and newly created empty ancestors. Housekeeping
used finite known paths and raw size/hash checks, including bounded helper
inode-retention paths; it was not a product application uninstaller. An initial
probe assertion incorrectly demanded a separate backup of an unchanged
copyright file. The exact original bytes were correctly retained in place,
the assertion was fixed without changing production code, and that initial
attempt also restored the scoped HOME state. Its source and failed result are
retained alongside the final successful evidence.

Product model/UI/DSP source, the existing state/parameter fixture and strict
publication-policy files remain unchanged from #56. Its earlier result for
4,967 host parameter names and 613 choices belongs to the exact #56 source;
it is not presented as a newly executed host test of this PR. The exact Git
object and file comparisons, including all 18 unchanged input hashes from
the actual Linux DEB probe, are recorded in
[compatibility-checkpoint.json](compatibility-checkpoint.json).

Native Windows/Apple fixture failures or pending jobs remain failures/pending
until corrected and executed. Actual previous-release product installer to
new product installer acceptance is separate. No helper pass or archive check
is a substitute for the existing release gates or publication approval.

The linked `docs/INSTALLATION.md` still contains beta.1 headings and package
examples inherited from #56, while `RELEASE_CHANNEL` is `beta.2`. This existing
documentation-version mismatch remains a separate release-review item. The
current investigation does not relabel its tested document bytes as a revised
installation guide or treat those old examples as completed beta.2 validation.

Edited documents, unknown files and licensed attribution can intentionally
retain historical equipment names. The supported conclusion is removal of
verified obsolete installer payloads under the tested scenarios, **not** a
claim that every original equipment-name string has vanished from a user's
computer.
