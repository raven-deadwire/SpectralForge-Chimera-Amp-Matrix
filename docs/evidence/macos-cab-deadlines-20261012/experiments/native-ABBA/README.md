# Native ABBA diagnostic: independent evidence audit

The completed ABBA artifact independently verifies in full. **This run does not establish a reproducible product performance improvement.** Actual inherited/user-interactive caller QoS was observed as intended, but the CAB miss sequence fell throughout the run, including after returning to inherited QoS. The unchanged inherited condition also showed substantial changes in its latency distribution. An arithmetic-only control produced deadline excursions with no CAB, IR worker, or UI.

No product source, published beta, acceptance deadline, or original measurement was changed by this audit. The separately planned BAAB experiment is not included here.

## Source and artifact binding

| Item | Verified value |
| --- | --- |
| Run | [38185786916, attempt 1](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38185786916) |
| Artifact | [11720400815](https://api.github.com/repos/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/artifacts/11720400815) |
| Original ZIP | [Artifact ZIP API](https://api.github.com/repos/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/artifacts/11720400815/zip) |
| ZIP bytes | 18,898,030 |
| ZIP SHA-256 | `97a35a4fccbf1a1e19059a9d2a377a3844035c0bbb8a0dce6c3064e0614e24b9` |
| Controller | [5bb5b65e4ae706b0e4c532be824108bd36485b91](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/commit/5bb5b65e4ae706b0e4c532be824108bd36485b91) |
| Frozen product | [7b20cd9d1f90e3e2969c250712b199e893cf50cd](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/commit/7b20cd9d1f90e3e2969c250712b199e893cf50cd) |
| Frozen tree before and after | `1b565308db6adc00937f7154905728ba27bcd6d9`, clean |
| JUCE | `d6181bde38d858c283c3b7bf699ce6340c050b5d` |
| Prespecified order | 01-A inherited, 02-B user-interactive, 03-B user-interactive, 04-A inherited |

`archives-manifest.json` includes the original GitHub artifact metadata and the independently supplied SHA-256 values for all four controller files. They match the archive's controller attestations exactly. The offline verifier did not import the runner or its summary function.

The native environment was macOS/Darwin 24.6.0, ARM64, Apple M1 (Virtual), VirtualMac2,1, 3 virtual cores, 7 GB, hosted runner image `macos15` / `20260907.0337.1`. The recorded VM node was `iad20-gt1018-1bbc9d3f-22b5-49d3-9dd1-c12182cad3f7-F272F682C0C1.local`. Before/after environment fields agree except their recorded timestamps. Build metadata records Release, arm64, deployment target 12.0, and AppleClang `17.0.0.17000013`.

## Complete independent verification

The verifier returned exit `0`, `verified_complete`, with no integrity, provenance, raw, summary, or receipt errors. The ZIP has **649 members**: the checksum index and all 648 files listed in it. The archive digest and every listed member hash verify. All **594 route populations and 950,400 intervals** are present: five CAB populations, including the ordinary gate, total 528,000 callback intervals; four sentinel populations total 422,400 synthetic intervals.

Every population contains 66 routes × 1,600 rows = 105,600 intervals. The original route grid, row indices, phase/automation boundaries, strict wall deadline flags, p99 order statistic, native CPU endpoints/deltas, unused cycle fields, and cumulative clock progression verified. Every reported route and population summary, including all original miss fields, matches the raw rows. The original numerical timing/paired/phase logs also match. First-versus-last maximum tie conventions are checked separately.

The ordinary CTest gate passed all five original tests and all 66 original route p99 criteria. Its 66 timing, 198 paired timing, 132 phase, and 66 native worker receipts verified. All four diagnostic CAB populations also passed their unchanged 66-route p99 comparisons. Sentinel p99 values were retained as descriptive comparisons, with no acceptance authority. **The CAB records still contain 55 individual callback misses, and the separate synthetic controls contain four misses.**

All 32 diagnostic commands completed with return code zero. All 16 A commands observed caller QoS 17 before and after workload. All 16 B commands entered with QoS 17, successfully set 33, and observed 33 before and after workload. All 96 `pthread` QoS queries succeeded. All 96 Mach time-constraint snapshots returned the default policy. The 32 clock-resolution receipts report a successful query and 42 ns thread CPU resolution; this is clock metadata, not a bound on observed execution latency.

## Measured outcomes

Each row below contains 105,600 original intervals. `CPU > budget` counts wall misses whose enclosing thread CPU interval also exceeds that route's deadline. It does not identify which instruction, algorithm, or host activity used that interval.

### CAB callbacks

| Population | Actual caller QoS | Wall misses | CPU > budget | CPU ≤ budget | Worker/automation misses | Forced-publication misses | Maximum wall, ms |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Ordinary gate | 17 in four CAB timing processes | 24 | 9 | 15 | 20 | 4 | 20.938709 |
| 01-A | 17 | 17 | 0 | 17 | 15 | 2 | 20.554084 |
| 02-B | 33 | 7 | 1 | 6 | 7 | 0 | 20.097459 |
| 03-B | 33 | 6 | 3 | 3 | 6 | 0 | 2.823083 |
| 04-A | 17 | 1 | 1 | 0 | 1 | 0 | 3.255750 |

Maximum wall duration is over every route's samples, so it is not necessarily itself a deadline miss. The 03-B maximum belongs to a 48 kHz / 256 / stereo array route with a 5.333333 ms deadline; its largest actual miss is 2.358083 ms on expanded integration, 44.1 kHz / 64 / stereo. The 04-A maximum belongs to a 44.1 kHz / 256 / stereo expanded route with a 5.804989 ms deadline; its sole miss is 1.258 ms on expanded integration, 96 kHz / 64 / stereo.

The first phase contains 79,200 callbacks per CAB population, the forced-publication phase 26,400. The ordinary gate's CPU-over misses split 8 in the worker phase and 1 in forced publication. Among the four CAB trials, all five CPU-over misses occur in the worker phase; both forced-publication misses are in 01-A and have CPU below the deadline. Zero forced misses in later trials remain measurements of these populations, not proof that forced publication always meets deadlines.

### Arithmetic controls

| Trial | Actual caller QoS | Synthetic misses | CPU > budget | Paced misses | Burst misses | Maximum wall, ms |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| 01-A | 17 | 2 | 0 | 1 | 1 | 4.992000 |
| 02-B | 33 | 1 | 0 | 1 | 0 | 2.662458 |
| 03-B | 33 | 0 | 0 | 0 | 0 | 0.500833 |
| 04-A | 17 | 1 | 1 | 1 | 0 | 0.816459 |

All four original synthetic misses are retained below. These route names are deadline-reference labels: no modeled cabinet is instantiated by the sentinel.

| Trial | Synthetic suite / engine | Rate / buffer / channels | Index / phase index | Phase | Wall, µs | Paired CPU, µs | Budget, µs |
| --- | --- | --- | --- | --- | ---: | ---: | ---: |
| 01-A | extended / expanded-v2 | 44100 / 32 / 1 | 1180 / 1180 | paced | 767.375 | 69.917 | 725.623583 |
| 01-A | extended / expanded-v2 | 44100 / 32 / 1 | 1212 / 12 | burst | 4992.000 | 13.375 | 725.623583 |
| 02-B | integration / expanded-v2 | 96000 / 64 / 1 | 480 / 480 | paced | 2662.458 | 8.792 | 666.666667 |
| 04-A | integration / array-v3-6x10 | 96000 / 64 / 1 | 181 / 181 | paced | 816.459 | 820.417 | 666.666667 |

The largest synthetic excursion is in the unpaced burst phase, with no sleep inside the timed interval. None of the four sentinel misses is at first-phase index modulo 8 equal to 1. Therefore these four records cannot all be explained merely as the immediate next interval after a scheduled sleep. The residue is only a position proxy; the harness does not record actual wake-up timestamps or off-CPU traces.

The last sentinel miss also has CPU greater than budget, despite executing only 256 fixed scalar arithmetic iterations and no CAB, IR worker, or UI. It is a concrete reason to avoid treating every CPU-over historical miss as proof of expensive convolution. It does not prove a specific clock/accounting problem or establish that every CAB outlier has the sentinel's cause.

## QoS contrast and time/context drift

Across the two equal-sized CAB trials per condition, A has 18 / 211,200 misses and B 13 / 211,200. This raw difference is not sufficient evidence of a QoS fix. The observed order is **17 → 7 → 6 → 1**: returning from B to A still reduces misses. The repeated A condition varies from 17 to 1, a larger shift than the total A-versus-B difference of five events. Only two trials per condition are available, and callbacks are not assumed independent.

The distribution changes also run against a simple “fewer misses means faster processing” interpretation. For each of 66 matched routes, the following table compares that route's recorded quantile in late A versus early A, then takes the median of those 66 ratios. The B/A column compares the mean of the two B quantiles with the mean of the two A quantiles for the same route. These are descriptive within-route comparisons, not pooled quantiles or causal estimates.

| Sample subset | Quantile | Median route B/A ratio | Routes B lower than A | Median route late-A/early-A ratio | Routes late A lower than early A |
| --- | --- | ---: | ---: | ---: | ---: |
| All 1,600 | p50 | 0.983790 | 35 / 66 | 2.763607 | 0 / 66 |
| All 1,600 | p99 | 0.958055 | 39 / 66 | 2.511561 | 1 / 66 |
| Worker/automation 1,200 | p50 | 0.992562 | 42 / 66 | 4.285062 | 0 / 66 |
| Worker/automation 1,200 | p99 | 0.965802 | 40 / 66 | 2.521698 | 1 / 66 |
| Forced publication 400 | p50 | 0.995177 | 36 / 66 | 1.196351 | 19 / 66 |
| Forced publication 400 | p99 | 1.001582 | 33 / 66 | 1.136180 | 19 / 66 |

All 66 late-A whole-population medians are higher than their early-A counterparts even though misses fall from 17 to 1. The more closely matched forced-publication phase has a median B/A p99 ratio of 1.001582, with exactly half the routes lower under B. This run supplies no consistent forced-phase p99 improvement.

Whole-command durations also change: CAB suites span 98.13, 95.20, 125.92, and 129.07 seconds for 01-A through 04-A respectively. Those durations include setup, pacing, and work outside the callback interval and must not be substituted for callback measurements. They support retaining the time/context qualification rather than attributing all observed changes to a caller QoS label.

ABBA balances simple order effects under suitable assumptions, but it cannot identify arbitrary host/context shifts from only four trials. The QoS intervention covers preparation, warmup, sleeps, and the caller's measured processing. It can change IR worker publication and fade timing even though worker priority and product source remain unchanged. This audit does not identify whether scheduling, frequency/load, publication state, or another factor caused the observed distribution shifts.

## Measurement limits and release separation

The 66 route configurations belong to the frozen CAB harness, not a DAW/CoreAudio device or the complete plug-in processing/routing/UI path. The ordinary gate's route order differs from the diagnostic probe's order and is not an identical-order A/A trial. The arithmetic control has a different workload and duration exposure. Its elapsed time must not be subtracted from CAB timings.

Wall and CPU clocks enclose different intervals. Wall minus CPU is not exclusively scheduling delay; a CPU-over record does not uniquely classify a DSP bottleneck. No thread scheduling trace, host trace, actual wake-up timestamp, or per-callback active-kernel/fade state is provided by these rows. A source-level claim about the precise cause requires additional evidence.

The released `1.3.1-beta.1` evidence remains source-bound to its original release run. This is a later, separately controlled diagnostic on the same frozen product source. It neither alters the release's 54 macOS miss records nor warrants a claim that all real-time conditions or DAW usage are safe. No product QoS, convolution, allocation, or UI change is justified by this ABBA run alone.

## Reproduce and inspect

Place the unchanged ZIP in an archive cache as `diagnostic-38185786916-ABBA.zip`, then invoke the independently reviewed diagnostic verifier:

```sh
python ../verifier/analyze_diagnostic_artifacts.py \
  --archives archives-manifest.json \
  --archive-base /path/to/archive-cache \
  --output recomputed

python summarize_verified_contrasts.py \
  --input recomputed \
  --output recomputed
```

The full [route table](route-summary.csv) includes all 594 observed route populations and measured zeros. [Every original miss field](deadline-misses.csv) is retained for all 59 CAB-plus-sentinel misses. [Validation](validation.json), [per-trial conditions](condition-summary.json), [per-QoS aggregates](condition-aggregate.json), [phase facts](phase-summary.csv), [paced residues](paced-residue-summary.csv), [actual QoS receipts](qos-receipts.json), and [descriptive matched-route contrasts](matched-route-contrasts.json) are available separately. The original ZIP remains the primary source, including every zero-miss row; it is not duplicated in this repository bundle.
