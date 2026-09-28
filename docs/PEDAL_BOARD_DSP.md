# Five-slot PRE board: implementation boundary

The universal PRE board is an explicit opt-in engine. With `boardEnabled=0`, the
existing `PreFXChain`, raw parameter units, order controls and latency are used.
Enabling the new board is an engine choice, not a tone-preserving migration of
old raw values into hardware knob positions.

## Production state and routing

There are exactly five prepared DSP owners and a five-entry display/process
order. Reordering changes the order, not the owner of an automated control.
Parameters have model-specific names such as `board0_m26_drive`. The model
parameter has a permanently reserved integer range 0–255; adding catalog entries
does not rescale existing host automation. Unknown model values fail to Empty.
Model selection, engine enable, order and LOW tap are marked non-automatable;
host-driven structural changes are not supported in this build. Bypass also has
its own owner/model bank, so another model cannot inherit an old bypass lane.

Every owner reserves the same small oversampling transport latency. Empty and
settled bypass owners run only that delay, not their effect processors. This
keeps bypass, model choices and the LOW branch latency aligned without changing
host delay for every pedal choice. The LOW tap copies the signal after the chosen
position, then applies the remaining owner delays. Distortion before the tap
really reaches LOW. The tap does not create another effect slot.

All DSP objects, buffers and delay capacities are prepared before processing.
Host block processing uses a bounded five-owner loop. The current implementation
does **not** certify click-free structural editing during playback. Stop playback
before replacing, reordering or moving the LOW tap. Continuous output, bypass and
selected processing controls are smoothed; this is not a guarantee for every
filter/switch change or for state restoration during playback.

Owner/model IDs prevent an old automation lane from controlling a different
model's function. Reusing the same owner and model can still reconnect an older
host automation lane; instance retirement across arbitrary replacement histories
is not implemented. This is a known production limitation, not a completed
equivalent of the preparation prototype's unbounded identity allocator.

## Sound and controls

All hardware-reference controls use **new experimental DSP curves**, with values
stored separately from legacy raw parameters. They are not measured hardware
tapers, capture interpolation, original-circuit validation, or A2 calibration.
The panel inventory is generated from `Preparation/pedal_controls.py`; reviewed
control names do not establish sound equivalence.

| Path | Actual processing | Limit |
|---|---|---|
| Existing five drive voices | Existing 4× nonlinear DSP with per-model controls; BDDI independent blend/EQ/shift; B3K pre-drive GRUNT shaping, wet level/blend and mid EQ | Synthetic curves and filters; no claimed v2 circuit/capture matching |
| M87 / Dyna / Diamond / Cali / Mu references | Existing detector/character DSP with input/output, model-specific timing/ratio and actual GR; Diamond tilt/hi-cut; Cali additive dry | M87/FET timing retains the shared detector architecture; no measured hardware transient match |
| Mu | Linked stereo with left-bank threshold/attack/recovery/output/mode | SEP and right-bank controls visibly disabled |
| Envelope references | Envelope detector plus LP/BP/HP/mixed filter routing, sweep/range, independent M82 dry/FX; M82 DECAY changes stop frequency | AW-3 only the UP-like path; MODE is disabled, HUMAN/TEMPO not implemented |
| Fuzz references | Existing oversampled fuzz voices; dedicated level/fuzz/sustain, gated PINCH and experimental Factory bias/compression shaping | Pickup impedance and Factory self-oscillation/stability behavior are not simulated |
| Boost references | Existing five voices, dedicated panel controls, RC separate gain/output and EQ, EP DIP controls | No measured hardware tapers or circuit calibration |
| SD-1 / OCD v2 / M104 references | Independent oversampled asymmetric/soft/hard clipping algorithms and dedicated controls; OCD peak switch changes the algorithm | Original experiments, not captured or circuit-verified models; M104 is not a renamed old RAT/DOD algorithm |
| JB-2 reference | Two distinct nonlinear/filter state paths; both solo modes, switch selection, both serial orders and parallel sum inside one owner | Original approximations; controls and topology work, actual BOSS/JHS circuit correspondence unverified |
| Manual Wah | Smoothed position, range and resonance on a swept band-pass filter | Original design; no 535Q hardware certification |
| PRE EQ | Ten independent peak bands plus input/output gain | 16 kHz is Nyquist-limited at low sample rates; filter response is original DSP |
| PRE modulation | Separate per-owner modulation instance, independent of existing POST modulation | The same original modulation voices as POST, not a newly verified Phase 90 |
| Mono/poly octaver | Catalog and parameter reservation only | Disabled; no reuse of the Transpose engine and no octave DSP claim |

Default normalized positions are UI seeds, not hardware factory settings. The new
board does not convert or overwrite the previous raw PRE bank. Controls known to
be unsupported are disabled in the production panel, not simulated by changing
their labels.

`StudioModules.h` adds a default-on legacy FET parallel option. Existing calls
retain the 12% dry equation. The new board explicitly requests wet-only FET
compression before adding the Cali DRY control, avoiding a hidden fixed dry path.

## Verification

`Tests/PedalBoardTests.cpp` is a JUCE DSP test executable. It exercises all 37
available catalog paths at 44.1/48/96 kHz, exact Empty/LOW impulse alignment for
all six tap boundaries, disabled octavers, repeated SD-1 instances, audible order
changes without moving parameter owners, LOW membership, representative dedicated
controls, all six JB-2 topologies, invalid-order normalization and the legacy/new
FET parallel equations.

The separate Linux `Tests/PedalBoardAllocationTests.cpp` harness intercepts
malloc/calloc/realloc/free and C++ new/delete during 900 board callbacks. The
matrix covers 44.1/48/96 kHz, mono/stereo, 64/256/1024 samples, five simultaneously
assigned owners, and model/order/tap/bypass transitions. The measured result is
zero intercepted allocation calls and zero frees. This does not measure all
possible third-party allocator mechanisms or prove click-free structural edits.

These synthetic checks do not establish Windows host automation correctness,
real DAW shutdown behavior, musical DI quality, hardware matching, or exhaustive
response of every control interaction. Production UI/host state tests, complete
plugin callback allocation checks and actual Windows/DAW checks are separate
evidence.
