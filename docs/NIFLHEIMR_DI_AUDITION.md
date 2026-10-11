# Niflheimr reproducible DI audition

This protocol prepares controlled listening, not musical acceptance. It does not
edit `release_approved`, release gates, DSP or original preset definitions.
A successful render/contract test leaves listening **PENDING**. Synthetic fixtures
prove routing/tool integrity only. No actual bass DI or licensed bass IR was
found or supplied for this implementation.

## Build and connect inputs

Requires CMake/JUCE build dependencies, Python 3.9+ and NumPy (CI: 2.2.6).

```sh
cmake -S . -B build-audition -DCMAKE_BUILD_TYPE=Release -DCHIMERA_ENABLE_NIFLHEIMR_MEASUREMENTS=ON
cmake --build build-audition --config Release --target ChimeraNiflheimrRender ChimeraNiflheimrRigRender --parallel 2
ctest --test-dir build-audition -C Release -R 'ChimeraNiflheimr(Audition|Renderer).*Tests' --output-on-failure --no-tests=error
```

Copy `Validation/audition/niflheimr-di-template.json` to a personal config. Set
`di` and `ir` to absolute paths or paths relative to the config. These inputs
remain external; do not commit unlicensed IRs or personal recordings.
The template timeline is a recording checklist, not evidence that any recording
exists. Edit segment bounds to the actual take before rendering.

Prepare the missing-input plan now:

```sh
python Tools/audition_niflheimr.py --config Validation/audition/niflheimr-di-template.json --output audition-plan --plan
```

With DI and IR connected (Windows binaries have `.exe` suffixes):

```sh
python Tools/audition_niflheimr.py --config my-audition.json --output audition-take-01 \
  --head-renderer build-audition/ChimeraNiflheimrRender_artefacts/Release/ChimeraNiflheimrRender \
  --rig-renderer build-audition/ChimeraNiflheimrRigRender_artefacts/Release/ChimeraNiflheimrRigRender
```

Use a fresh output path. The coordinator rejects missing/silent/invalid inputs,
input overload, sample-rate mismatch, changed hashes, truncated/short output,
nonfinite audio and renderer failure. It keeps failed staging directories and
logs. No retries silently replace a failed render.

## Record once, render every voice

Record mono, dry bass DI without amp/cab, gate, compressor, EQ, pitch shift or
normalization. Keep instrument volume/tone/pickup selection, tuning, string age,
playing technique, interface and hardware gain in a take note. Do not change
interface gain between passages. Use 48 kHz PCM16/24/32 or float32 WAV; the common
IR must already be at the same rate and meet the production import limits
(8 samples to 1 second, maximum 4 MB). No hidden resampling is applied. Maximum
input duration is ten minutes. Avoid clipping at acquisition.

Apply `input_gain_db` **once** to the DI, then write the immutable prepared float
WAV. Every voice/revision receives these exact bytes. A shared 250 ms leading
silence settles initial routing/control ramps before the first note. The renderer
adds two seconds of tail. Reported host/amp latency and leading silence are
removed from audition copies; raw render WAVs preserve the complete timeline.
This is integer reported-latency alignment, not phase-response equalization.

## Comparison scopes

| Output group | Signal path | Conditions |
|---|---|---|
| `head-only` | Existing `ChimeraNiflheimrRender` / production Amp | Identical fourteen native controls and oversampling; no PRE, IR or POST. |
| `head-cab` | Production processor, neutral PRE/POST/gate, native amp, common IR | Identical controls; AMP IN/OUT and global INPUT/OUTPUT at 0 dB; common cabinet. |
| `preset-cab` | Production processor, original factory rig 43–47, common IR | Authored PRE, amp settings/output trims, POST and gate retained; cabinet replaced for controlled comparison. |

The last group is a **common-cabinet variant of the full preset**, not its original
filters-only cabinet. Every lane uses the same IR, production JUCE IR normalization,
no IR trimming, and common 20 Hz/20 kHz cabinet cuts. IR loading occurs before
`prepareToPlay`, which synchronously installs kernels. It is not a convolution
added after POST. The same common cabinet policy applies to `head-cab` and
`preset-cab`. Original `.chimera` rigs are not modified. Resolved comparison rig
states are saved alongside WAVs for inspection and recall.

`controls` are normalized 0–1 except `mid_frequency` (Hz). The template fixes all
fourteen controls explicitly; preset-cab intentionally uses the authored settings
instead. Compare voices within a group before drawing conclusions across groups:
PRE compression, gate and POST can conceal or amplify a head-only problem.

## Listening material and acceptance record

| Segment | Focus | What to check |
|---|---|---|
| Fast repeat | Garmr death/grind; all voices | 160–220 BPM low-string sixteenths and hard stops. Distinguishable successive notes, stable pick click, gaps remaining audible, no growing low-mid blanket. |
| Low interval | Ymir slam/brutal death | Low B/A note changes, rests, string crossings and octave jumps. Weight returns quickly; previous fundamental/low mids do not obscure the next note. |
| Attack dynamics | All voices; Hrímfaxi/Nidavellir anchors | Soft/hard isolated notes, separate pick/finger takes if useful. Attack survives distortion, dynamics remain useful; avoid mistaking extra treble for definition. |
| Sustain/rest | Hel sludge/doom | Long notes followed by rests and a new low note. Preserve dark mid-body, saturation and intentional slow response while preventing uncontrolled sub build-up. Fast-repeat scoring alone must not erase Hel's character. |

Use the same monitor chain and listening volume, first solo and then against a
fixed drums/guitar backing track at unchanged levels. Evaluate sustained body and
next-note separation independently. Hrímfaxi and Nidavellir provide accepted
character anchors; they are not an automatic pass threshold for the other genres.

For every segment and output group, RMS-match the five clips (or ten with a
baseline). The target defaults to -24 dBFS; if any matched peak exceeds -3 dBFS,
apply additional **common attenuation to the entire group**. No limiter or input
normalization is used. This matches segment energy, not perceived LUFS loudness;
listen at comfortable fixed volume and record residual perceived imbalance.
Do not cross-compare separately matched segment loudness as a dynamics test.
Raw WAVs retain gain/dynamics. Use those for absolute level/dynamic comparisons.

`listen/<segment>/<scope>/sample-NN.wav` uses a reproducible shuffled order.
Fill `listening.csv` with clarity, attack, low separation, genre character,
sustain (1–5) and explicit notes before opening `report.json`, which contains the
identity key. The filename alone does not expose channel/revision. Change
`blind_seed` for a fresh session. This is blinded ordering, not an ABX significance
claim. Rate each clip and then compare any chosen A/B pair in the same group.

The report records input/IR/configured-source/binary hashes, source fingerprints,
controls or resolved rig states, latency, matching gain, RMS/peak, crest factor and
band power fractions. Crest/band metrics are descriptive and **cannot establish
that muddiness is fixed**. Human verdicts stay in the listening worksheet; the tool
never promotes those to release approval. An incomplete take, missing passages or
no trusted musical listener leaves acceptance pending.

## Genuine before/after A/B

Build the pre-voicing source in a separate checkout and port only these test/tool
files to that checkout. Preserve that baseline's DSP/preset sources. Supply its
binaries with `--baseline-head-renderer` and `--baseline-rig-renderer` in the same
command. Each segment/group then contains five old and five current clips with
shared level matching and randomized identity. Both render manifests and binary
hashes are saved. Missing either baseline binary is rejected.

Use the same compiler, build flags, sample rate, block size, oversampling and
config for both builds. Baseline source must be recorded by its build manifest;
a current-source renderer is not a pre-fix reference merely because it is labelled
baseline. If no compatible baseline is available, report a current five-voice
comparison only; do not claim an improvement or Garmr/Ymir/Hel PASS.
