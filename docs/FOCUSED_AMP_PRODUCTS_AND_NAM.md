# Focused Náströnd / Niflheimr products and NAM delivery

Updated: **2026-10-12 KST**. Status: **PLANNED; separate product targets and
Niflheimr NAM delivery are not implemented or qualified by this document**.

## Owner decision and verified baseline

The October 12 follow-up requires:

1. A five-channel Niflheimr NAM pack, alongside the existing Náströnd NAM work.
2. Separately installable Náströnd and Niflheimr VST products.
3. A simpler playing/editing workflow, using Neural DSP and Odeholm Audio as
   references for focused amp products.

Product-source audit: main
[3817246f](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/commit/3817246f779a50f19b5378ec9661a2275e6b91b2).
Both five-channel amps already exist as native DSP inside Chimera. The current
CMake project is a single Chimera product; no NAM runtime is linked into it.
Náströnd NAM evidence was inspected separately at PR #22 head
[859fea9b](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/commit/859fea9bc49c4b9981c926a314655e485c51027c).
Do not mix these source identities.

This specification changes product scope, not released catalog counts,
serialized Chimera identities, fidelity thresholds or historical acceptance.
The [central roadmap](DEVELOPMENT_ROADMAP_2026_2027.md) owns all dates; the
catalog JSON mirrors them.

## Three products, one shared amp implementation

| Product | Role | Routing and inventory | First-format scope |
|---|---|---|---|
| SpectralForge Chimera | Full guitar/bass amp-and-effects environment | Existing Classic/Dual/Matrix and broad catalog | Existing formats; AAX Native plan remains separate |
| SpectralForge Náströnd | Focused five-channel guitar amp | One selected Náströnd channel, one CAB section, curated PRE/POST and one main EQ | Separate VST3; Windows first reviewable package, macOS Universal separately verified |
| SpectralForge Niflheimr | Focused five-channel bass amp | One selected Niflheimr channel, common LOW/Blend, curated compression, one CAB section and one main EQ | Separate VST3; Windows first reviewable package, macOS Universal separately verified |

Initial focused-product version target: **1.0.0-beta.1 for each product**.
Their version and release acceptance are independent of Chimera 1.3.x.
AU and an application that runs without a DAW are additional format work,
outside the initial VST3 candidate estimate. Focused-product AAX follows the
shared [AAX plan](AAX_NATIVE_PLAN.md) with separate IDs and acceptance.

Implement separate product targets in the existing repository. Share the
underlying amp DSP, oversampling/crossfade utilities, CAB renderer, selected
utilities and improved EQ components. Product descriptors/processors/editors
own their limited topology, resources, parameters and packaging. Avoid
independently maintained copies of the amp sound engine.

## Focused playing workflow

Use a shared, compact layout with product-specific head identity:

- Header: preset previous/next and browser, input/output meters and levels,
  whole-product A/B, compact tuner and gate access.
- Main AMP page: five clearly named channel selectors and the amp controls.
  Open on AMP, retain per-channel settings and keep Niflheimr Blend visible.
- A short AMP / CAB / EQ / FX page row. Gate, Transpose, tuner and click are
  not separate signal-navigation nodes. The first focused scope requires
  tuner/gate; Transpose may be added only with the separately qualified
  shared pitch implementation and latency evidence.
- One selected, sufficiently large EQ graph with the improved input/output
  analyzer, band editing, Undo/Redo, audition and comparison components.
  Preserve ordinary knob/keyboard accessibility and readable scaling.
- Curated section bypass and a small number of useful presets/IR choices;
  retain a personal WAV IR option. No full catalog browser, mode switch,
  multi-lane routing editor or Room scene in the initial focused versions.

Simplification comes from the fixed path and limited peripheral selection.
Preserve the native amp's actual tone controls rather than flattening five
channel identities into a generic distortion macro.

### Amp and peripheral scope

| Area | Náströnd | Niflheimr |
|---|---|---|
| Stable channel order | Fenrir, Surtr, Níðhöggr, Fimbulvetr, Ragnarök | Hrímfaxi, Garmr, Nidavellir, Ymir, Hel |
| Amp controls | Gain, Bass, Mid, Treble, Mid Frequency, Presence, Depth, Master | Same eight controls, plus Blend on the main panel |
| Compact Character section | Clank, Crush, Impact, Rot, Bloom | Mass, Split, Fang, Fold, Thrust |
| PRE | Gate and a small fixed compressor/boost selection | Gate and clean compression; selected corrective tone processing only |
| CAB | Curated guitar cabinet choices, CAB bypass and personal IR | Curated bass cabinet choices, CAB bypass and personal IR |
| Main EQ | One post-CAB bank using the improved graph/analyzer | Same shared bank, with clear low-frequency detail |
| POST | One compact delay and reverb section | Dry default; no new time-effect inventory in the first edition |

Retain all **13 Náströnd** and **14 Niflheimr** native controls, including
channel-specific recall and reset. The Character section can expand without
covering the main channel/level controls. Reuse the EQ quality work rather
than creating a second analyzer implementation or a cosmetic imitation.
The first focused EQ bank is static; later common Dynamic EQ features have
their own integration/acceptance scope and are not implied by sharing a graph.

Freeze whether the first buses are mono-to-mono and stereo-to-stereo only,
or additionally mono-to-stereo, at the product scope review. Current Chimera
permits matching input/output channel sets only. Stereo delay/reverb from a
mono input therefore needs explicit processor, state and host work; do not
advertise that configuration from the current bus implementation alone.

### Niflheimr LOW / Blend contract

Preserve the native architecture: common LOW plus a linear blend between the
clean and dirty remainders, followed by common EQ, Presence, Depth and Master.
Blend zero selects the cleaned input, not raw DI or total plugin bypass.
Its meaning differs from Chimera's Matrix LOW DI/AMP Mix.

A full-band distortion inserted before that split would affect the clean
foundation as well. Keep the first PRE selection clean as above. Do not
replace native Blend with a generic crossfader between raw DI and a full-head
NAM output, or replace its topology merely because another bass plugin also
uses parallel bands. Verify endpoints and intermediate Blend positions,
low-note magnitude/phase, transient clarity and channel/quality changes.

## Dedicated builds and release identity

The audited Chimera engine owns/prepares three amp/cab lanes and six native
parameter contexts; the wrapper prepares multiple oversampling/crossfade
resources and both native amp families. Classic's steady audio path still
processes one selected lane. This is a preparation/resource-scope finding,
not evidence that every amp model processes audio simultaneously.

Focused targets should construct and prepare only the required amp family,
lane, CAB/FX resources, artwork and parameter banks. Measure CPU, memory,
startup, editor-open/closed cost and teardown before claiming a lighter build.
A smaller window alone is not evidence of a smaller resource footprint.

Each product requires its own stable:

- Plugin/format class and product IDs, bundle ID and installed filename.
- Installer application identity and owned paths.
- State product discriminator/schema and host parameter mapping.
- Preset/user-data locations, factory preset identities and user-IR handling.
- Version, release metadata and update channel.

Keep the manufacturer identity shared where appropriate. Existing Chimera
IDs remain unchanged. Verify all three products installed together, simultaneous
instances, independent updates and uninstall, and session save/reopen. Installing
or removing one product must not replace another product or its user assets.

## Factory presets and interoperability

Author focused factory banks, initially covering all five channel roles per
product. Existing Náströnd presets are complete Chimera rigs:
Rotten Grind uses Dual Níðhöggr/Ragnarök, and Slam Impact uses Matrix
Fenrir/Fimbulvetr/Ragnarök. They cannot transfer identically to a one-amp
edition by hiding their routing. Create and audition new focused arrangements
with distinct preset identities; preserve the original Chimera banks.

Niflheimr's five existing rigs already use single-lane Classic, but include
selected PRE/POST compression and tone processing. Preserve those supported
contributions or author new focused voicings; copying amp knobs alone does
not reproduce the complete rig.

General .chimera preset import is outside the first focused scope. If a
limited settings exchange is implemented, explicitly support only eligible
amp/channel, CAB and included FX data, and report unsupported routing/modules.
Never silently discard a multi-lane preset's sound-producing state.

## NAM is a separate required delivery

The minimum Niflheimr pack has **five accepted fixed-setting, amp-only .nam
models**, one for each channel in the stable order above. A voicing choice
is frozen after audition; its full 14-control vector, including Blend, is
recorded. Extra gain/Blend variants add models and validation work beyond
that minimum.

A .nam capture at fixed settings does not reproduce continuous access to
the 13/14 native controls, several of which alter nonlinear stages, timing
or crossover behavior. Initial focused VST3 products use the current native
cores. A future NAM-powered edition needs a separate runtime and control
strategy, verified latency and, for Niflheimr, an explicit clean/dirty capture
boundary. No such runtime is claimed by this plan.

### Reusable pipeline and source contract

Reuse Niflheimr's production-wrapper renderer and audition harness, together
with the reusable training/checkpoint/export/player-validation work in PR #22.
PR #22 currently hardcodes Náströnd channels/controls and pins a pre-Niflheimr
v1.2.0-beta.1 source. Generalize with a per-family manifest rather than
globally repinning the historical Náströnd experiment.

Every family/channel capture records:

- Exact native source commit and source-file fingerprints.
- Stable family/channel identity, every native control and oversampling choice.
- 48 kHz training/capture rate, measured latency/alignment, block configuration
  and fixed input calibration; additional rates require explicit qualification.
- Amp-only boundary: no CAB, PRE/POST, Transpose or automatic normalization.
- Input/output hashes, training architecture/code/config, dataset split,
  checkpoint/export hashes and supported player/version.

Separate training and validation data from sealed test material and played
instrument DI. Cover low B/Drop A bass, picked/fingered attacks, dynamics,
muted stops and long decays. Use matching input/CAB/IR conditions for native
versus NAM listening, with any fixed comparison gain documented.

### Feasibility before expensive training

Náströnd's October 11 full report records **0/5 qualified channels**, not five
finished models. The full ESR target is below 0.01. The current bare A2
architecture has a 6,347-sample (132.23 ms at 48 kHz) receptive field; the
reported Fimbulvetr quiet-tail witness has a residual-RMS lower bound of about
-68.03 dBFS against the retained -70 dBFS criterion. This is a bound for that
witness window, not achieved model performance. More epochs do not remove
that demonstrated memory limitation.

Preserve the prior TONE3000 compatibility decision and existing Náströnd
criteria. Do not silently change player, use an unsupported model architecture,
or relax acceptance to create a completion result. Report a compatible method
decision or the unresolved blocker.

Niflheimr Hel has a 240 ms base recovery in the native prototype. That is a
reason to test history dependence before training, not proof that the same
Náströnd witness applies or that Niflheimr is impossible to capture. Run
family/channel tail and common-suffix feasibility probes against the selected
compatible architecture. Freeze Niflheimr's numeric fidelity contract before
full training; do not pick thresholds after observing the model's errors.

Qualification evidence includes full/active/quiet error, level/peak and
low-frequency/phase behavior, held-out DI and decays, channel identity and
listening. Track training completion, audio fidelity, official export
round-trip, TONE3000 player load/preset restore and final file hashes separately.
A pipeline smoke pass establishes neither fidelity nor musical acceptance.

## Schedule and completion

All dated checkpoints are in the
[central roadmap](DEVELOPMENT_ROADMAP_2026_2027.md#focused-náströnd--niflheimr-products-and-required-nam-delivery)
and its JSON mirror. The product capacity decision is October 16; the proposed
November 30–December 25 focused implementation/candidate window is **not yet
allocated** and conflicts with the currently assigned Room Core window.
Resolve allocation and dependent dates together. Do not assume extra staff
or count both projects against the same four weeks.

The initial four-week estimate covers native VST3 internal candidates with
the focused scope above. NAM training/fidelity, NAM runtime, AU/standalone and
signed AAX are separately estimated tracks. Public VST3 and NAM dates remain
uncommitted until their respective evidence is complete.

| Deliverable | Required completion evidence |
|---|---|
| Each focused VST3 | Product-specific builds/IDs, all five native channels and control memories, supported bus and bypass behavior, dedicated presets, resource measurements and native UI/audio |
| Focused release candidate | Actual platform/DAW lifecycle, automation/session restore, multiple instances, three-product coexistence and verified installer/update/uninstall |
| Niflheimr NAM pack | Five source-bound qualified models, frozen fidelity/held-out-DI results, supported player load/restore and hashes with installation/calibration guidance |
| Future focused AAX | Product-specific signatures and normal-Pro-Tools/installer acceptance under the shared AAX plan |

## Evidence and references

Repository observations above are pinned to the audited main unless noted:

- [Native amp controls](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/blob/3817246f779a50f19b5378ec9661a2275e6b91b2/Source/OriginalAmpDefinition.h)
  and [Niflheimr controls](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/blob/3817246f779a50f19b5378ec9661a2275e6b91b2/Source/NiflheimrDefinition.h).
- [Niflheimr DSP](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/blob/3817246f779a50f19b5378ec9661a2275e6b91b2/Source/NiflheimrDSP.h)
  and [native prototype/capture contract](NIFLHEIMR_NATIVE_PROTOTYPE.md).
- [Náströnd presets](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/blob/3817246f779a50f19b5378ec9661a2275e6b91b2/Source/OriginalPresets.h)
  and [Niflheimr presets](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/blob/3817246f779a50f19b5378ec9661a2275e6b91b2/Source/NiflheimrPresets.h).
- [Engine](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/blob/3817246f779a50f19b5378ec9661a2275e6b91b2/Source/ChimeraDSP.h),
  [amp wrapper](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/blob/3817246f779a50f19b5378ec9661a2275e6b91b2/Source/Amplifier.h)
  and [native engine resources](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/blob/3817246f779a50f19b5378ec9661a2275e6b91b2/Source/AmpNativeDSP.h).
- [Product build](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/blob/3817246f779a50f19b5378ec9661a2275e6b91b2/CMakeLists.txt),
  [processor/state](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/blob/3817246f779a50f19b5378ec9661a2275e6b91b2/Source/PluginProcessor.cpp)
  and [installer](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/blob/3817246f779a50f19b5378ec9661a2275e6b91b2/Installer/Chimera.iss).
- PR #22 at 859fea9b:
  [full NAM report](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/blob/859fea9bc49c4b9981c926a314655e485c51027c/Tools/Nam/reports/refinement-20261011.md)
  and [accuracy blocker](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/blob/859fea9bc49c4b9981c926a314655e485c51027c/Tools/Nam/ACCURACY-BLOCKER.md).

Official workflow references checked October 12, 2026:

- [Neural DSP Omega Ampworks Granophyre](https://neuraldsp.com/plugins/omega-ampworks-granophyre):
  one amp with focused drive, EQ and cabinet sections.
- [Neural DSP Parallax X](https://neuraldsp.com/plugins/parallax):
  bass-specific control grouping and clean-low/distorted-band workflow;
  reference for task focus, not a replacement for Niflheimr's native topology.
- [Odeholm Audio thall amp](https://odeholm-audio.com/products/thall-amp):
  concentrated amp controls, curated IR, personal IR loading and compact utilities.
- [Odeholm Audio Slam Amp](https://odeholm-audio.com/products/slam-amp):
  major sections within one interface, independent section bypass and
  permanent level meters.

These references guide interaction and product scope. They do not imply
copied DSP/assets, measured sound equivalence or matching latency/CPU claims.
