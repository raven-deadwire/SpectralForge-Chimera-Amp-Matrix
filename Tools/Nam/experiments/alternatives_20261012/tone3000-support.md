# TONE3000 compatibility audit — 2026-10-12

This source audit accompanies isolated alternatives experiments. It does not approve any model or change the accuracy thresholds. The baseline is PR #22 commit `859fea9bc49c4b9981c926a314655e485c51027c`.

## Decision

The selected official TONE3000 plugin accepts the fixed A2 architecture at its local-import boundary. Additional training, objective changes, sample weighting, and fitted weights can retain that architecture. Extended dilation, added layers or heads, and recurrent networks do not satisfy the current target's import contract.

An unchanged A2 model plus the plugin's native POST EQ or a separate native IR block is a different, potentially compatible **preset-chain** deliverable. It must not be described as long memory inside a portable single `.nam`. A static correction is useful only if the target's long-lived response can be factored into a stable linear subsystem; input-dependent sag/blocking and saturation after the linear subsystem may remain unmodeled.

## Pinned and current sources

| Component | Commit / observation |
|---|---|
| PR #22 TONE3000 plugin | `ef6f178ae1ac6412b55fec6f058d86e640e5aaaf` |
| PR #22 NAM Core | `1f42f88535884450104b8711d7595019afa0495b` |
| PR #22 trainer | `0072676419459f5d39e36f5b9fd4172f28d62cbf` |
| PR #22 JUCE | `d6181bde38d858c283c3b7bf699ce6340c050b5d` |
| Official plugin main at audit | `ab5e5ae4388753184687d67816815aa481f2cea9`, commit dated 2026-10-06, `VERSION` is `0.0.12` |
| Public TONE3000 WASM main at audit | `a6c895049771bacc40c74dfa19369c2ebf75cdb1`, dated 2026-08-13, core submodule `1f42f885...` |

The pinned and observed current plugin both use the same substantive local A2 shape gate. Source inspection does not establish which installed binary the user has, or which exact build is deployed on tone3000.com.

## Hosting, training, and playback are different contracts

The official API supports architecture filters `1`, `2`, and `custom`; packs may contain all three. Those fields prove catalog availability, not that the official plugin plays every architecture. The official plugin guide says A2-Full/A2-Lite only and explicitly excludes A1. The online trainer currently supports A2; the custom-architecture playground is described as future work.

Official references, accessed 2026-10-12:

- [TONE3000 Plugin Guide](https://www.tone3000.com/guides/tone3000-plugin), Quick Start and NAM A2 Size.
- [TONE3000 API](https://www.tone3000.com/api), Architecture, List Models, and Search Tones.
- [NAM A2 Complete Guide](https://www.tone3000.com/guides/nam-a2-the-complete-guide), architecture-pack compatibility and custom training sections.

## What the official desktop plugin actually accepts

[Pinned ProcessorModelLoader.cpp](https://github.com/tone-3000/tone3000-plugin/blob/ef6f178ae1ac6412b55fec6f058d86e640e5aaaf/plugin/src/ProcessorModelLoader.cpp), `namConfigIsA2()`:

- A bare `WaveNet` must pass `nam::wavenet::a2_fast::is_a2_shape(config)`.
- A `SlimmableContainer` must be nonempty and every child must recursively pass the same gate.
- Other architectures are rejected. The local-load error is `Only A2 NAM files are supported`.

[Current inspected ProcessorModelLoader.cpp](https://github.com/tone-3000/tone3000-plugin/blob/ab5e5ae4388753184687d67816815aa481f2cea9/plugin/src/ProcessorModelLoader.cpp) retains that contract. [Local-load tests](https://github.com/tone-3000/tone3000-plugin/blob/ab5e5ae4388753184687d67816815aa481f2cea9/test/src/local_load_tests.cpp) explicitly reject an LSTM file at import.

[Pinned a2_fast.cpp](https://github.com/sdatkinson/NeuralAmpModelerCore/blob/1f42f88535884450104b8711d7595019afa0495b/NAM/wavenet/a2_fast.cpp), `is_a2_shape()` starting at line 754, requires one layer array, mono input/conditioning, channels=bottleneck in {3,8}, exact 23-layer kernel and dilation arrays, LeakyReLU slope 0.01 throughout, no gating, no post-stack head, no FiLM/grouping, and a one-output 16-tap layer-array head. Changing temporal structure fails this gate.

[Pinned a2_fast.h](https://github.com/sdatkinson/NeuralAmpModelerCore/blob/1f42f88535884450104b8711d7595019afa0495b/NAM/wavenet/a2_fast.h) defines those fixed arrays. The full model receptive field includes the 16-tap head:

`R = 1 + 3*5*(1+3+7+17+41+101+239) + 14*(1+13) + 15 = 6347 samples`.

At 48 kHz this is 132.229 ms of past-input context, not 132 ms of processing latency. The guide's 6,332-sample illustration omits the head's additional 15 samples; it must not replace the code-derived full-model value in the experiments.

Lowering the training sample rate is not a transparent workaround for this target. The pinned plugin's loader explicitly runs models at the 48 kHz chain base rate and only logs unexpected model sample-rate metadata; it does not add per-model resampling. A 24 kHz-trained model would therefore not obtain correctly timed 264 ms context merely by changing its `.nam` metadata. Multirate inference requires a separately supported processing path and timing validation.

### Loader success is insufficient for arbitrary extension fields

[Pinned model.cpp](https://github.com/sdatkinson/NeuralAmpModelerCore/blob/1f42f88535884450104b8711d7595019afa0495b/NAM/wavenet/model.cpp) has a generic `condition_dsp` parser, but `create_config()` selects the A2 fast implementation before generic parsing. The current `is_a2_shape()` does not explicitly reject a `condition_dsp` field. Consequently such a field may be accepted syntactically and ignored by fast inference. Do not treat added metadata, a load success, or generic Core support as proof that correction executes. This is not an approved way to bypass the target contract.

[Pinned container.cpp](https://github.com/sdatkinson/NeuralAmpModelerCore/blob/1f42f88535884450104b8711d7595019afa0495b/NAM/container.cpp), `ContainerModel::process()`, dispatches to the one active submodel. `SlimmableContainer` selects a size/tier; it is not an arbitrary serial or parallel DSP graph.

## Supported native POST EQ candidate

[BlockEq.h](https://github.com/tone-3000/tone3000-plugin/blob/ef6f178ae1ac6412b55fec6f058d86e640e5aaaf/plugin/include/BlockEq.h) and [BlockEq.cpp](https://github.com/tone-3000/tone3000-plugin/blob/ef6f178ae1ac6412b55fec6f058d86e640e5aaaf/plugin/src/BlockEq.cpp) implement six stateful RBJ biquads. The normal position is after the NAM model on its wet path and before dry/wet mixing; `pre=false` selects this position.

| Parameter | Supported range / role |
|---|---|
| Frequency | 20–20,000 Hz |
| Gain | −15 to +15 dB |
| Q | 0.1–10 |
| Band 0 | Low cut or low shelf |
| Bands 1–4 | Bell |
| Band 5 | High cut or high shelf |

Coefficients are normalized to float32 and use transposed direct-form II state. The pole family is constrained; these controls do not expose arbitrary IIR coefficients. The Bell formula uses `A=10^(gainDb/40)` and `alpha=sin(omega)/(2Q)`, so its denominator is proportional to `[1+alpha/A, -2cos(omega), 1-alpha/A]`.

[ProcessorState.cpp](https://github.com/tone-3000/tone3000-plugin/blob/ef6f178ae1ac6412b55fec6f058d86e640e5aaaf/plugin/src/ProcessorState.cpp) serializes each block's `Eq` tree and restores its position and bands. [PresetFile.cpp](https://github.com/tone-3000/tone3000-plugin/blob/ef6f178ae1ac6412b55fec6f058d86e640e5aaaf/plugin/src/PresetFile.cpp) provides the native `.t3kpreset` framing. These are suitable components to test without modifying upstream code.

Possible experiment: determine a stable linear post-filter from training data or the frozen native teacher, fit it within these exact controls, train A2 against the inverse-filtered target, and evaluate the composed output independently. Report raw A2 and composed-chain metrics separately. A resonator outside the supported pole family may prevent exact factorization even though the EQ itself is a supported feature.

## Supported native IR candidate and its caveats

The pinned `ProcessorModelLoader.cpp` supports mono/stereo `.wav` IRs with a 10-second cap on loaded length and native uniform/nonuniform convolution. A longer FIR approximation may express a linear tail beyond the EQ family's limits. This still requires an external IR and preset chain. This audit inspected the IR source; it did not execute or qualify that complete IR loader/convolver path.

The actual loader applies `Trim::yes`; its short/long threshold is one second. Catalog gear tags override the length fallback for the audible classification: `cab` means cabinet-like, `space` means reverb-like, and other/absent tags use length. Cabinet-like IRs receive a −18 dB output pad and default to full wet; reverb-like IRs have no such pad and default to 50% mix. The uniform/nonuniform engine choice remains based on length. Energy normalization also exists. Accurate use requires explicit full-wet routing, controlled normalization/gain, and actual loaded-convolver output verification. Merely saving an impulse-response WAV is not enough to establish amplitude or tail accuracy.

## Public web engine is not the desktop target

[Public WASM loader](https://github.com/tone-3000/neural-amp-modeler-wasm/blob/a6c895049771bacc40c74dfa19369c2ebf75cdb1/wasm/nam-engine.cpp) calls generic `nam::get_dsp(JSON)` and does not contain the desktop local A2 gate. It also applies a 10 Hz output DC blocker and loudness normalization. Bare-core accuracy does not automatically equal web-player accuracy.

The public repository is an inspectable implementation, not proof of the current production web build, catalog admission, custom-architecture playback, or compatibility with the official desktop plugin. No hosted production playback or installed DAW acceptance is claimed by this audit.

## Candidate classification

| Candidate | Current official plugin path | Long-memory claim |
|---|---|---|
| Refine A2 weights / objective / sampling | Retains fixed A2 shape; render validation still required | Cannot create input history beyond the fixed receptive field |
| Fit final A2 head weights | Retains architecture if only weights change | Cannot cure the finite-history floor |
| Longer training crops / warmup | Retains architecture | May improve optimization; does not extend receptive field |
| Lower-rate training with only sample-rate metadata changed | Target has no per-model rate conversion | Does not provide correct-time extended context in this player |
| Extended dilation/layers/head | Local A2 import rejects changed shape | Custom Core experiment only |
| LSTM or recurrent conditioner | Not an approved official-plugin import path | Structural alternative; do not claim compatibility |
| A2 plus native POST BlockEq | Supported components in a preset chain | Potential for factorable linear tails inside the constrained filter family |
| A2 plus native external IR | Supported components in a preset chain | Finite long-memory linear correction, subject to actual loader transformations |
| Arbitrary IIR JSON field / custom container | No supported execution contract established | Excluded from compatible candidate claims |

No accuracy threshold has been changed. Loading, format roundtrip, engine parity, acoustic accuracy, five-channel character preservation, and human listening remain separate verdicts. A local engine-component harness is not full plugin/DAW acceptance and does not approve publication.

## Local build verification in this experiment

The unmodified pinned sources were checked out into isolated dependency folders. The existing `Tools/Nam/CMakeLists.txt` built `ChimeraTone3000Tool` and `ChimeraNamRender` in Release mode with `NAM_ENABLE_A2_FAST=1`. No upstream source was edited.

```sh
cmake -S Tools/Nam -B /workspace/scratch/e8564bd8a57f/build/nam-tools -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCHIMERA_JUCE_SOURCE=/workspace/scratch/e8564bd8a57f/deps/JUCE \
  -DCHIMERA_TONE3000_SOURCE=/workspace/scratch/e8564bd8a57f/deps/tone3000-plugin
cmake --build /workspace/scratch/e8564bd8a57f/build/nam-tools \
  --target ChimeraTone3000Tool ChimeraNamRender --parallel 3
```

All eight native teacher source-header hashes match the frozen capture manifest; see `native-renderer-source-hashes.json`. Each channel's first four seconds of immutable training audio was re-rendered with its original manifest settings to check the rebuilt native renderer. All five outputs are bit-exact to the archived targets (maximum absolute error zero). This small reproducibility check does not repeat a full training experiment or measure model accuracy.

`native-tool-build.json` records dependency pins, binary SHA-256 values, the five reproducibility results, and scope limitations. The console target uses real upstream `NamEngine`, `BlockEq`, and `PresetFile`; it is not the entire installed plugin or DAW host. Subsequent candidate renders must provide their own model hashes, shape checks, numerical parity and acoustic evaluations.

### Executed import-boundary probes

`gate-probe.py` and `gate-probe.json` record a 12,000-sample nonzero synthetic input rendered through the actual pinned A2 detector and engine in `ChimeraTone3000Tool`. The full `ProcessorModelLoader` and UI were source-inspected, not executed by this console test. All five expected outcomes were observed:

| Probe | Observed outcome |
|---|---|
| Unchanged archived Fenrir A2 | Accepted; finite, nonzero output |
| Last dilation increased by one | Rejected at A2 gate; no output file |
| Head kernel 16 changed to 32 | Rejected at A2 gate; no output file |
| Architecture marker changed to LSTM | Rejected at A2 gate; no output file |
| Invalid, unregistered `condition_dsp` payload added | Accepted, but output bit-exact to unchanged model |

The final diagnostic confirms that this extension field is not constructed or executed by the A2 fast path. It must **not** be reported as compatible recurrent correction. The artifact is a deliberately invalid probe and is not a candidate model or deliverable. The test records all model/input/binary hashes and makes no accuracy or publication approval.

### Native EQ-only helper for the factorization experiment

`BlockEqProbe.cpp` processes raw float32 audio through the actual pinned `BlockEq` independently of a NAM model. It uses only the supported six-band settings. For every invocation it also writes the native EQ `ValueTree` to a binary memory stream, restores it through the actual native deserializer, and requires identical effective parameters and bit-exact output. The resulting audio comes from the restored instance.

`build-eq-probe.py` compiles only that new wrapper and links the already built BlockEq/JUCE object files. It does not modify or rebuild a whole plugin. `eq-probe-build.json` identifies all reused object hashes and build arguments. `eq-probe-smoke.json` records a short nonzero input through the Fimbulvetr experimental settings, confirming that filtering runs and the native EQ serialization roundtrip preserves it exactly. The memory experiment's separate parity and reconstruction reports determine whether those legal settings are acoustically useful.
