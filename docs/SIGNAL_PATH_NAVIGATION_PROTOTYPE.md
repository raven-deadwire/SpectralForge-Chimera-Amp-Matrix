# 1.3.1 Signal Path navigation prototype

Implementation target: 1.3.1 display/navigation only. Based on PR #35 source
`b7e4f18efa295f0babf2278ca63117fbccb54143` (release/1.3.0-preparation).
Design authority: [PR #36 specification](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/blob/e97f8259078cbc6c7755e58557898b5d0f780aa5/docs/SIGNAL_PATH_EDITOR.md).
The product/package version remains that of the unmerged 1.3 candidate; this
work does not publish or claim a released 1.3.1 installer.

## Implemented behavior

- Persistent compact path in the existing gap above the editing area. PATH +
  opens a non-modal native graph; the main editor remains usable.
- Buttons with native focus, Tab traversal and Return activation. Selection
  is editor-only and remembered separately for Classic, Dual and Matrix.
- Read-only snapshot of current parameter/board/model sources; enabled/bypass,
  model, channel, solo/mute, crossover, clean tap and blend update at existing
  editor refresh cadence. Stable buttons survive parameter changes.
- PRE opens the current owner bank, amp opens the existing ALL controls for
  that lane, CAB opens the existing workspace focused on that lane. POST opens
  the existing POST editing page/section. Input, gate, Transpose, utilities and
  output focus their existing controls. Tuner has a separate inspector using
  existing A4/on/mute parameters, with no enable/mute operation on opening.
- No routing parameter, processor state field, parameter ordinal or DSP source
  is added or modified. Inactive blocks remain navigable for inspection.

## DSP topology audit

| Source | Display contract |
|---|---|
| PluginProcessor::process | Input mode/gain → tuner tap and clean gate detector → input gate if selected → Transpose → PRE → Engine → post-rig gate if selected → POST → doubler/click → output gain/tuner mute |
| PedalBoardDSP::process | The actual five owner positions; boardLowTap 0–5 after Gate/Transpose with remaining PRE latency alignment |
| PreFXChain::process | Legacy envelope/comp order → clean tap → fuzz → boost/drive in configured order; clean tap has gain-stage latency alignment |
| Engine::process, Classic | One full-range AMP → CAB → lane level/mute/polarity → sum |
| Engine::process, Dual Blend | Two full-range copies → independent AMP/CAB/level; per-lane blend weights are applied at level before sum |
| Engine::process, Dual Crossover | LR4 low/high → band tone → AMP/CAB → lane level/mute/polarity → sum, with no Dual blend weights |
| Engine::process, Matrix | Wet PRE crossover feeds MID/HIGH. LOW is replaced by the clean-tap crossover LOW. LOW tone → compressor → aligned DI versus AMP → CAB → LOW linear blend → level/mute/polarity → band sum |
| PostFXChain::process | Native/legacy bus compressor → preamp → existing EQ → modulation → delay → reverb |

The tuner/detector/clean-tap connections are distinguished from serial wet
connections. Matrix LOW AMP is processed internally even when ampon1 is off;
that switch forces its mix weight to zero. The CAB's own two microphone blend
precedes the LOW DI/AMP blend. IR phase is not implicitly removed.

The graph displays current control targets and enabled/bypass state, not
sample-level intermediate crossfade gains. Native model internal switches
(standby, meter OFF, etc.) remain in their existing editors; the graph's
processor bypass indication refers to the owning stage/instance bypass.
Classic has native amp context 0, Dual 1/2 and Matrix 3/4/5. CAB state retains
the existing three lane banks shared across modes; navigation does not invent
six independent CAB audio banks.

## Interaction boundaries

The strip occupies design coordinates (20,309,985,20); the room, heads and
microphones retain their existing area starting at y=330. The expanded graph
is its own component/window. No room/head/microphone mouse handler is altered,
no head-image knob is added, and no graph drop/reorder handler exists.
Selecting a graph block creates no host change gesture and serializes no UI
selection into presets/A-B. Closing the editor tears down its graph/tuner/CAB
windows before processor destruction. Stale context clicks are refused.

## Validation

`ChimeraSignalPathUITests` exercises the native editor and graph, synthetic
pointer down/up events on real buttons and Return activation, stage topology,
all six amp contexts, all three CAB focus lanes, 48 preset navigations, four
editor scales, host automation, A/B/project restore and editor recreation.
A processor listener counts parameter notifications and begin/end gestures;
full parameter snapshots must match before/after navigation.

Run on a Linux desktop:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCHIMERA_UI_PROBES=ON
cmake --build build --target ChimeraUITests ChimeraCabPanelStateTests --parallel 2
python3 Tools/run_linux_ui_tests.py -- ctest --test-dir build -C Release \
  -R '^(ChimeraSignalPathUITests|ChimeraNativeStateTests|ChimeraCabPanelStateTests)$' \
  --output-on-failure --no-tests=error
```

The new three-platform workflow uses the exact PR head, runs these contracts
and uploads snapshots/logs plus a standalone prototype. CAB state/scene tests
retain the existing microphone drag/unit selection and room navigation gates.
Standalone preview binaries are not installer or signing acceptance evidence.

## Remaining scope

- No common Tone EQ/Final EQ, Dynamic EQ, multiband dynamics or output limiter
  is invented or displayed. The existing POST EQ remains visible.
- No new signal-order commands, order serialization, Undo or drag editing.
  Existing PRE board operations remain existing product behavior. New graph
  order/Undo belongs to 1.4, drag to 1.5.
- Embedded head knobs remain their separate planned workstream.
- Windows/macOS CI and real Studio One host automation/lifecycle/playing
  acceptance must be observed independently of Linux native test evidence.
- Existing 1.3 release DSP/timing/installer blockers are not resolved by this
  UI-only work. Callback timing under a loaded GUI is not newly certified.

## Local evidence — October 10, 2026 KST

| Check | Observed result |
|---|---|
| Native Signal Path contracts | PASS, 63.39 seconds; each pointer click must actually select its block, AMP/CAB must open the correct existing editor; navigation host notifications/gestures = 0 |
| Native amp/POST state | PASS, 5.78 seconds |
| Existing CAB state/scene/microphone contracts | PASS, 74.78 seconds |
| Existing UI refresh contracts, final UI source | PASS, including stable metadata/panels, detached ALL lifetime, hide/reveal and all supported editor scales |
| Linux standalone prototype | Built as an ELF executable using the production editor; a preview, not installer acceptance |
| Windows/macOS native run | Pending new PR CI |
| Studio One lifecycle/automation/playing | Not exercised |

The existing native state/CAB runs preceded the final UI line/selection polish;
DSP and state serialization are unchanged. Final navigation and UI-refresh
runs use the final UI source. Snapshots are replaced rather than appended on
repeat runs. Images alone are not interaction proof: see the native click
assertions and [navigation log](validation/signal-path/navigation.log).
The local binary's embedded revision is the base commit because it was built
before this feature commit. [Source digests](validation/signal-path/provenance.json)
identify the tested working tree; exact committed-head evidence belongs to CI.

[Classic](validation/signal-path/classic.png) ·
[Dual Blend](validation/signal-path/dual-blend.png) ·
[Dual Crossover](validation/signal-path/dual-crossover.png) ·
[Matrix](validation/signal-path/matrix.png) ·
[Matrix post-gate/detector render](validation/signal-path/matrix-post-gate.png)
