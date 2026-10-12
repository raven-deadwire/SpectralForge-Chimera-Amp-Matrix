# Candidate evaluator review — 2026-10-12

The isolated evaluator passed preflight review. The preflight review did not generate the reserved test signal, run new native renders, fit a model, or inspect new test results. Completed component experiments were reused. A subsequently requested qualification supplement is recorded separately below.

## Concrete corrections

- **Faithful native EQ arithmetic.** Python parity now uses the previously verified `BlockEq` float32 coefficients, float32 input and float32 recursive state over the complete continuous signal. The previous float64 state path implemented a numerically different filter. The estimator is still compared against actual `ChimeraTone3000Tool` output.
- **Completed selection before new data.** Before generating seed `2610122201` for 24 seconds, the evaluator verifies the completed five-channel validation report and fit-plan hashes, the original future seed/duration, data manifest, policy and EQ implementation. Every requested model/job pair must occur together in the frozen baseline, selected or descriptive best-ESR records. Recombining a model and another candidate's EQ after selection is rejected.
- **Teacher controls fixed before evaluation.** Native jobs are reconstructed from identical training/validation snapshots in the frozen manifest and sealed before reference generation. The evaluator no longer trusts an unsealed external validation job JSON.
- **Verified runtime and immutable inputs.** Binary hashes must match the existing successful build record, including its pinned player/core/JUCE commits and five bit-exact teacher-prefix checks. The evaluator seals source files, implementation helpers, selection files, original and canonical jobs, and already revealed test files, then checks them again before marking the comparison complete.
- **Complete output required.** Every native render, alternative block size, restored preset and silence render must return the expected sample count/rate. Output byte length and finite mono samples are checked. Target renders must report A2-Full, disabled normalization and unity normalization gain. Python parity output is also checked for completeness and finiteness. Nonzero CLI exits remain errors.
- **Reproducible invocation.** Input paths are resolved, safe arm labels are required, and an already existing empty output directory is accepted. Existing nonempty output directories remain rejected. The new `--build-record` option defaults to the existing report beside the evaluator.

## Verification

Seven failure-injection tests passed in `test_evaluate_candidates.py`. They cover a changed validation report, an incomplete channel set, a changed future seed, a model/EQ pair not selected together, release approval entering a research selection, truncated/partial/nonfinite audio, and runtime metadata that hides gain or incomplete rendering.

The actual completed selection at `/workspace/scratch/e8564bd8a57f/headfit-evidence/selection.json` passed preflight, including all four folders and five channels. Its SHA-256 is `0f136e033c1a9d2aaf046dbc93c1c9f033c4ff3b157002f99b6ac00ba3ee6584`.

The corrected Python EQ function was compared with the **existing cached** actual `BlockEq` outputs from `memory_eq_parity.py`. The component input hash and all five job hashes matched `memory-factorization/native-eq-parity.json`; output was float32 and bit-exact for every channel. No component was rendered again for this review.

Run the focused tests from this directory with the pinned trainer on `PYTHONPATH`:

```sh
PYTHONPATH=/workspace/scratch/e8564bd8a57f/deps/trainer /workspace/scratch/e8564bd8a57f/nam-venv/bin/python -m unittest test_evaluate_candidates -v
```

Reviewed evaluator SHA-256: `6b7205e5e3441f4de91c9a784946e7545e204c3baf51217d7f51ea5487ba7435`.

## Preserved interpretation and limits

`Tools/Nam/quality_profile.py` is unchanged, SHA-256 `6c245586da78b30c4f977293e54589097854ba7b49675a6b7d30fa05d1ab402e`. Its original accuracy, quiet-tail, level, engine-parity and silence thresholds remain in use. The existing `1e-6` block-size and preset-output comparison tolerances are also unchanged.

Old revealed test data remains explicitly labelled diagnostic regression. Spectral band power, crest and channel contrasts describe synthetic excitation at fixed snapshots; they do not establish musical character preservation or approve instrument listening. Offline callback timing remains diagnostic under shared compute and does not qualify real-time DAW behavior. Compatibility conclusions concern the compiled pinned `NamEngine`, `BlockEq` and `PresetFile` components. GUI/DAW and instrument-DI listening fields remain `NOT_EXECUTED`, and `release_approved` remains false regardless of numerical results.

## Subsequent selected-model qualification supplement

The initial comparison launch evaluated baseline, descriptive best-ESR raw, and descriptive best-ESR EQ-chain folders. An orchestration omission left out the previously selected Nidhoggr raw candidate with lambda `0.01`; its validation guard accepted it while the raw candidate with the lowest ESR was different. The remaining four selected model/job pairs were already measured exactly in those three folders.

After the original comparison finished, `supplement_selected.py` verified its completed report and original candidate-selection seal, matched the four existing selected model/job pairs by both SHA-256 values, and rendered only the missing selected Nidhoggr. The candidate selection preceded the original shared test generation. The supplemental plan was sealed before its measurements. The existing input and native reference were reused; no new excitation or native reference was generated, no model was fitted or changed, and no candidate was selected after seeing test results. This is a supplementary measurement on the same test, not another independent test.

The supplement exited successfully and verified every original and supplemental seal at completion. The output `/dev/shm/nam-selected-supplement-20261012/selected-summary.json` contains all five selected channel rows, their exact provenance aliases, channel contrasts, and unchanged acceptance results. Its SHA-256 is `5dd77d1c865c90953bd26e77363ad8ff8a1387e9099b5790a631385143c20c6e`.

For the omitted Nidhoggr candidate, new-test ESR is `0.11287915945723248`; RMS error is `-0.531425046466406 dB`, peak error is `+1.5871257150491864 dB`, quiet residual RMS is `-76.49920966306627 dBFS`, and quiet residual peak is `-65.52925786755746 dBFS`. Fidelity fails. Its Python/native residual is `-143.8387501498816 dBFS`; block-size and preset-restore differences are both exactly zero; steady silence is `-191.8248976787817 dBFS`. The component engine checks pass.

The actual five-channel selected set has mean new-test ESR `0.09894643097992867`, versus the baseline's `0.10123350478532842`, a relative improvement of `2.2592063865116807%` on this shared new test. All five selected models pass the scoped component engine checks; none passes the complete fidelity policy. Release approval remains false.
