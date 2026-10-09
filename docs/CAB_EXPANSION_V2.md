# CAB speaker and microphone expansion v2

Selectable enclosures and the current fixed-diameter compatibility rules are
documented in [CAB layout v3](CAB_LAYOUT_V3.md). The four-unit generator and earlier
validation results below describe the v2 implementation, not permission to fit an
arbitrary speaker diameter into a current cabinet selection.

This development branch adds playable Chimera designs to the existing CAB panel.
It does not replace v1 sounds or convert captured hardware identities into models.
No external IR, measured transfer, trained weights or commercial numeric specification
was used to generate the new responses. Research references identify intended roles;
the numerical definitions are authored design assumptions, not measured clones.

## Playable inventory

There are 14 new speaker designs (8 guitar, 6 bass), 20 new microphone designs
(9 dynamic, 3 ribbon, 8 condenser), and three additional tweeter designs. The original
two speaker definitions remain available as Chimera Guitar 12 and Chimera Bass 10;
the three microphone roles are Attack Dynamic, Body Ribbon and Detail Condenser.

| Guitar speaker | Research role |
|---|---|
| Ember 30 | Modern ceramic midrange / Vintage 30 |
| Steel 75 | Scooped ceramic / G12T-75 |
| Verdant 25 | Warm British / G12M-25 |
| Granite 55 | Low-resonance British / G12H55 |
| Silver 12 | Clean ceramic / Jensen family |
| Carnivore 12 | Eminence Karnivore |
| Raven 100 | Celestion G12-100 Raven, not G12K-100 |
| Chimera Ruin 12 | Original heavy guitar role |

| Bass speaker | Research role |
|---|---|
| Foundry 10 | Classic sealed 10-inch / SVT |
| Vector 10 | Modern neo 10-inch |
| Clarity 12 | Reference 12-inch / Glockenklang and Vanderkley |
| Alloy 10 | Hybrid attack / Hartke |
| Monolith 15 | Large 15-inch bass |
| Chimera Depth 12 | Original extended-range bass role |

| Family | New microphone designs |
|---|---|
| Dynamic | Needle 57, Hammer 421, Spear 441, Edge 906, Anchor 20, Veil 7, Pin 201, Fang 88, Depth 112 |
| Ribbon | Silk 121, Focus 160, Velvet 4038 |
| Condenser | Halo 87, Prism 414, Pencil 184, Titan 47 FET, Crystal 4050, Copper 201 FET, Ember 67, Chimera Strike |

The immutable order, stable definition IDs, reference associations and coefficients
are in `Source/CabExpansionModel.h`. Future expansion must append a new versioned
parameter bank; do not expand these choice ranges and remap normalized automation.
These new names are the implementation's working product names, not evidence of a
previously approved naming list. Ashdown is excluded.

## Actual acoustic processing

Each driver has independent LF resonance/damping, inductive roll-off, breakup
frequency/Q/amplitude, a complex notch, compliance and coherent-radiation radius.
Each microphone has independent band limits, two resonances, pressure/gradient
weight, capsule aperture and proximity response. A frequency-dependent aperture
term and ray incidence act inside the cone integral, so the new microphones also
interact differently with Position and Distance; they are not gain-only aliases.

The v2 generator contains four matching selected drivers in a 2x2 array. Its
original helper scaled dimensions and volume with driver diameter. Current
cabinet requests first enforce a fixed matching family and diameter, so choosing
a speaker cannot enlarge the box. Use Bass 1x15 for Monolith 15 and Bass 1x12 or
2x12 for Clarity 12 and Chimera Depth 12. These are authored designs, not claims to
replicate a MESA or SVT cabinet. Mixed driver types remain unsupported. Each A/B
mic can address the same or a different radiator within the selected layout.

Closed/Open rear, 2–60 cm distance and 0–1 radial position remain real model inputs.
Silk HF, Metal HF and Air HF have independent crossover, roll-off and directivity.
Their level is the existing TWEETER control; at zero the tweeter is silent.
This is a linear reduced-order model. No excursion, thermal compression, ported
Helmholtz model, room reverb or measured hardware-fidelity claim is added.

The 85 ms kernel, no trimming/normalization, zero reported processing latency,
worker generation, cache bound, stale-request rejection and 50 ms transition remain.
Legacy generator code is unchanged and dispatched directly when all extensions
are zero. Every new selection remains independently stored while a captured IR is
active; IR LOADER does not remike an imported fixed capture.

## UI and state

The cabinet page has a Guitar/Bass grouped speaker selector and independently
grouped Dynamic/Ribbon/Condenser selectors for Mic A and B. The lower mic selector
switches Attack Dynamic, Body Ribbon and Detail Condenser and is disabled when
another microphone design is active. Each choice displays its individual
catalog illustration and a small research reference. Spatial nodes use all twenty individual illustrations with per-image capsule,
mount and XLR anchors. End-address bodies rotate towards the baffle; side-address
bodies stay upright. These are illustrated controls, not a full 3D mesh renderer.

Twelve `xcab` parameters append at indices 4824–4835 (AU version hint 8). The old
4824 IDs, ranges, defaults, ordering and choice mappings remain unchanged. Missing
extensions restore to zero from their definitions, not the dirty current session.
Schema 11, shared captured-IR bytes, the 64 MiB budget, existing Matrix LOW
DI-to-AMP+CAB blend and all four routing modes remain in use.

## Verification

- Existing frozen v1 complex anchors and bit-identical legacy kernel dispatch.
- 8,960 expanded boundary combinations, response magnitude <8, Position tick
  delta <0.025 and Distance tick delta <0.15.
- Pairwise level-matched 12-frequency spectral RMS difference >0.15 dB for every
  driver pair and every microphone pair. This is a differentiation check, not
  a psychoacoustic or musical-quality approval.
- Generated response at 44.1/48/96 kHz; finite energy and final-10% tail energy
  <0.001 of total; natural distance attenuation; rear/tweeter waveform changes.
- Every new driver and microphone changes actual production-path audio; both
  slots, all routes, project/comparison recall, UI/host automation and legacy load.
- Both v1 and v2 run the same six-path mono/stereo 64/256-frame convolution,
  impulse-equivalence, transition, watched-allocation and p99 deadline checks.
  Timing thresholds, response length and historical failures are not waived.

Actual instrument DI listening, target-machine/DAW acceptance and exact-source
Windows installer validation remain separate. `release_approved=false`.

### Local checkpoint — 2026-10-09

Release build with GCC 13, CMake and JUCE 8.0.8 succeeded on Linux. At DSP/source
commit `8383dcbf24e61011d0e8cf52122e38e482aaf9ac`, the following six CTest suites
passed in 76.25 seconds:

| Suite | Result |
|---|---|
| ChimeraGateProcessorTests | Pass; released parameter contract and processor regression |
| ChimeraCabExpansionModelTests | Pass; 8,960 boundary cases, 14 drivers and 20 microphones |
| ChimeraOriginalCabModelTests | Pass; frozen v1 response and legacy dispatch |
| ChimeraOriginalCabIntegrationTests | Pass; all 12 rate/block/channel configurations |
| ChimeraCabExpansionIntegrationTests | Pass; all 12 rate/block/channel configurations |
| ChimeraCabExpansionStateTests | Pass; production audio, routes, recall, host bindings and software UI |

Expanded-model maximum tested response magnitude was 1.27528; minimum pairwise
level-matched spectral RMS differences were 1.3166 dB (drivers) and 0.276468 dB
(microphones). Both integration suites observed zero watched callback C++ new/delete.
All p99 timing gates passed. Occasional wall-clock deadline misses still occurred;
this is not a guarantee of glitch-free operation on a user's DAW or machine.

Software screenshots exposed overlapping sidebar previews and controls. A follow-up
UI-only correction removed redundant sidebar pictures, moved level/filter controls,
placed the tweeter selector beside the driver selector, and awaited scene refresh
before the automated host-selection screenshot. Its native C++ build and expanded
state/UI suite passed again (20.05 seconds); both screenshots were visually checked.
All twenty microphone images were checked at boundary positions with legacy,
10-inch and 15-inch driver geometry (480 placements).

Full native-window interaction tests could not run here because the environment
does not permit the required X display socket. They remain enabled in CI. No new
Windows/macOS CI result or Windows installer is claimed: pushing the development
branch was blocked by automatic approval review pending authorization to transmit
the committed changes to the existing GitHub repository. No release was published.
