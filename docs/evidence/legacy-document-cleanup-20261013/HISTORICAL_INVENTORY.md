# Historical installed-document inventory

Audit date: 2026-10-13 (KST). Naming-policy baseline: Draft PR #56,
`d7450883ddbeba58e44cf217072f325654c16128`. This audit concerns upgrade
compatibility; it does not change model IDs, presets, DSP, or product naming.

## Result and evidence boundary

All **21 public Windows ZIP, macOS PKG and Linux TAR archives** from the seven
releases below were downloaded and their complete SHA-256 digests and sizes
matched the GitHub release asset metadata. The audit read **1,057 document
occurrences** and verified **1,036 entries against their embedded payload
manifests**. The other 21 files are the payload manifests themselves, which were
hashed from the verified archives; these files do not list their own digest.

The resulting allowlist contains **79 distinct installed-document paths and
259 distinct raw byte variants**. Of those paths, 29 are retired research
documents, 38 are protected notices or licensing records, and 12 are retained
public-document paths whose earlier bytes are needed to distinguish untouched
files from user edits.

The inventory proves archive content and source-defined installation mapping.
**It does not prove that an installer was executed**, or that upgrade, repair,
or uninstall succeeded on any operating system. Native lifecycle results must
be recorded separately. Windows evidence is from the public ZIP produced from
the installer stage and the historical Inno Setup routing rules; this audit
does not claim to have executed or unpacked the corresponding Setup EXE.
Linux evidence in the allowlist is from the public TAR, not DEB execution.

Separately, the public v1.3.1-beta.1 Linux DEB was downloaded and its complete
digest verified. All 20 documentation files have the same raw bytes as that
release's TAR. Its control archive contains only `control`, with no maintainer
scripts. [deb-document-comparison.json](deb-document-comparison.json) records
this additional archive check; it also has `actual_install_verified: false`.

The machine-readable evidence is
[legacy-document-inventory.json](../../../Validation/legacy-document-inventory.json).
The independent offline recheck is [archive-audit.json](archive-audit.json).
Neither historical document bodies nor binary archives are added to the source
repository or to new product packages.

## Public releases compared

Counts include the embedded payload manifest, verification record, notices,
and the historical document subtree actually routed to the document location.
Windows-only ZIP evidence directories without an Inno document installation
rule are excluded from the Windows installed-document count.

| Release | Pinned source | Windows documents | macOS documents | Linux TAR documents |
| --- | --- | ---: | ---: | ---: |
| [v1.0.0-beta.1](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/releases/tag/v1.0.0-beta.1) | `95b4091af2c914188ea32672f05e00544a1d2098` | 55 | 51 | 51 |
| [v1.1.0-beta.1](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/releases/tag/v1.1.0-beta.1) | `f5b8ce7d39f69621ffe193fd4be6fbd760a6e6cf` | 61 | 57 | 57 |
| [v1.1.1-beta.1](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/releases/tag/v1.1.1-beta.1) | `91850fdca0c329983e051b113719699dba9831fd` | 62 | 58 | 58 |
| [v1.1.2-beta.1](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/releases/tag/v1.1.2-beta.1) | `88b62738fdbd9bcdca34f7b6e1092d6ed411eb94` | 64 | 72 | 72 |
| [v1.2.0-beta.1](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/releases/tag/v1.2.0-beta.1) | `d93560445c78552768a0ed592e5ec62c3c648fb6` | 65 | 73 | 73 |
| [v1.3.0-beta.1](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/releases/tag/v1.3.0-beta.1) | `7f692693f942a0f1dd1973062dc4d5d47b9be4ac` | 24 | 20 | 20 |
| [v1.3.1-beta.1](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/releases/tag/v1.3.1-beta.1) | `7b20cd9d1f90e3e2969c250712b199e893cf50cd` | 24 | 20 | 20 |

The two earliest releases do not embed a `source_sha` in their payload
manifest. Their outer archive digests and original document bytes were still
verified. The inventory records the public release's pinned source separately
and leaves the missing embedded source value `null`.

## Exact installation locations

These paths are unchanged across the seven historical packaging sources.

| Platform and installation | Document destination | Ownership boundary |
| --- | --- | --- |
| Windows Setup | `{app}\Documentation` where the default `{app}` is `%ProgramFiles%\SpectralForge\Chimera Amp Matrix` | `*.md`, `*.html`, two explicit JSON files, and historical `reference/*` are routed here. The selected app directory can differ from the default. |
| Windows Setup text documents | `{app}` | `*.txt`, including `COPYRIGHT.txt`, `WINDOWS_INSTALL.txt`, `README.txt`, and `Verification.txt`, are installed at the app root. |
| macOS PKG | `/Library/Application Support/SpectralForge/Chimera/Documentation` | A directory outside the app/VST3/AU bundles. A bundle replacement setting does not establish that these separate document files were removed. |
| Linux TAR `install.sh` | `$HOME/.local/share/doc/chimera-amp-matrix` | The historical script creates the directory and merges the new tree using `cp -R`; missing new-package files are not removed by that operation. |
| Linux DEB | `/usr/share/doc/chimera-amp-matrix` | The latest beta.1 DEB's actual document bytes match its TAR; older DEB destinations are source-defined here. Package-manager removal and modified-file preservation require separate native lifecycle evidence. |

Supporting historical sources:
[Windows installer](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/blob/7b20cd9d1f90e3e2969c250712b199e893cf50cd/Installer/Chimera.iss),
[platform packager](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/blob/7b20cd9d1f90e3e2969c250712b199e893cf50cd/Tools/package_release.py),
[Linux TAR installer](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/blob/7b20cd9d1f90e3e2969c250712b199e893cf50cd/Tools/install-linux.sh).

The VST3 directories, user settings directories, `%ProgramData%` personal IR
pack, user preset/IR locations, and DAW project directories are outside this
document-cleanup inventory. There is no recursive search for matching names.

## Retired documents with verified bytes

All paths below are relative to the platform's document root. Windows text
exceptions above do not affect these retired files. Only a matching path,
platform, exact size, and exact raw SHA-256 can authorize retirement.

| Path | Observed installed-document platforms |
| --- | --- |
| `AMP_NATIVE_DSP.md` | Windows, macOS, Linux |
| `AMP_VALIDATION.md` | Windows, macOS, Linux |
| `AMP_VOICES_OPEN_BETA.md` | Windows, macOS, Linux |
| `ARTWORK_PROMPTS.json` | Windows |
| `FACTORY_CAB_VOICING_1_3.md` | Windows, macOS, Linux |
| `FX_AND_IR_DESIGN.md` | Windows, macOS, Linux |
| `GRAPHICS_DSP_UPDATE.md` | Windows |
| `MODELS_AND_REFERENCE.md` | Windows, macOS, Linux |
| `NAM_REFERENCE_RESULTS.md` | Windows, macOS, Linux |
| `NATIVE_NAM_CALIBRATION.md` | Windows, macOS, Linux |
| `OPEN_BETA_NAM_VALIDATION.md` | Windows, macOS, Linux |
| `PEDAL_BOARD_DSP.md` | Windows, macOS, Linux |
| `POST_NATIVE_DSP.md` | Windows, macOS, Linux |
| `VALIDATION_STATUS.md` | Windows, macOS, Linux |
| `WINDOWS_SIGNING.md` | Windows |
| `reference/artwork-manifest.json` | Windows, macOS, Linux |
| `reference/native-nam-manifest.json` | Windows, macOS, Linux |
| `evidence/native-nam-20261004/additional-fit.json` | macOS, Linux |
| `evidence/native-nam-20261004/calibration.json` | macOS, Linux |
| `evidence/native-nam-20261004/core-tests.log` | macOS, Linux |
| `evidence/native-nam-20261004/gain.json` | macOS, Linux |
| `evidence/native-nam-20261004/gate-processor-tests.log` | macOS, Linux |
| `evidence/native-nam-20261004/integrated-final.log` | macOS, Linux |
| `evidence/native-nam-20261004/native-tests.log` | macOS, Linux |
| `evidence/native-nam-20261004/newamp-tests.log` | macOS, Linux |
| `evidence/native-nam-20261004/preset-levels.json` | macOS, Linux |
| `evidence/native-nam-20261004/renderer-tests.log` | macOS, Linux |
| `evidence/native-nam-20261004/source-hashes.json` | macOS, Linux |
| `evidence/native-nam-20261004/tone.json` | macOS, Linux |

Compared with a direct upgrade from v1.3.1-beta.1, the flat retired set is
**10 files on Windows** and **7 files on macOS/Linux**. Older installations can
carry additional documents from earlier upgrades. The complete historical
scope is 17 Windows paths and 26 macOS/Linux paths.

The existing naming-policy list alone was insufficient. In particular,
`WINDOWS_SIGNING.md`, `OPEN_BETA_NAM_VALIDATION.md`, `FX_AND_IR_DESIGN.md`,
`NATIVE_NAM_CALIBRATION.md` and historical nested records need an explicit
compatibility decision. Conversely, `NEW_AMP_DSP.md` and `UPDATE_TEST_BUILD.md`
were not found in these seven public releases. Their presence in a policy
does not supply file ownership or a verified cleanup digest. Private previews
and manually assembled builds may require additional archive evidence.

## Preservation and byte matching

`COPYRIGHT.txt` and `THIRD_PARTY_NOTICES.md` are always protected. Another
36 historical `reference/*.json` records contain structured license,
copyright, or attribution fields, including IR metadata. These receive
`preserve_always: true`; broad deletion of the `reference` directory would
violate that boundary. The inventory contains the exact finite path list.

Modified documents are not original installer-owned bytes for cleanup
purposes, even if they retain the same filename. They must survive upgrade,
repair, and uninstall. Unknown files, renamed copies, links, user IRs,
presets, and projects are likewise outside deletion authority. Preserved
documents and required attribution can still contain historical equipment
names; a new-package naming check cannot certify their disappearance.

The source comparison found 674 document occurrences that match the pinned
Git blob exactly and 334 Windows occurrences with observed CRLF checkout
bytes. The other 49 files were generated by packaging. No source-byte
discrepancy was found. **Source bytes are never added as alternative cleanup
hashes.** A hypothetical LF/CRLF conversion, BOM removal, whitespace trim,
or normalized hash could incorrectly classify a user-edited document as
untouched. Every allowlisted variant here was observed in a verified archive.

## Reproduce the archive audit

Download the exact 21 archive names listed in the inventory from their pinned
public releases into a local directory, then run:

```sh
python Tools/audit_legacy_document_inventory.py \
  --packages-dir /path/to/pinned-release-archives \
  --output docs/evidence/legacy-document-cleanup-20261013/archive-audit.json
```

The script hashes each complete archive, reads regular documentation members
without installing or extracting executable payloads, and verifies its exact
document set, sizes, SHA-256 variants, archive provenance, and embedded
payload-manifest entries. It fails on missing packages, mismatched bytes,
duplicate document paths, unexpected scoped links, and incomplete evidence.
It does not fetch files or regenerate/expand the cleanup allowlist.

The archive audit is an input to native lifecycle tests. Publication approval
and claims of completed installation validation cannot be inferred from it.
