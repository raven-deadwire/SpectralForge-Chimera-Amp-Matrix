# Chimera development roadmap, October 2026–September 2027

Updated: **2026-10-11, Asia/Seoul**. This is the current implementation plan
following the owner's request to review the [FabFilter product range](https://www.fabfilter.com/products),
restore the omitted graphical EQ/dynamics/multiband work, add a visible editable
signal path, and open Chimera on the amplifier view.

This plan supersedes the forward dates in the earlier 132-model schedule,
the October 8 CAB prototype calendar, and the unassigned next-head-update date.
Released 1.1.1, 1.1.2 and 1.2 remain history. Dates below are **internal work and
candidate targets, not guaranteed public release dates**. Select work by actual
completion, priority and satisfied dependencies; do not wait for an obsolete
calendar window. Re-estimate the next four weeks at each checkpoint.

## Schedule authority and delivery status

This document is the single authority for version windows, room scope and
prototype/test-installer/candidate targets. The `roadmap` section of
[the catalog JSON](../Validation/catalog-count-contract.json) is a synchronized
planning mirror, not a new release gate. The [CAB roadmap](CABINET_PANEL_ROADMAP.md)
owns the detailed room integration requirements and links here for dates.
When a checkpoint changes, update this calendar, its JSON mirror and the
corresponding [issue #46](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/issues/46)
checkpoint together.

**Room status: schedule registered; implementation and acceptance pending.**
Merging this plan does not deliver Room Core/Advanced, create a test installer
or close issue #46. Keep that issue open until its implementation, audio,
state/automation, performance, native UI and real-host acceptance criteria are
complete for the planned core and advanced stages. An actual feature source,
technical verification, host/listening acceptance and delivery are separate
states.

## Release-track boundary and continuing priorities

The October 10 planning baseline was PR #35 at
[e05ca7e](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/commit/e05ca7ee9398a063615e6795f8754caa8406bfd6).
Its failed tests describe that historical source only. Use the latest
[PR #35 delivery record](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/pull/35)
and [release preparation](RELEASE_1_3_PREPARATION.md) for current 1.3.0 status;
this plan neither reopens historical blockers nor changes release eligibility.

At consolidation, the current main source already has `page = 0` in
`Source/PluginEditor.h`. Preserve that AMP/RIGS startup default and the saved
Classic/Dual/Matrix mode; no startup-source change is included here. This fact
does not establish later embedded-head or signal-path completion.

The retained catalog is AMP25 + PRE39 + POST21 = **85** active character
models, 26 serialized amp IDs including recall-only Iron Compact, and 48
selectable presets. CAB inventory remains nine layouts, fourteen speakers and
twenty microphone models, counted separately from character models.

Continue the current 1.3.0 release work independently of the next-update
plan. Do not alter its product source, version, assets, release evidence,
validation policy, workflows or publisher as part of this consolidation.
Continue EQ/head/navigation, low-B/−2-semitone Transpose and NAM/reference work
on their own development tracks and report each track's actual evidence.

## Full reordered calendar

The October 10 revision moved the model-led June 4 integration target to
August 13, 2027 to account for common DSP and routing. The owner's October 11
request now adds adjustable room acoustics to the dated plan.

Reserve four weeks for **1.3.3 Room Core** and two additional weeks inside
**1.8 Room Advanced / Motion & Space**. Versions 1.3.1 and 1.3.2 keep their
existing windows; 1.4–1.7 move four weeks, and 1.9 onward move six weeks.
The complete integration target becomes **September 24, 2027**.
These are work estimates to review against measured progress, not an
assumption of extra staffing. Retain the character-model addition order.

| Version | Work window, KST | Main deliverable | Active character models |
|---|---|---|---:|
| 1.3 | Current release track; verify latest source and delivery state | Niflheimr/CAB installer and current release work | 85 |
| 1.3.1 | 2026-10-10–2026-11-01 | Graphical 12-band EQ, input/output FFT, Tone/Final positions, signal-path display and navigation; representative embedded head controls | 85 |
| 1.3.2 | 2026-11-02–2026-11-27 | Dynamic EQ, clean compressor, gate/expander improvements, live output limiter; expand head-control coverage | 85 |
| 1.3.3 | 2026-11-30–2026-12-25 | Room Core: shared Size/Absorption/Bypass, per-rig Send, actual early reflections and decay, state/automation and LOW DI isolation | 85 |
| 1.4 | 2026-12-28–2027-02-05 | Independent four-band dynamics, PRE Expansion A six models, first constrained signal-order editor | 91 |
| 1.5 | 2027-02-08–2027-03-05 | PRE Expansion B six models, drag-order UX; separately gated M/S, external sidechain and six-band dynamics expansion | 97 |
| 1.6 | 2027-03-08–2027-04-09 | Five reference amps and an independent three-band saturation processor | 102 |
| 1.7 | 2027-04-12–2027-05-07 | Nine POST Dynamics/Tone models and a common two-filter modulation processor | 111 |
| 1.8 | 2027-05-10–2027-06-18 | Nine Motion/Space models, advanced delay/reverb and Room Advanced conditions/spatial modeling; six-week window includes two added room weeks | 120 |
| 1.9 | 2027-06-21–2027-07-16 | Nine Creative variants, expanded modulation and routing polish | 129 |
| 1.10 | 2027-07-19–2027-08-27 | Dvergr Analog, Seidr Dynamic and Mimir Impact synth layers; integrate qualified tracking work | 132 |
| 2.0 | 2027-08-30–2027-09-24 | Whole-product host/audio/state/performance/preset/installer integration and RC | 132 |

Model research, asset inventory and independent prototypes can start before
their integration windows. A blocked capture, pitch method, studio-quality
option or host-specific feature blocks that item, not unrelated development.
If measured velocity cannot support a window, update its remaining scope and
dependent dates before promising a release. The dates do not authorize weaker
audio, compatibility or acceptance criteria.

### Near-term reviewable checkpoints

| Target | Reviewable result |
|---|---|
| 2026-10-14 | Actual EQ graph/control prototype and one original plus one simple reference head; startup behavior checked in the next candidate |
| 2026-10-23 | First EQ-focused test installer: static EQ/FFT and working navigation, with exact included head coverage listed |
| 2026-10-30 | 1.3.1 validation candidate if gates pass; remaining work closed or explicitly carried at the November 1 checkpoint |
| 2026-11-13 | Dynamic EQ/GR and clean compressor preview using repeatable audio |
| 2026-11-20 | First 1.3.2 test installer |
| 2026-11-27 | 1.3.2 candidate and head-coverage report |
| 2026-12-04 | Room bus, gate placement, parameter/state and callback-budget design review |
| 2026-12-11 | Playable Room Size/Send/Absorption prototype in Classic, Dual and Matrix |
| 2026-12-18 | First Room Core Windows test installer, native UI captures and measured LOW DI isolation |
| 2026-12-25 | 1.3.3 candidate with room transitions, state, CPU, host/tail evidence and EN/DE/KR guidance |
| 2027-01-08 | Four-band dynamics prototype with band sum/phase evidence |
| 2027-01-15 | First constrained order-editing preview with save/restore and undo |
| 2027-02-05 | Combined 1.4 candidate: verified four-band dynamics, PRE A and supported order editing |

A preview means implemented behavior, not a mock screenshot. October 23 does
not promise Dynamic EQ, multiband processing or all 25 interactive head layouts.
Keep the existing functional detailed controls as a fallback while dense
faceplates are completed. Full coverage means 25 active heads plus the one
recall-only model, across the existing six native contexts.

## Room acoustics schedule and scope

The owner requested adjustable room conditions on October 11 and then asked
for their version/date assignment. [Issue #46](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/issues/46)
tracks actual implementation and acceptance. This schedule assigns two stages.

### 1.3.3 Room Core — November 30–December 25, 2026

Deliver one shared room environment with Small / Medium / Large Size,
Absorption / Damping, Bypass and per-rig Room Send. Include actual early
reflections and late decay, a defined wet-return level, state/automation/A-B
and old-project Room Off defaults. The first controls operate on the RIGS/CAB
room surface in Classic, Dual and Matrix.

| Week, KST | Reviewable work |
|---|---|
| 2026-11-30–2026-12-04 | Freeze weighted send/return topology, gate/tail policy, parameter IDs and numeric callback/memory budgets; verify Room Off preserves existing audio. |
| 2026-12-07–2026-12-11 | Implement bounded room-size/absorption models, early reflections and decay; deliver a playable three-mode prototype. |
| 2026-12-14–2026-12-18 | Complete basic UI, state/automation and smooth switching; measure low-note isolation and full-product cost; produce the first Windows test installer. |
| 2026-12-21–2026-12-25 | Resolve platform/host/tail regressions, verify the installer and document the 1.3.3 candidate. |

Independent Early Level, Decay, Diffusion, room HP/LP and Width controls belong
to the dated 1.8 advanced stage. Individual controls may be brought forward
only with their response/interaction evidence; the required 1.3.3 result is
the playable basic room with actual reflections and decay.

Room excitation follows each audible AMP/CAB contribution. Matrix LOW uses
only its effective AMP/CAB portion; DI 100% and LOW AMP off create no new LOW
room input. Preserve lane blend/gain/mute/solo/polarity. With the optional
post-rig gate, apply the same existing gate envelope to the direct rig sum
and room excitation; mix the wet return after that gate and before Tone EQ /
POST. This avoids new excitation through a closed gate and preserves an
already generated tail. Verify the final topology before implementation.

### 1.8 Room Advanced — May 10–June 18, 2027

Expand independent early/late balance, decay, diffusion, wet filters and width.
Add declared modeled room dimensions, wall/floor/ceiling absorption or material
presets, cabinet-to-boundary placement and a room-mic stage separate from close
Mic A/B. The existing nine Motion/Space models and advanced POST delay/reverb
remain in this version; allocate six weeks in total, including two added room
weeks.

| Checkpoint, KST | Reviewable result |
|---|---|
| 2026-12-25 | Re-estimate the advanced stage using the actual Room Core architecture and measured cost. |
| 2027-05-14 | Fix spatial-model assumptions, controls, surface presets, acceptance limits and the remaining work estimate. |
| 2027-05-28 | Playable advanced-condition/spatial prototype with declared parameter-to-response behavior. |
| 2027-06-11 | First 1.8 test installer including the implemented room and POST scope. |
| 2027-06-18 | Integrated 1.8 candidate and explicit coverage/evidence report. |

The additional two weeks are a planning allowance, not proof that every
physical-room claim is achievable. Review the scope on December 25 and May 14;
revise affected dates and unqualified items explicitly rather than silently
dropping the spatial controls or calling an ambience knob physical remiking.
The core room can be used from 1.3.3 independently of this later extension.

RavenForge room styling, common equipment scale, existing close microphones
and private IR identity remain the design baseline. Room is a shared processor
and adds no character models to the 85/132 catalog counts.

## Reference-to-feature mapping

The linked product pages establish the reference functions. The Chimera column
is our own implementation scope; it is not a claim of identical algorithms,
sound, feature count or performance.

| Official reference | Function to study | Chimera implementation and timing |
|---|---|---|
| [Pro-Q 4](https://www.fabfilter.com/products/pro-q-4-equalizer-plug-in) | Graph-node parametric EQ, analyzer, dynamic/spectral processing | 12-band graphical EQ and FFT in 1.3.1; initial Dynamic EQ in 1.3.2; spectral/linear-phase work separately gated |
| [Pro-C 3](https://www.fabfilter.com/products/pro-c-3-compressor-plug-in) | Detailed compressor timing, detector control and GR feedback | One transparent common compressor in 1.3.2; additional character models remain in POST expansion |
| [Pro-MB](https://www.fabfilter.com/products/pro-mb-multiband-compressor-plug-in) | Independent band dynamics, detector ranges, band solo and metering | Four processing bands in 1.4; six-band expansion is a separate 1.5 performance gate |
| [Pro-L 2](https://www.fabfilter.com/products/pro-l-2-limiter-plug-in) | Output limiting, true-peak and loudness feedback | Live peak control in 1.3.2; measured Studio true-peak/LUFS option targeted in 1.5 |
| [Pro-DS](https://www.fabfilter.com/products/pro-ds-de-esser-plug-in) | Selective high-frequency detection and attenuation | Guitar/bass pick, fret and harshness control using the Dynamic EQ detector; focused controls/presets in 1.3.2 |
| [Pro-G](https://www.fabfilter.com/products/pro-g-gate-expander-plug-in) | Gate/expander timing, sidechain and clear gain-reduction display | Improve the existing gate in 1.3.2; preserve attack/sustain and use a defined DI detector tap |
| [Saturn 2](https://www.fabfilter.com/products/saturn-2-multiband-distortion-saturation-plug-in) | Band-specific saturation, drive/mix and modulation | Independent three-band saturation in 1.6; low-band clean preservation, a small set of authored saturation types |
| [Timeless 3](https://www.fabfilter.com/products/timeless-3-delay-plug-in) | Dual delay, tempo, feedback filtering, ducking and modulation | Advanced dual delay controls in 1.8; multi-tap/pitch-feedback/freeze expansion in 1.9 where validated |
| [Pro-R 2](https://www.fabfilter.com/products/pro-r-2-reverb-plug-in) | Algorithmic room/plate/space, decay shaping, wet EQ and ducking | Room/Hall/Plate controls, predelay/width, three-region decay shaping and wet EQ in 1.8 |
| [Volcano 3](https://www.fabfilter.com/products/volcano-3-filter-plug-in) | Multiple filters, drive and modulation | Two-filter LP/HP/BP/notch processor with LFO/envelope follower in 1.7; advanced modulation in 1.9 |
| [Twin 3](https://www.fabfilter.com/products/twin-3-synthesizer-plug-in) | Oscillator/filter/envelope, MIDI and modulation architecture | Common synth foundation and the three planned layers in 1.10; instrument pitch tracking remains its own research gate |
| [One](https://www.fabfilter.com/products/one-basic-synthesizer-plug-in) | Simple oscillator/filter/envelope interaction | Basic view of the shared synth foundation; no duplicate model count |
| [Simplon](https://www.fabfilter.com/products/simplon-basic-filter-plug-in) | Accessible two-filter controls | Basic view of the common filter processor |
| [Micro](https://www.fabfilter.com/products/micro-mini-filter-plug-in) | Simple envelope-following filter | Compact filter mode and useful starting settings |

The requested graphical EQ means freely placed frequency/gain/Q nodes, not only
a fixed-frequency pedal EQ. Existing PRE graphic EQ and POST character EQs are
retained; neither fulfills this common graphical processor requirement.

## Global processing and signal placement

**Global means processing the merged rig signal. It does not mean only the
last position.** Both the pre-POST tone/dynamics block and the final block are
global in Classic, Dual and Matrix.

| Position | Default role | Reason |
|---|---|---|
| Input and PRE/rig section | Existing gate, transpose, owned PRE pedals, amp and CAB routing | Preserve the instrument/amp relationship and clean DI path |
| Room return, from 1.3.3 | Weighted per-rig AMP/CAB sends into one shared room; wet return after the optional post-rig gate and before Tone EQ / POST | Preserve LOW DI and natural room decay; downstream Tone/dynamics intentionally also process room return |
| Immediately after rig merge and optional room return, before POST | Tone EQ, clean compressor/expander, independent multiband dynamics | Shape the rig and room result before POST delay/reverb tails are generated |
| Existing POST chain | Character dynamics/preamp/EQ, modulation, delay and reverb | Retain existing model roles and state; order editing is staged |
| After POST and Doubler/Width | Final EQ | Adjust the complete wet sound and stereo result |
| After Final EQ | Output/master trim, then output limiter | The limiter sees the final gain and every merged path |
| After limiter | Output meters and output | No subsequent gain-raising audio stage |

Tone EQ and Final EQ use one engine with **two independent instances**, each
with up to **12 total EQ nodes**. Dynamic is a mode of a node. Initially **at
most four nodes across both instances combined** may be dynamic; this is not
12 static plus four extra nodes, nor four per instance. Show the shared limit
and never silently disable another band when it is reached.

Prepare stable band IDs, internal detector routes, channel modes and host-bus
contracts with 1.3.1. Expose each advanced control only after its processing and
host behavior are verified. Current gate and character compressors remain
distinct from the new clean common compressor.

The first common dynamics default is before POST. Later supported global
placements may intentionally process tails, but must state that result in the
UI. Tone EQ is not initially movable across the rig split or ahead of the
fixed Room return. Room send taps/return, Final EQ and the last limiter remain
fixed roles. See [signal-path editor](SIGNAL_PATH_EDITOR.md).

### Live and Studio quality

Live defaults use minimum-phase IIR EQ, no lookahead and no additional
oversampling for these common stages. The target is zero **additional module**
latency in that configuration, not zero latency for all of Chimera. Existing
Transpose/amp/CAB processing retains its actual measured/reported latency.

Analyzer FFT is a display function and must not force the audio through a
spectral processor. Move analysis work off the real-time callback; bound the
transfer buffer and suspend unnecessary hidden-editor display work.

Linear phase, spectral dynamics, true-peak limiting and high-quality
oversampling need separate measured latency, CPU, impulse-response and
dry/wet/branch-alignment acceptance. FabFilter itself documents different
[EQ processing modes](https://www.fabfilter.com/help/pro-q/using/processingmode)
and [linear-phase spectral bands](https://www.fabfilter.com/help/pro-q/using/spectral-dynamics).
Those implementation choices are references, not Chimera performance values.

Target a Studio true-peak/LUFS candidate in 1.5, independently of Live limiter
acceptance. Evaluate spectral/linear-phase prototypes during 1.7; include only
qualified work in 2.0, otherwise assign a concrete 2.x follow-up after the
research checkpoint. EQ Match, EQ Sketch, cross-instance overview, automatic
IR-to-reverb parameter inference and polyphonic guitar tracking have no
committed release date in this plan.

## Model additions retained

| Version | Character models |
|---|---|
| 1.4 — PRE A, six | Empress Heavy Menace; Tel-Ray Power Wah Boost; MXR MB301 Bass Synth; EBS MultiComp; Fortin 33; Empress ParaEQ MKII |
| 1.5 — PRE B, six | Revv G3; Wampler Dracarys; MXR Slash Octave Fuzz; Dunlop 535Q; BOSS GE-7; DigiTech Whammy |
| 1.6 — AMP, five | Crystal 120 / JC-120; 1987X Plexi; JCM800 2203; Hiwatt DR103; one bass head selected from Glockenklang Blue Rock or Vanderkley Aurora |
| 1.7 — POST, nine | Omnipressor, API 2500, Neve 33609; API 512c, Chandler TG2, Grace m101; API 550A, Mäag EQ4, Chandler Curve Bender |
| 1.8 — POST, nine | Rotary, Uni-Vibe, Ring Mod; UltraTap, Rose, DIG; Bricasti reference, Blackhole, Non-Linear |
| 1.9 — Creative, nine | Slicer, HarPeggiator, Time Warp; Laser, Frequency Shifter, Dynamic Freeze; Glitch, Bit Crusher, Granular Texture |
| 1.10 — SYNTH, three | Dvergr Analog, then Seidr Dynamic, then Mimir Impact |

These names are existing working names and development references, not newly
approved customer aliases. Heavy Menace must not receive the alias Hammer.
Ashdown and VT Bass remain excluded. ZUTA / Cinder 120 remains in the existing
catalog. Select only one of Blue Rock/Aurora for the five-amp addition.

Final character count: **PRE51 + AMP30 + POST48 + SYNTH3 = 132**.
Track common processor types, installed instances and optional modes
separately. Two EQ positions do not add two character models; a three-band
saturation processor is not three new character models. CAB, speakers,
microphones and presets are separate inventory dimensions.

Preserve PRE's maximum five simultaneously loaded pedals. Preserve all
existing preset names, IDs and ordinals, including the four full-title Guitar
Signatures, three Bass Signatures and both Original banks. Append new models,
parameters and presets; do not overwrite identity when moving a visual block.
The JSON's preset_counts describes the historical FactoryPresets.h bank
(31 + 3 = 34); selectable_preset_count describes all current banks (48).
Do not change the former to 48 and break its source contract.

## Shared foundations and dependencies

| Foundation | First implementation | Reused by |
|---|---|---|
| Graph node coordinates, selection, value entry, FFT transport | 1.3.1 EQ | Dynamic EQ, multiband, filter and reverb shaping |
| Stable processor/band IDs and order state | 1.3.1 state design; 1.4 order execution | Automation, routing, undo/redo and preset recall |
| Detector/envelope, range/GR, audition and fixed DI taps | 1.3.2 dynamics | Gate, multiband, ducking, envelope filters |
| Shared room send/return, gate-envelope reuse, tail and room state | 1.3.3 Room Core | 1.8 advanced room conditions and spatial modeling |
| Band splitting/filtering, sum and branch alignment | 1.4 multiband | Saturation and later band processing |
| Quality modes, latency reporting and dry/wet alignment | Live foundation first; Studio separately verified | Limiter, saturation, filters and Studio EQ |
| Bounded modulation routing, tempo, LFO/EF and MIDI | Foundation with common processors; products in 1.7 onward | Filter, delay, saturation and synth |

Matrix's LOW/MID/HIGH amp routing does not fulfill independent multiband
dynamics or saturation. The latter should work without assigning three amps.
The exact band topology is an implementation decision; do not claim Pro-MB
always uses conventional split/recombine crossovers.

MIDI synthesis, envelope-triggered effects and audio pitch tracking are
separate capabilities. Start low-B onset/pitch experiments before the synth
integration window. Do not call a MIDI oscillator or a delay-feedback pitch
effect a solution to live Transpose quality.

## Acceptance and delivery discipline

| Area | Required evidence |
|---|---|
| EQ/graph | Measured frequency/gain/Q response matches the displayed curve; stable smoothing, bypass and automation; state roundtrip |
| FFT/UI | Bounded audio-to-UI transfer; no FFT/UI allocation or blocking in the callback; sensible hidden/idle work |
| Dynamic EQ/dynamics | Detector, range and displayed GR reflect applied gain; repeatable attack/sustain/low-B/palm-mute comparisons |
| Multiband/saturation | Band isolation/solo, intended sum/phase, transitions and dry/wet alignment; actual four-/six-band callback cost |
| Limiter/Studio | Ceiling and intersample behavior for claimed modes, real host latency, extreme inputs, mode/bypass changes |
| Routing | Correct Classic/Dual/Matrix topology, untouched LOW clean DI branch, stable IDs, saved order and undo from first editable release |
| Room | Audible size/absorption/send response, LOW DI isolation and lane weighting, gate/tail continuity, old-project Room Off, exact state/automation and measured full-product realtime cost; spatial claims need declared model evidence |
| Head UI | Correct six native contexts, per-channel/shared control memory, matching hit/paint transforms, no writes from viewing, complete host gestures |
| Host and release | Existing 48 presets, old projects, A/B and IR state, representative sample rates/buffers, callback timing and actual host lifecycle/installer results |

Use precise statuses: IMPLEMENTED, TECHNICALLY VERIFIED, LISTENING/HOST
ACCEPTED, CANDIDATE or DELIVERED. Report missing evidence at the affected
feature. Existing tests supplement actual host/playing acceptance and must
not be replaced with unsupported PASS labels.

The two development tasks retain their cadence: daily 15:00 KST independent
implementation, weekdays 17:00 KST Astra validation. Use September 24 as the
roadmap end when those tasks consult this calendar; this document does not
change automation configuration. They must read this plan and the newest source,
continue independent work while a specific validation item is blocked, and
record the next reviewable result. Installer/NAM result delivery remains a
separate existing hourly condition watch.

## Related project records

- [Catalog counts and historical baseline](CATALOG_ROADMAP.md)
- [Embedded amplifier-head controls](AMP_HEAD_CONTROLS_NEXT_UPDATE.md)
- [Signal-path navigation and editing](SIGNAL_PATH_EDITOR.md)
- [CAB source/identity/acceptance boundaries](CABINET_PANEL_ROADMAP.md)
- [Room implementation and dated checkpoints](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/issues/46)
- [Transpose long-term work](TRANSPOSE_LONG_TERM.md)
- [1.3 release preparation](RELEASE_1_3_PREPARATION.md)
