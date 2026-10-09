# CAB enclosure and array layouts v3

This change is stacked on PR #33, source `10c7c030de20a6b663050cdaaad7672f67ae8c56`.
It keeps all 14 speaker, 20 microphone and three additional tweeter designs.
These are independently authored **linear reduced-order** designs, not measured
replicas, a trained model or a claim of hardware fidelity. `release_approved=false`.

## Selectable enclosures

The cabinet selector defaults to **Legacy 4-unit cabinet**, dispatching the
unchanged v1/v2 generator. New templates use the following authored geometry:

| Template | Columns × rows | Width × height × depth (m) | Net volume (L) |
|---|---:|---|---:|
| Guitar 1×12 | 1 × 1 | 0.48 × 0.48 × 0.30 | 50 |
| Guitar 2×12 | 2 × 1 | 0.78 × 0.49 × 0.32 | 88 |
| Guitar 4×12 | 2 × 2 | 0.74 × 0.76 × 0.36 | 155 |
| Bass 1×15 | 1 × 1 | 0.56 × 0.60 × 0.43 | 112 |
| Bass 2×10 | 2 × 1 | 0.61 × 0.40 × 0.37 | 70 |
| Bass 4×10 | 2 × 2 | 0.62 × 0.64 × 0.40 | 125 |
| Bass 8×10 | 2 × 4 | 0.63 × 1.22 × 0.40 | 238 |
| Bass 1×12 | 1 × 1 | 0.49 × 0.52 × 0.39 | 72 |
| Bass 2×12 | 2 × 1 | 0.77 × 0.49 × 0.40 | 118 |

The two bass 12-inch templates preserve useful configurations for Clarity 12 and
Chimera Depth 12. Speaker design and enclosure are independent controls. If the
selected driver's diameter differs from a template's nominal diameter, dimensions
and spacing scale by their ratio and volume by its cube. The UI shows the actual
count/diameter and identifies the scaled template; it never calls four 15-inch
speakers a 4×10. It does not silently replace the selected speaker design.

All units in one box use the same selected driver. Mic A/B independently address
any active unit, including the same unit. Numbering is left-to-right, top-to-bottom.
For a smaller array, an out-of-range stored/automated index uses the last available
unit. The raw value remains stored, so switching back restores the original target.
The UI disables unavailable targets and labels the effective fallback. This avoids
UI timer writes into host automation.

## Modeled audio and shared geometry

`Source/CabLayoutModel.h` owns both the physical geometry and request encoding.
The production worker calls its generator. The focused CAB view and each rig's
room view use the same centres, count, radius and enclosure aspect, with their
vertical display axis inverted relative to acoustic coordinates. The new enclosure
is drawn with a front baffle and roof; its amp head shares the existing support
plane. Legacy artwork and legacy geometry remain intact.

- Sealed loading is `sqrt(1 + N * Vas / Vb)` with actual driver count and net volume.
  It modifies LF resonance and damping. Open rear removes that sealed loading.
- Every cone contributes a complex radiating-surface integral, using its absolute
  centre and the chosen mic's position, distance, aperture and pressure/gradient
  response. Coherent summation retains geometric spreading and phase interference.
  Per-driver source strength is constant; there is no hidden loudness normalization.
- Three damped axial enclosure modes derive from width, height and depth. Open rear
  reduces their contribution and adds the existing delayed, inverted edge path
  using the new geometry.
- The optional tweeter has its own top-centre position, crossover/directivity and
  distance/phase path. The existing tweeter designs and level control are retained.
- The previous 85 ms kernel, anti-alias taper, zero reported convolution latency,
  off-callback generation, bounded cache, stale-request rejection and 50 ms swap
  path are retained. Acoustic propagation delay remains part of the kernel.

## Not implemented / not verified by this model

No mixed driver types within one box, per-unit level/delay, slanted baffle, rotatable
2×12, arbitrary custom enclosure editor, ports/Helmholtz resonators, partitioned
8×10 chambers, full boundary-element diffraction, mutual mechanical impedance,
nonlinear excursion, thermal compression or room reflections/reverberation.
The room scene is a control surface, not an acoustic room simulation. Surface
quadrature and axial modes are approximations, not a full wave-equation solver.
Actual instrument DI listening and target-machine/DAW acceptance remain separate.

## Host, state and captured IR compatibility

Nine `lcab` parameters append at indices **4836–4844**, with AU version hint **9**:
`layout`, `Aunit`, `Bunit` per lane. Existing 4836 IDs, order, ranges, defaults and
normalized mappings stay unchanged. Existing `ocab` unit choices remain 0–3; the
new unit bank is independently 0–7. Layout choices have a fixed 0–9 range. Future
expansion must not enlarge those ranges and remap normalized automation.

Zero layout means exact v1/v2 key and generator dispatch, regardless of dormant
new unit values. A missing bank restores to definition defaults instead of dirty
current-session values. Schema 11, project and comparison recall, the shared IR
asset table and 64 MiB storage budget remain unchanged. Captured IRs do not pass
through the new generator; Mic A/B can mix an imported capture with a modeled
array. IR LOADER navigation preserves the layout. Matrix LOW continues to mix DI
against AMP+CAB; the cabinet's internal A/B blend remains separate.

## Verification contracts

Existing suites and thresholds are retained. Three suites are added to the same
Windows/Linux/macOS CAB workflow:

- `ChimeraCabLayoutModelTests`: all nine geometries, every driver/mic/active-unit
  combination at rear/position/distance extremes (56,000 cases), coherent-ray
  equality and interference, net volume/open loading, key round trips, v1/v2
  bit-identical dispatch, 44.1/48/96 kHz kernels and distance/tail checks.
- `ChimeraCabLayoutIntegrationTests`: eight-driver, six-mic, mono/stereo production
  convolution at 44.1/48/96 kHz and 64/256 frames; latest-request convergence,
  prepared impulse equivalence, swap bounds, callback allocation/deletion audit
  and the unchanged p99 one-block-period gate.
- `ChimeraCabLayoutStateTests`: all layouts change production audio, independent
  targets, all routing modes, LOW DI isolation, project/comparison/legacy and
  deleted-source captured-IR recall, append-only host contracts, full array UI
  geometry, all 20 mic illustrations at all unit boundaries, software screenshots,
  drag host gestures and cancellation on layout automation.

The pre-existing native-window CAB tests remain enabled in the CI workflow.
Model-level gates remain magnitude <8, position tick <0.025, distance tick <0.15,
last-10%-tail energy <0.001, and production impulse residual <3e-6. CPU thresholds
are not relaxed or converted to advisory status. Timing results describe the
runner, not a universal DAW performance guarantee.
