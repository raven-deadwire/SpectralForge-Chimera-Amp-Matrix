# Auto Set Phase 6: DI regression protocol

This is an engineering regression path, not a listening approval. The checked-in
Transpose manifest still contains **nine missing recordings**. No real DI or
listening result is supplied by this change. Starting detector thresholds remain
provisional; do not tune a threshold after seeing a held-out recording.

## Reuse of the November 2 takes

Use `Validation/transpose-di-take-template.json` from PR #198 without changing its
Transpose labels, expected frequencies or acceptance criteria. All nine takes
(A1 guitar, B0 bass, A0 bass × native 44.1/48/96 kHz) are usable. Record each rate
independently, dry mono, preferably 24-bit PCM; do not normalize, resample, gate,
compress, transpose or bake an amp/cab into the DI. Keep interface gain fixed.
The existing Transpose `plan` must pass its own input/label checks before use.

| Existing segment | Phase 6 purpose |
| --- | --- |
| dynamics | Ordinary common-envelope loud/soft playing must not be treated as a time-varying spectral resonance |
| steady-open / steady-fret5 | Preserve A0/B0 fundamentals and natural harmonic sustain; measure before/after at f0, 2f0, 3f0 |
| riff / long | Repeated attacks, palm mutes, and note tails; inspect false positives and release |
| ghost / vibrato | Optional scratch/vibrato stress cases; not a separate proof of resonance origin |

The existing low-string material is **insufficient to establish that a peak is a
cabinet resonance**. If possible append 8–10 seconds of evenly played pitches
across at least one octave to each take, after the existing sections. Keep the
original 63 Transpose segments unchanged. These additional notes help determine
whether the suspicious frequency stays fixed as f0 moves. A fixed-frequency peak
may still come from the instrument or amp; the detector does not classify its
physical source.

## Amp/cab captures

`ChimeraAutoSetTests --dynamic-eq-capture DI.wav PROFILE.json OUTDIR` renders the
actual `ChimeraProcessor::processBlock` path and drains its production `Tap::tone`
after every block. This is after the rig and post-rig gates, before Tone EQ. It
waits for the real cabinet worker; an unsettled cabinet is an error. Capture is
mono channel average with duplicated mono input, Classic routing, block 256,
4× oversampling. Stereo/anti-phase, Dual/Matrix and live-host acceptance remain
separate; mono averaging is not equivalent to the stereo max-channel detector.

Profiles in `Validation/auto-set-phase6/` pair:

- Tight 515 native amp without CAB and the same amp through bundled V30/SM57.
- Metro Clean native bass amp without CAB and the same amp through a **user-
  supplied, licensed, mono bass IR**. The latter remains missing until `ir` and `ir_provenance` are set.

Each profile fixes input gain at 0 dB and native amp output trim at −18 dB (capture
headroom after amp distortion, applied equally to both sides of each pair),
disables gates, PRE drive and Transpose, and
bypasses static EQ and later dynamics. The complete resolved native parameters,
reported/tap latency and cabinet readiness are saved alongside `pre-tone.wav`.
For a further high-gain control, pair a sustained note through the amp with CAB
off/on at identical gain; do not compare unrelated presets or independently
normalized renders. Preserve IR bytes/hash, source/licence, model/channel,
controls, OSR, sample rate, block size, source SHA and renderer hash. Changing a
profile requires a fresh output directory and a new complete paired run.

## Reproducible execution

Build the existing native target (no new product dependency):

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCHIMERA_BETA_QUALIFICATION_ONLY=ON
cmake --build build --config Release --target ChimeraAutoSetTests --parallel 2
python -m pip install numpy scipy
python Tools/AutoSet/di_regression.py plan --takes /absolute/session/takes.json --spec /absolute/session/spec.json
```

Copy the takes manifest, spec and profiles to a private session directory. Fill
actual WAV/label paths and reviewed metadata. In the spec's `windows`, replace
null for each take with at least these four entries, using actual reviewed times:

```json
[{"segment":"riff","start_s":1.0},
 {"segment":"dynamics","start_s":21.0},
 {"segment":"steady-open","start_s":62.5},
 {"segment":"steady-fret5","start_s":70.5}]
```

These example times are **not recording labels**. Each window must fit six
seconds inside its actual source segment. Do not select favorable excerpts after
looking at detector output. The tool offsets window starts by the Tone tap latency (reported processor
latency minus the later POST FX latency); input and after-EQ WAVs then share exactly the same time base. Leave
sufficient tail in the source for that offset.

```sh
python Tools/AutoSet/di_regression.py run --takes /absolute/session/takes.json --spec /absolute/session/spec.json --renderer /absolute/build/ChimeraAutoSetTests_artefacts/Release/ChimeraAutoSetTests --out /absolute/session/results-001
```

These commands open no editor window and can run headlessly on Linux; if the
local JUCE backend requires a display, provide one (for example Xvfb). On Windows
use the `.exe` target. `plan` returns exit 2 for missing/invalid inputs, **not PASS**.
`run` refuses missing input and an existing output directory. It records hashes
of DI, labels, profiles, IR, renderer, script, spec, captures and analysis reports.
It does not modify source DI or Transpose labels and never sets listening complete.

For each six-second capture window the actual Controller runs Phase 6 alone
(Tone step off; input, gate and compressor locked), then the production
GraphicalEQ renders its assigned nodes. Four strengths (25/50/75/100%) get separate
before/after WAVs, applied parameters, UNDO parameters and peak reduction meters.
The bass safety guard is no more than **0.5 dB additional f0 loss** in steady and
dynamics windows (6th-order band measurement ±6%, first 250 ms excluded). This is
an initial engineering regression guard, not a musical acceptability guarantee.
A guard PASS is not proof that all harmonics are unaffected: inspect the recorded
2f0/3f0 deltas and the WAVs. Report every failure without changing the guard.

A completed measurement is `MEASURED_REQUIRES_REVIEW`, never a listening PASS.
Review amp-only versus amp+cab nodes, pitch dependence, false cuts during sustain,
quiet-note recovery, attack damage and loudness-matched listening separately.
Record reviewer/date, source/config hashes and outcomes before calling those
steps complete. Existing synthetic +12 dB/Q8/250 Hz controls are detector tests,
not measured real cabinet IRs.

## Synthetic capture smoke test

```sh
python Tools/AutoSet/synthetic_capture_smoke.py --renderer /absolute/build/ChimeraAutoSetTests_artefacts/Release/ChimeraAutoSetTests --out /absolute/session/synthetic-001
```

This exports the existing C++ note/resonance fixtures and exercises three native
profiles at all three rates: ordinary E2 sustain through Tight 515 with CAB off
and with the bundled V30 IR, and A0 common-envelope dynamics through Metro Clean
with CAB off. Its PASS means those nine synthetic cases placed no nodes and had
zero Dynamic EQ reduction. It is not a test of recorded instruments or a real
bass IR; output is marked `SYNTHETIC_ONLY` and listening remains false.
