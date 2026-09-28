# Chimera: five-slot preparation foundation

Base reviewed: `025d30471109649e5e3243ff1e7e89639a576b79` (2026-09-28).
This is an **isolated preparation subproject**, not a new audio-plugin release.
The production `Source/`, root `CMakeLists.txt`, factory models, host parameters,
IRs and release workflows are not modified or linked to this code.

## What is implemented

| Component | Implemented scope |
|---|---|
| `include/BoardState.h` | JUCE-independent C++20 state for 0–5 pedals. Add/replace/remove/duplicate/reorder, independent values and stored CC bindings, LOW tap, undo/redo, versioned text persistence. |
| `ui/state.js` | Matching browser state contract and preparation `.cbp` encoder/decoder. C++ → JS → C++ round trip tested. |
| `catalog.py` | 80 inventory entries: 15 existing amps, 46 existing FX variants, 8 planned amps and 11 new/candidate pedal entries. Existing 61 are inventory, NOT 61 validated replicas. |
| Control descriptors | Existing 25 PRE variants retain the old UI ranges for migration. SD-1, Distortion+, JB-2 have documented panel labels; OCD uses creator capture labels with manufacturer/range verification pending. Original octave/wah/EQ UI studies, partial ZUTA target controls, partial EICH/SVT-CL target records. |
| `audit_assets.py` | Read-only JSON/finite-value/metadata preflight for NAM; WAV/AIFF format, energy/onset/peak/tail measurements; exact-hash duplicate detection; evidence and capture-scope checks. |
| Browser preview | 5 cards, library search/category, replacement, duplicate only into a free slot, reorder, bypass, dedicated editor, stored CC mapping, LOW membership disclosure, undo/redo, export/import. **No audio or actual MIDI I/O.** |

### Five slots, not a hidden larger board

Bypassed pedals still consume slots. JB-2 uses one slot. Duplicates use their own
slot and independent ID; their CC bindings start empty to avoid accidental coupling.
The model library can grow without increasing the number of installed pedals.
Empty slots have no audio implementation. POST racks and global utilities are not
expanded or removed by this preparation work.

LOW tap is a boundary inside the same five-element ordered list. Moving across it
reports a membership change. This is **routing state**, not an implemented audio
split or phase/latency compensation engine.

### Identity and persistence

Model ID, instance ID, and display order are separate. Replacement intentionally
gets a new ID and clears old bindings. Undo, import, and deletion cannot rewind the
same-session ID allocator. Host integration must additionally namespace these IDs
per plugin/project and supply stable APVTS parameter mappings. Storing a CC binding
is not DAW automation or MIDI event-processing support.

`CHIMERA_BOARD_PREP` version 1 (`.cbp`) is a dedicated interchange format shared by
the C++ test component and browser preview. It is **not `.chimera`, VST state, or a
DAW project format**. Decoders reject overflow, duplicate IDs/keys, nonfinite values,
wrong versions, oversize text and more than five pedals. C++ validates structural
invariants; a production adapter must also enforce model-specific control ranges.
The browser importer already checks its catalog ranges.

The typed `LegacyPre` adapter preserves all four Envelope/Comp and Boost/Drive
order combinations, raw values, bypass states and `lowTap=2`. Parsing an actual
APVTS blob and migrating normalized host automation remain unimplemented. Existing
sound is untouched because no production audio path is connected to these files.

## Run the preview

Python 3.10+ is enough to generate the self-contained HTML (no network/assets/fonts):

```sh
cd Preparation
python build_preview.py
```

Open `Chimera_FiveSlot_Preview.html` in a desktop browser. New-model 0–1 values are
UI seed positions, not measured hardware tapers or factory defaults. Pending
models without a defined panel remain inventory records, not selectable mock DSP.
The library currently offers 33 editable PRE state models; all are silent.

## Run tests

Requires Python 3.10+, CMake 3.22+, a C++20 compiler, Node 18+ and these audit dependencies:

```sh
python -m pip install -r requirements.txt
python run_checks.py
```

For optional browser UI tests install Playwright and use an already installed
Chromium/Chrome executable (no automatic browser download in the runner):

```sh
python -m pip install playwright
python run_checks.py --browser /path/to/chromium
```

The runner writes logs and `reports/summary.json`, including explicit flags for
unperformed plugin/DAW/audio validation. The standalone native CMake project does
not fetch JUCE. On Windows a C++ compiler must be present; the executable path is
resolved for single- and multi-configuration generators.

## Audit user-supplied assets

```sh
python audit_assets.py /path/to/model.nam --evidence evidence.json --expected-scope amp_head --output nam-report.json
python audit_assets.py /path/to/cab.wav /path/to/cab.aiff --evidence evidence.json --expected-scope cabinet_ir --output ir-report.json
```

Start from `evidence-template.json`. Entries are keyed by file SHA-256, never by a
potentially ambiguous filename. Creator verification and A2 source review are
**reviewer attestations** with evidence URLs/dates, not facts the script has checked
online. A `SlimmableContainer` wrapper alone does not certify the network as A2.
`metadata_ready` only means the preflight policy found no unresolved blocking
metadata issue. NAM core loading, actual weight/config consistency, render tests,
input calibration, listening and rights review still have to follow.

Exit codes: 0 = metadata-ready results; 1 = invalid/review-needed inputs; 2 = bad
configuration. A successful preflight never grants redistribution rights. Even a
locally supplied `redistribution=allowed` record is not independently authorized by
this tool. T3K/other restricted capture files are not supplied in this project.

Audio inspection is read-only: no trimming, resampling or normalization. Impulse
RMS depends on file length and is not perceived loudness. Onset is the first sample
at -60 dB relative to that channel's peak; it is not a DAW latency measurement.
Near-full-scale samples are a review indicator, not proof of clipping. IRs are
linear audio assets and have **no A2 requirement**. Long reverb IRs are distinguished
by their declared scope and are not silently placed in a cabinet loader.

## What is deliberately still pending

Production EffectInstance/DSP wiring, native JUCE five-slot editor, real MIDI and
host automation, original `.chimera` import, deployment, A/B recall integration,
model-specific nonlinear algorithms, channel/control interpolation, real capture
verification and permission, acoustic/phase/latency checks, Windows DAW testing.

ZUTA/ENGL/Diezel/SVT-CL/SUNN and the other requested models remain requirements,
not implemented audio models. Generic legacy controls do not become authentic
hardware controls by being listed here. ZUTA loop/tube-pair/diode functions and
remaining SVT-CL controls are explicitly pending rather than silently invented.
Follow-up integration should keep Legacy sound preservation separate from a new
sound-engine upgrade. See existing issues #3–#5; this preparation does not close them.

## Sources used for definitions and boundaries

- Production catalog and raw parameter ranges: `Source/ModelCatalog.h`,
  `Source/AmpCatalog.h`, `Source/FXParameters.h`, pinned to the base commit above.
- BOSS SD-1: https://www.boss.info/global/products/sd-1/
- BOSS/JHS JB-2: https://www.boss.info/global/products/jb-2/
- MXR Distortion+ M104: https://www.jimdunlop.com/mxr-distortion/
- OCD capture-label source (NOT manufacturer verification): https://www.tone3000.com/tones/fulltone-ocd-v2-49361
- ZUTA GBG120: https://zutagroup.com/products/gbg120-tube-amp-by-zuta
- EICH T900: https://www.eich-amps.com/t900
- SVT-CL partial panel reference: https://ampeg.com/products/classic/svtcl/
- A2 structure/Full-Lite/Convert boundaries: https://www.tone3000.com/guides/nam-a2-the-complete-guide

No source photography, logos, commercial IRs, NAM weights, or font files are bundled.
