# A2 training alternatives audit, 2026-10-12

Source: PR #22 at `859fea9bc49c4b9981c926a314655e485c51027c`.
This experiment directory is separate from the PR's existing training and
acceptance implementation. Numerical accuracy remains **FAIL** and
`release_approved=false` remains mandatory.

## New completed diagnostic: output gain is insufficient

`audit_gain_feasibility.py` reads the SHA-256-pinned refinement report and uses
only algebra on its reported full ESR, RMS ratio, and peak ratio. No models,
audio arrays, normalization anchors, or original scores are changed. Eight
numerical tests passed, including comparison against direct waveform dot
products and residuals, the exactly recoverable pure-gain case, an orthogonal
signal, DC preservation, conflicting level intervals, and policy drift.

Let `q = mean(prediction^2) / mean(reference^2)` and
`c = mean(prediction * reference) / mean(reference^2)`. The reported ESR gives
`c = (q + 1 - ESR) / 2`. For any real scalar `a`,
`ESR(a * prediction, reference) = q*a^2 - 2*c*a + 1`; therefore the best possible
scalar-only ESR is `1 - c^2/q`. This is an oracle lower bound for a fixed archived
prediction. It is not a fitted candidate, an independent-test score for a new
candidate, or a gain to install in a preset.

| Channel | Archived test ESR | Oracle scalar-only ESR floor | RMS error (dB) | Peak error (dB) | Can any gain satisfy both level limits? |
|---|---:|---:|---:|---:|---|
| Fenrir | 0.085300 | 0.083398 | +0.008790 | +1.646816 | No |
| Surtr | 0.091362 | 0.091314 | -0.352661 | -0.213649 | Yes; ESR still fails |
| Nidhoggr | 0.099453 | 0.099452 | -0.460397 | +0.938944 | Yes; ESR still fails |
| Fimbulvetr | 0.043812 | 0.043343 | -0.386920 | -0.214834 | Yes; ESR and memory still fail |
| Ragnarok | 0.120449 | 0.120350 | -0.649558 | +2.360760 | No |

All five oracle ESR floors remain above the unchanged full ESR `< 0.01` gate.
The corresponding validation floors are 0.103815, 0.106663, 0.106765, 0.040107,
and 0.140679. The same gain obstruction is present on validation, not solely on
the previously exposed test.

The level obstruction is stronger than a poor least-squares gain estimate.
A gain adjustment `d` dB must belong to both
`[-0.5 - RMS_error, 0.5 - RMS_error]` and
`[-1 - peak_error, 1 - peak_error]`.

- Fenrir test RMS permits `[-0.508790, +0.491210]` dB while peak permits
  `[-2.646816, -0.646816]` dB. The intervals are disjoint.
- Ragnarok test RMS permits `[+0.149558, +1.149558]` dB while peak permits
  `[-3.360760, -1.360760]` dB. The intervals are disjoint.

These channels require a change to the predicted waveform or dynamics. A
single output pad cannot fix their crest factor. This does not prove that a
nonlinear-backbone update, a refitted finite convolutional head, or a different
architecture is ineffective; those are outside the scalar-only bound.

## What is already completed and must not be repeated

The 5,000-update legacy versus upstream-derived A2 comparison already used a
common initialization and matched crops. Its difference bundled the objective,
training target normalization, and LR schedule. The official recipe produced
higher aggregate validation ESR, although Fenrir improved. It does not justify
adopting or rerunning the whole official recipe merely because it is official.

The two 1,000-update refinements already included LR 0.001, 4,096-frame outputs,
batch 2, 10% terminal-decay crops, and export-consistent centering. They were
warm-started from the same archived legacy checkpoint with reset optimizers and
crop RNGs. The legacy arm was selected on validation before seed 26101122 was
generated. Repeating that bundle would not isolate a new mechanism.

The existing zero-history memory witness already proves that bare 6,347-sample
A2 cannot meet Fimbulvetr's fixed quiet RMS gate on that captured test. No
additional epochs, output-head fit, or finite A2 objective removes this proof.
The scalar audit is a new diagnostic and does not repeat that memory audit.

## Channel-specific development priorities

| Channel and captured role | Remaining measured problem | Training-only scope | Structural scope |
|---|---|---|---|
| Fenrir: tight attack / clank | Active ESR and oversized peaks; validation quiet peak also fails | Preserve attack while correcting transient shape; output-head refit or a peak-aware objective is testable | No impossibility proof for its tails on the tested witness |
| Surtr: dense saturation / vocal mids | Active errors, earlier 1-4 kHz discrepancy, exposed-test tail RMS -69.400 and peak -58.321 dBFS | Improve short-window spectral/shape accuracy without losing its midrange role; tail learnability remains an experiment | The witness lower bound -78.671 dBFS does not prove the -70 gate impossible |
| Nidhoggr: asymmetric grind / rot | Active errors; tail RMS -63.655 and peak -55.276 dBFS | Preserve asymmetric response and signal-dependent DC; no signal-mean subtraction or compensating EQ | Witness lower bound -70.298 dBFS is close to the -70 gate but is not an impossibility proof |
| Fimbulvetr: broad bloom / clear upper mids | Active ESR 0.043812 remains high; quiet tail effectively unchanged | Active passages may improve under fixed A2 | Witness RMS floor -68.033258 dBFS exceeds -70; full qualification needs longer memory or a proven compatible stateful chain |
| Ragnarok: deep impact / stiff punch | ESR falls while peak regression grows; test RMS too low and peak too high | Train/fit transient shape and envelope, with level guards during validation selection | Its quiet witness is below limits; a tail-only remedy cannot fix the measured active error |

These roles come from the frozen capture controls, not a new listening verdict.
The five `.nam` files represent five fixed operating points. They do not by
themselves establish continuous macro interpolation or the original amp's
control response. Real instrument DI and blind channel-role listening remain
uncompleted gates.

## Initial fitting plan, subsequently executed once

The plan below was subsequently executed as the isolated 30-head comparison
in `head_refit.py`, with a second fixed POST-EQ mode sharing the same backbone
forward pass. The completed comparison appears in the next section. None of
the earlier 5,000-update or 1,000-update experiments was rerun.

After restoring the pinned models and train/validation arrays, the most direct
nonduplicative experiment is **train-only final-convolution refitting with the
nonlinear backbone frozen**. The recovered A2-Full model has eight hidden
channels and a 16-tap final convolution to one output: 128 final-head weights,
not a 1x1 head. Its external `config.head` is null. Keep this exact architecture,
the 6,347-sample field, and all remaining weights fixed.

For each channel, collect the final head's causal 8-by-16 hidden features on
training audio. Subtract the corresponding zero-history hidden feature vector
so the fitted output obeys the same export-consistent centering as the existing
implementation. Fit only the 128 coefficients to the captured native targets.
No free DC intercept or per-test gain is allowed. A small, preregistered ridge
penalty toward the archived coefficients can stabilize correlated features;
normalize the penalty with training-only feature statistics. Fit coefficients
on train only and select among a fixed small penalty list on validation only.
The archived model remains a fallback candidate.

This is a parameter-only A2 candidate, so it is a plausible route to the current
loader. Actual acceptance still requires export/import, target-engine output
comparison, silence, block/state checks, and unchanged fidelity evaluation.
Its representation cannot reproduce Fimbulvetr's forbidden long tail. If
validation ESR or level/quiet behavior worsens, reject the fit and retain the
baseline; do not weaken the policy to rescue it.

An all-weight experiment should follow only if the head diagnostic indicates
remaining nonlinear-feature error. Isolate one new objective, such as a short-
time envelope/crest penalty added to the existing centered legacy objective,
against a matched continuation control from the refined checkpoint. Preserve
the sampler, crop sequence, LR schedule, update budget, centering and numerical
scale between arms. The previous official MRSTFT comparison already exists;
another bundled recipe comparison would not isolate the transient mechanism.
Keep the ordinary waveform loss so spectral or envelope improvements do not
conceal phase/time-domain errors. Set the complete objective and validation
selection rule before running the comparison.

## Completed follow-up: raw head, EQ chain, and guarded selection

The 30 fits were five channels by two modes by three preregistered penalties,
with no nonlinear-backbone updates. The complete coefficients, models and
engine jobs were frozen after validation and before new-test generation.
`head-refit-selection.json` records paired model/job hashes. These validation
results do not establish actual-engine parity, unseen performance, character
preservation, or release approval; those require the separate evaluator and
listening evidence.

The best raw or EQ column below means the lowest validation ESR within that
mode, even if the guard rejected it. A cell lists **ESR; RMS / peak error in
dB**. All five final selected channels still fail overall numerical fidelity.
Unrounded values are in `head-refit-validation.json`.

| Channel | Best raw head | Best EQ-chain head | Frozen guarded choice |
|---|---|---|---|
| Fenrir | 0.101798; -0.457697 / +0.629614 (`1e-6`) | 0.101988; -0.455187 / +0.632035 (`1e-6`) | Raw `1e-6`; restores the level component on validation, but quiet/active/full ESR still fail |
| Surtr | 0.105325; -0.399169 / +0.585744 (`1e-4`) | 0.105080; -0.387630 / +0.533527 (`1e-4`) | EQ chain `1e-4`; slightly lower ESR and quieter residual than raw |
| Nidhoggr | 0.104656; -0.407091 / +1.009514 (`1e-4`) | 0.104894; -0.391993 / +1.006916 (`1e-4`) | Both best-ESR arms rejected for peak >1 dB; raw `1e-2` selected at ESR 0.104774, RMS -0.409364, peak +0.992194 dB |
| Fimbulvetr | 0.038918; -0.131733 / +0.273044 (`1e-4`) | 0.038905; -0.145067 / +0.281926 (`1e-4`) | EQ chain `1e-4`; modest active/full improvement plus lower validation quiet residual |
| Ragnarok | 0.140819; -0.378268 / +3.855161 (`1e-4`) | 0.141048; -0.377480 / +3.909828 (`1e-6`) | Both rejected for peak regression; EQ chain also misses the 0.1% ESR improvement rule. Retain baseline ESR 0.141158, RMS -0.455752, peak +3.731642 dB |

Quiet residuals below are **RMS / peak in dBFS** at the unchanged scale.
The raw and EQ columns use each mode's best-ESR candidate, not a tail-selected
candidate. Existing gates remain -70 / -60 dBFS.

| Channel | Archived baseline | Best raw head | Best EQ-chain head |
|---|---:|---:|---:|
| Fenrir | -66.993150 / -50.332719 | -67.559139 / -50.747469 | -67.643196 / -50.711183 |
| Surtr | -77.991759 / -67.678410 | -77.991654 / -67.678259 | -81.818610 / -70.985271 |
| Nidhoggr | -75.305374 / -64.750177 | -75.305478 / -64.750218 | -75.872738 / -65.234282 |
| Fimbulvetr | -78.378737 / -67.668946 | -78.378659 / -67.668998 | -81.364893 / -70.898426 |
| Ragnarok | -90.117905 / -78.385856 | -90.118549 / -78.386342 | -91.655573 / -79.973048 |

These figures distinguish three effects. A raw head refit leaves the long
quiet tails of Surtr, Nidhoggr and Fimbulvetr almost unchanged, as expected
for the same finite-memory model. Fixed recursive POST EQ can change those
tails, but the baseline already passed the quiet gates on this validation
input; the result does not prove that the exposed Fimbulvetr failure or any
new independent test passes. Ragnarok gains a quieter tail while its peak
level becomes worse, so the guard correctly refuses the candidate.

The guard is not a claim of improvement in every active statistic. It checks
existing composite pass states, level/quiet limits and minimum full-ESR
improvement. Because all active components already fail, a small worsening of
an active statistic may remain eligible. Selected Surtr worst-active ESR is
0.668755 versus baseline 0.666179; Fimbulvetr chain p95 is 0.101232 versus
0.101196. Nidhoggr's selected worst-active ESR is 0.584314 versus baseline
0.563598.
They remain research candidates with reported tradeoffs. No selection rule
was changed after seeing the new test.

### Next priorities that preserve the supported A2 architecture

| Channel | Highest-value next controlled training question |
|---|---|
| Fenrir | Can a waveform/transient-aware objective preserve the newly improved crest factor while reducing active error and the early quiet-transition peak? Use train-only target-derived transient weights, not windows chosen from the exposed tests. |
| Surtr | Can centered legacy waveform loss plus one isolated spectral or envelope term improve the active midrange response? Keep the fixed legal chain as a separately measured mode; small full-ESR improvement alone is insufficient. |
| Nidhoggr | Can a peak/envelope-aware objective avoid crossing +1 dB while reducing error in asymmetric distortion? Preserve signal-dependent DC and asymmetry; retain zero-history-only centering. |
| Fimbulvetr | If the actual-engine/full-chain evidence warrants continuing, can backbone training against the fixed filtered output reduce active error while correctly carrying the EQ state? A bare A2-only objective cannot remove the proved long-history obstruction. |
| Ragnarok | Can limited nonlinear-backbone fine-tuning correct attack/crest shape and active error together? Neither output gain nor the tested head-only refits fix the peak problem, and a tail-only modification addresses little of the existing active error. |

These are proposed comparisons, not measured improvements. Keep the exact
supported A2 dimensions, activations and export schema. Preregister one new
objective against a matched continuation control from the same frozen
checkpoint, retain the original waveform loss, and compare the same crops and
budget. For an EQ-chain objective, filter state must be warmed or carried
correctly before scoring; resetting it at every crop would train a different
system. Freeze the model and job pair before another independent evaluation.
No additional fitting was performed while preparing this audit note.

The subsequently completed [active nonlinear-history probe](nonlinear-memory/README.md)
adds a second structural caution: after a common silence longer than the
receptive field, the native teacher still has history-dependent incremental
responses that a finite-history A2 plus fixed linear POST filter cannot match
exactly for all witness traces. That probe does not prove that its own lower
bound exceeds the product's full/active accuracy gates or explains all present
ESR. Architecture-preserving fine-tuning can still reduce approximation error,
but none of the proposed objectives guarantees the complete nonlinear
recovery behavior, especially Nidhoggr's long recovery.

## Independent evaluation audit after the frozen selection

The new shared synthetic input uses seed `2610122201`, 24 seconds, with the
unchanged native source, controls and acceptance policy. The equal-channel
means below compare each candidate with the baseline on **that same input**.
The earlier mean 0.088075 came from another input and cannot serve as the
baseline for this new test. The best-ESR raw and EQ modes remain descriptive
arms even where the validation guard rejected them.

| Arm | Mean ESR, new shared input | Reduction from same-input baseline | Full fidelity / actual-engine checks |
|---|---:|---:|---:|
| Archived baseline | 0.10123350478532842 | — | 0/5 / 5/5 |
| Best validation-ESR raw head | 0.09908061626303855 | 2.126656% | 0/5 / 5/5 |
| Best validation-ESR EQ-chain head | 0.09911670661273811 | 2.091006% | 0/5 / 5/5 |
| Frozen guarded per-channel choice | 0.09894643097992867 | 2.259206% | 0/5 / 5/5 |

The guarded choice was fixed before test generation. Four selected model/job
pairs exactly duplicate already measured arms: Fenrir raw, Surtr EQ chain,
Fimbulvetr EQ chain and Ragnarok baseline. The fifth, Nidhoggr raw `1e-2`, was
accidentally omitted from the initial evaluator launch and measured in a
documented supplement on the same input. Its original model/job hashes occur
in the pre-test selection, so this fills an omitted measurement without
test-based selection or refitting. It is not an additional independent test.

| Channel | Baseline ESR | Best raw ESR | Best EQ ESR | Frozen selected ESR | Selected RMS / peak error, dB | Selected quiet RMS / peak, dBFS |
|---|---:|---:|---:|---:|---:|---:|
| Fenrir | 0.110451 | 0.106828 | 0.106913 | 0.106828 | -0.443467 / +1.121730 | -62.278314 / -45.954526 |
| Surtr | 0.109072 | 0.106052 | 0.105654 | 0.105654 | -0.439003 / +0.907644 | -78.122593 / -67.155361 |
| Nidhoggr | 0.115863 | 0.112776 | 0.112833 | 0.112879 | -0.531425 / +1.587126 | -76.499210 / -65.529258 |
| Fimbulvetr | 0.047991 | 0.046332 | 0.046580 | 0.046580 | -0.126652 / -0.547853 | -78.313504 / -69.045166 |
| Ragnarok | 0.122791 | 0.123416 | 0.123603 | 0.122791 | -0.474774 / +3.060530 | -85.909980 / -73.445129 |

Every selected channel still fails the full and active ESR criteria. Only
Surtr and Fimbulvetr pass the level component on this input; four channels
pass the quiet component. Fenrir's validation level improvement does not
fully generalize: the new peak error remains above +1 dB, and its quiet
transition remains far above the residual limits. Nidhoggr's stronger ridge
passed the validation level guard, but both RMS and peak limits fail on the
new input. Ragnarok's two fitted modes actually worsen new-test ESR; the
validation-frozen baseline fallback avoids that regression. Fimbulvetr's raw
arm has lower new ESR than the selected chain, but the new result was not
used to switch the preregistered choice.

The old revealed input remains an immutable regression diagnostic. On it,
mean ESR is baseline 0.08807536606575210, raw 0.08574361466659397, EQ chain
0.08570353689328844 and selected 0.08595389250742849. The corresponding
selected reduction is 2.408703%; it is not a second unseen qualification.
Its tail windows expose effects that the new input alone would miss:

| Channel | Old baseline quiet RMS / peak, dBFS | Old best EQ-chain quiet RMS / peak, dBFS | Interpretation |
|---|---:|---:|---|
| Surtr | -69.399586 / -58.321202 | -74.866812 / -64.644042 | Both quiet limits now pass; the frozen selection includes this chain |
| Nidhoggr | -63.655397 / -55.275514 | -77.484514 / -68.776276 | Quiet limits pass, but the EQ candidate was rejected for validation peak error; selected raw still fails this old quiet test |
| Fimbulvetr | -62.863772 / -54.770577 | -67.445690 / -59.691651 | RMS improves 4.581918 dB and peak 4.921074 dB, but both limits still fail |

Fimbulvetr's new-input baseline already passes the quiet component. Therefore
the new chain's quiet pass does not resolve the originally demonstrated
long-tail failure. The optimistic finite-memory-pretarget-plus-EQ oracle
and the achieved fitted chain must remain separate findings. For Surtr, the
new chain's quiet RMS is about 1.035 dB worse than the new baseline, though
both pass; tail improvement is input-dependent.

Numerical channel-character proxies give mixed evidence. Mean absolute error
in five-band pairwise channel contrasts, over ten channel pairs, changes from
0.332695 dB for baseline to 0.251666 dB for raw, 0.271671 dB for EQ and
0.281324 dB for the guarded selection. Those 50 correlated measurements
describe broad spectral differentiation, not musical character acceptance.
Mean absolute crest error worsens from 1.608373 dB to 1.753004, 1.798865 and
1.797380 dB respectively. Better average ESR or spectral contrast therefore
does not establish accurate attacks, compression or peak dynamics.

The actual pinned, unmodified `NamEngine`, `BlockEq` and `PresetFile` passed
loading, normalization, output-parity, block-state and preset-restore checks
in a console harness. All measured model/job pairs have bit-identical
block-64 versus block-256 and preset-restored outputs. Selected Python/native
parity residuals range from -148.97 to -98.27 dBFS, below the unchanged
-80 dBFS gate. This is scoped engine evidence; GUI/DAW acceptance, real-time
qualification, recorded-instrument DI listening and channel-role listening
remain unexecuted. No model is approved for release.

The independent audit recomputed eight numeric metrics directly from each of
32 saved output/reference records, without calling the evaluator's scoring
function. All differences were exactly zero. All 76 main-plan and 91
supplement-plan sealed files rehashed unchanged; the four reused selected
records exactly equal their original results. The supplement's fifth pair
matches the original pre-test selection. No test-dependent fitting, parameter
adjustment or reselection was found in the reviewed code and records. Reused
validation has a long selection history, and a single fresh synthetic input
does not establish generalization to recorded instruments or other controls.

Evidence: [independent numeric/provenance review](evaluation-independent-review.json),
[main comparison](independent-comparison.json),
[selected supplement](selected-summary.json),
[original selection](head-refit-selection.json),
[new-input plan](independent-test-plan.json) and
[supplement plan](supplement-plan.json).

## Selection and leakage precautions

`train_a2.py` currently saves each channel's best checkpoint by full validation
ESR only. `recipe_selection.py` then ranks fully passing channels and mean ESR.
When both arms have zero fully passing channels, this permits the measured
Ragnarok peak regression. A separate experiment should explicitly retain an
archived fallback and screen candidates for no new component failures and no
material level/quiet regression before ranking ESR. This changes experimental
selection, not numerical acceptance thresholds.

Per-channel selection from the existing legacy and official arms is also a
possible zero-training candidate, but should not be called a new trained
solution. For example, official Fenrir validation ESR and level are better,
while official Fimbulvetr ESR is slightly better but its worst active window is
worse. A selection rule based only on mean ESR would conceal this tradeoff.
No mixture of those existing legacy/official recipe arms was selected or
tested by this audit; the separate head-refit choices above are different
candidates.

The original test and seed 26101122 have now been examined to guide the next
design. They remain useful immutable regression sets, but they must not be
advertised as unseen qualification for future choices. Fit no weights, output
gain, filters, alignment, thresholds, or duration cutoffs on them. Freeze the
new candidate's hashes and selection record before a new deterministic test
seed is generated or opened. Existing train/validation data can be reused;
record that validation has a long selection history. Real recorded DI adds a
separate domain check that another synthetic seed cannot replace.

## Frozen acceptance and provenance

The source report is pinned by SHA-256
`89c19c30d87159d1c5091f76d8c8ee7b90caab534513073e23e6128710c37f93`.
The diagnostic enforces exact equality of its quality profile. The profile is
a project policy, not an official TONE3000 certification.

| Check | Unchanged criterion |
|---|---|
| Full ESR | strictly less than 0.01 |
| Active median / p95 / worst ESR | at most 0.005 / 0.01 / 0.02 |
| Active classification | reference RMS at least -60 dBFS |
| Quiet residual RMS / peak | at most -70 / -60 dBFS |
| RMS / peak level error | absolute error at most 0.5 / 1.0 dB |
| Steady silence | at most -80 dBFS |
| Engine parity residual | at most -80 dBFS |
| Window / initial skip | 4,096 / 6,347 samples |

Keep native source `d93560445c78552768a0ed592e5ec62c3c648fb6`, all eight frozen
DSP header hashes, five captured controls, 48 kHz rate, 4x oversampling,
unshifted production latency, fixed per-channel v0.2 anchors, and the test
partition hashes intact. No channel interpolation, target EQ fitting, silence
truncation, candidate-relative normalization, or source-DSP change is part of
the proposed parameter-only experiment.

Reproduce the new diagnostic:

```sh
python -m unittest discover -s Tools/Nam/experiments/alternatives_20261012 -p 'test_gain_feasibility.py' -v
python Tools/Nam/experiments/alternatives_20261012/audit_gain_feasibility.py \
  --out Tools/Nam/experiments/alternatives_20261012/gain-feasibility.json
```

Primary project evidence:

- [Frozen refinement report](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/blob/859fea9bc49c4b9981c926a314655e485c51027c/Tools/Nam/reports/refinement-20261011.json)
- [Original full-run audit](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/blob/859fea9bc49c4b9981c926a314655e485c51027c/Tools/Nam/reports/full-37947011057-audit.md)
- [Training and checkpoint selection](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/blob/859fea9bc49c4b9981c926a314655e485c51027c/Tools/Nam/train_a2.py)
- [Frozen capture controls](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/blob/859fea9bc49c4b9981c926a314655e485c51027c/Tools/Nam/capture_channels.py)
