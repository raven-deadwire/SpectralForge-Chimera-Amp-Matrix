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
for the operation. A preservation failure stops the surrounding operation
before its ordinary destructive file phase. Unsafe managed links can require
the user to repair the installation path before the installer can proceed;
the implementation does not follow a link to make cleanup succeed.

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

On removal, protected files are held without delete sharing so that earlier
Inno uninstall records cannot delete them by pathname. The guard runs before
the uninstall file phase. The prior public releases all used Inno Setup 6.7.1;
the new build enforces at least that version, supported by the exact historical
build-log and Setup-asset joins in
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
Their ownership and access follow the privileged installation context.

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
automatically discarded.

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

Results must be bound to the source actually tested. The initial proposal is
commit `88dbec2cb7c4e8a07cdf6211ee71fe14e2dd947a`, with source tree
`06dcc5197c4a118610abc7ac05af471f09e00fc3`. Follow-up review fixes and final
source/test fingerprints are recorded in this directory as they complete.

The prior #56 Python regressions were rerun; the packaging subset's first
attempt lacked CMake in this container, and its rerun using the installed
CMake runtime passed. The initial local cleanup tests passed 26 POSIX/helper/
dpkg cases and 11 Windows rule-generation cases. These do not imply Windows
or macOS installer execution. Auxiliary CI runs real installer engines with
explicitly labeled synthetic documents and records invocation counts.

The separate Linux probe installs the actual public beta.1 DEB into an
isolated dpkg root, verifies its 20 document files and original runtime bytes,
then upgrades to an internal document-lifecycle fixture. The fixture retains
the old application/plugin bytes and replaces only documentation and hooks.
It uses `--force-depends` and `--force-script-chrootless`, so it does not verify
dependency installation, the rebuilt beta.2 product, DAW operation or audio.
The report's `next_release_product_installer_verified` remains `false`.

The final local native-helper checkpoint passed **41 tests with 1 explicit
skip across 42 cases**. The skipped non-root UID/GID restoration case cannot
run in this container's single-UID mapping; it remains enabled for a capable
runner. The 16 added cases cover native document installation, deterministic
source/target/directory races and ownership restoration. The final actual
Linux DEB probe passed **7 native dpkg calls and 73 byte-preservation
assertions** with unchanged source inputs and an unchanged host dpkg database.
Its code fingerprints and exact limitations are recorded in
[LinuxDocumentUpgradeVerification.json](linux-native-dpkg/LinuxDocumentUpgradeVerification.json).

Product model/UI/DSP source, the existing state/parameter fixture and strict
publication-policy files remain unchanged from #56. Its earlier result for
4,967 host parameter names and 613 choices belongs to the exact #56 source;
it is not presented as a newly executed host test of this PR.

Native Windows/Apple fixture failures or pending jobs remain failures/pending
until corrected and executed. Actual previous-release product installer to
new product installer acceptance is separate. No helper pass or archive check
is a substitute for the existing release gates or publication approval.

Edited documents, unknown files and licensed attribution can intentionally
retain historical equipment names. The supported conclusion is removal of
verified obsolete installer payloads under the tested scenarios, **not** a
claim that every original equipment-name string has vanished from a user's
computer.
