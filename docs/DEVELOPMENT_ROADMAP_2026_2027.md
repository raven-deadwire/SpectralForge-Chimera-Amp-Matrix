# Chimera development roadmap, October 2026–December 2027

Updated: **2026-10-19, Asia/Seoul**. The owner requested a new remaining-work
schedule after using 1.3.1: simplify the signal-chain display, substantially
improve the EQ analyzer and editing workflow, implement the missing
[FabFilter reference functions](https://www.fabfilter.com/products), and add
AAX support. The same-day follow-up makes **five-channel Niflheimr NAM delivery
required** and adds **separate, simplified Náströnd and Niflheimr VST3 products**,
using Neural DSP and Odeholm Audio as workflow references.

The verified public baseline is [1.3.2-beta.1](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/releases/tag/v1.3.2-beta.1), published **2026-10-18 01:51:47 KST** from `3bec6c70a50bbb4021dcc6133f37e0c2fb3e9e58`, merged by #95 into main `ec6fd639e2958d34605bfb04bb81ca02c1cfb38f`. Tags, 30 assets and feed remain immutable. Signal navigation, precision tuner, EQ editing/analyzer and common dynamics are public beta features; original-analysis fidelity and actual DAW acceptance remain distinct. The release's strict 21 PASS / 107 BLOCKED outcome is retained, not relabelled as full acceptance.

The owner's latest October 12 correction rejects a roadmap stretched into
2028 and requires **at least one substantive update or maintenance delivery
every week**. This revision removes the proposed stabilization-only pause,
keeps October/November work active, and explicitly allocates twelve weeks for
shared Original FX and the two focused products. No additional staffing is
assumed. The earlier 2028 chat proposal was not applied to this repository.

Dates below are **internal work and candidate targets, not guaranteed public
release dates**. Weekly changes continue during every larger feature window.
The October 23 development/conditional review target now covers issue #97 in 1.3.2-beta.2. The lower unpublished lifecycle reservation is cancelled after the higher public release. Re-estimate
the next four weeks using actual results; do not wait for a calendar boundary.

## Exact versions and deployment calendar

The owner's follow-up requests concrete version numbers, update contents and
deployment dates. [The release calendar](RELEASE_CALENDAR_2026_2027.md) expands
these work windows into Wednesday test builds, Thursday candidates and Friday
public-release targets. Its machine-readable mirror is
[release-calendar.json](../Validation/release-calendar.json). The calendar is a
planning record, not an updater feed and not a claim of published artifacts.

Assign **1.3.3 to shared Original FX**; Room Core becomes **1.3.4**, retaining
February 22–March 19. Version 1.4.0 onward keeps its existing windows. Use three
numeric components for public versions, retain the existing Chimera `v` tags,
and give the two focused products and two NAM packs separate tag namespaces.
October 16 beta.3 (#62) is permanently cancelled with missed/deferred/cancellation history. Beta.1 published early on October 18 and its former October 23 reservation is completed history. Conditional targets are beta.2 (October 23, issue #97), beta.3 (October 30), then beta.4–beta.7 in November. Beta counters increase
per product/base version; already published tags and files remain immutable.

Chimera 1.x stays on its existing public-beta track in this plan. The 1.3.3
beta counter continues through the January/February maintenance allocation;
do not publish stable 1.3.3 and then return to lower-precedence 1.3.3 betas.
An earlier stable release or urgent patch consumes the next unused patch
number and requires explicit remapping of later reservations before shipping.
RC labels require feature freeze and actual candidate acceptance; stable labels
require the declared release checks and are not generated merely by a date.

First focused public betas remain January 22 (Náströnd) and February 19
(Niflheimr). Reserve February 26 / March 12 for Náströnd RC / stable review and
March 19 / April 2 for Niflheimr RC / stable review. These stabilization checks
consume the existing maintenance/validation capacity; a larger correction
explicitly displaces later work instead of assuming a second full-time team.

Main already implements explicit source/version weekly publication, beta/RC/stable generation and updater transitions. Calendar CI and the publisher now check reservations against immutable public history and live tags/releases. VERSION/RELEASE_CHANNEL remain the source identity, not a value inferred from the calendar. Historical beta.2 approval does not authorize another source/version. Product-specific identities and real installation/host qualification remain required.

#62's unpublished 1.3.1-beta.3 reservation is cancelled under the higher-version-first rule. Original October 14/15/16 targets, defer reason and October 19 cancellation are retained. It cannot resume after 1.3.2-beta.1. #68/#90 remain historical implementation/qualification candidates; their old failures and scope are not current release authorization. #95 and the official release are the public receipt.

**Next update: issue #97, planned version 1.3.2-beta.2.** Its four P0s are Neural DSP Transpose analysis applied to the product, FabFilter analysis applied to the relevant processing, beta lifecycle/remaining-time improvements and signed AAX Native on Windows/macOS with normal Pro Tools acceptance. System requirements and actual test configurations accompany the build. Existing public features are reused; their mere presence is not evidence of original-analysis completion. October 21/22/23 are test/candidate/development-review targets, not permission to publish. Incomplete P0s require explicit reservation review rather than automatic rollover, an empty bump or omission of AAX. The October 30 remaining EQ scope follows P0 completion.

The public beta.1 expiry **2027-02-11 23:59 KST** is historical and version-specific. Beta.2 expiry and publication approval are undecided; no approval or timestamp is inherited. Continue independent implementation and verification while prerequisites are resolved. The previous November 27 first-AAX review is superseded by #97's next-update requirement; signing and real-host evidence remain required.

<!-- release-reservation-status:start -->
| Version | Original checkpoint (KST) | Active target (KST) | Reservation status |
|---|---|---|---|
| `1.3.1-beta.3` | 2026-10-16 | TBD | cancelled |
| `1.3.2-beta.2` | 2026-10-23 | 2026-10-23 | blocked |
| `1.3.2-beta.3` | 2026-10-30 | 2026-10-30 | planned |
<!-- release-reservation-status:end -->

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

## Released baseline and remaining priorities

| Delivered baseline | Verified scope | Remaining follow-up |
|---|---|---|
| 1.3.0-beta.1 | Niflheimr and CAB generation shipped; superseded by 1.3.1-beta.1 | Actual instrument/host acceptance and identified performance issues continue on their own records |
| 1.3.1-beta.1, October 12 | Tone/Final static EQ with 12 nodes each and input/output FFT; actual route navigation; 25 active heads plus one saved-project-compatible head across six native contexts | Compact signal display, analyzer quality and EQ workflow; dynamics and AAX remain unimplemented |

Public beta delivery is distinct from complete production, listening and
commercial-host acceptance. Preserve the existing source-bound beta decision
and historical evidence. This planning revision changes no product code,
published package, source pin, release policy or acceptance threshold.

The retained active catalog is AMP25 + PRE39 + POST21 = **85** character models.
There are 26 serialized amp IDs and 48 selectable presets. Nine CAB layouts,
fourteen speakers and twenty microphones remain separate inventory dimensions.

The immediate priority order is:

1. **Signal-chain and EQ usability:** five primary navigation blocks, a large
   selected-EQ view, readable moving spectra and the missing band-edit workflow; also remove version-specific publication pins without weakening evidence checks.
2. **Audio and host reliability:** continue low-B/-2 Transpose quality/latency,
   real-host lifecycle, UI/callback performance and NAM/reference acceptance.
   A reproducible critical regression takes priority over feature expansion.
3. **Common dynamics:** keep the November Dynamic EQ/compressor/gate/limiter
   window, reusing the improved graph and editing foundation.
4. **AAX Native:** start build/readiness work alongside the above; budget it
   explicitly and keep external host/signing dependencies visible.
5. **Focused amp products and Niflheimr NAM:** develop separately installable
   Náströnd and Niflheimr products around their existing five-channel native
   cores, plus the required Niflheimr NAM pack. Freeze shared capacity on
   October 16; the new allocation below separates shared FX, each product and Room.
6. **Room and model expansion:** preserve their scope and model order while
   moving their windows twelve weeks to fund the focused-product work.

AMP/RIGS startup and the saved Classic/Dual/Matrix mode are already present.
Preserve existing EQ frequency/gain/Q editing, filter types, calibrated stereo
FFT and whole-product A/B; these are not new backlog items.
See [the concrete EQ follow-up](EQ_ANALYZER_WORKFLOW_UPGRADE.md) and
[signal-path presentation requirements](SIGNAL_PATH_EDITOR.md).

## Full reordered calendar

The previously registered integration target was September 24, 2027. This
revision adds **three consecutive four-week allocations**, rather than putting
two products and Room in the same four weeks: shared Original FX first,
Náströnd productization second, Niflheimr productization third. Existing Room
Core and version 1.4.0 onward shift twelve weeks, retaining their original
durations and model order. The new **2.0.0 integration/RC target is December 17,
2027**, with December 20–31 reserved for final corrections and carryover.

The twelve weeks are an explicit planning allowance, not measured throughput.
Reuse the existing working amp, compressor, EQ, drive and space code. Replace
or extend a DSP algorithm only for a documented missing behavior or quality
problem; product shells and different control layouts do not justify separate
engine rewrites. Shared improvements return to Chimera in the same weekly flow.
A first beta does not require every later Original-engine enhancement to exist.

| Version / allocation | Work window, KST | Main deliverable | Retained character-model plan |
|---|---|---|---:|
| 1.3.2-beta.2 → 1.3.2-beta.3 | 2026-10-12–2026-11-01 | Lifecycle candidate, then integrated signal/tuner/dynamics and focused EQ/analyzer improvements | 85 |
| 1.3.2-beta.4 through beta.7 | 2026-11-02–2026-11-27 | Follow-up qualification and substantive improvements to integrated Dynamic EQ, compressor, gate/expander and limiter | 85 |
| 1.3.3 — shared Original FX, first allocation | 2026-11-30–2026-12-25 | Reusable FX interfaces, guitar BOOST and bass Drive/Fuzz behavior, common COMP/EQ integration, PRE board/state foundation; qualified existing pitch/modulation/space reuse | 85 |
| Náströnd focused VST3 beta | 2026-12-28–2027-01-22 | Five-channel guitar product, full controls, four replaceable PRE slots, focused five-area UI, independent package/presets | 85 |
| Niflheimr focused VST3 beta | 2027-01-25–2027-02-19 | Five-channel bass product, full controls, four PRE slots, preserved LOW/Blend semantics, independent package/presets | 85 |
| 1.3.4 | 2027-02-22–2027-03-19 | Room Core: Size/Absorption/Bypass, per-rig Send, actual early/late response, state and LOW DI isolation | 85 |
| 1.4.0 | 2027-03-22–2027-04-30 | Independent four-band dynamics, PRE A six models and constrained signal-order editor | 91 |
| 1.5.0 | 2027-05-03–2027-05-28 | PRE B six models, drag-order UX; separately qualified advanced dynamics modes | 97 |
| 1.6.0 | 2027-05-31–2027-07-02 | Five reference amps and common three-band saturation | 102 |
| 1.7.0 | 2027-07-05–2027-07-30 | Nine POST Dynamics/Tone models and common two-filter modulation | 111 |
| 1.8.0 | 2027-08-02–2027-09-10 | Nine Motion/Space models, advanced shared delay/reverb and Room Advanced | 120 |
| 1.9.0 | 2027-09-13–2027-10-08 | Nine Creative variants, modulation and routing polish | 129 |
| 1.10.0 | 2027-10-11–2027-11-19 | Dvergr Analog, Seidr Dynamic and Mimir Impact; integrate qualified tracking work | 132 |
| 2.0.0 | 2027-11-22–2027-12-17 | Whole-product host/audio/state/performance/preset/installer integration and RC | 132 |
| Correction reserve | 2027-12-20–2027-12-31 | Remaining release corrections and bounded carryover | 132 |

Counts retain the original character-model inventory contract. Shared processor
types, product instances, modes and focused-product PRE slot counts are separate.
If an Original FX becomes an additional selectable character model, audit and
update its catalogue identity/count explicitly; do not silently count it inside
the retained 132 or count modes as separate pedals.

Model research and independent prototypes can precede integration, within the
same capacity budget. A blocked NAM architecture, pitch method, optional quality
mode or host-specific format blocks that item, not unrelated weekly fixes.
Record changed scope and dependent dates when measured velocity differs.

### Weekly delivery contract

- Deliver at least one **implemented feature improvement or maintenance fix**
  each week, with a source/PR, change-specific verification and a working test
  build. A report, plan, empty version bump or unchanged CI rerun alone does not
  meet the weekly product-delivery goal.
- Keep one principal feature in development and one bounded maintenance item.
  Choose Monday's smallest useful deliverable, integrate through the week, and
  use Friday's existing development/validation runs for the weekly candidate.
  Weekend development may prepare the next slice; do not promise extra capacity.
- Plan approximately 60% feature work, 20% maintenance and 20% verification/
  integration, including packaging and bounded research. These are planning
  shares of the same capacity, not extra tracks. AAX, NAM and pitch experiments
  must fit that budget or explicitly displace another scheduled item.
- Use the first two weeks as concrete commitments and the following weeks as
  rolling targets. Review the next four weeks every fourth Friday, starting
  November 6. Larger feature milestones normally occupy four to six weeks while
  weekly maintenance and incremental improvements remain visible.
- If the principal feature cannot qualify, deliver an independently safe small
  fix already in the maintenance queue. If no candidate qualifies, state the
  missed weekly goal and recovery step; do not fabricate a build or relax a gate.
- Run targeted checks first. Broader candidate validation covers the changed
  paths and required release gates. Reuse valid source-bound evidence; do not
  repeat full CI or NAM training without a changed cause or useful hypothesis.
- Provide the candidate's change, source, actual checks, download/artifact and
  known limitations. Use existing publication authorization when applicable;
  verified fixes may ship weekly, while source-unverified changes stay in their
  own candidate. Do not newly require all historical open issues to close.

### Near-term reviewable checkpoints

These are weekly **implemented changes plus verification/build** targets.
The October 14/16 analyzer prototype reviews remain subordinate tasks, not
additional completed releases. October 23/30 EQ scope is retained; split any
measured carryover explicitly instead of counting unfinished controls as done.

| Week ending, KST | Minimum reviewable weekly increment |
|---|---|
| 2026-10-16 | Historical missed target: #62 beta.3 subsequently cancelled after October 18 beta.1 publication; retain missed/deferred/internal CI history. October 23 now reviews issue #97 beta.2; no inherited expiry or approval. |
| 2026-10-23 | Improved EQ installer: five-block chain, large EQ, filled IN/OUT, smoothing/resolution/Freeze, persistent-slot add/delete and Undo/Redo |
| 2026-10-30 | Remaining EQ workflow: multi-selection, band audition, local A/B, peak hold/range controls; full follow-up candidate or explicitly bounded carryover |
| 2026-11-06 | Substantive Dynamic EQ/GR follow-up to the integrated #68 candidate using existing EQ nodes and repeatable audio |
| 2026-11-13 | Shared clean-compressor increment with actual detector/GR response; Dynamic EQ refinement |
| 2026-11-20 | Gate/expander improvement and a qualified live-limiter increment in the 1.3.2 test installer |
| 2026-11-27 | Common-dynamics integration/maintenance candidate with state, automation and real-time evidence |
| 2026-12-04 | First reusable guitar BOOST improvement integrated with the existing shared FX path |
| 2026-12-11 | Bass Drive/Fuzz role and low-frequency routing increment, preserving native Blend semantics |
| 2026-12-18 | Four-slot focused PRE board/state implementation using independent reusable instances; Chimera keeps five slots |
| 2026-12-25 | Shared-FX first candidate with actual control/audio/state verification and explicit remaining product work |
| 2027-01-08 | Playable Náströnd focused-product preview; its earlier week still supplies a small verified increment |
| 2027-01-22 | First Náströnd VST3 beta candidate, conditional on its product/host/package checks |
| 2027-02-05 | Playable Niflheimr focused-product preview with LOW/Blend routing demonstrated |
| 2027-02-19 | First Niflheimr VST3 beta candidate with coexistence and product checks |
| 2027-02-26 | Room bus/state/CPU-budget review plus a verified implementation/maintenance increment |
| 2027-03-05 | Playable Room Size/Send/Absorption prototype |
| 2027-03-12 | First Room Core Windows test installer |
| 2027-03-19 | Room Core integration candidate and advanced-stage re-estimate |
| 2027-04-02 | Independent four-band dynamics prototype |
| 2027-04-09 | Constrained order-editing preview with state/undo |
| 2027-04-30 | Combined 1.4.0 candidate |

Unlisted Fridays follow the same weekly delivery contract. A preview requires
working behavior. Analyzer acceptance includes moving audio and pointer/
keyboard interaction. Preserve already delivered full head-control coverage.
See [EQ analyzer and workflow upgrade](EQ_ANALYZER_WORKFLOW_UPGRADE.md).

### Focused Náströnd / Niflheimr products and required NAM delivery

Chimera retains Classic/Dual/Matrix and its five-slot PRE board. Each focused
edition uses one active native amp-family channel with independent product,
state, preset, installer and update identities. Amp, compressor, EQ, drive and
space engines are shared; new behavior is developed once and integrated into
the relevant products. See [the focused-product specification](FOCUSED_AMP_PRODUCTS_AND_NAM.md).

Both focused UIs use **PRE / AMP / CAB / EQ / SPACE**, opening on AMP, with full
13/14 channel controls, four replaceable simultaneous PRE slots and large
controls for the selected slot. Gate/tuner/transpose utilities do not create
duplicate processing nodes. Guitar families are BOOST, COMP, PITCH
(Octaver/Whammy modes), WAH and MODULATION (Chorus/Phaser modes). Bass families
are COMP, BASS DRIVE, FUZZ, OCTAVER, FILTER and CHORUS. Modes do not add slots.
Guitar drive is designed primarily for its boost role.

Niflheimr preserves its common LOW and native clean/dirty Blend structure.
Native Blend is not Matrix LOW DI/AMP mix or a raw-DI bypass. Define Drive/Fuzz
insertion and the preserved clean-low route before product integration; any
additional full-band DI branch requires an explicit new design.

| Allocation / checkpoint | Reviewable result |
|---|---|
| 2026-10-16 / 2026-10-23 | Shared-core/identity/scope and extraction review within the October weekly budget |
| 2026-11-30–2026-12-25 | Four weeks for reusable Original FX first increments and focused PRE/state foundation; no blanket DSP rewrite |
| 2026-12-28–2027-01-22 | Four weeks for Náströnd native VST3; five channels and all 13 controls; first beta candidate January 22 |
| 2027-01-25–2027-02-19 | Four weeks for Niflheimr native VST3; five channels and all 14 controls; first beta candidate February 19 |
| Weekly thereafter | Both product maintenance and qualified shared-engine improvements alongside the current main feature |

The new twelve-week reservation supersedes the former unallocated four-week
proposal. Room starts February 22, after the two product windows; dates are
synchronized with the JSON mirror and issue #46. The Windows VST3 beta target
is reviewable, conditional on actual product, host and package acceptance.
macOS Universal VST3 needs separate build/host evidence. Shared infrastructure
does not count as two already implemented products or additional staff.

Both **Náströnd and Niflheimr five-channel fixed-setting NAM packs remain
required**. Retain the Náströnd fidelity/TONE3000 contract and its existing
memory blocker. Niflheimr retains October 16 source/memory, October 23 frozen
capture/quality, October 30 pipeline/memory feasibility and November 13 first
full-training/evidence-report checkpoints. Report missing prerequisites when
necessary; neither smoke success nor a scheduled report proves qualification.
Bound experiments within the same weekly budget and decide the next method,
rather than repeatedly adding epochs to a known insufficient architecture.

The native VST beta is separate from NAM files, optional NAM runtime and signed
AAX. General Chimera-preset import, AU/standalone and signed AAX delivery remain
outside these native VST3 first-candidate estimates. Later enhancements retain
named backlog items; do not advertise a partial PRE library as complete.

### AAX Native parallel track

Initial Chimera scope: **Windows x64 and macOS Universal (Intel + Apple Silicon)**,
using the common Chimera DSP/UI through the JUCE AAX Native wrapper.
The current JUCE 8.0.8 baseline contains the AAX SDK; the current project does
not enable an AAX target. AAX DSP/HDX/Carbon processing is a separate project.
See [AAX Native implementation and delivery plan](AAX_NATIVE_PLAN.md).

| Target, KST | Reviewable result | Dependency |
|---|---|---|
| 2026-10-16 | Freeze platform/bus/ID scope and inventory developer-host/signing access; estimate shared workload | Account and equipment status must be recorded as known or unavailable |
| 2026-10-30 | Internal AAX Native build prototype for Windows/macOS | Working platform toolchains and actual bundle checks |
| 2026-11-20 | Pro Tools test candidate and lifecycle/automation/state/latency report | Pro Tools Developer access; normal Pro Tools checks additionally require signed binaries |
| Uncommitted public date | Signed AAX beta, installed and tested in normal Pro Tools | Avid/PACE access, signature verification, real-host acceptance and packaging |

These are planning targets, not Avid/PACE approval forecasts. Reserve an initial
**10–20 engineering-day estimate**, including host and packaging work, and
re-estimate on October 16 and 23. This allowance does not assume extra staffing.
If it conflicts with the immediate EQ quality work, explicitly revise AAX's
dependent milestones before promising an installer. External approval waits
must not stop unrelated EQ, dynamics or room work. The new focused products
can reuse this build/signing infrastructure, but each needs distinct AAX IDs
and its own signed host/installer acceptance. Their dates and additional work
are not included automatically in the original Chimera 10–20-day estimate.

### Continuing reliability and sound-quality tracks

| Track | Next checkpoint | Required result |
|---|---|---|
| Low-B/-2 Transpose, issue #8 / PR #45 | October 16 baseline review; October 23 same-input candidate comparison; October 30 method decision | Separate smear, pitch stability and measured latency; <=10 ms remains a target, not a completed feature |
| Náströnd five-channel NAM/reference work, PR #22 | October 23 channel-by-channel evidence review; October 30 qualification/carryover decision | Existing fidelity/memory blocker remains; retain TONE3000 compatibility and existing thresholds |
| Required Niflheimr five-channel NAM pack | October 16 memory/source review; October 23 capture/quality contract; October 30 smoke; November 13 fidelity report | Preserve channel order, native Blend/LOW capture boundary and separate full-training, listening and player evidence |
| DAW lifecycle, issue #3 | Every follow-up installer; first report October 23 | Reopen UI, remove plugin, restore sessions, close/exit and sustained operation in available real hosts; include Pro Tools when available |
| UI/CAB real-time performance | Every changed-source candidate | Compare average/p95/p99, worst callback and misses; record hidden-editor cost and actual display load |
| Windows signing / macOS notarization | October 16 readiness review, October 30 next-step report | Record real credentials/service access and completed signature/notarization evidence separately from AAX signing |

These checkpoints require a result or an explicit unmet prerequisite. They do
not invent a user's isolated DI, a successful NAM training run, a DAW session,
signing access or a guaranteed algorithm completion date. Historical product
release and completed CI fixes are not reopened as new feature work.

## Room acoustics schedule and scope

The owner requested adjustable room conditions on October 11 and then asked
for their version/date assignment. [Issue #46](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/issues/46)
tracks actual implementation and acceptance. This schedule assigns two stages.

### 1.3.4 Room Core — February 22–March 19, 2027

Deliver one shared room environment with Small / Medium / Large Size,
Absorption / Damping, Bypass and per-rig Room Send. Include actual early
reflections and late decay, a defined wet-return level, state/automation/A-B
and old-project Room Off defaults. The first controls operate on the RIGS/CAB
room surface in Classic, Dual and Matrix.

| Week, KST | Reviewable work |
|---|---|
| 2027-02-22–2027-02-26 | Freeze weighted send/return topology, gate/tail policy, parameter IDs and numeric callback/memory budgets; verify Room Off preserves existing audio. |
| 2027-03-01–2027-03-05 | Implement bounded room-size/absorption models, early reflections and decay; deliver a playable three-mode prototype. |
| 2027-03-08–2027-03-12 | Complete basic UI, state/automation and smooth switching; measure low-note isolation and full-product cost; produce the first Windows test installer. |
| 2027-03-15–2027-03-19 | Resolve platform/host/tail regressions, verify the installer and document the 1.3.4 candidate. |

Independent Early Level, Decay, Diffusion, room HP/LP and Width controls belong
to the dated 1.8.0 advanced stage. Individual controls may be brought forward
only with their response/interaction evidence; the required 1.3.4 result is
the playable basic room with actual reflections and decay.

Room excitation follows each audible AMP/CAB contribution. Matrix LOW uses
only its effective AMP/CAB portion; DI 100% and LOW AMP off create no new LOW
room input. Preserve lane blend/gain/mute/solo/polarity. With the optional
post-rig gate, apply the same existing gate envelope to the direct rig sum
and room excitation; mix the wet return after that gate and before Tone EQ /
POST. This avoids new excitation through a closed gate and preserves an
already generated tail. Verify the final topology before implementation.

### 1.8.0 Room Advanced — August 2–September 10, 2027

Expand independent early/late balance, decay, diffusion, wet filters and width.
Add declared modeled room dimensions, wall/floor/ceiling absorption or material
presets, cabinet-to-boundary placement and a room-mic stage separate from close
Mic A/B. The existing nine Motion/Space models and advanced POST delay/reverb
remain in this version; allocate six weeks in total, including two added room
weeks.

| Checkpoint, KST | Reviewable result |
|---|---|
| 2027-03-19 | Re-estimate the advanced stage using the actual Room Core architecture and measured cost. |
| 2027-08-06 | Fix spatial-model assumptions, controls, surface presets, acceptance limits and the remaining work estimate. |
| 2027-08-20 | Playable advanced-condition/spatial prototype with declared parameter-to-response behavior. |
| 2027-09-03 | First 1.8.0 test installer including the implemented room and POST scope. |
| 2027-09-10 | Integrated 1.8.0 candidate and explicit coverage/evidence report. |

The additional two weeks are a planning allowance, not proof that every
physical-room claim is achievable. Review the scope on March 19 and August 6;
revise affected dates and unqualified items explicitly rather than silently
dropping the spatial controls or calling an ambience knob physical remiking.
The core room can be used from 1.3.4 independently of this later extension.

RavenForge room styling, common equipment scale, existing close microphones
and private IR identity remain the design baseline. Room is a shared processor
and adds no character models to the 85/132 catalog counts.

## Reference-to-feature mapping

The linked product pages establish the reference functions. The Chimera column
is our own implementation scope; it is not a claim of identical algorithms,
sound, feature count or performance.

| Official reference | Function to study | Chimera implementation and timing |
|---|---|---|
| [Pro-Q 4](https://www.fabfilter.com/products/pro-q-4-equalizer-plug-in) | Graph-node parametric EQ, analyzer, dynamic/spectral processing | Static EQ/FFT delivered in 1.3.1-beta.1; analyzer and edit workflow in the October follow-up; Dynamic EQ and slope/peak editing in 1.3.2; advanced modes separately gated |
| [Pro-C 3](https://www.fabfilter.com/products/pro-c-3-compressor-plug-in) | Detailed compressor timing, detector control and GR feedback | One transparent common compressor in 1.3.2; additional character models remain in POST expansion |
| [Pro-MB](https://www.fabfilter.com/products/pro-mb-multiband-compressor-plug-in) | Independent band dynamics, detector ranges, band solo and metering | Four processing bands in 1.4.0; six-band expansion is a separate 1.5.0 performance gate |
| [Pro-L 2](https://www.fabfilter.com/products/pro-l-2-limiter-plug-in) | Output limiting, true-peak and loudness feedback | Live peak control in 1.3.2; measured Studio true-peak/LUFS option targeted in 1.5.0 |
| [Pro-DS](https://www.fabfilter.com/products/pro-ds-de-esser-plug-in) | Selective high-frequency detection and attenuation | Guitar/bass pick, fret and harshness control using the Dynamic EQ detector; focused controls/presets in 1.3.2 |
| [Pro-G](https://www.fabfilter.com/products/pro-g-gate-expander-plug-in) | Gate/expander timing, sidechain and clear gain-reduction display | Improve the existing gate in 1.3.2; preserve attack/sustain and use a defined DI detector tap |
| [Saturn 2](https://www.fabfilter.com/products/saturn-2-multiband-distortion-saturation-plug-in) | Band-specific saturation, drive/mix and modulation | Independent three-band saturation in 1.6.0; low-band clean preservation, a small set of authored saturation types |
| [Timeless 3](https://www.fabfilter.com/products/timeless-3-delay-plug-in) | Dual delay, tempo, feedback filtering, ducking and modulation | Advanced dual delay controls in 1.8.0; multi-tap/pitch-feedback/freeze expansion in 1.9.0 where validated |
| [Pro-R 2](https://www.fabfilter.com/products/pro-r-2-reverb-plug-in) | Algorithmic room/plate/space, decay shaping, wet EQ and ducking | Room/Hall/Plate controls, predelay/width, three-region decay shaping and wet EQ in 1.8.0 |
| [Volcano 3](https://www.fabfilter.com/products/volcano-3-filter-plug-in) | Multiple filters, drive and modulation | Two-filter LP/HP/BP/notch processor with LFO/envelope follower in 1.7.0; advanced modulation in 1.9.0 |
| [Twin 3](https://www.fabfilter.com/products/twin-3-synthesizer-plug-in) | Oscillator/filter/envelope, MIDI and modulation architecture | Common synth foundation and the three planned layers in 1.10.0; instrument pitch tracking remains its own research gate |
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
| Room return, from 1.3.4 | Weighted per-rig AMP/CAB sends into one shared room; wet return after the optional post-rig gate and before Tone EQ / POST | Preserve LOW DI and natural room decay; downstream Tone/dynamics intentionally also process room return |
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

Preserve the delivered stable band IDs. Prepare internal detector routes,
channel modes and host-bus contracts for the next common-dynamics stages. Expose each advanced control only after its processing and
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

Target a Studio true-peak/LUFS candidate in 1.5.0, independently of Live limiter
acceptance. Evaluate spectral/linear-phase prototypes during 1.7.0; include only
qualified work in 2.0.0, otherwise assign a concrete 2.x follow-up after the
research checkpoint. Review EQ Match, EQ Sketch and cross-instance/rig spectrum comparison during
1.7.0: requirements by July 9 and a feasibility/adoption decision by July 30,
2027. That decision must assign qualified scope to 2.0.0 or name a concrete 2.x
follow-up; it must not leave an unnamed indefinite backlog. These advanced
features are not promised in the October quality update. Automatic
IR-to-reverb parameter inference and polyphonic guitar tracking remain
separate research items without a committed feature-release date.

## Model additions retained

| Version | Character models |
|---|---|
| 1.4.0 — PRE A, six | Empress Heavy Menace; Tel-Ray Power Wah Boost; MXR MB301 Bass Synth; EBS MultiComp; Fortin 33; Empress ParaEQ MKII |
| 1.5.0 — PRE B, six | Revv G3; Wampler Dracarys; MXR Slash Octave Fuzz; Dunlop 535Q; BOSS GE-7; DigiTech Whammy |
| 1.6.0 — AMP, five | Crystal 120 / JC-120; 1987X Plexi; JCM800 2203; Hiwatt DR103; one bass head selected from Glockenklang Blue Rock or Vanderkley Aurora |
| 1.7.0 — POST, nine | Omnipressor, API 2500, Neve 33609; API 512c, Chandler TG2, Grace m101; API 550A, Mäag EQ4, Chandler Curve Bender |
| 1.8.0 — POST, nine | Rotary, Uni-Vibe, Ring Mod; UltraTap, Rose, DIG; Bricasti reference, Blackhole, Non-Linear |
| 1.9.0 — Creative, nine | Slicer, HarPeggiator, Time Warp; Laser, Frequency Shifter, Dynamic Freeze; Glitch, Bit Crusher, Granular Texture |
| 1.10.0 — SYNTH, three | Dvergr Analog, then Seidr Dynamic, then Mimir Impact |

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
| Graph nodes, value entry and FFT transport | Delivered in 1.3.1-beta.1; workflow/analyzer quality follow-up in October | Dynamic EQ, multiband, filter and reverb shaping |
| Stable processor/band IDs and order state | 1.3.1 state design; 1.4.0 order execution | Automation, routing, undo/redo and preset recall |
| Detector/envelope, range/GR, audition and fixed DI taps | 1.3.2 dynamics | Gate, multiband, ducking, envelope filters |
| Shared room send/return, gate-envelope reuse, tail and room state | 1.3.4 Room Core | 1.8.0 advanced room conditions and spatial modeling |
| Band splitting/filtering, sum and branch alignment | 1.4.0 multiband | Saturation and later band processing |
| Quality modes, latency reporting and dry/wet alignment | Live foundation first; Studio separately verified | Limiter, saturation, filters and Studio EQ |
| Bounded modulation routing, tempo, LFO/EF and MIDI | Foundation with common processors; products in 1.7.0 onward | Filter, delay, saturation and synth |

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
| Head UI | Preserve delivered six native contexts and full catalog coverage, per-channel/shared memory, hit/paint transforms and host gestures |
| AAX Native | Actual Windows/macOS AAX bundles, stable bus/parameter IDs, Developer-host evidence, then signed normal-Pro-Tools and installer/lifecycle evidence |
| Focused Náströnd / Niflheimr VST3 | Separate product identities, one active amp path, full native channel controls, authored focused presets, measured resource cost and three-product coexistence/host acceptance |
| Required Niflheimr NAM pack | Five source-bound amp-only captures, frozen fidelity criteria, full-training/held-out-DI evidence and actual supported-player load/restore verification |
| Host and release | Existing 48 presets, old projects, A/B and IR state, representative sample rates/buffers, callback timing and actual host lifecycle/installer results |

Use precise statuses: IMPLEMENTED, TECHNICALLY VERIFIED, LISTENING/HOST
ACCEPTED, CANDIDATE or DELIVERED. Report missing evidence at the affected
feature. Existing tests supplement actual host/playing acceptance and must
not be replaced with unsupported PASS labels.

The two development tasks retain daily 15:00 KST independent implementation
and weekdays 17:00 KST Astra validation. Apply the weekly delivery contract
above and continue through the December 31, 2027 correction reserve. Their
runtime configuration must be updated separately and verified; a document edit
alone does not schedule execution. Read current source and open work before
each run, avoid duplicate implementation, and keep independent work moving.
Do not assume a paused installer/NAM watch is running or re-enable it merely
because it appears in historical planning text.

## Related project records

- [Catalog counts and historical baseline](CATALOG_ROADMAP.md)
- [EQ analyzer and editing quality follow-up](EQ_ANALYZER_WORKFLOW_UPGRADE.md)
- [AAX Native development and delivery](AAX_NATIVE_PLAN.md)
- [Focused Náströnd / Niflheimr products and NAM delivery](FOCUSED_AMP_PRODUCTS_AND_NAM.md)
- [Embedded amplifier-head controls](AMP_HEAD_CONTROLS_NEXT_UPDATE.md)
- [Signal-path navigation and editing](SIGNAL_PATH_EDITOR.md)
- [CAB source/identity/acceptance boundaries](CABINET_PANEL_ROADMAP.md)
- [Room implementation and dated checkpoints](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/issues/46)
- [Transpose long-term work](TRANSPOSE_LONG_TERM.md)
- [1.3 release preparation](RELEASE_1_3_PREPARATION.md)

## October 13 addition — transparent-power bass research in the next update

Owner instruction: incorporate the ongoing expanded amp research into the next
update schedule. Register the research/qualification work in the **1.3.1-beta.3
cycle (original test October 14, candidate October 15, missed publication target October
16, 2026 KST; now deferred, replacement date TBD)**. This is not a promise that a newly researched DSP can qualify
in that cycle. Preserve the existing UI/EQ, reliability and beta-lifecycle scope;
research consumes the same bounded capacity.

| Checkpoint, KST | Required result |
|---|---|
| October 14 / next test-build review | Review the available Deep Research results; record unresolved results as pending. Build a reference manifest with exact device, capture point, EQ/Gain/Master, load, calibration provenance and licensing. Schedule the two roogbass Avalanche NAM files as qualification candidates, without carrying over an unidentified older capture's rejection. |
| October 15 / candidate review | Freeze the level-matched listening and unnormalized input-level sweep protocol; identify obtainable files, SHA-256 pins and remaining real-head measurement gaps. Record actual verification separately from literature claims. |
| October 16 / next-update checkpoint | Deliver the research decision/remaining-work record alongside the independently verified weekly plugin increment. Research documentation alone does not fulfill weekly implemented-delivery requirements. |
| October 23 / follow-up review | Review any obtained NAM/audio evidence, select the next bounded experiment and estimate original clean-core prototype work. If results or files remain unavailable, report that blocker and assign the next experiment; do not mark qualification complete. |

**Core comparison set:** Glockenklang Bass Art Classic/Blue Sky/Blue Bird;
Vanderkley Spartan/Aurora; PJB M-500/D-1000 (BP-800 separately); Trickfish Bullhead
1K. EBS 802, Genzler Magellan 800+, Bergantino forte HP Ultra and GR PureAmp 800
are control/complementary candidates. **High-end tube comparison is core scope:**
LeSuire Avalanche and Jad Freer Volta first, Reeves Custom 225/400 as contrasting
output-stage designs. Exact specifications remain evidence to verify, not
assumed measured behavior. Guitar expansion also retains Quilter Aviator Mach 3
qualification, with Laney Lionheart correctly identified in the comparison.

The target is clean low-note body/pressure, attack, repeated-note separation
and headroom, with instrument and playing differences retained. Output/Gain
increases, permanent saturation, bass boost or compression alone do not establish
success. Compare identical DI, cabinet and playback conditions at matched
listening level; retain original output for input-level/THD/compression/recovery
measurements (no normalization at every input step). Separate perceived pressure
from measured SPL of a declared physical playback system. Include low-B and
lower tuning, strong-to-weak notes, repeated attacks, two-note inputs and decay.

For TONE3000 **roogbass / 31892**, qualify both Avalanche NAM files independently.
Old-file identity is **unconfirmed**, not rejected and not proven different.
Verify capture settings, calibration, load and amp-only status; the preview's
Ampeg 8x10 IR is not part of the head qualification. A low ESR or a preamp capture
alone does not establish loaded power-stage fidelity. Freeze obtainable NAM
hashes, rights and source evidence before adoption; unavailable evidence stays
pending.

An **original transparent/high-headroom bass core** remains a development option,
separate from Niflheimr's distortion-channel design. Begin with neutral clean
response; evaluate bypassable low protection, four-band EQ and independent
internal headroom/output. Adopt optional Punch/dynamics only with demonstrated
benefit. Do not claim damping/load interaction from a disguised EQ or an ordinary
DI capture. DSP integration requires audio, state/automation, Classic/Dual/Matrix,
sample-rate, latency, CPU and actual host/listening acceptance.

**Expansion/count reconciliation:** retain the owner's current **42-amp target**
and bass candidate slots **Trace / SWR / B-15 / transparent-power** as the research
scope. The transparent-power slot yields one selected product model even when
many devices are researched. The older five-amp/AMP30/132 plan and its JSON mirrors
are now a historical baseline pending an explicit inventory/delta reconciliation;
do not silently assert that 42 amps fit those totals or change serialized IDs.
Reconcile the complete guitar/bass list, aliases and affected implementation
dates at the October 16 checkpoint, then synchronize calendar/catalog mirrors.
Original manufacturer names stay in internal research only; the next release's
customer UI, host labels, presets and shipped manuals use Chimera identities.

Research registration is complete when this plan is saved. Deep Research results,
file qualification, original DSP implementation and public delivery each remain
separate pending states until their own evidence exists.
