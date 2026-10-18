# Independent verification of native CAB scheduling diagnostics

This directory contains an offline verifier for the separately built macOS diagnostic controller. It imports no controller code and never calls the controller's `summarize` function. It reads immutable downloaded ZIPs, recomputes timing facts from the original rows, and checks those facts against the report and original logs. It changes no product source, released binary, test gate, measured callback, or archive.

The included preserved run failed during configuration. Its verification is **valid incomplete evidence**, with no measured populations; it is not a zero-miss result. No pending or completed ABBA/BAAB performance artifact was used to create the included failure-case outputs or synthetic verifier checks.

## Reproduce the included failed-run check

Use Python 3.10 or later; only the standard library is required. Download the original ZIP into an archive cache under its manifest filename, then run:

```sh
python analyze_diagnostic_artifacts.py \
  --archives failed-run-manifest.json \
  --archive-base /path/to/archive-cache \
  --output failed-run-recomputed
```

Relative ZIP paths resolve against `--archive-base`, or against the manifest's directory when that option is omitted. The verifier never extracts ZIP members to disk. Outputs must not overwrite the input ZIP or manifest. No original ZIP is bundled here.

| Bound item | Value |
| --- | --- |
| Workflow run | [38185416392, attempt 1](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38185416392) |
| Artifact ID | [11718754715](https://api.github.com/repos/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/artifacts/11718754715) |
| Original ZIP API | [Download ZIP](https://api.github.com/repos/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/artifacts/11718754715/zip) |
| Cache filename | `diagnostic-38185416392.zip` |
| ZIP SHA-256 | `9f11e9032aad0902223d89e29194977fe9929cb9960ecbc1e0b474eb45aee3d6` |
| ZIP size | 4,290 bytes |
| Controller commit | `144612f8cccc32e4b56bae92be758d7e1fc27bde` |
| Frozen product commit | `7b20cd9d1f90e3e2969c250712b199e893cf50cd` |
| Prespecified order | ABBA |

The artifact's GitHub API metadata is copied into `failed-run-manifest.json`. Its `workflow_run.head_sha` binds the controller commit; the archive report does not itself contain that commit. The report separately attests controller file hashes and clean frozen product source. If independently fetched controller file hashes are available, put the complete filename-to-SHA-256 map in `expected_controller_files`; the verifier will additionally compare it exactly. Neither these attestations nor ZIP integrity constitute a trace of every host operation.

For another diagnostic artifact, add a manifest record with `label`, `path`, `sha256`, `artifact_id`, `expected_run_id`, `expected_run_attempt`, `expected_controller_sha`, `expected_frozen_sha`, `expected_juce_sha`, `expected_order`, and the original `artifact_metadata`. Supported orders are ABBA and BAAB. The frozen product and JUCE pins remain `7b20cd9d1f90e3e2969c250712b199e893cf50cd` and `d6181bde38d858c283c3b7bf699ce6340c050b5d` respectively. A new source/protocol requires review of this verifier; changing a manifest cannot silently relax those pins or the population grid.

## Scope of verification

Each complete run contains nine separately executed population sets: one ordinary CAB gate, four diagnostic CAB trials, and four arithmetic sentinel trials. Each population contains **66 route configurations × 1,600 rows = 105,600 intervals**. Across the run that is 594 route populations and 950,400 intervals: 528,000 CAB callback intervals and 422,400 synthetic sentinel intervals. Sentinel rows are not audio callbacks.

The CAB grid is integration `original-v1`, `expanded-v2`, and `array-v3-6x10` at 44.1, 48, and 96 kHz, buffers 64 and 256, mono and stereo (36 routes), plus extended `expanded-v2` at those three rates, buffers 32, 64, 128, 256, and 512, mono and stereo (30 routes). Sentinel keys retain that grid with explicit `scheduler-sentinel-` suite and `sentinel-` engine prefixes. Each trial and workload must occupy its own directory.

The verifier checks the download digest against the manifest and original GitHub artifact metadata, then checks every ZIP member against `SHA256SUMS.json`, including duplicate/missing members. It verifies the run, order, controller metadata, clean frozen source before and after, and JUCE pin when measured/completed. Before-only evidence from an early failure is identified explicitly.

For every observed raw population it verifies the route identity, path, row order, schema, 1,600-row completeness, original phase boundary at index 1,200, automation flag boundary at index 80, and unchanged block-period deadline. Each miss remains the original `wall_us > block_size * 1e6 / sample_rate` comparison. p99 uses sorted index 1,584 and passes only when strictly below the deadline. Partial rows remain available, but a partial population gets no full-population p99 pass or miss total.

Clock checks preserve both original endpoints and their delta, require valid native thread CPU readings with no clock errors, enforce cumulative clock progression, and verify that unused cycle fields remain zero. **CPU time does not replace the wall gate.** A CPU envelope longer than wall is not rejected: the endpoints enclose different measurement boundaries.

The verifier checks reported route p50/p99/max, first maximum index and its paired CPU, miss counts, raw CSV hashes, and every reported miss field against the raw rows. It also checks the original C++ `TIMING`, `PAIRED_TIMING`, and phase receipts, including paired CPU readings and independently sorted CPU p99. The C++ paired receipt selects the last tied maximum; the Python controller summary selects the first. Both selections are checked against their actual definitions, without conflating them.

The ordinary gate retains the original five CTest names, command selection, timeout, serial execution, complete JUnit results, and original untruncated outputs. Diagnostic command flags and suite order must match the protocol. QoS verification uses original before/after workload observations (17 for inherited, 33 for user-interactive), request/setter receipts, and original process results. A scheduling label alone is insufficient. Snapshot query fields and return codes remain in the outputs. A recorded gate, worker, or execution failure remains a failure even if later trials pass.

## Outputs and status codes

| Output | Meaning |
| --- | --- |
| `validation.json` | Integrity, source/run/controller binding, actual coverage, original failures, missing route keys, gate state, and limitations. |
| `route-summary.csv` | Every verified observed route, including measured zero-miss routes, its unchanged p99 gate, maximum pairs, CPU-over split, and phase totals. Unobserved routes are listed as missing in validation instead of invented zero-valued measurements. |
| `deadline-misses.csv` | Every original field of every verified raw deadline miss; native CAB has blank synthetic-only fields. The immutable ZIP remains primary, including any corrupt/unverified rows. |
| `condition-summary.json` / `.csv` | Per-trial and workload coverage and facts, including zero observations for failed/not-run populations. |
| `condition-aggregate.json` / `.csv` | Within-run sums for each workload and QoS condition. CAB and sentinel facts remain separate; route p99 is not pooled across different deadlines. |
| `phase-summary.csv` | Per-route paced/worker and unpaced/forced-publication facts. |
| `paced-residue-summary.csv` | Per-trial first-phase index modulo 8, retaining zero-count bins and CPU-over counts. |
| `qos-receipts.json` | Original actual caller QoS, request, clock, and result snapshots for every observed command. |

Exit `0` means structurally verified complete evidence with no recorded execution/gate failure. Exit `3` means structurally verified incomplete evidence or a retained execution/gate failure. Exit `2` means an integrity, raw, provenance, or receipt inconsistency. These status codes describe evidence verification, not product approval. An ordinary callback deadline miss does not become a hidden command failure or disappear merely because p99 passes.

The included `failed-run-validation` records exit `3`, `verified_incomplete_failure`, the original configure return code `1`, and the original error. It preserves all nine expected populations with zero observed routes and **null full-population miss totals**. No JUCE completion, source-after check, CTest result, callback, or sentinel measurement is invented.

## Interpretation limits

The 66 routes refer to the frozen CAB harness's configuration populations. They are neither 66 CPU-only measurement envelopes nor a complete DAW/CoreAudio routing matrix. Every CAB sample's thread CPU interval encloses its wall interval with separate clock reads. Wall minus CPU is not exclusively scheduler delay, and CPU time is not proof that a DSP algorithm ran for the entire measured time. Host/guest scheduling and clock behavior remain possible contributors.

The arithmetic sentinel uses 256 fixed scalar iterations, with no CAB, IR worker, UI, or audio callback. Sample rate, buffer, and channel names supply deadline references while the synthetic workload remains constant. Its timed intervals are much shorter, and its work and elapsed exposure differ from CAB. Comparing recorded anomalies is informative, but equal interval counts do not establish equal exposure or equal miss probabilities, and subtracting sentinel elapsed time from CAB is invalid.

The first 1,200 intervals have a 1 ms sleep after indices divisible by 8. Residue 1 therefore marks the next scheduled interval position, not measured wake-up latency. The final 400 CAB intervals run after worker shutdown with forced six-slot publication; sentinel final intervals are arithmetic burst controls. Keep those phases separate. The common nominal sleep is implemented through `std::this_thread::sleep_for`; no sleep interval is subtracted from measured elapsed time.

The ordinary CTest order is original → expanded → extended → layouts. All diagnostic A/B trials use original → expanded → layouts → extended. The ordinary gate is not an identical-order A/A trial. Within-probe ABBA/BAAB comparisons retain the same diagnostic suite order. The QoS intervention also covers preparation, warmup, and sleeps, which can change worker publication and fade timing. Its effect is a whole test-context effect, not isolation of a single audio-thread scheduling mechanism. The phase split retains the forced-publication arm for more closely matched comparisons.

## Bounded verifier checks

```sh
python check_diagnostic_analyzer.py \
  --failed-archives failed-run-manifest.json \
  --archive-base /path/to/archive-cache \
  --output verifier-checks-recomputed.json
```

`verifier-checks.json` records accepted valid fixtures and rejected corruptions, including the original failed ZIP, a full synthetic 66 × 1,600 grid with zero-miss routes, first/last maximum ties, partial populations, changed deadline flags, changed p99, deleted reported misses, clock errors/deltas/progression, misplaced phase/automation boundaries, wrong actual QoS, and altered archive/member provenance. Synthetic fixtures are deleted and explicitly marked as **not performance evidence**. The original ZIP digest is checked again after the tests.
