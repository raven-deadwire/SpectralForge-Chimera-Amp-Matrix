# Cabinet control panel roadmap

## Current 1.3 preparation — 2026-10-09 KST

The original CAB engine, 14 speaker designs, 20 microphones and nine explicit layouts now exist in the CAB branch. The owner selected **1.3** for the current release preparation. Whole-room framing, cabinet artwork preservation and duplicate visible layout entries are being corrected before the final candidate. [Release preparation](RELEASE_1_3_PREPARATION.md) is the current delivery record; [head-front controls](AMP_HEAD_CONTROLS_NEXT_UPDATE.md) belong to the next update after 1.3, with no assigned version yet. The October 8 architecture and reference boundaries below remain applicable; its initial prototype schedule is historical.

## October 8 design baseline

Status at the October 8 checkpoint: **DESIGN DIRECTION APPROVED / IMPLEMENTATION PENDING**.
Updated 2026-10-08 KST after the owner's clarification:
study existing products, then independently design Chimera's own cabinet and
microphone engine. Delivery track remains **1.3.x CAB**, alongside Niflheimr.
This documentation update does not implement DSP, run training or approve a release.

## Product decision: original design first

**Reference research -> acoustic design requirements -> original DSP ->
production CAB controls -> technical and musical validation.**

Existing products are research and comparison references, not mandatory
one-to-one clones or a collection of files to rename. Learn useful response,
resonance, decay, directivity and microphone-combination behavior, then author
Chimera-specific cabinet definitions, microphone models and control laws.

The default factory path is **Original / Modeled**. Missing measurements of a
particular commercial cabinet must not stop original DSP, functional microphone
movement, or a playable CAB prototype. Do not merely build a UI shell while
waiting for a licensed grid. Good musical results, safe controls and distinctive
roles are required; exact hardware matching is a separate evidence claim.

This revision supersedes the earlier capture-only CAB requirements in the
October 8 roadmap, development summary, IR design notes, and embedded CAB text
in the two development/validation task prompts. It does not change their
non-CAB scope, cadence, legacy compatibility or release requirements.
The earlier reference inventory is retained below, not silently substituted.
The prior full plan remains in Git history at af1ad71cf6d279015627ae7f2bb0c869e670bb53.

## Three response sources, one panel

| Source | Response and controls | Dependency boundary |
|---|---|---|
| Original / Modeled | Independently authored cabinet/speaker response and virtual microphone models. Position and Distance operate within a declared modeled domain. | No measured cabinet grid is required. Mark simulated coordinates and assumptions as modeled, not measured. |
| Captured | Actual qualified fixed captures or measured grids. Enable continuous movement only where suitable measured coverage and interpolation validation exist. | Missing data or distribution permission blocks that capture bank, not the Original engine. |
| User IR | Preserve the user's WAV/AIFF and its existing audio. Level, blend, polarity, delay and cuts remain available. | Do not infer a new microphone or physical placement from one baked-in cabinet/mic IR. Extra virtual shaping, if added later, is explicit, off by default and labeled modeled. |

Use stable source-kind and definition-version identities. Never replace a
saved modeled cabinet with a subsequently acquired capture automatically,
or rename a captured commercial cabinet as an independently created response.
Changing implementation kind requires an explicit selection or migration.

## What learning means in this project

Research, parameter fitting and neural-network training are different actions.
The owner's direction requires reference-informed design; it does not require
an ML architecture where an explicit DSP model is more suitable.

| Stage | Work | Required record |
|---|---|---|
| Reference study | Review official specs, research and appropriately usable audio; compare several references per desired role where available. | Exact source, scope, known/unknown metadata and permitted use. Listening descriptions are not isolated transfer-function measurements. |
| Acoustic specification | Define desired spectral envelope, resonance/decay, position/distance behavior, phase, low-frequency support and headroom. | Authored targets and tolerances, explicitly separated from measured facts. |
| Original design | Implement bounded cabinet, speaker and mic definitions and their interacting controls. | Authored parameter definitions, rationale, generator version and reproducible response outputs. |
| Optional fitting/training | Fit or train only when useful, using an appropriate reviewed dataset; keep validation inputs separate. | Dataset/source hashes, allowed-use decision, preprocessing, fit/training configuration and held-out results. |
| Product validation | Compare raw and output-level-matched renders on common inputs; assess musical role and realtime behavior. | Technical evidence, listening observations and real-host results reported separately. |

Renaming, converting an IR to coefficients, or fitting weights to it does not
by itself establish an independent origin or clear its use for distribution.
Keep reference-only, fit/training-approved and redistribution-approved inputs
separate. Unknown permissions block that use, not unrelated original design.
Do not copy product code, artwork, presets or audio into the factory engine.

The user-supplied Origin Effects IR CAB LIBRARY remains a separately sourced
personal/reference collection, not a cleared factory or training dataset by
virtue of being uploaded. Use it only within reviewed permissions. Preserve its
original file identities; Bright/Medium/Dark labels are not physical coordinates.
No bulk fitting, derivative-bank distribution or neural training on that pack
is authorized merely by this design decision. No external purchase or contact
is part of this update.

## First working implementation

Build **one original guitar cabinet and one original bass cabinet** through
the same production CAB path before expanding the catalog. Working engineering
identities may describe a guitar 4x12 and bass 4x10 role; final product names
are a separate decision. They are not Mesa, Darkglass or other hardware clones.

The first result must be playable: Mic A/B selection, modeled position and
distance, level, blend, polarity, fine delay, cuts and bypass must affect actual
audio and survive project/A-B restoration. A response generator without product
integration is an intermediate result, not completion of the CAB panel.

Start with a small set of meaningfully different original microphone roles,
such as an attack-focused dynamic, body-focused ribbon and bass-definition
dynamic. Expand only after response and interaction tests establish useful
differences. Do not relabel eight copies of one EQ as eight microphone models.

## DSP design boundaries

The proposed architecture separates cabinet/speaker definition, microphone
response and spatial control, then produces a prepared response for each mic.
These are engineering requirements, not claims of completed physical modeling.

- Model low-frequency resonance and damping, broad response, mid/high resonances
  and notches, and high-frequency radiation behavior. Do not assume a simple
  low-frequency piston approximation also captures cone breakup or close-mic
  high-frequency behavior.
- Define position and distance jointly with microphone type. Use a declared
  modeled domain and finite, stable behavior near every boundary. State the
  reference plane and model assumptions if numerical distance is shown.
- Treat direct-arrival delay, response phase and user fine delay separately
  from processing latency. Do not add arbitrary room reverb when Distance moves.
  Preserve predictable near/far level behavior and expose any compensation policy.
- A single existing cabinet-plus-mic IR does not uniquely identify its separate
  cabinet and mic factors. Adding a second mic EQ is not a verified remiking.
- Author each Original response independently; do not use weighted averages of
  unrelated captures as proof of an original physical cabinet or positional grid.
- The first delivery uses a linear response at each fixed control setting.
  Level-dependent speaker compression/distortion requires a separate dynamic
  model and is not represented by a single static IR. Do not claim otherwise.
- Generate/resample/prepare/cache modeled kernels off the audio callback, reuse
  the existing convolution ownership model, coalesce rapid changes, reject stale
  generations and crossfade safely. Direct-filter alternatives need the same
  stability, transition and recall evidence. Do not claim an unmeasured CPU saving.

## Panel and compatibility contract

| Area | Requirement |
|---|---|
| Navigation | RIGS / CAB / PRE / POST, with a compact lane summary opening the matching CAB context. Global utilities remain available. |
| Routing | Classic, both Dual modes and Matrix LOW/MID/HIGH retain lane AMP -> CAB -> merge. No global-cab replacement of per-lane processing. |
| Mic A/B | Two independent mic/IR paths, enable, solo/mute, trim, polarity, pan for mono or explicitly defined balance for stereo. Works with mono input. |
| Movement | Original uses modeled Position/Distance; Captured uses validated measured coverage; fixed User IR does not pretend to expose measured motion. |
| Mix | Constant-sum blend and separate cabinet output. Identical unity A/B signals do not gain at midpoint; B disabled retains A unity. Define mute/solo behavior explicitly. |
| Phase | Mic polarity and fine delay are distinct controls. Automatic alignment, if included, is explicit and never silently changes saved values. |
| Other controls | Preserve lane cuts/bypass; support cabinet suggestions and Lock/Unlink without overwriting an explicitly locked cabinet. |
| User collection | OPEN IR, ADD FOLDER, drag/drop, Guitar/Bass/Unspecified correction and persistent REMOVE FROM LIST. Do not restore GET BASS IRS or ADD PERSONAL ZIP. |
| Names/artwork | Original Chimera names and artwork; exact research references remain in provenance metadata. No false endorsement or unverified exact-clone claim. |

Retain both existing fixed factory IRs and all personal capture identities.
Migrate old states to A only, B disabled, unity added trim, zero added mic delay,
normal mic polarity and unchanged original filters/onset/normalization. Existing
lane polarity stays at its existing stage; do not invert twice.

Keep the four existing cabinet-source choice values and normalized automation
mapping unchanged. Append new parameter identities rather than expanding an old
choice in a way that remaps host automation. Preserve MIDI, .chimera and A/B.

Removing an IR from the collection must not delete its source file or invalidate
already embedded project audio. Use a shared hash-addressed embedded asset table
and a total serialized-state budget, including metadata and A/B. The reviewed
processor's 64 MiB state cap and 4 MiB per-import cap need explicit consideration:
deduplication does not bound genuinely different large files across snapshots.
Reject a new over-budget operation while retaining the recoverable old state;
never truncate assets or save a state the reader rejects.

Version Original definitions and generator behavior so saved sessions do not
silently change tone with a new model revision. Keep old definitions available
or provide an equivalent explicit compatibility mechanism and regression proof.

Room, angle, rear mic, Sub Kick and separately adjustable tweeter/port layers
remain later scope. Original horn/port components may be modeled with explicit
assumptions; Captured horn/port identity must reflect actual capture metadata.
Neither source may expose a disconnected or misleading control.

## Target schedule and task ownership

The 1.3.x track and existing date windows remain internal planning targets,
not public delivery promises. Already authorized work may advance earlier;
neither old start dates nor missing external captures justify stopping it.

| Target window, Asia/Seoul | Development result | Validation responsibility |
|---|---|---|
| 2026-10-08–10-11 | Original response architecture, reference-use register and authored control/role targets. | Separate documented behavior, modeling assumptions and optional fitting inputs. |
| 2026-10-12–10-25 | Playable original guitar + bass prototype, two mic paths, worker/cache, CAB page and legacy migration. | Verify actual parameter-to-audio response, stability, state, phase and initial CPU costs. Not a UI-only prototype. |
| 2026-10-26–11-08 | Refine modeled spatial behavior and distinctive cabinet/mic roles; add original starting presets. Captured banks are optional parallel work. | Response-sweep/transition coverage, common-input comparisons, held-out checks where fitting is used, and listening when actual DI is available. |
| 2026-11-09–12-04 | Niflheimr-line integration, qualified Original factory inventory, manuals and 1.3.x candidate. | Target-hardware CPU, real-host lifecycle, guitar/bass listening and exact-source release evidence. |

Both existing task queues already read this document. Their CAB work must use
this Original-first revision instead of older measured-grid-only instructions.
The daily development track owns functional engine/UI/state/instrumentation;
the weekday Astra track owns reference interpretation, voicing and evidence
review. Missing Astra input does not stop independent implementation. Missing
real DI/host access still prevents claiming musical/host acceptance.

No schedule cadence, unrelated task scope, model-count contract, release policy,
threshold, waiver, tag, binary version or public release is changed here.

## Reference inventory, not a mandatory cloning or capture checklist

The previously proposed **7 guitar / 5 bass / 8 mic** lineup is retained as a
research and long-term coverage inventory. It is not an assertion of acquired
assets, a compulsory first-release count or a requirement to clone each product.
Original product roles may draw on multiple references. Keep their authored
identities distinct from captured hardware names and choose final counts from
verified differentiation, not branding alone. CAB counts remain outside 132.

| Research group | References retained | Identity constraints |
|---|---|---|
| Modern guitar 4x12 | MESA Rectifier Standard OS Straight/V30; Orange PPC412/V30; Bogner 412ST/V30 in the supplied Origin collection | Different enclosures, not interchangeable evidence. Existing Mesa Traditional is not Standard OS. |
| British guitar | Marshall 1960A/G12T-75; 1960AX/G12M-25; Framus/G12M; Origin's 1960B/G12H55 and 1960A/G12M | Record actual speaker/revision; a 1960A name alone does not prove G12T-75. |
| Clean/open guitar | Twin '65/Jensen C12K; Bassman/Jensen C12N or C12NA; Origin's Twin/JBL D120F and other vintage combos | JBL, C12K, C12N and C12NA remain distinct. |
| Requested modern drivers | Eminence Karnivore; Celestion G12-100 Raven | Do not substitute G12K-100 for Raven. Actual enclosure/driver details belong to each capture, not inferred from a speaker page. |
| Classic bass | Ampeg SVT-810E; separately identified SUNN 200S/215S-family or older 2x15 references | Keep exact era/enclosure/speaker identity and Classic/Heritage differences. |
| Modern/reference bass | Darkglass DG410NE; Glockenklang Double; Vanderkley 212MNT | Double is not Double Art; document horn/port scope. |
| Alternate bass | Hartke HyDrive HD410; MESA Standard PowerHouse 2x15; prior personal bass imports | HD410 is not HX410 or PRO2200. Ashdown stays excluded. |
| Dynamic microphone references | SM57, MD421-II, e906, RE20, D112 MkII | Record revision and switch state where known; original modeled mic names do not claim exact reproduction. |
| Ribbon/condenser references | R-121, M160, U87 Ai | Pattern, front/rear side, pad/filter and distance matter; missing metadata remains unknown. |

Existing private Raven/Karnivore/Shift Line/Traynor/Bassman/Hartke imports remain
separate from factory distribution. No personal audio is added by this update.

## Completion evidence

| Gate | Required result |
|---|---|
| Original provenance | Authored definitions/generator versions; approved scope for any fitting/training inputs; reproducible outputs; no unapproved third-party payloads. |
| Functional sound | Actual production-path cabinet and mic selection/motion/mix response. Authored role-specific targets, not file-count or UI-only success. |
| Modeled movement | Finite stable response across the declared domain; continuity and meaningful, mic-dependent position/distance effects. No universal center-to-edge rule asserted without evidence. |
| Captured assets, when included | Real files, capture/source/coordinate/gain/phase metadata, hashes and distribution evidence. Node reproduction and interpolation coverage validated separately. |
| Legacy | Existing factory/User IR projects preserve audio, automation, MIDI, A/B, original-file independence and modeled-definition revision compatibility. |
| Routing | Classic, Dual Blend/Crossover and Matrix; mono/stereo; LOW DI 0/50/100%, low-note and crossover interaction. A natural cabinet roll-off is not a unity-DI fidelity requirement. |
| Realtime | Watched callback allocation/blocking absent; repeatable timing and deadline misses on stated hardware, up to six stereo mic paths plus swap overlap. |
| Lifecycle | UI open/close, instance removal, project/DAW close, prepare/release and worker teardown. |
| Musical use | Same-DI guitar/bass raw and level-matched listening; articulate low notes, controlled fizz and distinct useful roles with Náströnd/Niflheimr. |
| Delivery | Actual qualified factory inventory, installer/user-asset preservation, EN/DE/KR manual and exact-source acceptance through existing release policy. |

Cover at least 44.1/48/96 kHz, practical buffer sizes and supported amp
oversampling modes. Define numeric thresholds before assigning PASS. Synthetic
probes can validate implementation but cannot stand in for unheard instrument DI
or an unexecuted commercial DAW. No physical-clone, training or release success
is inferred from this design approval.

## Technical and source references

- Original-first decision: owner's follow-up instruction, 2026-10-08 KST.
- Low-frequency electromechanical/acoustic modeling: https://doc.comsol.com/6.3/doc/com.comsol.help.models.aco.lumped_loudspeaker_driver/lumped_loudspeaker_driver.html
- Limits of lumped/piston speaker treatment: https://doc.comsol.com/6.3/doc/com.comsol.help.aco/aco_ug_pressure.05.045.html
- UX and fixed custom-IR behavior: https://neuraldsp.com/getting-started/tips-for-using-your-plugin
- Origin source and terms to review per intended use: https://origineffects.com/product/ir-cab-library/
- Earlier exact hardware/microphone source list: https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/blob/af1ad71cf6d279015627ae7f2bb0c869e670bb53/docs/CABINET_PANEL_ROADMAP.md#references

Repository dependencies: Source/Cabinet.h, Source/IRLibrary.cpp,
Source/PluginProcessor.cpp, existing state/routing tests and current PR #24.
Read their latest revisions before implementation. This plan does not assert
that an open branch has merged or that a future prototype already exists.
