# Long-memory alternatives: frozen-source diagnosis and legal POST-EQ screen

Date: 2026-10-12. All work is an isolated experiment. **Release approval remains false.**

## Sources and scope

- Native teacher: [`d93560445c78552768a0ed592e5ec62c3c648fb6`](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/tree/d93560445c78552768a0ed592e5ec62c3c648fb6), specifically `Source/OriginalAmpDSP.h` and `Source/OriginalAmpDefinition.h`.
- Captured channel settings: [`859fea9bc49c4b9981c926a314655e485c51027c/Tools/Nam/capture_channels.py`](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/blob/859fea9bc49c4b9981c926a314655e485c51027c/Tools/Nam/capture_channels.py).
- Native supported EQ: TONE3000 plugin [`ef6f178ae1ac6412b55fec6f058d86e640e5aaaf/plugin/src/BlockEq.cpp`](https://github.com/tone3000/tone3000-plugin/blob/ef6f178ae1ac6412b55fec6f058d86e640e5aaaf/plugin/src/BlockEq.cpp) and `plugin/include/BlockEq.h`.
- Existing TRAIN/validation targets were restored from the prior failed-full evidence archive and checked against its capture manifest hashes. No repeated NAM training was used for this screen.
- The old 2026-10-11 independent test was already inspected in the prior report. Its reuse here is labeled a **revealed diagnostic**, never a new independent test or a selection split.

## 1. What makes the native amplifier remember?

The native amplifier contains both a stationary output resonance and input-dependent nonlinear memory. These require different solutions.

The BLOOM section follows power-stage saturation. It filters that signal with a 180 Hz one-pole low-pass, drives a damped second-order resonator, and adds the result back to the signal. Its source-defined poles are

\[
r=\exp[-1/(f_s(0.004+0.04B))],\qquad
f_0=95+40B,\qquad
p_{1,2}=r e^{\pm j2\pi f_0/f_s}.
\]

The output also has an 8 Hz DC blocker, whose time constant is approximately 19.89 ms. Later fixed filters adjust the resonance amplitude and phase without removing its poles in the quiet linear regime.

The following values are for the **captured PR #22 controls**, which differ from the visible noon defaults. Times are rounded.

| Channel | Effective BLOOM | Resonance | Resonance time constant | Sag release | Blocking recovery |
|---|---:|---:|---:|---:|---:|
| Fenrir | 0.156 | 101.24 Hz | 10.24 ms | 45 ms | 21.12 ms |
| Surtr | 0.915 | 131.60 Hz | 40.60 ms | 160 ms | 90.40 ms |
| Nidhoggr | 1.080 | 138.20 Hz | 47.20 ms | 90 ms | 604.80 ms |
| Fimbulvetr | 1.465 | 153.60 Hz | 62.60 ms | 300 ms | 217.44 ms |
| Ragnarok | 0.608 | 119.32 Hz | 28.32 ms | 55 ms | 133.56 ms |

In particular, Fimbulvetr's 300 ms sag release is **not** the same quantity as its 62.6 ms audible resonance decay. Nidhoggr has an even longer blocking recovery. Increasing a stationary output filter's memory therefore cannot generally recover all repeated-note, gain-recovery and asymmetric-distortion behavior.

At exactly zero gain-cell input, `tanh(v + point) - tanh(point)` becomes zero despite a nonzero internal bias memory. Sag acts as a multiplier. This explains why quiet output can be dominated by stationary resonator/DC poles while long nonlinear state still changes the response to later nonzero input.

The native amplifier additionally applies a nonlinear output rail above an internal magnitude of 1.5 after BLOOM, voicing and output filtering. Therefore the BLOOM transfer used below is an exact transfer of that **internal stationary subnetwork**, not an exact linear factorization of the whole active amplifier. The final nonlinear rail and oversampling remain part of the target.

## 2. TRAIN-only source-pole attribution

The existing 96-second TRAIN input ends with 0.5 seconds of exactly zero input. The diagnosis uses native samples from one complete A2 receptive field after that silence begins: 132.229 ms through 500 ms. Source-defined resonance frequency, resonance time constant and the 8 Hz DC pole were held fixed. Only three nuisance amplitudes—sine, cosine and DC-pole amplitude—were least-squares fitted.

| Channel | Native late-tail relative energy not explained by the source poles |
|---|---:|
| Fenrir | 2.6093e-8 |
| Surtr | 1.1228e-9 |
| Nidhoggr | 2.9652e-9 |
| Fimbulvetr | 2.0584e-6 |
| Ragnarok | 1.8048e-8 |

For Fimbulvetr, this source-pole model explains approximately **99.999794%** of the late-tail energy in that TRAIN witness. This supports targeting the stationary output resonance first. It does not establish that nonlinear history is negligible during active playing.

Reproduce with `memory_source_poles.py`; evidence is `memory_source_poles.json`. These relative values describe a quiet-tail attribution fit, **not NAM audio ESR**.

## 3. One bell cannot exactly reproduce the Fimbulvetr pole; four legal bells can approximate it

The actual supported native EQ contains six fixed-role bands: a low cut/shelf, four bells, and a high cut/shelf. The verified bounds are 20–20,000 Hz, ±15 dB and Q 0.1–10.

For an RBJ bell, the low-frequency equivalent pole Q is approximately `Q_parameter * 10^(gain_dB/40)`. Fimbulvetr's 153.6 Hz/62.6 ms source pole has physical Q approximately 30.21. At Q=10, a single bell would require approximately +19.20 dB, beyond the +15 dB bound. A Q=10/+15 dB bell near that frequency has a 49.15 ms time constant.

This is a **single-bell pole-range result**. It is not a proof that the entire legal six-band family cannot approximate the finite observed decay. Also, a frequency parameter minimum of 20 Hz does not by itself prohibit a slower real pole: an overdamped second-order filter can have real poles below its nominal frequency.

The isolated screen used the four legal bell slots and neutral edge shelves. Each channel received one deterministic complex-response fit of the source BLOOM network, with fixed source-centered frequency points, TRAIN native PSD weighting, no restarts, and a 600-function-evaluation cap. No validation result chose a setting. All settings and TRAIN evidence were saved in `memory-factorization/training-freeze.json` before validation was evaluated once.

| Channel | Weighted complex-response RMS error | TRAIN inverse-target late-tail change | Validation inverse-target late-tail change |
|---|---:|---:|---:|
| Fenrir | 0.001175 | +1.24 dB | +1.30 dB |
| Surtr | 0.002899 | −8.37 dB | −11.69 dB |
| Nidhoggr | 0.003453 | −9.48 dB | −4.05 dB |
| Fimbulvetr | 0.009642 | −1.98 dB | −2.36 dB |
| Ragnarok | 0.002460 | −0.35 dB | −1.77 dB |

Negative late-tail change means attenuation after causal inverse filtering. These are subnetwork/deembedding diagnostics; they are **not five new NAM accuracy scores**. Fenrir's tail was already extremely quiet, and this factor did not help it. Fimbulvetr remains the hardest stationary pole to approximate. The local solver put multiple bands near the gain limits and its last bell at Q=10. This is the candidate found by the declared search, not a global optimum or a proof of impossibility.

Frozen native job files are `memory-factorization/{Name}-factor-post-eq.json`. `memory_factorization.load_eq_sos(job)` provides the verified corresponding coefficient family for the separate head-fit experiment. It intentionally accepts only this tested topology.

## 4. Reuse of the already-revealed hard witness

After freezing all settings, the same factors were applied once to the 2026-10-11 diagnostic input. For an optimistic screen, the inverse-filtered native target was assumed to be reproduced perfectly until its available raw-input history became all zero. It was then forced to zero, as required by the centered finite-history A2 in that situation, and passed through the legal POST filter.

This **truncation oracle is not a trained model**: even its assumed perfect active behavior may require histories an A2 cannot represent. Its purpose is to determine whether the specific bare-A2 quiet witness necessarily continues to rule out a legal POST-chain candidate.

For Fimbulvetr's original hard window beginning at sample 1,132,747:

| Quantity | dBFS |
|---|---:|
| Original bare-A2 quiet RMS lower bound | −68.0333 |
| Inverse-target zero-history variance bound | −70.8285 |
| Legal POST truncation-oracle residual RMS | −71.2570 |
| Legal POST truncation-oracle residual peak | −63.3477 |
| Unchanged project residual RMS limit | −70.0000 |
| Unchanged project residual peak limit | −60.0000 |

Thus the known quiet witness is **potentially feasible with this legal paired chain**, despite imperfect pole matching. That is a reason to test an A2 head fitted through the frozen POST filter against the original native target. It is not an accuracy pass, a general memory proof, or release approval.

The ordinary all-signal ESR/level/transient errors remain the responsibility of the real fitted NAM-plus-EQ candidate. Evidence: `memory-factorization/revealed-witness-diagnostic.json` and `memory_revealed_witness.py`.

## 5. Actual native EQ compatibility and floating-point behavior

`BlockEqProbe` compiles the unmodified pinned TONE3000 `BlockEq` component. The test used a fixed 8-second input containing an impulse, positive/negative DC steps, multitone/noise, and one second of silence. It evaluated all five jobs at block sizes 64 and 257.

- All five jobs were accepted by the actual component and remained within its parameter bounds.
- Native EQ `ValueTree` serialization/restore produced equal settings and **bit-exact output** for all five.
- Block-size 64 versus 257 output was **bit-exact** for all five.
- SciPy filtering with the helper's coefficients and **float32 input, coefficients and state** was **bit-exact** to the native component for all five.
- A float64-state linear approximation had residual RMS −83.45 to −92.20 dBFS relative to the native float32 component on this probe. The large opposing gain settings make this distinction relevant.

These are actual **EQ component** checks. They do not replace full NAM-plus-EQ engine, complete `.t3kpreset`, GUI, DAW or recorded-instrument acceptance. The separate head-fit work must evaluate its summed exported NAM output through the real component; filtering individual feature streams during linear fitting is an approximation to float32 arithmetic.

Evidence: `memory_eq_parity.py`, `memory-factorization/native-eq-parity.json`, and the separate `BlockEqProbe.cpp`/build provenance.

## Development implication

The immediately testable structural alternative is **unchanged supported A2 architecture plus a paired legal native POST-EQ preset**, with the A2 fit evaluated through that filter against the untouched original native target. The `.nam` alone does not contain the POST filter's state or correction. This path can target the quiet stationary resonance without inventing support for arbitrary IIR or recurrent models.

It cannot be assumed to solve nonlinear sag/blocking/bias history. Those require separate active-history evaluation and, if still limiting, a larger-history or stateful architecture in an explicitly supported engine. Neither those unsupported structures nor a change to shipping native amplifier character has been introduced by this experiment.

## Follow-up long-history probe: incremental response after a common silent gap

The next structural probe should distinguish nonlinear history from a stationary filter's ringing. It should **not rerun the previous different-prefix/common-decaying-suffix history experiment** unchanged.

Predeclare two one-second conditioning prefixes with the same 55/110/165 Hz harmonic shape at different levels (0.65 and 0.005), then an identical silent gap chosen from 150, 250, 500, 750 and 1,000 ms. All gaps exceed the fixed 6,347-sample receptive field. At the end of each gap, use the same 200 ms probe note/chord. Hold channel controls fixed at the captured settings. Run a matched all-zero suffix for each conditioned history as well.

For each gap/channel, use four native renders to measure

\[
\Delta = (y_{A,\mathrm{probe}}-y_{A,0})-
         (y_{B,\mathrm{probe}}-y_{B,0}).
\]

The subtraction removes a stationary linear post-filter's inherited tail. A centered finite-history A2 followed only by a fixed linear POST filter predicts the same incremental response once the common gap exceeds its receptive field. A repeatable native difference beyond the measured floating-point noise floor would identify **nonlinear incremental memory that a stationary POST correction cannot generally reproduce**.

Use two fixed probe levels, for example 0.03 and 0.3, to expose both small-signal recovery and saturation. Compare the actual exported A2-plus-EQ engine through the same four-render test to measure its numerical baseline. Report native/candidate incremental-difference RMS, peak, 20 ms onset/late windows, and response-level changes using the unchanged output anchor. This diagnostic must remain distinct from the full fidelity gate and recorded-instrument listening.

This test can reveal nonlinear memory arising from sag, blocking/bias recovery **or their interaction with the final output rail**. It must not attribute the result to one internal state without a separate controlled source-state diagnostic. The follow-up was subsequently executed once as a separately frozen diagnostic: see `nonlinear-memory/README.md`, its original `plan.json`/`results.json`, and `anchored-summary.json`. It does not retune or select the frozen EQ factors described above.

## Reproducibility and selection controls

- `memory_source_poles.py` verifies the native commit and TRAIN capture/input hashes before inspecting the TRAIN terminal decay. Frequencies and decay constants come from the frozen source; only explanatory amplitudes are fit.
- `memory_factorization.py` uses one deterministic starting point and bounded solver call per channel. It uses the source subnetwork response and TRAIN PSD only. Each exported job has its own SHA-256. The complete TRAIN report is written to `training-freeze.json`; its SHA-256 is recorded in the final validation report before validation outputs are opened.
- The inverse target is produced with a causal stable inverse of each frozen minimum-phase RBJ bell. Both forward and inverse diagnostics use continuous filter state from the start of the full signal; they are not reset at crop or quiet-window boundaries.
- `memory_revealed_witness.py` checks the frozen report/job hashes and the original revealed-test input/native hashes. It contains no optimizer or settings update. The diagnostic is not returned to the fitting stage.
- `memory_eq_parity.py` uses a fixed random seed (26101281) and saves input/binary/job hashes. The native harness checks actual parameter serialization and restored output equality. The full candidate engine remains a separate validation step.
- No old test was used to fit or choose the factors. No acceptance threshold, shipping DSP, existing PR #22 model, or release-approval state was changed.
