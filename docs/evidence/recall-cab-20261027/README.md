# Recall CAB experiment evidence

See [the analysis and adoption limits](../../RECALL_CAB_READINESS_20261027.md).

* `summary.json`: first six-policy pass, 18 runs / 72 recalls.
* `warm-control/summary.json`: three additional warmed response-cache-only runs / 12 recalls, without recycled kernels.
* Matching `.csv` and `.log` files preserve all outcomes, including 12 failed runs and 4 censored recoveries.
* `build-identity.json`: source identity, compiler/configuration, executable hashes and explicit unverified platforms/hosts.
* `safety-*`, `ir-library.log`, `prepared-state.log`: focused experiment correctness/lifecycle evidence.
* `experiment-regressions.log`, `experiment-regression-details.log`: experiment full processor/CAB numerical passes and CAB CPU-gate failure.
* `final-default/`: separate final-source build identities, three focused correctness passes, repeated CAB CPU-gate failure and three no-policy-override runs / 12 recalls. One probe run failed the response gate; all 12 recalls had callback overruns. This phase is not combined with the 84-case experiment when reporting rates.

The main and warm-control comparison manifests contain the same Source/Tests/CMake SHA-256. The runner now includes the warm response-cache control by default; `--warm-control-only` reproduces the supplement separately. Metrics are defined in the analysis. Negative recovery values mean no recovery observed within the rendered one-second horizon; they are not zero latency.

These are Linux headless synthetic measurements, not device xrun counts, Windows/macOS validation, live playing, OBS/DAW listening or release approval.

The experiment source is `61690ebff18a9ea957f11ffdd2435b88b00c7061`; the final default source is `d7abb79c97ffd02fe4690eb2793a5473f9bddae9`. Use `--final-default` to reproduce the latter without forcing a benchmark policy. Full final-default preset-bank validation has not been rerun.
