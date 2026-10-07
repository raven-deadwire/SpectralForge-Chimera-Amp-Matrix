# Niflheimr native prototype

Implementation baseline: `15427d5717d30f566de27bd3dcc0eafa459a5563`.
Design source: **Niflheimr Full Product/DSP Plan v1.0, 2026-10-07 KST**.
This is a development implementation; it is not a release or a completed
instrument-DI listening assessment. Avalanche is outside this development scope.

## Product path

Niflheimr is appended at model ID **25**. All existing model IDs, the 15-choice
legacy host range, and all previously released parameter ordinals remain intact.
It is selectable as a Bass head, with clean/high-gain search tags, and always
uses the native engine. It is one amplifier with five actual channels:

| Channel | Implemented nonlinear path |
| --- | --- |
| Modern Tight | Separate symmetric BODY and EDGE saturation with fast recovery |
| Death Grind | Three asymmetric serial stages with interstage coupling/bandwidth |
| Industrial Bite | Clean BODY with a separately driven asymmetric upper knee |
| Slam Impact | Parallel knee/rounded saturation, onset-dependent drive relief |
| Sludge Mass | BODY-weighted asymmetric rail/knee fuzz and slower density response |

These are authored structures. They do not claim recovered hardware coefficients
or measured equivalence to Tech 21, Mesa, EBS, GR Bass, Darkglass or Sunn.

## Control and state contract

Stable control order (host/DSP): `gain`, `bass`, `middle`, `treble`,
`mid_frequency`, `presence`, `depth`, `master`, `mass`, `split`, `fang`, `fold`,
`thrust`, `blend`. The panel displays MID FREQ before TREBLE and MASTER last.

Each of the six Classic/Dual/Matrix contexts owns five independent 14-control
banks. IDs use `originalAmp_c{context}_niflheimr_ch{channel}_{control}`. The
selected channel uses the existing native channel ID. The new 432 parameters
are appended after all released Náströnd channel parameters, with version hint 5.
APVTS stores these values; the audio thread reads atomics through the native
parameter cache. Reset Channel affects only the selected channel's 14 values.

All normalized controls initially use 0.5, except BLEND 0.65. MID FREQ initially
uses 650 Hz, with a 150–2500 Hz range. The EQ ranges and crossover/time constants
are prototype values. GAIN 5 being musically sufficient for metal remains a
listening target, not something a finite-output test can approve.

## Signal path

The existing native wrapper supplies oversampling, latency alignment and channel
crossfades. The core receives the already raised sample rate and does not
oversample again. DC/subsonic cleanup precedes complementary LOW/BODY/EDGE
splits. Only the selected DIRTY topology runs in steady state.

The common LOW is outside the high-gain kernels. An eighth-order 180 Hz
Butterworth high-pass filters the nonlinear difference (kernel output minus
its original residual), which is then added back to the original residual.
This rejects nonlinear leakage without creating a competing low-band path;
it addresses measured cancellation in the first implementation. The protection
frequency/order remain authored voicing choices requiring DI/IR listening. CLEAN is the cleaned input;
DIRTY is common LOW plus the selected channel remainder. Mixing uses
`LOW + (1 - blend) * cleanRemainder + blend * dirtyRemainder`. At BLEND zero the
core explicitly selects the cleaned input, so DIRTY controls and moving SPLIT
cannot recolour CLEAN. Linear EQ/PRESENCE/DEPTH and MASTER follow the blend.
Numerical emergency rails handle abnormal overload; they are not intended
output saturation or a replacement for gain staging.

GAIN and character controls are smoothed. Sample-rate-dependent envelopes,
filters and coefficient updates use the internal rate. No core allocation,
file access, state serialization or mutex runs inside set/reset/tick.
The amplifier wrapper chunks blocks larger than the prepare hint into
non-owning views, preserving state and its prepared buffer capacity.

The Matrix core only receives its lane's signal. It does not reinsert a
full-range input. Legacy Matrix Low `drive=0` does not override native GAIN;
Niflheimr BLEND and the separate Matrix DI/AMP MIX retain different meanings.

## Build and render

The regular CMake build includes:

- `ChimeraNiflheimrCoreTests`: standalone core invariants.
- `ChimeraNiflheimrIntegrationTests`: production native/oversampling/state paths.
- `ChimeraNiflheimrRender`: five-channel WAV render using the production wrapper.
- `ChimeraNiflheimrRendererTests`: CLI validation and output/metadata checks.

The existing native and UI tests also cover the appended catalogue, actual
processor state, channel recall and panel binding. The catalogue generator and
its source definitions are updated together.

Example (use the built executable path for the chosen platform):

```sh
ChimeraNiflheimrRender bass-di.wav niflheimr-audition --oversampling 4 --block-size 128
```

The destination must be new. The renderer preserves the same input for all
five channels, writes floating-point head-only WAVs and records source/output
hashes, compiled source-file fingerprints, parameters, rate and latency. It does not apply Cab, PRE, POST or input
normalization. Apply the same suitable bass IR and one fixed output matching
gain per channel for listening. Synthetic inputs must be explicitly labelled
with `--input-kind synthetic`; they are technical fixtures, not played bass DI.

## Acceptance still required

The initial standalone core test run on Linux/g++ established the following
technical evidence (synthetic fixtures, not instrument-DI acceptance):

| Check | Observed result |
| --- | --- |
| Protected fundamentals, 810 states from 27.5 to 82.4 Hz | 0.987530–1.03695 amplitude ratio relative to CLEAN |
| 10 channel pairs at common settings | Minimum level-matched residual 0.475101; EQ matching/listening still pending |
| GAIN 5→7.5→10 | Every channel changes beyond level alone; minimum normalized residual 0.160345 |
| Five character macros in five channels | All 25 engaged responses change the output |
| 10 Hz response at internal 768 kHz / 1.536 MHz / 3.072 MHz | 0.707078 / 0.707092 / 0.707100 amplitude ratio |
| Core set/reset/tick allocation watch | Zero allocations |

The first core exhibited destructive low-frequency interaction at some dirty
settings. The checked-in regression covers the corrected signal path, not a
relaxed tolerance. Stereo isolation, nonfinite input/state recovery, extreme
settings and decay to silence are also exercised by the standalone core test.

Technical passes establish implementation invariants only. Before product
approval, retain evidence for:

- Actual Low B/Drop A DI at fixed input level and identical Cab/IR, including
  picking/finger, muted riffs, strong/weak transitions and long decays.
- GAIN 5/7.5/10 usefulness and channel identity in all ten pairs, especially
  Modern Tight versus Industrial Bite and Death Grind versus Slam Impact.
- Identity after output level and spectral differences are reduced, plus
  first-hit/sustain/recovery behaviour, not only different waveforms.
- Low-frequency magnitude/phase across BLEND/SPLIT/MASS, external Matrix LOW DI
  interaction, alias spectra and measured worst-case CPU at supported qualities.
- Windows/macOS builds, actual DAW project reopen, automation, editor lifecycle
  and simultaneous channel/quality changes on target hardware.

The current fascia is a neutral functional prototype. Final head artwork,
public Nordic channel names, factory voicing, release packaging and optional
five fixed-setting NAM exports follow sound and product acceptance.

## Local integration verification

Linux Release VST3 and the UI executable build successfully. Seven selected
technical suites pass: Original core/integration, Niflheimr core/integration/
renderer, existing AmpNative, and actual processor NativeState. The last test
checks all released ordinals, binary/A-B recall and missing-bank migration
without opening a native window. It is separate from the UI acceptance gate.

The production comparison covers 100 channel/rate/oversampling routes. Direct
core versus the 1x wrapper, normal block repartitioning and oversized/empty
blocks all have zero measured sample residual; the watched audio path performs
zero allocations. See `Validation/niflheimr-prototype-local.json` for source
fingerprints and per-suite results. These are local modified-source results,
not an exact-head CI or release verdict.

Native UI execution is blocked in this environment because no active display
server/window manager is available. Windows/macOS CI, native UI and actual DAW
acceptance remain separate. The local catalogue/preparation checks also pass.
