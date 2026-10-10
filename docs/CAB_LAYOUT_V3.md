# CAB enclosure and array layouts v3

The layout expansion was stacked on PR #33, source `10c7c030de20a6b663050cdaaad7672f67ae8c56`.
Release 1.3 retains all 14 speaker, 20 microphone and three additional tweeter designs,
and replaces the unshipped 8×10 modeled preview with a 2-column, 3-row **Bass 6×10**.
These are independently authored **linear reduced-order** designs, not measured
replicas, a trained model or a claim of hardware fidelity. `release_approved=false`.

## Front-view equipment and IR imagery

All 14 speaker designs now have separate authored front elevations, including
different cone ribs, surrounds, dustcaps, mounting patterns and material finishes.
Chimera Guitar 12 and Chimera Bass 10 also face forward. The cabinet baffle is
drawn independently of the speakers, so no second cone is baked into the box.

One physical scale is shared by the active rigs within the room. Room framing fits
only visible equipment with 0.08 m of side allowance, instead of reserving the
0.56 m microphone travel margin used by the focused view. No per-rig scale boost
changes the size ratios. The focused view fits its selected head and cabinet with
a fixed microphone travel allowance, so moving a microphone cannot reframe it. Nominal
outer diameters follow the cabinet's fixed 10/12/15-inch specification; the active
cone radius and pickup coordinates remain those of the acoustic model. Selecting
another speaker cannot enlarge the cabinet, its holes, the microphone or the amp
head. Heads and microphones use physical body dimensions instead of a fraction of
the cabinet's width; their dimension sources and authored assumptions are recorded
in [head dimensions](CAB_HEAD_DIMENSIONS.md) and
[microphone dimensions](CAB_MICROPHONE_DIMENSIONS.md). The room and focused view
use the same geometry and cached speaker artwork. The original cabinet materials,
trim and corner fittings are restored independently of the driver drawings.
No audio parameters are added.

IR Loader shows enlarged cabinet and microphone artwork independently for A/B.
Documented capture configurations determine driver counts and diameters. Unknown
cabinet configurations use a closed grille image, and mixed or unspecified
microphones use explicitly labelled generic illustrations. These pictures do not
assign an inferred acoustic model or change the loaded IR.

`ChimeraCabDriverVisualTests` checks diameter ratios, fixed microphone dimensions,
front-facing silhouettes, distinct rendered designs, boundary placement, room
updates, capture imagery and parameter-neutral navigation. It also generates
comparison screenshots for visual review.

## Selectable enclosures

The menu shows each of the following nine configurations once. Stored layout zero
is displayed through **Guitar 4x12** or **Bass 4x10** according to its Guitar/Bass
category; merely opening the panel does not rewrite it to an explicit layout.
The explicit selections use the following authored geometry. Product labels contain category, count and nominal diameter;
implementation generations are not exposed in the selectors.

| Template | Columns × rows | Width × height × depth (m) | Net volume (L) |
|---|---:|---|---:|
| Guitar 1×12 | 1 × 1 | 0.48 × 0.48 × 0.30 | 50 |
| Guitar 2×12 | 2 × 1 | 0.78 × 0.49 × 0.32 | 88 |
| Guitar 4×12 | 2 × 2 | 0.74 × 0.76 × 0.36 | 155 |
| Bass 1×15 | 1 × 1 | 0.56 × 0.60 × 0.43 | 112 |
| Bass 2×10 | 2 × 1 | 0.61 × 0.40 × 0.37 | 70 |
| Bass 4×10 | 2 × 2 | 0.62 × 0.64 × 0.40 | 125 |
| Bass 6×10 | 2 × 3 | 0.63 × 0.94 × 0.40 | 178.5 |
| Bass 1×12 | 1 × 1 | 0.49 × 0.52 × 0.39 | 72 |
| Bass 2×12 | 2 × 1 | 0.77 × 0.49 × 0.40 | 118 |

Each layout has a fixed instrument category, nominal diameter, enclosure size,
spacing and volume. A 10-inch cabinet cannot load a 12- or 15-inch speaker. Choosing
a cabinet keeps a compatible current speaker or loads the first compatible
speaker; incompatible choices are disabled. Guitar 12-inch layouts accept the
eight guitar designs. Bass 10-inch layouts accept Foundry 10, Vector 10 and Alloy
10; bass 12-inch layouts accept Clarity 12 and Chimera Depth 12; Bass 1x15 accepts
Monolith 15. Chimera Guitar 12 and Chimera Bass 10 are available when their selected
family matches the layout. The four-speaker category can be changed explicitly
between Guitar and Bass, which loads its corresponding Chimera speaker.

Stored or automated mismatches resolve at use time to Ember 30 (guitar 12-inch),
Foundry 10 (bass 10-inch), Clarity 12 (bass 12-inch) or Monolith 15 (bass 15-inch).
Audio, geometry, captions and the selected image all use that same effective
speaker. The original stored value is not rewritten by painting or UI timers.
The four-speaker layout's family follows its explicit Guitar/Bass parameter;
automating a driver cannot change that cabinet's family or diameter.

All units in one box use the same selected driver. Mic A/B independently address
any active unit, including the same unit. Numbering is left-to-right, top-to-bottom.
For a smaller array, an out-of-range stored/automated index uses the last available
unit, except for the 6×10 migration described below. The raw value remains stored,
so switching back to another layout retains the original target.
The UI disables unavailable targets and labels the effective fallback. This avoids
UI timer writes into host automation.

## Modeled audio and shared geometry

### 1.3 replacement of the 8×10 preview

The large bass cabinet keeps serialized ordinal **7**, now identified as `b610-v3`.
The other layout ordinals and the normalized 0–9 host range do not move. Its six
10-inch drivers use the existing speaker parameters, cone-field calculation,
microphone models and tweeter options. No additional ML training or new hardware
measurement is claimed. The 1.22 m preview height loses one 0.2794 m row pitch,
rounded to an authored 0.94 m. Width and depth stay 0.63 m and 0.40 m.

Net volume is 178.5 L, exactly 6/8 of the previous 238 L. This preserves the
per-driver sealed compliance term `N * Vas / Vb` for the same selected speaker.
Six sources contribute to the coherent field; the shorter height changes the
axial mode, edge paths and tweeter position. The result is a new 6×10 response,
not an assertion that the previous 8×10 audio is unchanged.

Unpublished preview states with layout 7 now select the 6×10. Previous units
7 and 8 (zero-based 6 and 7) resolve to units 5 and 6 (zero-based 4 and 5), keeping
the left/right column in the last remaining row. Stored values are untouched;
the shared `effectiveUnit` resolver is used by DSP and display. Captured/User IR
8×10 files keep their original audio and capture metadata.

### Shared model

`Source/CabLayoutModel.h` owns both the physical geometry and request encoding.
The production worker calls its generator. The focused CAB view and each rig's
room view use the same centres, count, radius and enclosure aspect, with their
vertical display axis inverted relative to acoustic coordinates. The new enclosure
is drawn with a front baffle and a roof projected from its physical depth; its amp
head shares the support plane. Valid same-family four-speaker requests retain
their existing acoustic geometry and generator. Incompatible saved combinations
now resolve to a fitting speaker before request encoding and rendering.

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
multi-chamber cabinets, full boundary-element diffraction, mutual mechanical impedance,
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

Zero layout retains exact v1/v2 key and generator dispatch for compatible
same-family requests, regardless of dormant new unit values. Earlier mismatched
family/diameter combinations intentionally use the fitting fallback instead;
their former enlarged-box response is not preserved. A missing bank restores to
definition defaults instead of dirty current-session values. The processor's
existing saved schema value 10, project and comparison recall, the shared IR
asset table and 64 MiB storage budget remain unchanged. Captured IRs do not pass
through the new generator; Mic A/B can mix an imported capture with a modeled
array. IR LOADER navigation preserves the layout. Matrix LOW continues to mix DI
against AMP+CAB; the cabinet's internal A/B blend remains separate.

## Verification contracts

Existing suites and thresholds are retained. Four suites are added to the same
Windows/Linux/macOS CAB workflow, with a fifth camera-framing suite in 1.3:

- `ChimeraCabLayoutModelTests`: fixed nominal geometry, driver compatibility and
  fallback across layouts, driver/mic/active-unit combinations at
  rear/position/distance extremes, coherent-ray equality and interference,
  net volume/open loading, key round trips, compatible v1/v2 bit-identical
  dispatch, 44.1/48/96 kHz kernels and distance/tail checks.
- `ChimeraCabLayoutIntegrationTests`: six-driver, six-mic, mono/stereo production
  convolution at 44.1/48/96 kHz and 64/256 frames; latest-request convergence,
  prepared impulse equivalence, swap bounds, callback allocation/deletion audit
  and the unchanged p99 one-block-period gate.
- `ChimeraCabLayoutStateTests`: all layouts change production audio, independent
  targets, all routing modes, LOW DI isolation, project/comparison/legacy and
  deleted-source captured-IR recall, append-only host contracts, full array UI
  geometry, all 20 mic illustrations at all unit boundaries, software screenshots,
  drag host gestures and cancellation on layout automation, compatible speaker
  loading on explicit cabinet choice, disabled incompatible menu items, and
  parameter-neutral display of restored mismatches.
- `ChimeraCabDriverVisualTests`: 14 distinct circular speaker fronts, physical
  diameter ratios, fixed mic/head dimensions, all microphone boundary placements,
  room cache ownership, capture images and parameter-neutral IR navigation.
- `ChimeraCabCameraFramingTests`: active-rig fitting, common physical scale,
  focused microphone allowance and stable framing over microphone movement.

The pre-existing native-window CAB tests remain enabled in the CI workflow.
Model-level gates remain magnitude <8, position tick <0.025, distance tick <0.15,
last-10%-tail energy <0.001, and production impulse residual <3e-6. CPU thresholds
are not relaxed or converted to advisory status. Timing results describe the
runner, not a universal DAW performance guarantee.


## Historical fixed-fit validation before the 1.3 replacement (2026-10-09)

The fixed-fit revision passes the local Release layout model, layout production
integration, expanded state, layout state/software geometry, and driver/IR visual
suites. Coverage includes 270 explicit-layout requests plus all 30 four-unit
family/driver combinations, 16,800 compatible acoustic boundary cases, 30 invalid
visual restore cases, 9,600 microphone boundary placements, 14 distinct fronts,
real microphone dimensions and common room/IR scale. The complete panel suite
passes integrated capture/state, 78 physical head cases and Matrix LOW isolation
before stopping at the required native-display precondition. Native drag tests
remain unverified locally and must pass the desktop CI jobs.

The unchanged CPU gate failed locally in the original and expanded four-unit
engines; their impulse/swap and zero watched callback allocation/deletion checks
passed. The new layout engine passed its CPU gate. These local failures are kept
as failures; no CPU threshold or test gate was relaxed. Exact-revision platform
CI, installer verification and actual DAW acceptance are separate evidence.

## 1.3 local model verification

The updated pure C++ model suite passes 270 explicit compatibility requests and
15,840 compatible driver/mic/unit boundary cases. Maximum tested magnitude is
1.04961, position tick 0.00136444 and distance tick 0.00996308. It checks all
44.1/48/96 kHz kernels, the six-source coherent field, inherited per-driver sealed
loading, the changed height mode and column-preserving preview target migration.
This is model-level evidence; production/native UI and exact-source platform CI
are separate release checks.
