# Common dynamics — 1.3.2 candidate

This candidate adds the common dynamics in the 1.3.2 roadmap. It is still under
qualification; this document is not a publication receipt or DAW acceptance.

## Controls and placement

Choose TONE EQ or FINAL EQ in the signal strip, then DYNAMICS. The same global
workspace is available in Classic, Dual and Matrix. The detailed ROUTING view
shows both new stages in their actual processing positions.

| Stage | Placement | Operation |
| --- | --- | --- |
| Dynamic EQ | Existing Tone or Final EQ node | Stereo-linked downward dynamic Bell or shelf; four shared slots total |
| Clean compressor | After Tone EQ, before POST | Peak/RMS detector, detector high-pass, ratio, soft knee, attack/release, makeup, parallel mix |
| Gate/expander | Existing PRE or POST GATE gain stage | Fixed clean input detector before PRE; existing hold, hysteresis and attenuation range; added attack and downward-expander ratio |
| Live limiter | After Final EQ, final output trim and tuner mute | Stereo-linked sample peak ceiling, immediate gain reduction, controlled release |

Parameter defaults remain disabled/unassigned, with the original gate mode and
0.5 ms attack. Old states acquire those defaults, including A/B states.
Selecting a factory preset explicitly applies its authored dynamics/EQ recipe:
two Dynamic EQ slots, role-specific gate/expander timing, selective common
compression and the −1 dBFS sample-peak limiter. Saved user sounds keep their
stored values. See [factory finishing](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/blob/main/docs/PRESET_DYNAMICS_EQ.md).
The parameters append after all released parameters and the new tuner controls.
Indices 4986–5025 use version hint 12. No released ID, ordinal or version hint is
replaced. Dynamic target assignment is structural and not host-automatable;
its five continuous controls and the other common dynamics controls are.

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

The detector is a band-pass at the node frequency and Q, fed from that EQ
instance's input. Stereo-linked peak detection uses a 20 ms envelope decay,
followed by user attack/release on requested gain reduction. Static gain minus
dynamic cut remains within the EQ's existing -24 to +24 dB bounds. Accordingly,
the displayed cut can be smaller than Max cut when static gain is already near
-24 dB. Node badges, the response display and the dynamics workspace show the
applied cut. The response is a live estimate, not a frozen transfer function.

## Live behavior and measurement limits

These stages add no lookahead, extra oversampling or audio delay. The EQ uses
minimum-phase IIR filters with its existing 32-sample coefficient update clock.
This is zero *additional module* latency; the full plugin still reports latency
from its existing amp/CAB/transpose/POST processing.

Compressor GR shows the compressed branch before parallel mixing and makeup.
The limiter sees output trim and all wet paths; output peak metering follows it.
The beta lifecycle's final activation gain is bounded to 0–1 and cannot raise
the limited output. Disabling the limiter returns gain to unity over a short
release. The limiter measures sample peaks, not intersample/true peaks or LUFS.

The tuner display's AUDIO and PEAK percentages measure audio callback wall time
divided by the current block duration. 100% means the deadline was reached;
570% means 5.7 times the available time. This is not whole-computer CPU usage.
Wall time includes scheduling delays. Peak decays with processed audio time and
can remain visible while processing is stopped. Values are not clamped to hide
overruns. The paired timing probe retains warmup, late starts, every callback and
the matching thread CPU clock/cycles; completing the probe is not a performance
PASS. Shared-machine observations cannot establish driver/DAW suitability.

## Verification

`ChimeraCommonDynamicsTests` covers bypass, analytic compressor curves, measured
gain reduction, parallel alignment, stereo linking, envelope recovery, limiter
ceiling/placement, oversized host buffers, four-node allocation, state migration,
automation contracts and allocation-free module processing. Existing Gate,
Graphical EQ, lifecycle and native state contracts remain required.

`ChimeraCommonDynamicsUITests` opens the real editor, exercises navigation and
attachments, checks 75–150% layouts and duplicate-slot rejection. Platform CI
runs these with the tuner and existing signal/EQ tests. Physical audio-interface
and DAW acceptance is a separate release gate, not implied by synthetic tests.
