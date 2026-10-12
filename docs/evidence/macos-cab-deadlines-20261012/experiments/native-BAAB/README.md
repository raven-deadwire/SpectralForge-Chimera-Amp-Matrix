# Native BAAB diagnostic: independent evidence audit

The reverse-order native run independently verifies in full. **It does not reproduce a caller-QoS improvement.** The CAB sequence is B49 → A7 → A7 → B18 misses. This yields A14 versus B67 across equal-sized conditions, reversing the direction in the separate ABBA run (A18 versus B13). The forced-publication phase also shows no reproducible p99 improvement under B.

This bundle preserves the complete original results, including all misses and measured zeros. It changes no frozen product, published beta, deadline, or p99 criterion. The [paired conclusion](paired-conclusion.md) compares both allocations without treating their binary builds or temporal conditions as identical.

## Source and artifact binding

| Item | Verified value |
| --- | --- |
| Run | [38186063935, attempt 1](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38186063935) |
| Artifact | [11719919391](https://api.github.com/repos/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/artifacts/11719919391) |
| Original ZIP | [Artifact ZIP API](https://api.github.com/repos/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/artifacts/11719919391/zip) |
| ZIP bytes | 19,136,392 |
| ZIP SHA-256 | `e35401f002edb8bb7bbe6fbd91b68534e22137ea456c66cf710b337b905b64cd` |
| Controller | [6345092ddefd11d8833eda8c794b7943e0195ef1](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/commit/6345092ddefd11d8833eda8c794b7943e0195ef1) |
| Frozen product | [7b20cd9d1f90e3e2969c250712b199e893cf50cd](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/commit/7b20cd9d1f90e3e2969c250712b199e893cf50cd) |
| Frozen tree before and after | `1b565308db6adc00937f7154905728ba27bcd6d9`, clean |
| JUCE | `d6181bde38d858c283c3b7bf699ce6340c050b5d` |
| Prespecified order | 01-B user-interactive, 02-A inherited, 03-A inherited, 04-B user-interactive |

The manifest includes the original API artifact metadata and independently supplied controller file hashes. All match. Probe source and CMake hashes are unchanged from ABBA; runner and README hashes differ and are independently bound to this BAAB controller. Each run builds its own probe; the resulting executable hashes differ between allocations. Within each run, all A/B commands use that run's single probe executable.

Environment: Darwin 24.6.0, ARM64, Apple M1 (Virtual), VirtualMac2,1, three virtual cores, 7 GB, image `macos15` / `20260907.0337.1`, AppleClang `17.0.0.17000013`, CMake/CTest 4.4.3. Build metadata records Release, arm64, deployment target 12.0. The node is `sjc22-be112-fc4d49ae-a5e3-4fc5-99cf-c515ab6374a2-96CD4511522F.local`, distinct from ABBA's recorded VM node. Before/after environment fields agree except their timestamps. These records identify separate allocations; they do not establish physical-host equivalence or a controlled host load.

## Complete independent verification

The unchanged offline verifier returned exit `0`, `verified_complete`. The archive has 650 members: the checksum index and all 649 listed files, each hash verified. Compared with ABBA, BAAB additionally retains `build-metadata/CMakeOBJCXXCompiler.cmake`; the raw measurement schema and grid remain unchanged.

All **594 route populations and 950,400 intervals** verify: five CAB populations total 528,000 callback intervals, and four sentinel populations total 422,400 synthetic intervals. Each population retains 66 routes × 1,600 rows. Route identities, strict deadline flags, original p99 order statistic, phase/automation boundaries, callback indices, CPU endpoint differences/progression, and native clock-validity fields all verify. Every reported numerical summary and original timing/paired/phase receipt matches the raw rows. All 116 original miss records are retained: **115 CAB callback misses and one separate synthetic miss**.

The ordinary gate passes all five original CTests and all 66 route p99 criteria. Its original 66 timing, 198 paired, 132 phase, and 66 native worker receipts verify. All four diagnostic CAB populations also pass their original 66 p99 comparisons. Sentinel p99 comparisons remain descriptive and have no acceptance authority.

All 32 diagnostic commands complete successfully. All 16 A commands observe QoS 17 before/after workload; all 16 B commands enter at 17, successfully request 33, then observe 33 before/after workload. All 96 `pthread` QoS queries and 96 Mach policy queries succeed, with the latter returning default time-constraint policy. Every clock-resolution receipt reports 42 ns thread CPU resolution. No scheduling fallback or hidden higher-QoS gate replacement occurs.

## CAB outcomes and phases

Each row contains 105,600 measured callbacks. The first phase has 79,200 callbacks with the worker and automation; the final phase has 26,400 callbacks after worker shutdown with forced publication.

| Population | Actual caller QoS | Wall misses | CPU > budget | CPU ≤ budget | Worker / automation | Forced publication | Maximum wall, ms |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Ordinary gate | 17 in four timing processes | 34 | 11 | 23 | 30 | 4 | 31.674042 |
| 01-B | 33 | 49 | 4 | 45 | 47 | 2 | 37.240292 |
| 02-A | 17 | 7 | 2 | 5 | 5 | 2 | 4.213416 |
| 03-A | 17 | 7 | 6 | 1 | 7 | 0 | 5.682167 |
| 04-B | 33 | 18 | 15 | 3 | 18 | 0 | 21.338708 |

The largest CAB miss is 01-B, `original-v1` integration, 48 kHz / 256 / mono, callback **327**: **37,240.292 µs wall**, **119.792 µs paired CPU**, deadline **5,333.333333 µs**. No automation was requested on that callback. Most of its wall interval is not represented in the recorded CPU interval; no scheduler, host, memory, or waiting-event trace identifies the cause.

The largest wall miss whose paired CPU also exceeds budget is 04-B expanded extended, 48 kHz / 128 / stereo, callback **258**: **13,458.584 µs wall**, **13,461.333 µs CPU**, deadline **2,666.666667 µs**. The enclosing CPU interval is not a DSP-only interval.

The 02-A overall maximum (4.213416 ms) belongs to original integration, 44.1 kHz / 256 / stereo, with a longer 5.804989 ms deadline. Its largest actual miss is 4.135208 ms on original integration, 48 kHz / 64 / stereo. Maximum duration and maximum deadline miss must therefore be distinguished.

The ordinary CPU-over split is 10 worker-phase and one forced-phase miss. The four diagnostic trials have 27 CPU-over misses: 26 worker-phase and one forced-phase. Both A and B have two forced-phase wall misses in aggregate. The 01-B forced misses include original integration, 96 kHz / 64 / mono, phase index 27: **10,306.125 µs wall / 62.583 µs CPU**; and expanded extended at the same rate/buffer/channels, phase index 229: **717 µs wall / 719.291 µs CPU**. Thus higher caller QoS does not eliminate worker-absent forced-phase misses in this allocation.

All six forced-publication targets derive from one Mic-A key at position 0.85; separate kernel instances receive the same target response. The phase includes serialized transitions from outgoing configurations and the subsequent tail. It does not represent six distinct steady responses, and the raw records do not identify the exact fade/kernel state of every callback.

## Arithmetic controls

| Trial | Actual caller QoS | Synthetic misses | CPU > budget | Paced / burst misses | Maximum wall, ms |
| --- | ---: | ---: | ---: | ---: | ---: |
| 01-B | 33 | 0 | 0 | 0 / 0 | 0.259042 |
| 02-A | 17 | 1 | 0 | 1 / 0 | 2.023542 |
| 03-A | 17 | 0 | 0 | 0 / 0 | 1.324958 |
| 04-B | 33 | 0 | 0 | 0 / 0 | 0.388250 |

The single synthetic miss is 02-A, extended expanded label, **96 kHz / 128 / mono**, callback/phase index **787**: **1,519.167 µs wall**, **28.084 µs paired CPU**, deadline **1,333.333333 µs**. It contains fixed arithmetic and no CAB, IR worker, UI, or audio callback. Index 787 has first-phase residue 3, not the immediate post-scheduled-sleep position 1.

The 02-A overall synthetic maximum of 2.023542 ms is below its own 5.333333 ms deadline on the 48 kHz / 256 / stereo array label. Likewise, 03-A's 1.324958 ms maximum has a 2.902494 ms deadline. Their relationship to another route's shorter deadline is not a miss classification.

The different synthetic exposure prevents subtracting these durations from CAB measurements. Zero synthetic misses in a particular trial do not prove a CAB-specific cause for that trial's CAB misses. Across both native runs, synthetic misses occur with both low and high recorded thread CPU, without CAB or UI work; neither clock pattern uniquely diagnoses a product stage.

## Explicit BAAB matched-route contrasts

The supplied script checks the verified order before computing:

- Per-route B/A: `(metric_01B + metric_04B) / (metric_02A + metric_03A)`.
- Repeated B: `metric_04B / metric_01B`.
- Repeated A: `metric_03A / metric_02A`.

The table takes the median of 66 within-route ratios. It does not pool routes with different budgets or assume independent callbacks.

| Subset | Quantile | Median B/A | Routes B lower | Median late-B / early-B | Median second-A / first-A |
| --- | --- | ---: | ---: | ---: | ---: |
| All 1,600 | p50 | 1.007959 | 23 / 66 | 1.839500 | 1.643840 |
| All 1,600 | p99 | 1.031000 | 26 / 66 | 1.011481 | 1.061610 |
| Worker/automation 1,200 | p50 | 1.003559 | 31 / 66 | 1.770229 | 1.351934 |
| Worker/automation 1,200 | p99 | 1.053596 | 24 / 66 | 1.014911 | 1.106225 |
| Forced publication 400 | p50 | 1.014033 | 29 / 66 | 0.979361 | 0.979992 |
| Forced publication 400 | p99 | 1.236916 | 13 / 66 | 0.874302 | 0.540544 |

Same-QoS worker medians increase while forced-phase p99 decreases, so a single uniform speed factor does not describe the run. Whole CAB command groups span 103.31, 103.20, 119.78, and 124.49 seconds respectively; those spans include preparation and pacing and are not callback measurements. Changes in context, preparation/publication timing, or other untraced conditions remain possible.

The B/A direction reverses between ABBA and BAAB for total misses and whole-population p99. Forced-phase p99 provides no repeated improvement under B. These observations do not justify changing product caller QoS, IR worker priority, convolution, memory, or UI code. They also do not prove that high QoS itself caused the worse BAAB result.

## Reproduction and files

Download the unchanged artifact ZIP into a cache as `diagnostic-38186063935-BAAB.zip` and run:

```sh
python ../verifier/analyze_diagnostic_artifacts.py \
  --archives archives-manifest.json \
  --archive-base /path/to/archive-cache \
  --output recomputed

python summarize_verified_contrasts.py \
  --input recomputed \
  --output recomputed
```

The [full route CSV](route-summary.csv) preserves all 594 measured populations, including zeros. The [raw miss CSV](deadline-misses.csv) retains every original field from 116 CAB-plus-sentinel misses. [Validation](validation.json), [condition summaries](condition-summary.json), [QoS aggregates](condition-aggregate.json), [phase results](phase-summary.csv), [paced residues](paced-residue-summary.csv), [actual scheduling receipts](qos-receipts.json), and [BAAB-specific contrasts](matched-route-contrasts.json) remain separate. The ZIP is not extracted in full or duplicated in this bundle.

The released beta's original evidence remains unchanged. This is a later diagnostic on frozen product source, not a replacement acceptance run or a claim that a DAW, CoreAudio callback, complete plug-in routing mode, or GUI workload has been validated.
