# Náströnd — five TONE3000 A2 captures

Internal offline production tools for five independent Náströnd channel models,
plus TONE3000's own six-band parametric EQ presets. These tools are not part of
Chimera's shipping interface. `CHIMERA_BUILD_NAM_TOOLS` defaults to `OFF`.

The current deliverable is a **test build**, pending the accuracy and listening
checks recorded in its reports. It is not a release approval. The original
single parametric-NAM experiment was superseded by the user's request for one
NAM per channel and TONE3000 as the target player.

## Frozen sources

| Component | Revision |
| --- | --- |
| Chimera native DSP | `d93560445c78552768a0ed592e5ec62c3c648fb6`, v1.2.0-beta.1 |
| JUCE | 8.0.8 |
| sdatkinson/neural-amp-modeler | `0072676419459f5d39e36f5b9fd4172f28d62cbf` |
| tone-3000/tone3000-plugin | `ef6f178ae1ac6412b55fec6f058d86e640e5aaaf` |
| TONE3000's NAM core | `1f42f88535884450104b8711d7595019afa0495b` |
| Python runtime used | Python 3.11, PyTorch 2.8.0 CPU |

The target plugin's local-import gate accepts only A2 shapes. A legacy A1
WaveNet or a ConcatWaveNet parametric fork is not a substitute. Each exported
file is the official 8-channel A2-Full WaveNet (23 layers, 12,145 weights,
6,347-sample receptive field at 48 kHz). There is no separately trained Lite
tier in these files. TONE3000 explicitly accepts a bare A2 WaveNet.

The user's requested parametric control is implemented by the target player's
six-band POST EQ. Each `.t3kpreset` embeds its NAM bytes and its EQ settings;
no TONE3000 account or uploaded model URL is needed to load the preset offline.
The 13 original amp controls are fixed capture settings, documented in the
package. The EQ does not claim to reproduce those amp controls.

## Sound and capture boundary

`capture_channels.py` defines five authored settings: Fenrir attack/clank,
Surtr dense vocal mids, Nidhoggr asymmetric rot, Fimbulvetr broad bloom with
open upper mids, and Ragnarok low-end impact. These are distinct actual native
channels, not renamed copies of one capture. Every setting includes all 13
native controls.

`RenderNativeAmp.cpp` runs the released production `Amp`/`AmpNativeDSP` route
at 48 kHz mono, block 256, Modern mode and 4x oversampling. It includes the real
amp wrapper and its delay compensation, with unity input/output trims. It
excludes PRE/POST effects, cabinet IR, Dual/Matrix, low-DI mix and transpose.
MASTER is part of the captured amp.

The new channel-capture workflow retains the *entire* native output latency
(6 samples in this build). An oversampler's group delay must not be removed as
though it were only a pure delay: doing so can turn high-frequency samples into
noncausal prediction targets. No correlation-based alignment is fitted.

Excitation consists of deterministic synthetic harmonic/plucked/noise probes
with varied input levels. Each channel receives the same 96-second training,
24-second validation, separate 24-second final-test and 8-second audition
signals, with disjoint seeds. They are not recorded instrument DI. Source
headers, executable and audio hashes are recorded; the transitive local DSP
headers are compared to the frozen release before capture.

Training uses a common 0.1 numerical target scale. Final files then receive a
fixed per-channel output calibration: the shared validation input through each
channel's actual TONE3000 POST EQ measures -20 dBFS RMS. This preserves input
drive and avoids relying on sine-based normalization to balance different
channel responses. The native reference receives exactly the same fixed gain
in all accuracy comparisons. Normalize is disabled; per-block/global In and
Out remain 0 dB (encoded as 0.5 by TONE3000), Mix is 100%, and the independent
global EQ/gate/pitch/spread are disabled. Playing dynamics and frequency
content can still change relative loudness. Add a cabinet IR after the amp.

## Build

Clone the pinned trainer and the target player's recursive submodules. Install
the trainer's dependencies in a virtual environment. Use the pinned trainer as
`PYTHONPATH` so an unrelated installed `nam` fork cannot be selected.

```sh
cmake -S Tools/Nam -B build-nam -DCMAKE_BUILD_TYPE=Release \
  -DCHIMERA_JUCE_SOURCE=/absolute/path/to/JUCE \
  -DCHIMERA_TONE3000_SOURCE=/absolute/path/to/tone3000-plugin
cmake --build build-nam --config Release --target ChimeraNamRender ChimeraTone3000Tool
```

Executable paths vary by platform; use their absolute paths below.

```sh
python Tools/Nam/capture_channels.py \
  --renderer /absolute/path/to/ChimeraNamRender \
  --out /absolute/path/to/new-data --train-seconds 96 --workers 4

PYTHONPATH=/absolute/path/to/neural-amp-modeler python Tools/Nam/train_a2.py \
  --data /absolute/path/to/new-data --out /absolute/path/to/new-run \
  --trainer /absolute/path/to/neural-amp-modeler --steps 5000 --threads 4 --batch 2

PYTHONPATH=/absolute/path/to/neural-amp-modeler python Tools/Nam/package_a2.py \
  --data /absolute/path/to/new-data --run /absolute/path/to/new-run \
  --out /absolute/path/to/new-package --tool /absolute/path/to/ChimeraTone3000Tool
```

Capture/package directories must be empty. `--resume` continues a training
checkpoint only when its dataset/config/loss identity matches. Checkpoints
include optimizer/RNG state and the best independently selected state for each
channel. Only load checkpoints produced by this trusted workflow.

## Verification

The official PackedWaveNet trains five independent models against five targets.
Equal block-diagonal convolutions use a grouped-convolution execution shortcut;
weights and export remain upstream-compatible. A comparison with upstream's
masked convolution found maximum output difference 2.98e-8 and gradient
difference 2.48e-10 in the numerical probe. The export extracts five independent
WaveNets through upstream's API, never a five-voice slimmable container.

Each final bias is corrected against the known zero-input steady state. The
final model is then revalidated; no per-clip gain or timing correction hides
errors. The official export/import roundtrip must match within 1e-6.

`ChimeraTone3000Tool` compiles the target player's unmodified `NamEngine`,
`BlockEq`, `PresetFile` and its pinned NAM core. It checks the same A2-shape
predicate used by the local-file import gate, compares raw C++/Python output,
measures silence, and creates/reads presets through the real preset writer.
EQ frequencies, gains, Q, roles and input/output values are checked after
serialization. TONE3000's normalization rule is reproduced with its -18 dB
default target, valid metadata range, and +/-12 dB adjustment clamp.

Accuracy targets remain median window ESR <=0.005, p95 <=0.01, worst <=0.02,
absolute RMS error <=0.5 dB, peak error <=1 dB, silence <=-80 dBFS and Python/C++
residual RMS <=-80 dBFS. Reports explicitly record failures. Smoke timing during
training is not a ten-minute dedicated real-time qualification. Actual GUI/DAW
loading, saved-session restoration, and recorded-DI listening remain separate
acceptance steps. No public model release or homepage update is performed by
this pipeline.

Upstream NAM and TONE3000 are their respective authors' projects. Their source
and dependency licenses apply; no affiliation or endorsement is implied.
