# Dynamics spectrum workspaces — October 20, 2026

Status: **development scope; implementation and acceptance must be reported
against the resulting source and build**. This document is not a publication
receipt or a claim of FabFilter-equivalent behavior.

## Owner decision and source baseline

On October 20, 2026, Asia/Seoul, the owner reaffirmed the requested Pro-MB-type
functionality and required every dynamics function to be adjusted while its
spectrum remains visible. The owner explicitly directed separate function
screens instead of combining all dynamics controls on one screen. The supplied
FabFilter catalog also retains the later creative, filter, reverb and synth
reference work; it does not bring every catalog product into this increment.

The inspected public product is **1.3.2-beta.1**, built from
`3bec6c70a50bbb4021dcc6133f37e0c2fb3e9e58`. The development baseline is main
`ec6fd639e2958d34605bfb04bb81ca02c1cfb38f`. The former contains shared Dynamic EQ
slots and common compression, gate/expansion and sample-peak limiting; these
are not evidence that independent multiband dynamics has been delivered.

The new instruction supersedes the combined DYNAMICS control-panel design and
the treatment of independent multiband work as future-only scope. It preserves
the existing Tone/Final EQ identities, audio-routing roles and saved states.
The revised interface and new processors belong to a feature development
change, not the separate factory-preset maintenance publication in PR #108.

Related records:

- [Next-release P0 requirements, issue #97](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/issues/97).
- [October 19 FabFilter evidence checkpoint](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/issues/97#issuecomment-6263258522).
- [Schedule and release-gate reconciliation, PR #93](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/pull/93), inspected at `7f3a838f5bf1a50bc5949fc3e31141a7da231d40`.
- [Previous roadmap at that source](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/blob/7f3a838f5bf1a50bc5949fc3e31141a7da231d40/docs/DEVELOPMENT_ROADMAP_2026_2027.md), which still assigned independent four-band dynamics to 1.4.0.
- [Existing common-dynamics baseline](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/blob/ec6fd639e2958d34605bfb04bb81ca02c1cfb38f/docs/COMMON_DYNAMICS_1_3_2.md).

## Six focused dynamics editors

Use a function selector or equivalent direct navigation to open one dedicated
editor at a time. Each editor reserves a substantial, persistent area for its
live spectrum and the controls for that function. A selector is not permission
to stack the six complete control sets into the same view or hide the spectrum
while a parameter is being edited.

| Editor | Processing role | Required visible information |
| --- | --- | --- |
| Dynamic EQ | Existing dynamic nodes in the selected Tone or Final EQ bank | Selected bank/node, actual IN/OUT spectra, static EQ response, configured dynamic range, applied change and GR |
| Compressor | Selected common, PRE, POST or Matrix LOW compression source | Actual selected-module IN/OUT spectra, its existing detector/threshold/ratio/timing/mix controls and relevant GR |
| Multiband | Independent original four-band dynamics processor | Actual module IN/OUT spectra, band regions/edges, selected-band controls, band solo and per-band GR |
| Gate / Expander | Existing configurable PRE or post-rig attenuation stage | Actual gain-stage IN/OUT spectra, clearly identified clean-input detector, gate/expander mode, threshold/range, attack/hold/release and applied attenuation |
| Limiter | Final sample-peak control after output trim and tuner mute | Actual limiter IN/OUT spectra, ceiling/release, measured sample peaks and limiter GR |
| De-esser | Focused selective harshness attenuation for guitar/bass | Actual module IN/OUT spectra, selected detection/processing region, sensitivity/range/timing controls and applied reduction |

The Tone and Final static EQ editors remain separately selectable. Dynamic EQ
keeps the existing two banks of 12 nodes and four shared dynamic assignments;
it does not create four extra EQ nodes per bank. Selecting a function or band
must not enable processing, alter a preset, create a host gesture or rewrite
another function's settings.

The spectrum in a dynamics editor is a view of that function's audio. An
adjacent EQ's capture or the final mix cannot be relabeled as the compressor,
gate, multiband, limiter or de-esser output. Where a detector uses a different
tap, identify that signal explicitly. In particular, the clean gate detector
and a post-rig gate's processed signal can be different.

The spectrum level axis represents dBFS. EQ gain, configured reduction range
and current gain reduction are separate quantities. A dynamics overlay must
not imply that a time-varying nonlinear processor has one fixed EQ transfer
function. Any displayed response estimate must describe the active processing
and be identified as an estimate where appropriate.

Native PRE compressors retain their individual pedal settings and stable board
owners. Opening one pedal's detailed editor must select that exact owner's
capture; two compressor pedals must never be concatenated into a single trace.
The spectrum stays fixed above the scrollable attached-control area, including
the larger Variable Mu control set. Compressor overview cards open this editor
instead of providing a second set of spectrum-free knobs.

The native POST compressor likewise opens its full spectrum editor while
preserving its authored controls and parameter attachments. Common compression,
legacy PRE/POST compression and Matrix LOW compression remain distinct sources.
Opening a native editor must not switch away from an inactive/legacy audio path
until the user deliberately edits or enables that processor.

The requested PRE owner and native/legacy POST identity belong to the visible
editor. The active spectrum consumer publishes that identity at the capture
boundary; a hidden editor must not retarget another editor's stream. Native PRE
capture is valid only while its board is active and that owner is the selected
compressor. Native POST capture is valid only while native section 0 is active.
An inactive native editor shows its inactive status and stored controls without
borrowing the legacy processor's trace or reduction meter. Changing the active
board/native path, selected model or owner invalidates both live and frozen
frames before the next source is displayed.

The Matrix LOW head overview and its expanded controls use a **COMP SPECTRUM**
entry in the existing tone / compressor / blend row. It opens a separate LOW
compressor spectrum with the released `lowcomp` and `lowampmix` attachments.
The displayed tap brackets compression; the DI/AMP blend follows that tap.
Leaving Matrix routing invalidates the LOW trace. The global gate controls
likewise open the focused Gate / Expander workspace rather than exposing
threshold/timing/range adjustments outside their spectrum.

## Independent multiband and de-esser scope

The first multiband processor has four independently adjustable processing
bands and works in Classic, Dual and Matrix without assigning an amplifier to
each band. Matrix LOW/MID/HIGH amp routing and the four shared Dynamic EQ slots
do not fulfill this requirement. Keep the new processor's band identity,
controls, bypass and state distinct from both existing mechanisms.

The implementation must define its band topology, reconstruction behavior,
detector ranges, supported compression/expansion modes, gain controls and solo
semantics. Expose only modes that actually process audio. Four-band behavior,
band isolation, intended sum/phase, bypass transitions, dry/wet alignment and
callback cost require evidence from the implemented engine. The design is an
original Chimera implementation; it does not assert that Pro-MB always uses a
conventional crossover or that a particular Chimera topology matches it.

The de-esser receives its own editor and processing identity. Its instrument
use is selective control of pick/fret attack and harsh upper-frequency energy.
Its selected frequency region, detector behavior and applied reduction must
be visible. Existing Dynamic EQ presets alone do not constitute the new
dedicated editor and processor. Document its actual audio placement and
operation when the code is finalized.

The current development integration places these common stages in this order:
Tone EQ → Clean Compressor → Multiband → De-esser → POST → Final EQ → output
trim/tuner mute → Limiter. This is the inspected development routing, with
measurement and release acceptance pending. It does not relocate the existing
PRE/post-rig Gate or Matrix LOW compressor.

The inspected original multiband core uses four contiguous bands with three
smoothed Linkwitz–Riley splits and downstream allpass compensation. At settled
crossovers and unity band gains its design sums to a flat magnitude response
with IIR phase rotation. The parallel dry branch follows the same band filters
as the processed branch. Global bypass reaches the original sample after its
short transition; that transition can exhibit phase coloration. These design
properties need the corresponding numerical and listening results.

The current modes are downward/upward compression and downward/upward
expansion, with linked Peak/RMS detection, per-band range/ratio/threshold,
attack/release/knee, makeup, bypass and solo. The de-esser uses a separate
split-band design that attenuates the high branch and provides an unattenuated
high-band listen mode. Its frequency, threshold, range, ratio, knee and timing
controls belong to its own processor.

Both original cores declare no added delay line or lookahead. Their IIR group
delay and frequency-dependent phase remain, and the whole plugin retains its
existing reported latency. Source inspection alone does not verify real-time
cost, original FabFilter behavior or host/playing acceptance.

Independent six-band expansion, external sidechain and M/S modes remain later
work unless separately implemented and qualified. The immediate four-band
implementation does not imply those modes or FabFilter feature parity.

## Capture, UI and state contracts

Audio capture must be bounded and must not allocate, lock, perform FFT/UI work
or wait for the editor in the real-time callback. Run spectral analysis and
painting outside that callback. Keep stereo energy visible even for opposing
channel polarity. Opening a view must use fresh valid observations; changing
function, sample rate or FFT settings must not display another function's
stale spectrum as current audio.

Suspend unneeded hidden-editor capture and analysis. A paused/frozen spectrum
must be visibly marked, and a stopped or bypassed processor must have an
explicitly defined display behavior. Do not synthesize a moving spectrum or
inject a precomputed response as proof of live capture.

Preserve released parameter IDs, ordinals, automation values and EQ assignment
semantics. New processor parameters append after the released list; old
projects and A/B states default the new processing to bypassed/neutral values.
Opening or navigating editors is transient UI state unless an explicit,
compatible saved preference is introduced. Factory and user presets, personal
IRs and existing routing remain subject to their current restoration contracts.

The final limiter retains its last gain-controlling role. New common dynamics
must not be inserted after it or silently change the Matrix LOW DI branch.
The resulting routing graph must identify the implemented positions rather
than presenting the order of editor buttons as a complete audio graph.

## FabFilter reference scope retained

The table records product-planning references, not copied algorithms, assets,
brand claims or a promise that all listed products ship in this increment.

| Reference | Chimera scope | Work boundary |
| --- | --- | --- |
| Pro-Q 4 | Tone/Final graphical EQ and Dynamic EQ workflow | Preserve existing EQ and provide its focused spectrum editor |
| Pro-C 3 | Common compression and detector/GR workflow | Dedicated compressor spectrum editor |
| Pro-MB | Independent band dynamics, band solo and per-band feedback | Original four-band processor and dedicated spectrum editor in this development scope |
| Pro-DS | Focused selective attenuation | Dedicated de-esser processor/editor in this development scope |
| Pro-G | Gate/expander timing and attenuation feedback | Dedicated gate/expander spectrum editor |
| Pro-L 2 | Output limiting and reduction feedback | Dedicated existing sample-peak limiter editor; true-peak/LUFS claims require separate work |
| Pro-R 2 | Reverb, decay shaping and wet-tone workflow | Later POST reverb work; not a dynamics editor |
| Saturn 2 | Band-specific saturation and modulation | Later creative processing |
| Timeless 3 | Delay, feedback filtering and modulation | Later POST/creative delay work |
| Volcano 3 | Filter and modulation workflow | Later creative/filter work |
| Twin 3 | Synth oscillator/filter/envelope architecture | Later synth work |
| One | Compact synth workflow | Later shared synth foundation |
| Simplon | Accessible two-filter workflow | Later shared filter foundation |
| Micro | Compact envelope-following filter workflow | Later shared filter foundation |

Pro-R 2 stays in the requested catalog even though the current editor split is
for dynamics. Creative/synth references retain their own later implementation
and acceptance; they are not silently removed or represented by inactive
buttons in the dynamics workspace.

## Evidence and completion status

At document preparation, the scope above is a development change. The source,
build, tests and visual evidence must be recorded after their actual results
exist. Do not inherit a PASS from public beta.1 or a different PR's source.

| Evidence | Required result | Status at preparation |
| --- | --- | --- |
| Function-specific capture | Six editors show the actual selected module's input/output while its controls are edited | Pending |
| Native UI | Legible dedicated views, working navigation/attachments and no unintended parameter changes | Pending |
| New DSP | Four-band isolation/sum/solo and de-esser selectivity, measured reduction, bypass and transitions | Pending |
| Compatibility | Existing parameters, projects/A-B, presets and IR restoration; neutral defaults for new stages | Pending |
| Real-time behavior | Allocation/lock-free capture, bounded queues, actual callback cost and reported latency on the implemented routes | Pending |
| FabFilter comparison | Identified product/version and repeatable original response, detector/time, phase/latency and interaction measurements | Not established |
| Playing and hosts | Real guitar/bass DI, level-matched listening and installed DAW automation/recall/lifecycle | Not established |
| Delivery | Exact source/build identity, selected package and applicable release evidence | Not established |

Synthetic tests and screenshots can establish specific functional behavior.
They do not establish FabFilter equivalence, original-response matching,
audible quality or real-host acceptance. The next status update must identify
what has actually been implemented, tested and delivered separately.

The development regression entry `ChimeraUITests <snapshot-directory>
--native-dynamics-only` exercises five native PRE compressor models with
reordered stable owners, three native POST models, Matrix LOW compression,
actual module capture, inactive/native-versus-legacy isolation, frozen-source
invalidation, attached control edits and navigation without sound-parameter
writes at 75/100/125/150% editor scales. `--pedal-menus-only` retains the PRE
catalog, layout, bank and deliberate-activation checks. `--native-panels-only`
also covers Gate spectrum entry and the broader original native panels. These
are added regression definitions; record their execution results and exact
source/build identity before marking the evidence rows above as passed.

## PR #110 failure repair (2026-10-20 KST)

The failing baseline is `9d5b7fc20e4f268a9e6e80293b81013374d0ed86`.
Runs `39325032429`, `39325032475`, `39325032442`, and `39325032551`
were inspected; failures are not collectively labelled as one defect.

- `ChimeraGateProcessorTests` indirectly includes `NativeMessageLoop.h` via
  `EQPendingInputTests.h`. Without UI probes, Windows/Linux omitted
  `JUCE_MODAL_LOOPS_PERMITTED=1`; macOS omitted `NativeMessageLoop.mm` in both
  configurations. Supply these dependencies on the test target, keeping the
  native adapter outside shipping targets. The original Linux header compile
  error was reproduced against JUCE 8.0.8 and disappears with the target define.
- Dynamics capture depended on child visibility/hierarchy callbacks. JUCE does
  not notify the child when only its workspace ancestor becomes visible. A
  locally visible spectrum could therefore leave `uiConsumer` null indefinitely.
  The message-thread timer now acquires an idle stream when the view is actually
  showing, through the existing callback acknowledgement and FIFO drain. It
  does not take a stream already owned by a native detail dialog.
- The UI regression now opens/hides/reopens only an ancestor and checks real
  processor audio, hidden FFT suspension, and stream release. The six-page
  loop reports page, scale, source and drain state on failure.
- Measured stage regressions exercise compressor attenuation, both gate
  placements with a distinguishing lane gain, limiter input after output trim,
  multiband attenuation, de-esser high/low selectivity, dynamic Tone/Final EQ
  cuts with a distinguishing POST gain, different PRE/POST makeup gains and
  the filtered Matrix LOW source. Existing native PRE owner and native/legacy
  POST exclusion, freeze, automation, A/B and legacy-state tests remain enabled.

These are code changes and test assertions, not a declaration of CI success.
Only completed jobs for the exact repair commit may be recorded as passing.
The independent core passed locally; native platform integration and screenshots
must be assessed from the repair's own run artifacts. Draft status and public
release approval are unchanged; no publishing workflow is requested.

The first repair's exact-source native runs passed both dynamics UI suites on
Windows and Linux, but macOS exposed a separate test sequencing defect: an
offline audio burst could finish before native timers armed or consumed the
stream. Native event dispatch is not a timer-completion guarantee. The test
driver now waits for the real callback acknowledgement/capture enable and FIFO
consumption with a bounded 2-second native pump, and services native messages
between finite audio batches. No direct timer invocation, fake analyzer frames,
test retry, attenuation relaxation or performance deadline change is used in
CI. Failure diagnostics retain stage, pending samples and frame state. A focused
three-platform workflow runs both dynamics UI suites and retains their source
SHA, result XML, complete test log and screenshots while the existing broader
regressions remain enabled.

## Quantitative Pro-MB scope audit (2026-10-20)

See [the independent multiband audit](MULTIBAND_PRO_MB_AUDIT_20261020.md)
for measured reconstruction, complex phase, internal/external dry mixing,
ON/OFF and band bypass, detector/timing results, and the official Pro-MB
feature comparison. The production core at `e91709d5` reconstructs correctly
in the tested settled cases; this is not zero-phase transparency. In particular,
48 kHz / default crossovers / 700 Hz neutral input shows a -28.645 dB minimum
quadrature envelope during global OFF and -59.385 dB during cold ON. The
10 ms raw-bypass transition remains a known quality limitation, not a PASS
for transparent switching. Added regressions characterize that limitation
without duplicating the existing compensated splitter or claiming a fix.

The UI now distinguishes global bypass from phase-preserving band bypass and
Parallel mix=0, and clarifies timing constants. Host parameters, defaults,
serializer, presets and production DSP remain unchanged. Pro-MB executable
response matching and real-DI/DAW acceptance are still not established.
