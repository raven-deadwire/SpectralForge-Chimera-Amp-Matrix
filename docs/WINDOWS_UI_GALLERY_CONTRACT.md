# Windows pedal/rack gallery file contract

The native-panel/board rewrite in commit `3104392` changed the UI producer
(`Tests/ChimeraUITests.cpp`) to `PRE-types-*`, `POST-native-*` and `POST-fx-*`.
It removed the legacy compressor/envelope-first screenshots along with their
old controls. `build.yml` retained the obsolete upload/exclusion patterns.
The intended images therefore remained in the general UI artifact while the
dedicated pedal/rack artifact was empty. No producer or UI restoration is needed.

## Existing evidence

Run [37576072412](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/37576072412),
source `80dcd3aa50333286e632a46d3a7defd6bef451d5`, has no dedicated gallery artifact.
Its general UI artifact `11463732266` was downloaded and its archive SHA-256
verified against the GitHub artifact digest:
`588a4f1639b77c3c1152ebf5a8598c4a37804f4586ea66692380b0b08c4f8966`.
All 29 current gallery PNGs were present and passed the new file checker.
This is historical evidence, not a Windows execution of this patch.

## Corrected transport

- PRE: eight five-slot batches, `PRE-types-1.png` through `PRE-types-8.png`.
- Native POST: sections 0–2, choices 0–2 (nine PNGs).
- POST FX: section 3 choices 0–5, sections 4–5 choices 0–2 (12 PNGs).

The general artifact excludes these three current globs; the dedicated gallery
uploads them with `if-no-files-found: error`. After CTest, an always-run
Windows-only `check_ui_gallery.py` step requires every expected file and a PNG
IHDR with nonzero dimensions. A partially populated gallery must also fail,
even though upload-artifact would find some files. Upload still runs on failure
to retain any available diagnostic images. The JUCE producer continues to check
actual snapshot encoding. The checker does not decode pixels or judge appearance.

`test_ui_gallery_contract.py` checks the complete set, removal of each image,
whole-group loss, stale-only inputs, empty/invalid/zero-dimension files, CLI
failure, and workflow upload/exclusion/failure wiring. It runs in the existing
contract and platform preflight steps. The Node 24 workflow fixture digest for
`build.yml` is intentionally refreshed for this scoped contract change; other
workflow digests remain unchanged.

No UI acceptance assertion, release-gate policy, threshold, waiver, evidence
ownership mapping, or product source code changes. Gallery completeness is a
CI file transport requirement and does not promote visual acceptance to PASS.
