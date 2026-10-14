# EQ high-rate attribution evidence, 2026-10-12

This directory preserves observations for development Draft PR
[#52](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/pull/52).
Read [the engineering report](../../EQ_HIGH_RATE_ATTRIBUTION_20261012.md) for
causal findings and the October 23 qualification decision.

**Physical DAW/audio-interface performance and final installer acceptance remain
`NOT_ASSESSED`.** A functional CTest or CSV-accounting PASS is not a performance
acceptance decision. Near-100% aggregate production can include late blocks,
deadline misses and long bursts of catch-up.

## Preserved result and source identities

The completed chronological N/4-hop attack fix is
[`beb72f520721685f3698883297371be7def92198`](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/commit/beb72f520721685f3698883297371be7def92198),
validated by [run 38224717793](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38224717793).
Its [84 original evidence files](../eq-transient-fix-20261012/README.md) are
unchanged, including their original LF/CRLF bytes. Their canonical SHA-256 tree
digest is `bb47058bd2798bea744da7d61754aefb5b7dbd5d8488086c313a5ae92897bc2e`.
This follow-up does not reopen or rerun the original negative-control diagnosis.

The optimization and attribution implementation is
[`ddb0880bd61a4c794236783b869767b96208bf41`](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/commit/ddb0880bd61a4c794236783b869767b96208bf41).
The follow-up
[`72e361ba91ea2c6b8d830921a71275fa25cba493`](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/commit/72e361ba91ea2c6b8d830921a71275fa25cba493)
only fixes strict immutable-byte provenance across checkout line endings.
All canonical Git blob bytes for C++, CMake and workflows are identical between
those two commits; Windows worktree line endings are recorded separately.
They are separate executions, not pooled repetitions or relabelled results.

| Dataset | Source and scope | Evidence |
|---|---|---|
| Minimal local paired experiment | Exact retained header versus candidate; direct local JUCE link | [Recipe, A/A and A/B](local-paired/README.md) |
| Full CMake local execution | `ddb0880`; four CTests and 22 direct attribution cases; native UI NOT_RUN | [Manifest](local-full-ddb0880/receipt-manifest.json), [cost](local-full-ddb0880/paired-analysis.md), [attribution](local-full-ddb0880/attribution-analysis.md) |
| First Linux CI | `ddb0880`, run `38240150849`; six CTests and 90 attribution cases | [Manifest](ci-38240150849/Linux/receipt-manifest.json), [cost](ci-38240150849/Linux/paired-analysis.md) |
| First macOS CI | `ddb0880`, run `38240150849`; six CTests and 90 attribution cases | [Manifest](ci-38240150849/macOS/receipt-manifest.json), [cost](ci-38240150849/macOS/paired-analysis.md) |
| First Windows CI | `ddb0880`; compilation succeeded, provenance failed, tests NOT_RUN | [Result](ci-38240150849/Windows/result.json), [failure excerpt](ci-38240150849/Windows/provenance-failure.log) |
| Second Linux CI | `72e361ba`, run `38243938568`; six CTests and 90 attribution cases | [Manifest](ci-38243938568/Linux/receipt-manifest.json), [cost](ci-38243938568/Linux/paired-analysis.md), [attribution](ci-38243938568/Linux/attribution-analysis.md) |
| Second Windows CI | `72e361ba`, run `38243938568`; six CTests and 90 attribution cases | [Manifest](ci-38243938568/Windows/receipt-manifest.json), [cost](ci-38243938568/Windows/paired-analysis.md), [attribution](ci-38243938568/Windows/attribution-analysis.md) |
| Second macOS CI | `72e361ba`, run `38243938568`; six CTests and 90 attribution cases | [Manifest](ci-38243938568/macOS/receipt-manifest.json), [cost](ci-38243938568/macOS/paired-analysis.md), [attribution](ci-38243938568/macOS/attribution-analysis.md) |

The [read-only integrity record](evidence-verification.json) verifies preserved
raw and derived bytes. [Tested-code equivalence](tested-code-equivalence.json)
keeps the final evidence-only commit separate from the executed source.

The [completed second-run jobs](ci-38243938568/workflow-jobs.json) record all
three successful OS jobs. The [macOS two-run cost comparison](macos-two-run-comparison.md)
preserves the repeated 192 kHz / FFT 2048 / full-FIFO and 44.1 kHz / FFT 8192 /
80 ms counterexamples. The candidate remains in the development Draft; those
unresolved observations and unperformed host/package qualification withhold
final October 23 acceptance.

The [first-run attribution audit](ci-38240150849/attribution-analysis.md) includes
no-op/EQ/full-route controls, actual CPU counters, positive-sleep wake delays,
source-time reconciliation, consumer gaps excluding the first setup interval,
rendering costs and retained counterexamples. First-run JSON supplements keep
CPU residuals and rebase-trigger classifications. The [portability evidence](provenance-portability/validation-result.json)
retains all seven checkout/corruption scenarios; preserving canonical Git bytes
does not replace the separate actual-checkout-byte check.

## Raw receipts and derived reports

Each `receipt-manifest.json` records the exact tested source, workflow/artifact
identity where applicable, downloaded ZIP digest, original raw length/SHA-256,
stored SHA-256 and any compression. The downloaded full CI artifacts also retain
native screenshots. This repository directory retains all numeric CSV receipts,
JUnit, test logs and build/source/runner provenance needed for this diagnosis.
PNG screenshots and build binaries are not duplicated here.

Large callback CSVs are losslessly stored as `.csv.gz` with gzip timestamp zero.
The original CSV byte digest is checked after decompression. CSV values, line
endings and outlier rows are not edited. `paired-analysis.*`,
`attribution-audit.json` and `attribution-supplement.json` are derived read-only
audits, separate from raw receipts. Absolute scratch paths in those reports
describe where analysis ran; the matching manifest gives durable repository
locations and source identities.

Large derived attribution JSON files also use `.json.gz`. Their separate
`derived-analysis-manifest.json` records raw and compressed hashes and can be
read with the same materializer. The audit's structural/FIFO checks and its
`cpu_measurement_quality` are separate: CPU_COUNTER_INCONSISTENT rows cannot
support unreconciled process-aggregate inference, and Windows CPU comparisons
retain their observed 15.625 ms quantization limitation.

Important distinctions retained in the raw evidence:

- Update wall time is inclusive analyzer cost. Actual paired thread CPU also
  includes FIFO feeding. Legacy FFT-only time includes a magnitude wrapper and
  cannot be directly equated with candidate real-transform-only time.
- Native process, producer and message CPU are separate measured counters.
  Update/path/paint/dispatch are inclusive wall durations. Nested CF/AppKit,
  FFT and paint intervals must not be added and called CPU.
- Native attribution's source classification uses a coarse 95% lower bound.
  `NEAR_NOMINAL_DIAGNOSTIC` neither means exactly 100% production nor a PASS.
- Due-sample pacing retains clock debt and reports catch-up; it does not emulate
  a hardware-driven real-time callback. Historical sleep/rebase controls retain
  their discarded source time.
- Pending FIFO samples at stop are not losses. Native short captures, paired
  cost and parity have separate accounting from intentional overflow fixtures.
  A 192 kHz × 500 ms stall deliberately exceeds 65,536-frame capacity, rejects
  30,464 frames, discards 65,536 stale frames and tests fresh-window recovery.
- Callback execution overruns, non-improving cost cells and the process/thread
  counter mismatches in both macOS runs (including second-run case 85) are
  preserved even where functional tests pass.

## Reproduce the read-only audit

The following commands materialize original receipt bytes into a new empty
scratch directory, then read them. They do not rerun a benchmark, require native
audio hardware, or modify the retained evidence. Use Python 3 with its standard
library and substitute another manifest for a different source/platform.

```sh
python3 docs/evidence/eq-high-rate-attribution-20261012/materialize_receipts.py \
  docs/evidence/eq-high-rate-attribution-20261012/ci-38240150849/macOS/receipt-manifest.json \
  /tmp/chimera-eq-ddb0880-macos
python3 docs/evidence/eq-high-rate-attribution-20261012/analyze_native_eq.py \
  /tmp/chimera-eq-ddb0880-macos/eq-attribution \
  --output /tmp/chimera-eq-ddb0880-macos-audit.json --table
python3 docs/evidence/eq-high-rate-attribution-20261012/eq-performance-report.py \
  /tmp/chimera-eq-ddb0880-macos --platform macOS \
  --expected-source ddb0880bd61a4c794236783b869767b96208bf41 \
  --output-prefix /tmp/chimera-eq-ddb0880-macos-cost
python3 docs/evidence/eq-high-rate-attribution-20261012/eq-functional-receipt-report.py \
  /tmp/chimera-eq-ddb0880-macos \
  --report-prefix /tmp/chimera-eq-ddb0880-macos-cost
```

Omit the materializer's destination argument for byte verification only. Local
headless receipts restore the original `eq-attribution-headless` subdirectory;
native receipts restore `eq-attribution`. Their scope must not be interchanged.

For a new execution, use the source-bound CMake/CTest commands in
`.github/workflows/graphical-eq.yml` and keep the full provenance/JUnit/raw CSV
artifact. Do not substitute these diagnostic commands for a physical-host
qualification session.
