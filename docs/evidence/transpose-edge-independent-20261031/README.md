# Independent splice-edge validation

Synthetic-only comparison of PR #234's transient-path edge experiment.
Product `Source/`, default engine selection and PDC are unchanged.
The local source tree was published as registration commit
`340c78cd508261e7c86cb5bcfa2f72ceda2b7251` before the fresh renders.

## Evidence

| File | Scope |
|---|---|
| `preregistration.json` | Fixed seeds, unchanged latency spec, pass/report rules, source and binary SHA-256 |
| `original-replay.json` | 20 original hashes; exact RNG replay of 9 original worst OFF cases and their ON counterparts |
| `sweep-rows.json.gz`, `sweep-summary.json` | 1,836 fresh conditions × four engines; raw and floor-corrected residual, harmonic gain, every >1 dB regression |
| `corr40-split-*.json.gz`, `target-summary.json` | 612 rows per engine, original T1–T4 metrics, fresh-only+steady scores, signed instrument delay differences |
| `onset-diagnostic.json`, `onset-*.csv.gz` | Post-hoc sample trace of all 13 conditions whose reported reset timing changed; initial detection vs reset application |
| `parity.json` | 216 option-OFF comparisons with the pre-patch main binary |
| `blocks.json` | 432 conditions, 14 fixed/random partition comparisons each |
| `contract-tests*.log` | Initial LSan environment failure and successful ASan/UBSan/contract run with LSan disabled; not a leak certification |
| `cpu-results.json.gz`, `cpu-summary.json` | All 1,800 runs, five paired alternating-order repeats per condition, mean/p99/max and every miss count |
| `cpu-overruns.json` | Every wall or thread-CPU callback overrun, including callback index, size and budget |
| `cpu-callback-sha256.json` | SHA-256 of each generated raw callback CSV gzip; large intermediate CSVs are not committed |
| `build-*.json`, `contract-build-command.json` | Actual compiler commands and pinned JUCE revision; original absolute workspace paths are provenance, not required paths |
| `*-started.json` | Exclusive per-phase markers, including UTC execution times |
| `SHA256.json` | Hashes of the committed evidence files (excluding itself) |

The registration and original failures remain unchanged. Supplementary
diagnostics explain observed differences; they do not retune the candidate,
thresholds, seeds or scorers. `onset_samples` logs reset application, not the
first novelty detector flag. Both are shown in the diagnostic traces.

## Reproduction

Requirements: Linux, GCC, Python with NumPy/SciPy, pinned JUCE 8.0.8.
Build an unmodified worktree at
`472b93e68d20bcad07a028cc67a17b50f8171838` first, then this candidate.
Use `Tools/TransposeNext/build.py --juce /path/to/JUCE --out /path/to/build`
in each worktree. Do not overwrite the original probe with the candidate.

From the candidate repository, choose new output directories:

```bash
export OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1
python Tools/TransposeNext/edge_validation.py freeze --bin /path/to/candidate-build --baseline /path/to/original-build/TransposeProbe --out /tmp/edge-reproduction
# Record/commit the new registration before rendering; never overwrite this evidence.
python Tools/TransposeNext/edge_validation.py sweep --bin /path/to/candidate-build --baseline /path/to/original-build/TransposeProbe --out /tmp/edge-reproduction
python Tools/TransposeNext/edge_validation.py targets --bin /path/to/candidate-build --baseline /path/to/original-build/TransposeProbe --out /tmp/edge-reproduction
python Tools/TransposeNext/edge_validation.py blocks --bin /path/to/candidate-build --baseline /path/to/original-build/TransposeProbe --out /tmp/edge-reproduction
python Tools/TransposeNext/edge_validation.py parity --bin /path/to/candidate-build --baseline /path/to/original-build/TransposeProbe --out /tmp/edge-reproduction
# CPU phase alone, after the other phases finish:
python Tools/TransposeNext/edge_validation.py cpu --bin /path/to/candidate-build --baseline /path/to/original-build/TransposeProbe --out /tmp/edge-reproduction
```

The runner uses exclusive start markers and rejects changed frozen inputs.
Keep failed/partial runs; use a new directory for a separately labelled rerun.
There is no resampling of real DI, no listening and no actual host in this suite.

`compile_diagnostics.py /path/to/candidate-build` rebuilds the three
supplementary native executables using the new build manifest. Run
`ASAN_OPTIONS=detect_leaks=0 /path/to/candidate-build/EdgeContractTests` for
the same contract/ASan/UBSan check used here; LSan was not available in this
execution environment. The reused JUCE objects are not sanitizer-instrumented.

For the original replay, extract PR #234's evidence at its recorded head to
a separate directory and run `replay_original.py ORIGINAL_DIR BIN_DIR OUT_DIR`.
The replay verifies the original manifest before measuring. Run
`inspect_onsets.py /path/to/candidate-build/EdgeOnsetTrace /tmp/edge-reproduction`
and `summarize_cpu.py /tmp/edge-reproduction` from their repository locations.
Never replace these committed measurements with another run.
