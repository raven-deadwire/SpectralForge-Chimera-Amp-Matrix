# Head-refit experiment: implementation review and validation result

This note covers the isolated head-fitting experiment and its original
validation set. Actual target-engine and new-test results belong to the
separate candidate evaluation. No numerical fidelity or release approval is
granted here.

## Completed work

The experiment fitted 30 heads: five independent fixed channels, two modes,
and three preregistered ridge strengths. The nonlinear A2 backbone received
no optimizer updates. One forward pass over each channel's existing 96-second
training input supplied sufficient statistics for both modes. Combined
per-channel runtime was 179.971 seconds.

- **Raw:** refit the existing eight-channel, 16-tap final convolution.
- **EQ chain:** refit the same head while accounting for the fixed legal native
  POST EQ derived by the separate source/TRAIN-only factorization experiment.

Both modes preserve the `.nam` A2 configuration, its 6,347-sample receptive
field, all nonlinear backbone weights, captured native targets, channel
controls, source headers, dataset hashes, and acceptance profile. The EQ chain
adds state outside the finite A2 model using the verified target's existing
EQ topology. The `.nam` alone does not contain that EQ state; model and job
hashes are inseparable in this experiment's result records.

## Numerical implementation reviewed

The actual upstream final-convolution input is captured with a forward hook.
`unfold` yields 16 causal taps per channel, and the feature order is output
time, then channel, then tap. One-hot final-convolution tests verified channel
and tap order against the actual pinned A2 model.

Let `F_t` be the 128-dimensional feature vector, `z` its zero-history value,
and `s` the existing model head scale. The centered raw features are
`phi_t = s * (F_t - z)`. Raw fitting minimizes prediction MSE on the existing
training output after the original 6,347-sample skip, with ridge toward the
archived coefficients. Chain fitting substitutes the continuously filtered
features `H(phi_t)`. Filter state is carried from sample zero through every
chunk, including samples before the scoring skip. A dedicated test verifies
that pre-skip excitation contributes to the later scored tail.

Each penalty is `lambda * trace(training_gram) / 128`, for the frozen set
`{1e-6, 1e-4, 1e-2}`. The corresponding solve was checked against an independent
augmented least-squares calculation. Training and validation inputs have
separate filter states. No target-derived offset, output pad, EQ setting,
alignment shift, or cutoff is fitted during validation.

The exported bias enforces the same known zero-history centering used in
PR #22. The fitted pre-export state is compared directly with the imported
state, including the new head coefficients and bias. This corrects the initial
review finding that comparing two imports of the same file would not verify
the fitted pre-export state. A corruption-injection test proves that changed
head weights are rejected. The original backbone is separately checked for
exact preservation.

The ridge solve uses float64 filtered features to retain linearity. Validation
first sums each exported head, includes its numerical bias residual, and then
filters its mono output with native-rounded float32 coefficients and float32
SOS states. It does not rank EQ candidates using a float64-state projection
that differs from the actual BlockEq arithmetic. Final target-engine comparison
is still required because feature projection and actual convolution can have
small floating-point summation differences.

Maximum training output reconstruction errors were between `1.70e-6` and
`3.75e-6` native output units across the five channels. The original validation
baseline reproduced the archived refinement metrics. Thirteen actual-model,
numerical-fit, causal-state, export, and selection-guard tests passed; their
source hashes and command are saved in `head-refit-test-evidence.json`.

## Validation-only results

| Channel | Selected mode / ridge | Baseline ESR | Selected ESR | Selected RMS / peak error, dB | Selected quiet residual RMS / peak, dBFS |
|---|---|---:|---:|---:|---:|
| Fenrir | raw / 1e-6 | 0.105275 | 0.101798 | -0.458 / +0.630 | -67.559 / -50.747 |
| Surtr | EQ chain / 1e-4 | 0.106748 | 0.105080 | -0.388 / +0.534 | -81.819 / -70.985 |
| Nidhoggr | raw / 1e-2 | 0.106767 | 0.104774 | -0.409 / +0.992 | -75.305 / -64.750 |
| Fimbulvetr | EQ chain / 1e-4 | 0.040682 | 0.038905 | -0.145 / +0.282 | -81.365 / -70.898 |
| Ragnarok | archived baseline retained | 0.141158 | 0.141158 | -0.456 / +3.732 | -90.118 / -78.386 |

The validation mean ESR moves from **0.100126 to 0.098343**. This is a modest
improvement, and all five channels still fail full numerical fidelity.

Fenrir demonstrates that refitting waveform-producing hidden features can
improve its crest factor: its validation peak error changes from +1.500 dB to
+0.630 dB while RMS stays within the original limit. Its level component now
passes, but its quiet component and active/full ESR still fail. This agrees
with the separate scalar diagnostic: the useful correction was not just an
overall gain change.

For Surtr and Fimbulvetr, the fixed EQ-chain candidates improve the validation
quiet residual as well as ESR. Those validation tails already passed the quiet
gates for the baseline, so this result must not be represented as resolution
of the separately exposed Fimbulvetr test obstruction. The new independent
test and actual target engine must assess the full chain.

Nidhoggr's best-ESR candidates in both modes regress peak level. The frozen
guard rejects them and selects the stronger-ridge raw candidate. Ragnarok's
new best-ESR raw and EQ-chain candidates also regress peak level; the chain
additionally misses the required 0.1% ESR improvement. The original model is
therefore retained for Ragnarok. Lower average ESR does not override these
failures.

## Selection and evidence interface

Selection requires no newly failed composite acceptance component, guarded
level/quiet behavior against the original raw baseline, and at least 0.1%
validation ESR reduction. It then chooses the smallest ESR among eligible
raw and EQ-chain candidates. A tie favors the raw mode by fixed ordering.
If no candidate qualifies, the original baseline is retained. This is an
experimental selection rule; the original acceptance thresholds are unchanged.

The following folders each contain five `Nastrond-{Name}.nam` files plus five
`{Name}-job.json` files:

- `baseline/`: archived raw models, normalization disabled.
- `best-esr-raw/`: best raw head by validation ESR, including rejected examples.
- `best-esr-eq-chain/`: best full chain by validation ESR, including rejected examples.
- `selected/`: guarded per-channel selection with baseline fallback.

`selection.json` stores paired model/job SHA-256 values under `baseline`,
`best_esr.raw`, `best_esr.eq-chain`, and `selected`. Selected entries also name
their mode. This record was sealed only after all five channels completed and
the source models, EQ source jobs, quality profile, script and input manifest
were rechecked. The sealed selection SHA-256 is:

`0f136e033c1a9d2aaf046dbc93c1c9f033c4ff3b157002f99b6ac00ba3ee6584`

The plan fixed new test seed `2610122201` and duration 24 seconds before
fitting. This script never generated or decoded that new test. Existing test
bytes were read only by the manifest-hash verification, and no old test audio
was decoded or used for fitting or selection. The separate evaluator must
verify this sealed record before generating the new test; it must not tune
any candidate afterward.

The two descriptive best-ESR folders do not become approved candidates by
being evaluated. No files are approved for release, real-instrument listening,
or GUI/DAW qualification by this numerical experiment.
