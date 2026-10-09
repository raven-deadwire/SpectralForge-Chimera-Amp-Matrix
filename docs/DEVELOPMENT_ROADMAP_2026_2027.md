# Chimera development roadmap, October 2026–August 2027

Updated: **2026-10-10, Asia/Seoul**. This is the current implementation plan
following the owner's request to review the [FabFilter product range](https://www.fabfilter.com/products),
restore the omitted graphical EQ/dynamics/multiband work, add a visible editable
signal path, and open Chimera on the amplifier view.

This plan supersedes the forward dates in the earlier 132-model schedule,
the October 8 CAB prototype calendar, and the unassigned next-head-update date.
Released 1.1.1, 1.1.2 and 1.2 remain history. Dates below are **internal work and
candidate targets, not guaranteed public release dates**. Select work by actual
completion, priority and satisfied dependencies; do not wait for an obsolete
calendar window. Re-estimate the next four weeks at each checkpoint.

## Current baseline and immediate work

The inspected 1.3 candidate is PR #35, commit
[e05ca7e](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/commit/e05ca7ee9398a063615e6795f8754caa8406bfd6).
It contains Niflheimr, the original CAB engine, nine cabinet layouts, fourteen
speakers, twenty microphone models and 48 selectable presets. Its active
character catalog is AMP25 + PRE39 + POST21 = **85**. There are 26 serialized
amp IDs because the retired Iron Compact is retained for saved-project recall.
A source feature, a passing technical test and a distributable installer are
different completion states.

At this checkpoint the product builds compiled, but the product CTest runs
failed the integrated processor channel-level test. The Windows candidate also
reported two CAB integration timing failures and skipped Setup generation.
See [product run](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/37951956620)
and [Windows candidate run](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/37951956736).
Read their successors before reporting a current status. Do not bypass a failed
audio test to label an installer complete.

Immediate priorities:

1. Diagnose and resolve the actual 1.3 candidate failures, then produce and
   verify the Windows installer and deliver the real file.
2. Change new editor instances from PRE to **RIGS / amplifier view**. This is
   a presentation default, not a change to DSP order, saved audio mode or tone.
3. Start the EQ, common state and head/navigation work below on an isolated
   next-update branch. Existing installer preparation must not wait for EQ.
4. Continue Transpose low-B/−2-semitone and NAM/reference investigations as
   separate measured work. They do not establish one another's success.

The inspected e05ca7e candidate still opens PRE. The separately prepared
startup correction changes the editor page default from 1 to 0; it requires
integration and actual UI validation in a follow-up candidate. New
Classic sessions show the existing amp view; saved Dual/Matrix sessions open
their corresponding RIGS overview without forcing Classic. An already open
editor keeps its current page during preset/state restoration. This correction
does not implement the later embedded knobs or editable signal-path strip.

## Full reordered calendar

The former June 4 integration target covered a model-led plan that omitted
substantial common processors and routing work. This revision gives that work
explicit time and moves the integration target to **August 13, 2027**.
The model addition order is retained.

| Version | Work window, KST | Main deliverable | Active character models |
|---|---|---|---:|
| 1.3 | Current candidate; deliver after its real gates pass | Niflheimr/CAB installer and candidate fixes; startup RIGS is a prepared follow-up correction | 85 |
| 1.3.1 | 2026-10-10–2026-11-01 | Graphical 12-band EQ, input/output FFT, Tone/Final positions, signal-path display and navigation; representative embedded head controls | 85 |
| 1.3.2 | 2026-11-02–2026-11-27 | Dynamic EQ, clean compressor, gate/expander improvements, live output limiter; expand head-control coverage | 85 |
| 1.4 | 2026-11-30–2027-01-08 | Independent four-band dynamics, PRE Expansion A six models, first constrained signal-order editor | 91 |
| 1.5 | 2027-01-11–2027-02-05 | PRE Expansion B six models, drag-order UX; separately gated M/S, external sidechain and six-band dynamics expansion | 97 |
| 1.6 | 2027-02-08–2027-03-12 | Five reference amps and an independent three-band saturation processor | 102 |
| 1.7 | 2027-03-15–2027-04-09 | Nine POST Dynamics/Tone models and a common two-filter modulation processor | 111 |
| 1.8 | 2027-04-12–2027-05-07 | Nine Motion/Space models, advanced delay and reverb controls | 120 |
| 1.9 | 2027-05-10–2027-06-04 | Nine Creative variants, expanded modulation and routing polish | 129 |
| 1.10 | 2027-06-07–2027-07-16 | Dvergr Analog, Seidr Dynamic and Mimir Impact synth layers; integrate qualified tracking work | 132 |
| 2.0 | 2027-07-19–2027-08-13 | Whole-product host/audio/state/performance/preset/installer integration and RC | 132 |

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
| 2026-12-11 | Four-band dynamics prototype with band sum/phase evidence |
| 2026-12-18 | First constrained order-editing preview with save/restore and undo |
| 2027-01-08 | Combined 1.4 candidate: verified four-band dynamics, PRE A and supported order editing |

A preview means implemented behavior, not a mock screenshot. October 23 does
not promise Dynamic EQ, multiband processing or all 25 interactive head layouts.
Keep the existing functional detailed controls as a fallback while dense
faceplates are completed. Full coverage means 25 active heads plus the one
recall-only model, across the existing six native contexts.

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
| Immediately after rig merge, before POST | Tone EQ, clean compressor/expander, independent multiband dynamics | Shape the performance before delay/reverb tails are generated |
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
UI. Tone EQ is not initially movable across the rig split; Final EQ and the
last limiter remain fixed roles. See [signal-path editor](SIGNAL_PATH_EDITOR.md).

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
| Head UI | Correct six native contexts, per-channel/shared control memory, matching hit/paint transforms, no writes from viewing, complete host gestures |
| Host and release | Existing 48 presets, old projects, A/B and IR state, representative sample rates/buffers, callback timing and actual host lifecycle/installer results |

Use precise statuses: IMPLEMENTED, TECHNICALLY VERIFIED, LISTENING/HOST
ACCEPTED, CANDIDATE or DELIVERED. Report missing evidence at the affected
feature. Existing tests supplement actual host/playing acceptance and must
not be replaced with unsupported PASS labels.

The two development tasks retain their cadence: daily 15:00 KST independent
implementation, weekdays 17:00 KST Astra validation. Their roadmap end moves to
August 13 with this calendar. They must read this plan and the newest source,
continue independent work while a specific validation item is blocked, and
record the next reviewable result. Installer/NAM result delivery remains a
separate existing hourly condition watch.

## Related project records

- [Catalog counts and historical baseline](CATALOG_ROADMAP.md)
- [Embedded amplifier-head controls](AMP_HEAD_CONTROLS_NEXT_UPDATE.md)
- [Signal-path navigation and editing](SIGNAL_PATH_EDITOR.md)
- [CAB source/identity/acceptance boundaries](CABINET_PANEL_ROADMAP.md)
- [Transpose long-term work](TRANSPOSE_LONG_TERM.md)
- [1.3 release preparation](RELEASE_1_3_PREPARATION.md)

