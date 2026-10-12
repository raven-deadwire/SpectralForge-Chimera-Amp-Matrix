# Final local EQ analyzer validation — independent analysis

Source files exactly match `beb72f520721685f3698883297371be7def92198`. Four local tests passed in **170.78s**. No functional, sample-accounting, spectrum-coverage, or audio-invariance failure was found. **Real-time acceptance remains open:** individual callback deadline overruns and native timing outliers were recorded.

## Source identity

The production, test and build files match `beb72f520721685f3698883297371be7def92198` byte for byte. Documentation/evidence was collected separately after the test run; the JSON preserves the observed untracked evidence paths. CMake-generated `chimera-build-version.json` still identifies `360a2d843981691cc8670f292610dbd7c2816bd7` (`1.3.1-preview.360a2d8439`). These are locally rebuilt test executables from changed source, not a cleanly configured or approved beb72f5 test installer.

## Functional results

- **18/18** temporal fixtures detected their attack within the explicit observation bound; all had zero FIFO drops.
- The three disappearing early attacks are visible after40ms: **−24.0521dBFS** at96k/2048, **−26.851dBFS** at192k/2048, and **−29.984dBFS** at192k/4096.
- **90/90** partition cases: exact0dB differences in raw bins, smoothed bins and displayed pixels; minimum observed peak−36.845dBFS.
- **24/24** finite-FIFO fixtures conserve every sample and recover. Only the intended192k/500ms cases overflow:96,000 produced =30,464 rejected +65,536 discarded on recovery. This holds at every FFT size.
- Exactly full65,536-sample queues consumed without loss in **15.333–19.324ms** across the six configurations; these are synthetic bounded catch-up measurements.
- **9/9** whole-processor comparisons are bit identical, maximum error0, with **42 existing host-latency samples unchanged** through capture toggles, resolution changes, impulses and intentional FIFO overflow. Across all cases,884,736 stereo frames /1,769,472 channel samples were compared.
- Underlying tests also passed122EQ host-parameter/project/A-B/legacy/factory-default contracts,24-node automation/gestures, and the existing3,686-parameter processor contract.

`rawDB` reports the latest actual FFT; an early burst can legitimately be absent from that last window while its displayed envelope remains present. Large FFTs need real observation time:96k/8192 late-burst display is−79.49dBFS at40ms and−31.44dBFS after the bounded tail. At48k/8192, the first40ms batch has no full hop. The fixtures begin with N real silent samples; they do not establish sample-zero startup impulse behavior.

## Analyzer CPU — controlled25Hz-equivalent consumer benchmark

Each row includes160 measured calls, including no-FFT calls. Percentages are mean update wall time ×25Hz as an equivalent share of one core; they are neither total application CPU nor DAW audio-thread acceptance.

| Rate | FFT | Update p50(ms) | p99(ms) | Max(ms) | Mean(ms) | One-core equivalent |
|---:|---:|---:|---:|---:|---:|---:|
| 48kHz | 2048 | 0.533 | 0.824 | 0.881 | 0.529 | 1.323% |
| 48kHz | 4096 | 0.759 | 0.795 | 0.820 | 0.714 | 1.784% |
| 48kHz | 8192 | 0.599 | 0.982 | 1.303 | 0.637 | 1.594% |
| 96kHz | 2048 | 1.061 | 1.451 | 1.786 | 1.081 | 2.701% |
| 96kHz | 4096 | 0.945 | 1.269 | 1.496 | 0.936 | 2.340% |
| 96kHz | 8192 | 1.165 | 1.602 | 1.794 | 1.160 | 2.901% |
| 192kHz | 2048 | 2.072 | 2.563 | 3.351 | 2.132 | 5.330% |
| 192kHz | 4096 | 1.896 | 2.332 | 3.408 | 1.916 | 4.791% |
| 192kHz | 8192 | 2.444 | 3.112 | 3.353 | 2.397 | 5.994% |

Total: **1,440 updates /7,350 FFT windows**, including10 updates without FFT work at48k/8192. Every benchmark row reports zero FIFO loss.

## Callback deadlines — p99 passing is not zero overruns

All36 EQ-pair cases recorded **zero audio-thread C++ new allocations** and p99 below their block budget. Nevertheless, **2 of21,600 measured callbacks exceeded the deadline**:

| Capture | Rate / block | FFT | p99(µs) | Maximum(µs) | Budget(µs) | Overruns |
|---|---|---:|---:|---:|---:|---:|
| On | 192kHz /64 | 4096 | 90.356 | 794.48 | 333.333 | 1 /600 |
| On | 192kHz /128 | 2048 | 78.078 | 1281.88 | 666.667 | 1 /600 |

Separately, the synthetic native full-processBlock capture recorded **2 overruns out of579 measured callbacks** at48kHz/128 samples with FFT2048: p99=1.920ms, maximum=3.198ms, budget=2.667ms. The other three full-processor rows had zero overruns. All four achieved sample coverage and near-nominal production; every row explicitly records `performance_acceptance=NOT_ASSESSED`. These observations do not isolate scheduling noise from analyzer-induced cost.

## Six native96/192kHz observations

Each configuration ran for10s with the native event loop and a synthetic full-processBlock producer, without screenshots. All six conserved samples exactly, recorded **zero rejected/discarded samples**, and produced within5% of the requested sample rate. All **1,493 individual update rows** were observed with no gaps. Per-row consumed deltas, drained snapshots, chronological N/4 window counts, summary totals, and p50/p99 values agree exactly.

| Rate | FFT | Produced / consumed / pending | Production ratio | Update p50/p99/max(ms) | Consumer gap p99/max(ms) | Process CPU |
|---:|---:|---|---:|---|---|---:|
| 96kHz | 2048 | 959,616 /956,032 /3,584 | 0.9994 | 1.105 /1.494 /3.975 | 41.809 /47.680 | 65.06% |
| 96kHz | 4096 | 957,952 /955,648 /2,304 | 0.9978 | 1.000 /2.427 /3.048 | 42.123 /45.100 | 67.96% |
| 96kHz | 8192 | 956,416 /953,600 /2,816 | 0.9961 | 1.258 /2.120 /4.307 | 42.004 /43.772 | 71.08% |
| 192kHz | 2048 | 1,900,672 /1,896,704 /3,968 | 0.9898 | 2.219 /5.641 /30.920 | 41.953 /44.892 | 107.43% |
| 192kHz | 4096 | 1,896,448 /1,890,816 /5,632 | 0.9877 | 1.996 /4.678 /16.507 | 42.169 /42.958 | 109.81% |
| 192kHz | 8192 | 1,875,584 /1,869,440 /6,144 | 0.9768 | 2.524 /5.031 /16.758 | 41.831 /72.323 | 110.71% |

The pending samples are accounted queue contents at the end, not loss. Aggregate process CPU includes multiple threads and all processing/UI work; values above100% mean more than one CPU core in aggregate. Native performance remains **NOT_ASSESSED**, despite successful sample flow. The largest observed update was30.920ms; the largest consumer gap was72.323ms. These Linux observations cannot establish that the historical macOS FIFO loss has been eliminated.

## Native UI and images

- Six48kHz native UI measurements covered both banks and all FFT sizes. First complete data appeared at62.273–227.027ms; actual component-render completion at65.234–229.850ms. These are observation/render measurements, not monitor presentation latency.
-32 live frames exercised changing harmonic-pluck input through processBlock, with62.5/187.5/750Hz frequency steps, positive input/output meters, and advancing FFT counts714→797.
- All **49 PNGs** pass both format verification and full decoding:32 live frames and17 static/UI images. All32 live frames have distinct pixel hashes. Image dimensions:40×1180×780,6×1140×412, and one each at885×585,1475×975 and1770×1170. This verifies file integrity, not aesthetic approval.
- The six snapshot-inclusive native UI rows and24 underlying EQ CPU rows are retained in the JSON; snapshot-inclusive process CPU must not be mistaken for analyzer-only cost.

## Assessment

The local data supports the development fix: transient coverage is restored, sample-flow accounting is exact, and audio/parameter/preset behavior remains intact. It does **not** justify final real-time, cross-platform DAW, October23 installer, or public-release acceptance. Individual callback deadline misses, native timing outliers, deliberately unassessed performance, and the stale configured build identity must stay explicit.

Full machine-readable details, input CSV SHA-256 hashes, source hashes, all native-row audits and image hashes are in `local-final-analysis.json`.
