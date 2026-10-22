# Focused Náströnd / Niflheimr products and NAM delivery

Updated: **2026-10-12 KST**. Status: **PLANNED; this document does not establish
implementation or qualification of separate product targets or either NAM pack**.

## Owner decision and verified baseline

The October 12 follow-up requires:

1. Required five-channel Náströnd and Niflheimr NAM packs, qualified separately
   from the native VST products.
2. Separately installable Náströnd and Niflheimr VST products.
3. A simpler playing/editing workflow, using Neural DSP and Odeholm Audio as
   references for focused amp products.
4. Four replaceable PRE slots per focused product, with five guitar families
   and six bass families; Chimera retains its existing five PRE slots.
5. Guitar gain shaping centred on a BOOST that drives the amp, plus shared
   original FX improvements delivered back to Chimera.
6. Weekly working changes and verified builds, continuing maintenance while
   the larger product work progresses.

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
| SpectralForge Niflheimr | Focused five-channel bass amp | One selected Niflheimr channel, native common LOW/Blend, four curated PRE slots, one CAB section and one main EQ | Separate VST3; Windows first reviewable package, macOS Universal separately verified |

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

Reuse existing compressor, EQ, drive and spatial engines as the implementation
baseline. The original FX programme adds or improves algorithms module by module;
it is not a rewrite of every effect before either VST can be tested. Share the
resulting DSP and fixes with Chimera, while each product exposes its own useful
controls, defaults and presets. A renamed module or a new skin does not establish
new DSP, improved sound or measured parity. Record what each revision actually
changes and qualifies.

## Focused playing workflow

Use a shared, compact layout with product-specific head identity:

- Header: preset previous/next and browser, input/output meters and levels,
  whole-product A/B, compact tuner and gate access.
- Main AMP page: five clearly named channel selectors and the amp controls.
  Open on AMP, retain per-channel settings and keep Niflheimr Blend visible.
- A compact PRE / AMP / CAB / EQ / SPACE page row. PRE shows four replaceable
  effect slots with individual bypass, clear family selection and an expanded
  editor for the selected slot. Do not repeat the same pages in another long
  signal-navigation row. Gate, Transpose, tuner and click remain compact
  utilities and do not inflate the PRE family count.
- The first focused scope requires tuner/gate. Any exposed PITCH family or
  Transpose function must use a qualified implementation with measured latency;
  the separate <=10 ms research target is not a promise for the first native
  VST3 beta. If a feature remains blocked, record its absence in that build
  rather than claiming the complete inventory or blocking unrelated work.
- One selected, sufficiently large EQ graph with the improved input/output
  analyzer, band editing, Undo/Redo, audition and comparison components.
  Preserve ordinary knob/keyboard accessibility and readable scaling.
- Curated section bypass and a small number of useful presets/IR choices;
  retain a personal WAV IR option. SPACE groups selected delay/reverb controls;
  Niflheimr defaults to dry and permits deliberate creative use. Modulation
  assigned to PRE remains visibly part of the PRE slots, not an extra hidden
  serial processor. No full catalog browser, mode switch, multi-lane routing
  editor or Room scene is required for the initial focused versions.

Simplification comes from the fixed path and limited peripheral selection.
Preserve the native amp's actual tone controls rather than flattening five
channel identities into a generic distortion macro.

### Amp and peripheral scope

| Area | Náströnd | Niflheimr |
|---|---|---|
| Stable channel order | Fenrir, Surtr, Níðhöggr, Fimbulvetr, Ragnarök | Hrímfaxi, Garmr, Nidavellir, Ymir, Hel |
| Amp controls | Gain, Bass, Mid, Treble, Mid Frequency, Presence, Depth, Master | Same eight controls, plus Blend on the main panel |
| Compact Character section | Clank, Crush, Impact, Rot, Bloom | Mass, Split, Fang, Fold, Thrust |
| PRE slots | Four replaceable slots; choose from BOOST, COMP, PITCH, WAH, MOD | Four replaceable slots; choose from COMP, DRIVE, FUZZ, OCTAVER, FILTER, CHORUS |
| CAB | Curated guitar cabinet choices, CAB bypass and personal IR | Curated bass cabinet choices, CAB bypass and personal IR |
| Main EQ | One post-CAB bank using the improved graph/analyzer | Same shared bank, with clear low-frequency detail |
| SPACE / POST | Compact selected delay and reverb, sharing qualified engines with Chimera | Same shared compact approach, dry by default and voiced for bass |

Retain all **13 Náströnd** and **14 Niflheimr** native controls, including
channel-specific recall and reset. The Character section can expand without
covering the main channel/level controls. Reuse the EQ quality work rather
than creating a second analyzer implementation or a cosmetic imitation.
The first focused EQ bank reuses the accepted common EQ implementation. Dynamic
EQ is included only after its own DSP, state, automation and audio acceptance;
sharing the graph alone does not establish dynamic processing.

### PRE inventory and original FX integration

Four slots means at most four selected PRE modules in the active focused chain,
not four total available families. The product descriptor limits choices to the
five guitar or six bass families above. Slots are replaceable; do not infer an
unrestricted drag-and-drop routing editor, duplicate-family stacking or a changed
Chimera slot count. Freeze allowed combinations and the actual module order at
the product review, persist them explicitly, and make the screen reflect the
processed path. Any later limited order editing retains its central-roadmap task.

- Náströnd BOOST is an amp-driving and input-shaping effect, with Boost, Tight
  and Tone as the initial principal controls. Its primary role is not a second
  standalone distortion amp. COMP, PITCH, WAH and MOD provide distinct additional
  roles; pitch/whammy/octave options or modulation modes do not count as extra
  installed families or extra simultaneous slots.
- Niflheimr distinguishes DRIVE and FUZZ as selectable families, alongside COMP,
  OCTAVER, FILTER and CHORUS. Bass voicing must be assessed for low-frequency
  response, attack and interaction with native Blend. Do not import the entire
  Chimera effect catalog or hide unsupported modules behind the focused selector.
- Gate and tuner are product utilities. Global Transpose and creative PRE pitch
  must have a documented relationship, with no unintended double transposition
  or duplicated latency. Newly added algorithms and corrective changes are
  shared with Chimera through the same module implementation.
- Qualify insertion/removal, bypass, parameter changes, state restore, supported
  slot combinations and declared sample rates/buffers. A missing research feature
  blocks its own availability and completeness claim; it does not stop verified
  maintenance builds or unrelated native product development.

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

A full-band PRE DRIVE or FUZZ inserted before that split also changes the input
to the clean foundation. Native Blend does not bypass active PRE effects; the
UI must not label its clean endpoint as raw DI. Record and audition the actual
insertion boundary for every bass module. If a later design protects common
LOW or the clean remainder from a distortion module, implement and validate
that explicit routing before advertising an unaffected clean-low path.

Do not replace native Blend with a generic crossfader between raw DI and a
full-head NAM output, or replace its topology merely because another bass plugin
also uses parallel bands. Verify endpoints and intermediate Blend positions,
low-note magnitude/phase, transient clarity and channel/quality changes with
PRE off and with every supported active PRE combination.

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

Both the Náströnd and Niflheimr NAM packs remain required deliverables. Each
requires at least **five accepted fixed-setting, amp-only .nam models**, one
for each channel in that family's stable order above. Neither native VST3
packaging nor a pipeline smoke pass substitutes for an accepted NAM pack.

For Niflheimr, a voicing choice
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
v1.2.0.0-beta.1 source. Generalize with a per-family manifest rather than
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

All dated checkpoints are owned by the
[central roadmap](DEVELOPMENT_ROADMAP_2026_2027.md#focused-náströnd--niflheimr-products-and-required-nam-delivery)
and its JSON mirror. The revised calendar explicitly allocates **three
consecutive four-week windows**. This replaces the former unallocated
November 30–December 25 estimate for both products and resolves its conflict
with Room Core by moving dependent Room/1.4.0-and-later work twelve weeks.

| Allocated window, KST | Main work | Reviewable result |
|---|---|---|
| 2026-11-30–2026-12-25 | First shared original FX increment and integration of reusable modules; freeze focused slot/state contracts and product descriptors | Verified shared modules and a concrete four-slot workflow, with accepted and blocked capabilities listed individually |
| 2026-12-28–2027-01-22 | Náströnd native VST3 target, focused UI, presets, packaging and host acceptance | First Náströnd VST3 beta target; public availability remains subject to that build's platform and release checks |
| 2027-01-25–2027-02-19 | Niflheimr native VST3 target, native Blend/PRE integration, presets, packaging and host acceptance | First Niflheimr VST3 beta target; assess bass-specific behavior and coexistence explicitly |

These are allocated working targets, not verified progress or a guarantee that
an unchecked binary can ship on the last day. The first FX window delivers
the accepted initial increment using existing engines where appropriate; it
does not promise a wholesale rewrite or simultaneous completion of every
original algorithm. Remaining module improvements continue in weekly changes.

The central schedule moves the dependent Room/1.4.0-and-later programme by twelve
weeks, places the 2.0.0 candidate window at **2027-11-22–2027-12-17**, and retains
**2027-12-20–2027-12-31** as a correction/buffer period. These are provisional
targets reviewed against actual weekly results, not a claim that all future
research must succeed by 2027. Changes to dependent version milestones remain
owned by the central roadmap and its JSON mirror.

### Weekly delivery and capacity rule

- Work on one principal implementation at a time, plus bounded maintenance and
  existing regressions. Shared work benefits all products but is not counted as
  separate full-time capacity for each product.
- Every week produces at least one actual code/UI/DSP or maintenance change and
  a build that verifies the changed behavior. Report the source, concrete
  change, validation outcome, available package and remaining blocker.
  Editing this roadmap or issuing a status report alone is not a weekly update.
- A larger incomplete feature does not stop a smaller useful, verified
  maintenance delivery. If the current feature fails validation, fix it or
  supply another independently qualified change; never call a failing or
  unchanged build a successful update.
- Public publication continues to use the applicable release checks. Clearly
  identify internal review builds and platform-limited candidates instead of
  presenting them as universally accepted releases.
- Review the next week's work after each result. If a checkpoint slips, record
  the specific dependency and the affected target; do not suspend unrelated
  work or silently double-book Room, a focused product and AAX.

NAM training/fidelity, a future NAM runtime, AU/standalone and signed AAX are
separate delivery tracks. Limit NAM and <=10 ms pitch experiments to defined
questions and record the pass/fail decision before repeating expensive work.
A blocked model or pitch algorithm blocks that feature's qualification and
release claim, not independent native VST3 development or weekly maintenance.
Both NAM packs remain open required deliveries until their evidence is complete.

| Deliverable | Required completion evidence |
|---|---|
| Each focused VST3 | Product-specific builds/IDs, all five native channels and control memories, four replaceable PRE slots with declared qualified inventory, supported buses/bypass, dedicated presets, resource measurements and native UI/audio |
| Focused release candidate | Actual platform/DAW lifecycle, automation/session restore, multiple instances, three-product coexistence and verified installer/update/uninstall |
| Shared original FX increment | Identified reused and newly changed algorithms, supported family/slot behavior, audio/state/automation evidence and common implementation integrated with Chimera |
| Náströnd NAM pack | Five source-bound qualified models, retained fidelity/held-out-DI results, supported player load/restore and hashes with installation/calibration guidance |
| Niflheimr NAM pack | Five source-bound qualified models, frozen fidelity/held-out-DI results, supported player load/restore and hashes with installation/calibration guidance |
| Future focused AAX | Product-specific signatures and normal-Pro-Tools/installer acceptance under the shared AAX plan |

## Product release numbering and deployment targets

Use independent product tags `nastrond-v<version>` and `niflheimr-v<version>`;
preserve Chimera's existing `v<version>` path. Each product needs its own update
manifest/feed resolution, plugin/state/preset/data paths and installer identity.
The current single-product publisher is not sufficient by renaming an archive.
Internal candidate IDs include the source and CI build, and are not published
to the updater. The first public tag identifies one immutable verified payload.

| Product | Internal test target | First public beta target | First RC review | Stable 1.0.0 review |
|---|---|---|---|---|
| Náströnd | January 8, 2027 | January 22, 2027: 1.0.0-beta.1 | February 26, 2027: 1.0.0-rc.1 | March 12, 2027 |
| Niflheimr | February 5, 2027 | February 19, 2027: 1.0.0-beta.1 | March 19, 2027: 1.0.0-rc.1 | April 2, 2027 |

Windows VST3 is the first package target. Reserve macOS Universal VST3 for the next weekly beta: January 29 for
Náströnd and February 26 for Niflheimr, conditional on its own build and host
evidence. Record the actual supported platform matrix on each release. General Linux,
AU/standalone and signed AAX are not silently added to the focused estimates.
Each beta covers the full five-channel native core, four replaceable slots,
declared qualified PRE inventory, CAB/EQ/SPACE, presets and product identity.
Any incomplete family remains explicitly listed; partial availability is not
completion of the full five-guitar/six-bass-family scope.

Stabilization uses the already budgeted maintenance/verification work. A
significant fix that cannot fit changes the next main milestone explicitly.
RC/production-channel generation is currently unsupported and must be added
and verified before those releases, with January 15 as the readiness target. Include updater channel selection,
existing-beta discovery of the stable successor, stable-default feeds and
explicit beta opt-in; a new stable feed alone does not migrate the client.
Neither the number of beta weeks nor the calendar alone authorizes stable.
See the full [release calendar](RELEASE_CALENDAR_2026_2027.md) for intermediate
betas and exact test/candidate/publication dates.

NAM tags use `nastrond-nam-v<version>` / `niflheimr-nam-v<version>`, independent
of plugin versions. Reserve November 20, 2026 and December 4, 2026 respectively
as conditional first 1.0.0-beta.1 pack targets. These dates do not assert that
the existing memory/fidelity blocker will be solved: release only after all
five channels meet the unchanged source, fidelity, held-out DI and supported
player checks. If prerequisites fail, report the result and next experiment;
keep native product work moving. A beta pack still requires qualified models.

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
