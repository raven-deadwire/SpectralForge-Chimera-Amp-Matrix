# Common dynamics — released baseline and focused-editor development

Updated: **October 20, 2026, Asia/Seoul**.

The existing common dynamics are part of the public **1.3.2-beta.1** product
source `3bec6c70a50bbb4021dcc6133f37e0c2fb3e9e58`, integrated in main
`ec6fd639e2958d34605bfb04bb81ca02c1cfb38f`. Their presence is not a claim of
FabFilter-equivalent response or completed real-DAW acceptance.

The owner now requires separate spectrum-visible editors for Dynamic EQ,
Compressor, Multiband, Gate/Expander, Limiter and De-esser, together with the
independent four-band dynamics and dedicated de-esser development. This
supersedes the previous combined DYNAMICS control panel. The complete new
scope and pending evidence are in
[Dynamics spectrum workspaces](DYNAMICS_SPECTRUM_WORKSPACES_20261020.md).

This document distinguishes the released processing contracts below from that
new development scope. It does not state that the new feature branch has
passed testing or been published.

## Editing and processing placement

The released UI reaches the combined DYNAMICS workspace from TONE EQ or FINAL
EQ. The revised workflow opens one function's dedicated editor at a time,
with that function's actual input/output spectrum visible beside its controls.
Tone and Final EQ remain separately selectable in Classic, Dual and Matrix.
The detailed ROUTING view must continue to show actual processing positions.

| Existing stage | Placement | Released operation |
| --- | --- | --- |
| Dynamic EQ | Existing Tone or Final EQ node | Stereo-linked downward dynamic Bell or shelf; four shared slots total |
| Clean compressor | After Tone EQ, before POST | Peak/RMS detector, detector high-pass, ratio, soft knee, attack/release, makeup, parallel mix |
| Gate/expander | Existing PRE or POST GATE gain stage | Fixed clean input detector before PRE; existing hold, hysteresis and attenuation range; added attack and downward-expander ratio |
| Live limiter | After Final EQ, final output trim and tuner mute | Stereo-linked sample peak ceiling, immediate gain reduction, controlled release |

Independent four-band dynamics and the dedicated de-esser are new development
stages. They are not delivered by the existing Matrix LOW/MID/HIGH amp split
or by renaming one of the four shared Dynamic EQ slots. Their final routing,
parameters, capture taps and validation results must follow the implemented
source. The limiter remains the final gain-controlling stage.

The inspected development order is Tone EQ → Clean Compressor → Multiband →
De-esser → POST → Final EQ → output trim/tuner mute → Limiter. Native PRE
compressors keep individual per-owner spectrum detail editors, and the native
POST compressor opens its own expanded spectrum editor. Source selection must
identify common, legacy/native PRE/POST and Matrix LOW compression separately;
opening an editor does not activate another processing path.

Inactive native PRE/POST editors must not display the legacy compressor's
audio. They show stored controls and an inactive source caption; activating or
deactivating the board/native path, changing the selected model, or switching
the PRE owner retires the prior live/frozen frame. Only the visible spectrum
consumer publishes its requested source identity. Matrix LOW compression has
a separate spectrum dialog reached from the LOW head's **COMP SPECTRUM** entry;
its original `lowcomp`/`lowampmix` IDs remain intact. The global gate entry opens
the focused Gate / Expander page, where its original Range `Full` value remains
96 dB and its host/text-entry mapping is preserved.

## Released parameter and state contracts

The original common-dynamics parameter defaults are disabled/unassigned.
Existing gate mode and 0.5 ms attack are preserved. Legacy states acquire
these defaults, including A/B states. The existing common-dynamics parameters
append after all earlier released parameters and the tuner controls: indices
4986–5025 use version hint 12. No released ID, ordinal or version hint is
replaced. Dynamic target assignment is structural and not host-automatable;
its five continuous controls and the other common dynamics controls are.

Keep these defaults distinct from an authored factory preset, which can enable
processing when explicitly recalled. New four-band/de-esser parameters must
append after the released list and restore bypassed/neutral values in older
projects. Editor selection itself must not change audio settings, factory/user
preset identities, A/B data, personal IR state or host automation.

## Four shared dynamic nodes

Each of the four slots names an existing Tone/Final band. There are still 12
nodes per EQ instance, not four additional bands. A slot requires an enabled
Bell/low-shelf/high-shelf node and an active EQ bank. The workspace explicitly
shows Unassigned, Band off, EQ bypassed or Bell/shelf only when appropriate.
Assigning a node does not silently turn on an EQ or change its static settings.

Nodes already assigned elsewhere cannot be selected through the normal menu.
If a malformed imported state contains duplicate targets, the earliest slot
continues processing; later duplicates visibly report idle. Another active node
is never evicted to satisfy a fifth request. Reassigning a slot is an explicit
edit. Deleting/disabling a referenced node suspends its dynamic processing;
the slot remains reserved until explicitly unassigned or reassigned.

The released detector is a band-pass at the node frequency and Q, fed from that
EQ instance's input. Stereo-linked peak detection uses a 20 ms envelope decay,
followed by user attack/release on requested gain reduction. Static gain minus
dynamic cut remains within the EQ's existing -24 to +24 dB bounds. Accordingly,
the displayed cut can be smaller than Max cut when static gain is already near
-24 dB. Node badges, the response display and the focused Dynamic EQ workspace
must show the applied cut. The response is a live estimate, not a frozen
transfer function.

The focused editor must distinguish static EQ response, configured reduction
range and the current applied change while showing actual IN/OUT spectra.
These four shared assignments remain separate from the four bands of the new
independent multiband processor.

## Live behavior and spectrum semantics

The released stages add no lookahead, extra oversampling or audio delay. The
EQ uses minimum-phase IIR filters with its existing 32-sample coefficient
update clock. This describes zero additional latency for those modules; the
full plugin still reports latency from its amp/CAB/transpose/POST processing.
Any new processor's latency and CPU cost require their own measurements.

Compressor GR shows the compressed branch before parallel mixing and makeup.
The limiter sees output trim and all wet paths; output peak metering follows
it. The beta lifecycle's final activation gain is bounded to 0–1 and cannot
raise the limited output. Disabling the limiter returns gain to unity over a
short release. The limiter measures sample peaks, not intersample/true peaks
or LUFS.

Each focused editor must capture its own processing boundary. An EQ spectrum
cannot be presented as another module's input/output. For Gate/Expander, label
the clean-input detector separately from a post-rig attenuation-stage signal.
Spectral dBFS, EQ gain and gain reduction need distinct labels. Capture remains
bounded and nonblocking; FFT and painting stay outside the audio callback, and
unneeded hidden-view analysis is suspended.

The tuner display's AUDIO and PEAK percentages measure audio callback wall time
divided by the current block duration. 100% means the deadline was reached;
570% means 5.7 times the available time. This is not whole-computer CPU usage.
Wall time includes scheduling delays. Peak decays with processed audio time and
can remain visible while processing is stopped. Values are not clamped to hide
overruns. The paired timing probe retains warmup, late starts, every callback and
the matching thread CPU clock/cycles; completing the probe is not a performance
PASS. Shared-machine observations cannot establish driver/DAW suitability.

## Verification boundaries

The existing `ChimeraCommonDynamicsTests` covers bypass, analytic compressor
curves, measured gain reduction, parallel alignment, stereo linking, envelope
recovery, limiter ceiling/placement, oversized host buffers, four-node
allocation, state migration, automation contracts and allocation-free module
processing. Existing Gate, Graphical EQ, lifecycle and native state contracts
remain applicable.

The earlier `ChimeraCommonDynamicsUITests` covers the released combined editor,
navigation/attachments, 75–150% layouts and duplicate-slot rejection. That
coverage does not certify six new focused editors. The revised UI needs actual
module-capture and spectrum-visible editing evidence, and the new multiband
and de-esser require independent DSP/state tests. Record source identity and
observed results in the development evidence; do not inherit prior-source PASS.

The added `ChimeraNativeDynamicsUITests` / `--native-dynamics-only` regression
targets native PRE, native POST and Matrix LOW at 75/100/125/150% editor scales.
It checks real module capture, inactive-source isolation, frozen-frame
invalidation, scrollable attached controls and navigation without parameter
writes or host gestures. The existing pedal-menu and gate/native suites have
been adapted to enter the actual spectrum editors. These test definitions are
development changes; their execution and platform evidence remain to be
recorded against the exact candidate source.

The FabFilter original-response, detector/time, phase/latency and host
comparisons remain unverified until their evidence is supplied. Physical
audio-interface and DAW acceptance is separate from synthetic tests and native
screenshots. Pro-R 2 reverb belongs to later POST work; Saturn 2, Timeless 3,
Volcano 3, Twin 3, One, Simplon and Micro retain their later creative/filter/synth
scope rather than being placed in the dynamics control set.
