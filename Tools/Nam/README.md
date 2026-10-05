# Náströnd — five TONE3000 A2 captures

Internal offline production tools for five independent Náströnd channel models,
plus TONE3000's own six-band parametric EQ presets. These tools are not part of
Chimera's shipping interface. `CHIMERA_BUILD_NAM_TOOLS` defaults to `OFF`.

The current deliverable is a **test build**, pending the accuracy and listening
checks recorded in its reports. It is not a release approval. The original
single parametric-NAM experiment cannot satisfy the specified TONE3000 player:
that player's A2 import and playback path does not expose additional model
conditioning controls. One file per channel does not remove the user's
parametric-model requirement. That requirement remains unsupported here.

**Target decision:** after the [finite-memory audit](ACCURACY-BLOCKER.md),
the user chose to retain TONE3000 compatibility. `quality_profile.py` keeps the
original active-audio ESR limits and evaluates very quiet decay using absolute
residual limits. This is a project policy, not an official TONE3000 certification.
The v0.2 files fail the revised policy as well; no release approval is implied.

## Frozen sources

| Component | Revision |
| --- | --- |
| Chimera native DSP | `d93560445c78552768a0ed592e5ec62c3c648fb6`, v1.2.0-beta.1 |
| JUCE | 8.0.8 |
| sdatkinson/neural-amp-modeler | `0072676419459f5d39e36f5b9fd4172f28d62cbf` |
| tone-3000/tone3000-plugin | `ef6f178ae1ac6412b55fec6f058d86e640e5aaaf` |
| TONE3000's NAM core | `1f42f88535884450104b8711d7595019afa0495b` |
| Python runtime used | Python 3.12, PyTorch 2.8.0 CPU |

The target plugin's local-import gate accepts only A2 shapes. A legacy A1
WaveNet or a ConcatWaveNet parametric fork is not a substitute. Each exported
file is the official 8-channel A2-Full WaveNet (23 layers, 12,145 weights,
6,347-sample receptive field at 48 kHz). There is no separately trained Lite
tier in these files. TONE3000 explicitly accepts a bare A2 WaveNet.

The target player's six-band POST EQ is external processing, not an
implementation of internally parametric NAM. Each `.t3kpreset` embeds its NAM bytes and its EQ settings;
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

The legacy trainer uses a common 0.1 numerical target scale. The optional
`--recipe official-a2` instead computes each channel's -18 dBFS RMS gain from
**training output only**, and undoes it on export. Neither option changes input
drive, the captured target, the frozen evaluation scale, or the player's
Normalize setting. The historical v0.2 package received a
fixed per-channel output calibration: the shared validation input through each
channel's actual TONE3000 POST EQ measures -20 dBFS RMS. This preserves input
drive and avoids relying on metadata-based normalization to balance different
channel responses. The native reference receives exactly the same fixed gain
in all accuracy comparisons. Normalize is disabled; per-block/global In and
Out remain 0 dB (encoded as 0.5 by TONE3000), Mix is 100%, and the independent
global EQ/gate/pitch/spread are disabled. Playing dynamics and frequency
content can still change relative loudness. Add a cabinet IR after the amp.

## Controlled recipe refinement and recovery

The current local refinement data has **480 seconds of training**; its
validation/test/audition audio hashes are unchanged from the original capture.
The default CLI/CI capture remains 96 seconds. Do not substitute the held-out
splits or official NAM V3 evaluation recording for training data.

`a2_recipe.py` uses the pinned trainer's actual MRSTFT implementation: sum each
independent channel's MSE plus 0.0005 MRSTFT, with no DC term or gradient
clipping. Adam starts at 0.004 with weight decay 3.17e-7. The 0.994 decay is
applied per virtual epoch, **not per optimizer step** (702 updates for this
480-second dataset, batch 4, 8192-frame crops). The comparison keeps seeded
random crops identical between arms. This sampler, batch size, and fixed
silence-bias correction are project adaptations; this is not a claim to have
replicated every part of TONE3000's cloud training service.

A controlled warm start imports the same channel states into both arms, resets
both optimizers, and rescales each final linear head to preserve physical raw
output under the new numerical gain. `initialization.json` records the source
checkpoint hash, gains, initial validation ESR, and measured output delta.
`crop_schedule_sha256` records the actual training crop sequence for each stage.
The new recipe's identity includes its loss and scheduler-defining options;
changing these requires a new, explicit warm-start run. The existing recipe
remains available for comparison and legacy checkpoint recovery.

Compare **current-state** `validation-step-N.json` as well as best-selected
`validation.json`. A retained initial model is not evidence that additional
training improved that channel. Recipe comparison changes normalization, loss,
gradient clipping, and learning-rate schedule together; it cannot isolate one
of them as the cause of an improvement or regression.

```sh
PYTHONPATH=/absolute/path/to/neural-amp-modeler python Tools/Nam/continue_a2.py \
  --data /absolute/path/to/data-v03 --run /absolute/path/to/new-official-run \
  --trainer /absolute/path/to/neural-amp-modeler \
  --warm-start /absolute/path/to/frozen-comparison-source \
  --recipe official-a2 --tail-fraction .05 --chunk 500 --max-stages 1
```

Run the legacy arm in a different directory with `--recipe legacy`, using the
same warm-start source, crop options, and update count. A bounded coordinator
invocation now defaults to **one verified stage**. Before `PAUSED_AFTER_STAGE`,
it checks fresh checkpoint/validation/export evidence, selected steps and model
hashes. Preserve the checkpoint and reports before another invocation. The
child inherits the nonblocking `fcntl` lock; `RUNNING` text or a PID alone is
not liveness evidence. Caught termination is `INTERRUPTED`; unexplained process
loss keeps its cause unknown. Unique log names preserve interrupted attempts.
`--max-stages 0` explicitly restores the older continuous loop, but starting a
background process is never proof that it will survive the execution session.

`--learning-rate` provides an explicit experimental override. For example,
`--recipe official-a2 --learning-rate .001` retains the upstream objective and
normalization while testing a lower warm-start learning rate. It is not the
upstream default. Use a separate directory and the same frozen source/crops.
`summarize_recipes.py --comparison <directory>` verifies the completed legacy
and official-a2 evidence before writing a report. `--candidate official-a2-lr001`
compares the named lower-LR arm against legacy and writes separate reports.

Loss/gradient parity with upstream, normalization/export invariance and the
bounded coordinator's failure paths are covered by `test_a2_recipe.py` and
`test_continue_a2.py`. Passing these checks is software verification only;
`quality_profile.py`, independent TONE3000-engine validation and listening
requirements still apply unchanged.

## GitHub Actions evidence

`NAM exact-source validation` (`.github/workflows/nam-validation.yml`) runs on
every PR at the **PR head SHA**, not the synthetic merge ref. The standalone
`Tools/Nam` CMake project explicitly builds both `ChimeraNamRender` and
`ChimeraTone3000Tool`; the product's default `CHIMERA_BUILD_NAM_TOOLS=OFF` is
unchanged. JUCE, trainer, player and NAM core commits are locked in
`ci-sources.json`, including verification of all recursive gitlink checkouts.
The renderer's transitive DSP headers must still match the frozen release.

PR smoke uses two seconds per split and two optimizer steps for all five
**full-size A2** models. It checks the grouped-convolution output/gradients
against upstream, native captures, official NAM export/import, the real
TONE3000 A2 gate/engine, six-band POST EQ, preset serialization, and playback
from the preset's embedded NAM/EQ after restoration. A passing smoke means
the pipeline is compatible; its deliberately untrained models can fail all
fidelity targets. Accuracy is reported separately, never marked approved.

For the expensive path, select **Run workflow → profile: full** (the manual
Actions UI becomes available once this workflow exists on the default branch),
select the intended ref, and set optimizer steps, default 5000. This uses the
96/24/24/8-second capture protocol and a bounded Ubuntu CPU job. It starts
fresh and preserves the optimizer/RNG/best-state checkpoint for later trusted
local resume. It does not launch automatically, allocate a paid GPU or alter
the current `quality_profile.py` gate. Historical all-relative measurements
and the [A2 memory audit](ACCURACY-BLOCKER.md) remain diagnostic evidence.
The full job fails if any validation/held-out numerical target fails.

Each run uploads one `NAM-{profile}-{full SHA}-{run id}-{attempt}` evidence
artifact with 30-day retention, including on failure:

- Exact pipeline/dependency trees, capture headers, renderer/tool hashes,
  CMake configuration, resolved Python environment and build/run logs.
- All captured inputs/targets and audio hashes, training configuration,
  stages, checkpoint, official roundtrip results and five final `.nam` files.
- Five embedded `.t3kpreset` files, validation/comparison JSON, synthetic
  native/NAM stereo A/B and target-EQ audio.
- `acceptance.json`, a complete file hash manifest and `SHA256SUMS`.

Setup or execution failure preserves **partial** evidence and remains FAIL;
missing outputs cannot pass. Source/audio mismatch aborts acceptance. The
held-out synthetic split is evaluated only after checkpoint selection and
export, using the current profile's frozen v0.2 reference scale and thresholds. No
recorded instrument DI is represented by these synthetic signals.
`release_approved` is always false; `instrument_DI_listening` and
`GUI_DAW_acceptance` stay **BLOCKED**, even if all numerical checks pass.
The workflow has read-only repository permissions and publishes no models.

## Local build

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

Accuracy for active windows remains median ESR <=0.005, p95 <=0.01 and worst
<=0.02. A window is active when the **reference** RMS is at least -60 dBFS
under the frozen v0.2 per-channel output scale. Quieter windows require residual
RMS <=-70 dBFS and residual peak <=-60 dBFS. The RMS residual must therefore
stay at least 10 dB below the active/quiet boundary, while the peak condition
catches brief excursions. These are numerical production tolerances, not
claims of perceptual inaudibility. The window is 4,096 samples; skipping the
first 6,347 samples is unchanged. Every quiet window is assessed, including
true zero input; none is silently discarded.

Full-signal ESR must also be below 0.01, applying the excellent-result range
in [TONE3000's capture guide](https://www.tone3000.com/guides/capture-your-gear-with-nam-dry-wet)
to our separately defined validation and independent inputs. This is not a
claim that our test input or metric aggregation is the service's exact test.

The anchor is independent of candidate loudness and final EQ level matching:
turning down a candidate cannot move reference windows into the quiet class.
Global RMS error <=0.5 dB, peak error <=1 dB, steady silence <=-80 dBFS and
Python/C++ residual RMS <=-80 dBFS also remain required. Reports explicitly
record both the historical all-relative metrics and the current profile.
Smoke timing during
training is not a ten-minute dedicated real-time qualification. Actual GUI/DAW
loading, saved-session restoration, and recorded-DI listening remain separate
acceptance steps. No public model release or homepage update is performed by
this pipeline.

## Refinement and independent evaluation

The refinement trainer supports an explicit learning-rate restart and records
batch size, supervised frames and schedule per stage. Existing model/data
identity checks still apply. Preserve the original checkpoint and its exported
models so the next build can be compared against the delivered baseline.
The finishing stage can use `--loss-normalization channel` for mean squared
error with a fixed per-channel energy scale, rather than reweighting every
excerpt by its own energy. `--validation-selection full` evaluates the full
validation signal with the same zero-input bias centering used at export.
Changing selection metrics re-scores stored checkpoints before comparing them.

`--warm-start /path/to/prior-run` starts a new optimizer from the prior run's
best channel weights. It requires the same architecture, numerical target
scale and frozen native source, and re-scores all initial weights on the new
run's validation input. Its checkpoint hash is recorded. This is distinct from
`--resume`, whose dataset identity must match. A larger training capture can
therefore reuse learned weights without pretending it is the original run.
`--tail-fraction` optionally reserves part of training for the actual captured
final decay; it never manufactures mismatched input/target pairs.

For sustained refinement in the Linux production workspace, `continue_a2.py`
runs bounded stages under an exclusive file lock, with atomic progress and
checkpoint writes. It stops for independent evaluation when all validation
fidelity criteria pass, for review on errors or less than 1% improvement across
five stages, or at its explicit step budget. It never changes acceptance
limits or automatically releases models. A caller must complete engine and
independent-input tests after `READY_FOR_INDEPENDENT_TESTS`.

```sh
PYTHONPATH=/absolute/path/to/neural-amp-modeler python Tools/Nam/continue_a2.py \
  --data /absolute/path/to/480-second-data --run /absolute/path/to/longrun \
  --trainer /absolute/path/to/neural-amp-modeler \
  --warm-start /absolute/path/to/prior-run --max-steps 60000 --chunk 2000
```

The 480-second training capture keeps the original validation, final-test and
audition input/target hashes unchanged. Reusing old weights is recorded as a
warm start; optimizer steps restart from zero for this larger-data run.

`capture_probe.py` renders an independently sourced input with the exact
renderer hash and channel settings used by the training capture. The NAM
project's official `v3_0_0.wav` is available from its GUI's documented download
link; its published strong MD5 is `36cd1af62985c2fac3e654333e36431e`.
For this refinement the entire 190-second input is evaluation-only. It is not
used for training, optimizer changes, checkpoint selection or level matching.
Do not distribute third-party input audio as part of this pack.

`compare_a2.py` compares the two frozen exported model sets on the reserved
24-second synthetic test and the external V3 input through the actual target
engine. It reports ESR, window distributions, level/peak errors and four
frequency-band level errors without fitted gain or time alignment. External
signal sections are reported separately. This is numerical evaluation, not
a claim that recorded-instrument listening or host GUI acceptance passed.

`history_probe.py` constructs two different past histories followed by the
same input to measure one finite-memory representation bound. Its result is
specific to that constructed pair and must not be generalized to other
inputs or used to excuse a failing fidelity measurement.

Level calibration here means a fixed digital RMS balance. It is not a measured
hardware input/output dBu calibration. Playback checks use 48 kHz and no
additional player oversampling; the captured source itself includes native
4x oversampling.

Upstream NAM and TONE3000 are their respective authors' projects. Their source
and dependency licenses apply; no affiliation or endorsement is implied.
