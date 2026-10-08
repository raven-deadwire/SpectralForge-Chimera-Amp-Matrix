# Original CAB / linear acoustic prototype v1

Status: working development implementation stacked on PR #29, not a release or
hardware-match approval. Engineering names **Guitar 4x12 / v1**, **Bass 4x10 / v1**
and **Attack dynamic / Body ribbon / Detail condenser** are not final branding.

## Provenance and scope

All numerical definitions in `Source/OriginalCabModel.h` are independently authored
engineering assumptions. **None is a measurement of a commercial product.** No
commercial IR, user IR, Origin pack, Trondheim/Ampeg capture, third-party waveform,
neural weights or fitted coefficients are inputs to this generator. Existing User IRs
retain the separate original-byte storage and decoder of PR #29. No new factory WAV
is embedded. Generated kernels come only from the documented analytic response.

Research reference (concepts only, no copied numeric definitions or code):
[COMSOL lumped loudspeaker driver](https://doc.comsol.com/6.3/doc/com.comsol.help.models.aco.lumped_loudspeaker_driver/lumped_loudspeaker_driver.html).
It supports separating low-frequency driver dynamics and enclosure loading, while
identifying limits of rigid-piston assumptions. This implementation is **not** that
FEM model, a full electromechanical solver, or measured speaker reconstruction.

## Separated model factors

| Factor | Authored implementation and domain |
|---|---|
| Driver | LF second-order damped resonance; two inductive roll-off poles; independently authored mid/high resonant mode. Guitar effective radius .132 m, Fs 76 Hz, Q .69; bass .106 m, Fs 46 Hz, Q .60. Nominal 12/10 inch sizes are engineering roles. |
| Enclosure | Four drivers in a 2x2 array; guitar .74 x .76 x .36 m / .155 m3, bass .62 x .64 x .40 m / .125 m3. Closed-box compliance scales Fs/Q with sqrt(1+4*Vas/Vb); separate damped box mode. Geometry is invented, not measured. |
| Rear | Closed or open. Open removes closed compliance and adds opposite-polarity, delayed, low-passed rear radiation traveling around the nearest baffle edge. This is a reduced-order diffraction image path, not an EQ label or a room reflection. |
| Radiation | 48 equal-area surface points per cone; complex geometric propagation and spreading summed across all four units. A frequency-dependent coherent radius is an explicitly phenomenological breakup approximation. No claim of accurate high-frequency piston or near-field FEM behavior. |
| Tweeter | Optional centre-mounted original source, 3.3 kHz second-order crossover, 17 kHz roll-off, separate distance, phase and simple directional factor. Independent blend 0-1, off by default, available for both engineering enclosures. |
| Microphone | Three original transducer roles with separate LF/HF poles, damped response mode and bounded pressure-gradient proximity term. No measured commercial mic emulation; angle and rear pickup are outside v1. |
| Position | 0-1 cone radii, centre to right edge of selected unit, in the front reference plane. Changes actual distances/interference of radiating surface points and neighbouring units. |
| Distance | 2-60 cm forward from that plane; physical propagation at assumed 343 m/s and natural level loss. No auto gain or time alignment. Microphone always faces the baffle. |

A/B independently choose the same or different unit (upper left/right, lower
left/right), mic role, Position and Distance. The cabinet, rear and tweeter are
shared within a rig. All four drivers use the same original driver definition;
there is no invented manufacturing mismatch. Symmetric arrangements can legitimately
sound identical. Each slot can alternatively use its existing fixed capture/User IR.

This is a linear response at a fixed setting, rendered into a convolution kernel;
it does not model level-dependent speaker compression, thermal drift, excursion
nonlinearity, room acoustics, measured microphone grids or a commercial cabinet.
The practical acoustic approximations and authored voicing must still be auditioned.

## Generation, realtime and state

- Response is evaluated off the audio callback, inverse-transformed with at least
  170 ms FFT period, truncated to 85 ms with a 20% tail taper. A smooth Nyquist taper
  limits the sampled bandwidth. Band-limited propagation can have small pre-ringing;
  this is not a claim of a strictly causal continuous-time wave solver.
- Kernels use zero-latency uniform convolution and retain the entire generated
  response. Impulse-output residual against the generated kernel must be < 3e-6
  with zero reported processing latency. A two-stage 256-sample-head experiment
  was rejected after Linux p99 showed expensive tail-computation bursts.
- Model kernels disable JUCE normalization and trimming, preserving natural relative
  distance gain and arrival phase. Existing fixed IR normalization remains unchanged.
  Acoustic arrival is part of the response, not additional reported host latency;
  existing user fine delay and plug-in processing latency remain separate.
- One packed lock-free request per mic carries complete bounded settings. Host
  resolution: Position .001 radius; Distance .1 cm; Tweeter .01. Audio only reads
  parameters and exchanges prepared pointers. It never generates a kernel or plans FFTs.
- The existing IR worker builds convolution engines, caches at most eight response
  buffers (cleared when sample rate/spec changes), discards superseded requests,
  and collects retired engines. Audio also rejects obsolete pending model keys;
  a rejection counter prevents an A->B->A request from losing its rebuild.
- Existing 50 ms linear crossfade and 20 ms controls are reused. New settings take
  effect when preparation completes; this is asynchronous control, not sample-accurate
  spectral automation or a continuous Doppler simulation. Generator throughput is measured separately from callback CPU.
- When either mic uses a model, only one mic per rig may crossfade at a time;
  A/B alternate priority when both wait. Six simultaneously published model requests
  use at most nine convolution engines, not twelve. The second mic waits for the
  first 50 ms fade. This audio-thread-only scheduling needs no mutex/allocation;
  there is no cross-rig ownership to stall on a routing change. Fixed-only pairs
  preserve their existing swap behavior. Audio tests enforce this overlap limit
  and eventual activation of all six queued kernels; CPU thresholds stay unchanged.
- The 39 `ocab` host parameters append at indices 4785-4823 with AU version hint 7.
  No prior ID, choice list or normalized source mapping changes. Both modeled switches
  default off, including migration of missing controls in old projects/comparisons.
- v1 design choices pin both definitions and generator behavior. Do not edit v1 tone
  when adding v2; add explicit new version IDs/dispatcher and migration tests instead.
  Frozen complex-response anchors guard v1. Shared IR schema 11 and 64 MiB budget
  remain unchanged. Modeled state adds only numeric parameters, no encoded kernels.
- Classic, Dual Blend, Dual Crossover and Matrix still run per-lane AMP -> CAB.
  Existing blend, level, polarity, fine delay, cuts and lane bypass are retained.

## Verification contract

Numeric thresholds are encoded in tests, independent of whether a run passes:

- `ChimeraOriginalCabModelTests`: 864 boundary settings; finite complex response,
  magnitude < 8; response continuity < .025 per .001 Position step and < .15
  per .1 cm Distance step; nonzero kernel,
  final 10% energy < .001 of total; farther distance lowers energy and moves peak
  > .8 ms for 10 -> 60 cm; selection/motion changes waveform; frozen v1 anchors
  within 1e-10. Tests at 44.1/48/96 kHz.
- `ChimeraOriginalCabIntegrationTests`: actual prepared Cab convolutions, mono/stereo,
  64/256 frames at those three rates; A/B blend residual < 2e-6, same-unit unity,
  channel isolation < 1e-8; storm coalescing/last-request convergence, finite bounded
  transitions; teardown. Thread-local C++ new/delete watch must remain zero.
  This watch does not intercept every malloc, platform allocator or lock.
- `ChimeraCabPanelStateTests`: original modeled production audio project/comparison
  restore residual < 1e-6 in all four routings; actual Position-to-audio and UI binding; Matrix low DI 0/50/100% and
  amp oversampling 1x/2x/4x/8x restoration;
  dirty-state legacy reset; original fixed/user IR and large-state contracts retained.
- Existing GateProcessor frozen parameter contract and NativeState appended-order
  checks include the new 39 parameters, retaining all prior expectations.
- Callback p50/p99/max and deadline misses are recorded for six mic paths plus swaps.
  A predeclared runner guard requires CAB-only p99 < one audio-block period.
  The benchmark uses the same no-denormals scope as production processBlock.
  POSIX thread CPU p99 is also diagnostic; -1 means unavailable on Windows. It does
  not replace or waive the wall-time/deadline gate.
  Deadline misses are still reported, not hidden by that p99 guard. This is
  **not target-hardware approval** and
  not a universal realtime-safety claim. No silent deadline waiver changes.

Actual instrument DI listening, target-machine CPU budgets, Windows/macOS/DAW
lifecycle and final branding remain acceptance work. Technical tests cannot establish
musical quality or a product release approval. `release_approved=false`.
