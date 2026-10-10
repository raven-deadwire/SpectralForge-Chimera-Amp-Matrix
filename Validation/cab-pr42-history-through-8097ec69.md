# PR #42 evidence history through 8097ec69

This is the frozen PR body as updated at 2026-10-10 20:42 UTC. It records results for the exact source and diagnostic harnesses identified below. Later source changes do not inherit these passing verdicts. Earlier statuses inside historical sections are retained as historical observations.

## Current follow-up: worker contention and larger modeled blocks

**Source:** [`8097ec69594ff728164c77f59e9ea1e39e384643`](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/commit/8097ec69594ff728164c77f59e9ea1e39e384643)  
**Tree:** `f12b4312f559e86e07c37899691b62858f4a4c3c`  
**Status at 2026-10-10 20:42 UTC: full Product passes 117/117 CTests and 198/198 p99 conditions across three OSes, with verified Windows/Mac/Linux packages. Separate Windows Candidate fails 40/41 (65/66 p99), and dedicated macOS fails 4/5 (65/66 p99). Both the background-priority and explicit ARM-loop experiments failed and are not adopted. An IR generation work-reduction candidate is being implemented separately. The Product release gate remains BLOCKED (21 PASS / 107 NOT_EXECUTED); no merge/release acceptance.**

The preceding `0f46f9f6` source passed dedicated CAB on three OSes but failed full Windows Candidate (40/41), Windows Product (38/41), and macOS Product (37/38). Candidate completed all 41 tests in 63 min 30 s, so its earlier whole-job timeout is resolved in that attempt. The remaining performance failures require production changes and another complete native validation.

### What this source changes

- Extend prepared modeled convolution through maximum blocks of 512, keeping JUCE 8.0.8's original partition geometry: 4B FFT / 3B partition / stride 3 through B=128, then 2B / B / stride 1. Modeled 256/512 kernels no longer create private JUCE loader threads. User/factory IRs and other unsupported preparations keep their existing path; each kernel still owns its mutable histories.
- Start the IR preparation worker with JUCE `Priority::low` (Windows LOWEST, macOS UTILITY; Linux ignores this non-realtime request). Observe its start/entry/priority after join and record the actual test caller context outside the measured callback population. Caller/process priority, affinity, power plans and sleeps are unchanged.
- Cache the 48 fixed cone geometry coefficients with the original expressions and operation order. Frequency-dependent propagation, point count, model keys, response length, FFT/taper and cancellation checkpoints are unchanged. Independent uncached original-function references compare exact double bits on each native compiler.

The 1600 callbacks, 80 automation requests, 1200/400 phase split, 50 ms fades, audio tolerances and strict `times[1584] < 1e6 * block / sr` gate remain unchanged. A four-partition MAC tile was rejected after a measured regression and is absent from production.

### Separate macOS priority diagnostic — completed with failures; background not adopted

[Run 38081844966](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38081844966) completed the fixed **ABBA** on a separate branch, harness `ffe5b62c901a7398777e586dc4b9a5f4979a9113` / tree `82c2da67269b18fb8f7fbc7758b377fffcdacfb5`. A is untouched `8097ec69` (low); B is the recorded two-file background-worker overlay. Both builds and both four-test correctness guards pass. All six normal/profile populations completed without timeout or harness error. [Artifact 11681351180](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38081844966/artifacts/11681351180), **25506690 bytes**, matches ZIP SHA256 `cf078208a7bfeb4e914e80ad5bf5be162e31562151cc8c14ffab9361646ba1e1`; all 237 indexed files, before/after source snapshots, 180 CSVs / 288000 callbacks and 540 same-callback paired records were independently verified, with zero clock errors.

| Fixed order | Strict p99 conditions | 96k/32 stereo p99 (budget 333.333 us) | Focal individual misses | All-route individual misses |
|---|---:|---:|---:|---:|
| A1 low | 29/30 | 355.875 us | 27 | 46 |
| B2 background | 29/30 | 362.458 us | 31 | 46 |
| B3 background | 29/30 | 350.375 us | 19 | 33 |
| A4 low | 30/30 | 319.584 us | 12 | 29 |

Both B trials fail the same strict gate; their p99 callbacks also exceed budget in paired CPU time (**365.125 / 353.125 us**). Across the four focal populations, 83 of 89 wall misses also exceed budget in paired CPU, while other observations show wall stalls with lower CPU. Low totals are 59/60 p99 and 75 misses (59 worker / 16 forced); background totals are 58/60 and 79 misses (71 / 8). No repetition is discarded. All 90 low and 90 background scheduling records observe native -1 and -2 respectively; all caller records retain base QoS 17. The actual QoS separation did not resolve the failure.

The separate extended profiles pass 30/30 for A and 29/30 for B, with 37 / 50 individual misses. Focal worker spectral-MAC p99 is 285.001 / 318.042 us. These instrumented nested wall-time quantiles are not additive or same-callback causal attribution. Model exact-bit references, the 144-route/eight-six-mic oracle, cancellation/latest-six-mic convergence and resource release pass for both variants; all six callback C++ new/delete reports are zero. The complete audit is frozen with SHA256 `c1fa0127d4b25b980bfc2b30d1d1a46ad0998bd031a92816d5e3d4f3a013d033`; the all-route raw comparison is `2eed40a338fa7c472db571b464c59511d5928a938a77718e2c7d4581e5bf641d`.

### Separate Apple ARM MAC experiment — completed with failures; not adopted

[ARM comparison run 38083603135](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38083603135), [job 114305382543](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38083603135/job/114305382543), uses harness `ffc108e59f44419b8e0d7679efe1b8cd74c6316b` / tree `9d32f5803169ee07ce10cb16ca482634b5dcff9b`. A is untouched 8097; B applies the exact two-file overlay `4d50d84b4148e6b9080fd68384179fcf961f5d011d56286f6214a72f2d7e2e47`. Both remain low-priority workers, with unchanged test populations, strict limits, automation, fades and convergence. Both Release arm64 builds and all four correctness/lifecycle guards pass. B additionally passes **60 geometries / 73 partitions / 1,705,864 exact-bit float comparisons**, with immutable inputs and output bounds guards, against an independent test-only scalar fused reference.

| Fixed order | Strict p99 conditions | 96k/32 stereo p99 (333.333 us budget) | Focal individual misses | All-route individual misses |
|---|---:|---:|---:|---:|
| A1 reference | 30/30 | 153.667 us | 2 | 5 |
| B2 explicit SIMD | 29/30 | 333.875 us | 16 | 17 |
| B3 explicit SIMD | 29/30 | 369.625 us | 37 | 38 |
| A4 reference | 30/30 | 299.625 us | 8 | 9 |

**Both B repetitions fail; both A repetitions pass. The SIMD candidate is rejected and remains absent from PR42.** Of the 53 focal B wall misses, 51 also exceed budget in same-callback thread CPU. The paired p99 CPU values are 335.292 / 373.250 us. Across the 30 routes, means of per-run p99 improve in one and worsen in 29; p50 improves in three and worsens in 27. These are descriptive observations from this ABBA trial: A's own p99 changes substantially over time, so no intrinsic SIMD cost percentage or sole cause is inferred.

The separate reference/SIMD profiles each pass 29/30 conditions, with focal p99 349.250 / 406.167 us and 29 / 42 total misses. All 140 individual misses across all six normal/profile populations are worker phase; forced phase has zero. All **180 CSVs / 288,000 callbacks / 540 paired records**, 180 native scheduling records (low -1 / caller QoS 17), source ledgers and indexed file hashes independently verify; zero clock errors and all six callback C++ new/delete reports are zero. Model-bit guards, the 144-route/eight-six-mic oracle, latest-six-mic convergence and resource release pass for both variants.

Native assembly also disproves the intended simplification: the existing loop already uses eight-bin vector instructions, while the proposed implementation expands `sumPartitions` from **308 to 471 static instructions**, retains generated remainder overlap/versioning paths and changes load/address generation. Static counts do not establish executed instruction counts or explain the measured tails.

[Artifact 11681770526](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38083603135/artifacts/11681770526): 25,369,197 bytes; verified outer ZIP SHA256 `d857feb281def2f13064dae824935f55af5c40f6ad3381ecdbc3df9ee54b0b09`. All failures and raw populations remain retained. The next candidate targets actual repeated IR-generation work, with full-response and waveform bit checks; neither failed diagnostic is being promoted.

### Completed local evidence

One frozen-source Linux / GCC 13.3 batch passed **10/10 CTests** in 60.35 s. An independent raw audit verified **66/66 strict p99 conditions**, all **105600 callback rows**, **198 paired records**, **132 phase summaries**, zero clock errors and four callback C++ new/delete reports of zero. **34 individual misses remain: 24 worker / 10 forced-publication.** The smallest p99 margin was expanded 96k/32 stereo at 302.366 / 333.333 us. The largest wall sample, Layout 96k/64 stereo, was 23136.001 us with 23137.610 us measured thread CPU. These results do not establish elimination of tails.

The **144-route audio oracle** passed, including 108 prepared modeled routes with zero modeled JUCE loaders, 18 mic-post cases, four reprepare cases and eight direct six-mic cases. Maximum direct six-mic residual was 4.84288e-8 against the unchanged 3e-6 tolerance. Native reference tests locally passed 864 v1 + 2592 v2/v3 complex comparisons and 144 geometry doubles, all exact bits. The separate original/cached generator probe produced 27 byte-identical waveforms / 143919 floats, with matching cancellation and invalid-input behavior.

The generator-only bounded ABBA probe reduced the representative 96k 6x10 median wall time from 149.912 to 79.094 ms (47.24%); all six old and six new observations are retained. This does not establish callback or native product performance.

A separate instrumented 12-condition profile passed p99 for both old and new sources. New p99 improved in eight conditions and worsened in four; misses increased **1 -> 7 / 19200**. At 96k/256 stereo, p50 changed 242.216 -> 140.341 us and p99 580.907 -> 504.432 us; at 96k/256 mono, p99 worsened 357.449 -> 538.753 us. All regressions are retained.

[Local source/binary hashes, all route summaries and probe samples](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/blob/8097ec69594ff728164c77f59e9ea1e39e384643/Validation/cab-worker-contention-local-20261011.json) · [Design, limitations and preceding failures](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/blob/8097ec69594ff728164c77f59e9ea1e39e384643/docs/CAB_REALTIME_PERFORMANCE_1_3.md)

### Completed macOS full Product and package

[Product job 114293879500](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38079665006/job/114293879500) completed on `8097ec69`: **38/38 inventory/JUnit tests PASS**, with no skips, missing or duplicate tests; all **66/66 strict p99 conditions** pass. The complete 105600-row raw population and same-callback pairs were verified. Four individual misses remain, all in the worker phase. Expanded extended 96k/32 stereo has p99 **201.166 us**, paired CPU **202.417 us**, and three misses at callbacks 1005/1006/1007. Their wall times are 340.041/386.083/344.625 us and paired CPU 342.042/387.292/345.834 us. The other miss, extended 96k/64 mono, is 2458.416 us wall / 141.583 us CPU. All 66 worker records observe native low (-1); caller base QoS is 17. Both exact-bit model references, the 144-route/eight-six-mic oracle and cancellation/latest-generation/resource-release checks pass (`stop_us=18032`). This separate Product success does not erase the dedicated macOS 371.5 us p99 failure.

[Mac package artifact 11681210847](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38079665006/artifacts/11681210847) was downloaded and its complete ZIP SHA256 verified as `f0bc654344a4fef8ab3d5a673007093101392d0e3abcd4ad887f7b2d5ce93540`. The enclosed `SpectralForge-Chimera-1.3.0-beta.1-macos-universal.pkg` is **388180231 bytes**, SHA256 **`4eefe28f0f9f19b25977230788927846246c471bdf1069707658cfc4116510b3`**, matching its sidecar. Its verification report matches the source-bound release-evidence manifest (`721f7a7bf98e16be3f7f9772416f54f71dc9657476fea84d5b2fb6e9ee217400`). The report confirms arm64/x86_64 Standalone, VST3 and AU slices, built-bundle code-seal checks, expanded-pkg binary hashes, system paths and manual. It does **not** certify actual installation, Developer ID signing, notarization or real-hardware audio.

### Completed Linux full Product and packages

[Linux Product job 114293879574](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38079665006/job/114293879574) completed on the verified `8097ec69` checkout: **38/38 inventory/JUnit tests PASS**, no skips, missing or duplicate tests; **66/66 p99, zero individual misses across 105600 callbacks**. All raw summaries and paired observations match; model exact-bit references, the 144-route/eight-six-mic oracle and lifecycle checks pass. All 66 scheduling records report the unchanged requested low priority; Linux does not apply that non-realtime request. The raw audit SHA256 is `48500fcc6e48cfb910f0d81db64aee66b6fe64841a111dc0163fc2525ad8c737`.

[Linux installer artifact 11681526370](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38079665006/artifacts/11681526370), **250133980 bytes**, matches ZIP SHA256 `fea094745a7186fbe132bc5b3575ae4d9e9d02e402a17dc6939c031e15531d6c`. Both complete payloads match their sidecars: the **119457830-byte .deb**, SHA256 `1838facecf7634990300ba61e1a853feebfc84d1593c5f0dcf092d820507d5cf`, and the **130668257-byte .tar.gz**, SHA256 `170e45f25a3e5898153776673ef598fccf3660bb07dd5429a9cd45e5794399f0`. Debian control identifies `chimera-amp-matrix` version `1.3.0~beta.1`, amd64. Package-verification bytes match the release manifest (`ee464c38ab79eaa550a66d49fb0ff2896bbe76e7359a92300bb77d67b7e4952a`). This does not establish installation on other distributions or real DAW/audio acceptance.

### Completed Windows Product, installers and release gate

[Windows Product job 114293879472](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38079665006/job/114293879472) completed on the actual `8097ec69` checkout / MSVC 19.51.36260: **41/41 inventory/JUnit tests PASS**, **66/66 p99**, **0/105600 individual misses**. All 198 paired records and raw callbacks match the complete log. All 66 worker records observe native low (-1); measured caller processes remain BelowNormal (16384), thread priority 0, affinity 15, with valid native queries. Both exact-bit model references, the 144-route/eight-six-mic oracle, zero modeled loaders and lifecycle guards pass. The tightest p99/budget ratio is extended 96k/32 stereo, **139.5 / 333.333 us**, paired cycles **342056**. Windows CPU duration is unavailable; cycles are not converted to time.

The **23 installer assertions and nine raw install/uninstall logs** pass, including full install, installed VST3 loading / GetPluginFactory, Start Menu application launch, same-version repair, private preset/IR preservation, remembered/custom/Unicode VST3 paths, VST3-only and Standalone-only selection, and removal of owned files. MSIX passes five assertions, including unsigned CI identity installation, registered AUMID launch and removal. Local Defender updates and scans all 54 inputs with no threats; their hashes remain unchanged. These are current Product results, not results of the failed Candidate workflow.

The complete [Setup artifact 11680694687](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38079665006/artifacts/11680694687) ZIP is **117895359 bytes**, SHA256 `2714ee887427ce710c0c6f40f0170d44acb950fce0725d603848071fc3e49fbc`. Its enclosed **117887027-byte Setup.exe** hashes to **`b366b0793ded69a6e66da322702512b0224c1dba0986679e541c09318f6d2d67`**. The complete [unsigned MSIX artifact 11680554903](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38079665006/artifacts/11680554903) ZIP is **63800403 bytes**, SHA256 `0856584d808a4589e05746a2cfb4a5724475b9cc509646dee54e947723a784c5`; its **63793146-byte .msix** hashes to **`42b75c08066d32f9f833f060675c2d67e3acb560d47b0e9d98dfa9b30a48b990`**. Both actual payloads match their sidecars and source-bound Defender inventory; Setup also matches its installer-verification JSON and report. Both are reported **NotSigned**. No publisher signing, Store certification or release approval is asserted.

Full Product totals are **117/117 CTests, 198/198 p99, 316800 callbacks**, with four individual misses, all macOS worker phase. Four release manifests / 20 referenced raw-file hashes match their independent artifacts and final-gate copies. The completed Product audit has SHA256 `7c1c39d6cc936c27b3217d63eee79ca3f93861f1129fc9e4661a0a7e6d9b9c62`.

The actual [final release-gate artifact 11680998746](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38079665006/artifacts/11680998746) has ZIP SHA256 `c80674b341d5b9041a54e29935abc6d8cc44fd205fbd5706f90ab1d697ab4241`. It remains **BLOCKED / ready=false**, with **21 PASS / 107 BLOCKED (NOT_EXECUTED) / 0 N/A**; hard gates are 21/63 PASS, producer errors are empty. A12 (7/7), A13 (7/7), E1 (3/3), and I1 (4/4) are the executed PASS groups. Candidate publication, publisher signing and macOS notarization are false. Workflow success does not override unexecuted product gates or the separate CAB/Candidate failures.

### Completed Windows Candidate — one performance failure remains

**Hardware distinction:** full Product ran on AMD EPYC 7763; Candidate ran on Intel Xeon Platinum 8370C. Both expose two cores/four logical processors and record the same caller priority class 16384 / thread 0 / affinity 15, but they are different hardware observations. Their raw wall-time ratio cannot be assigned wholly to source behavior or worker scheduling. The unchanged-source Product pass does not erase the Candidate failure.

[Candidate run 38079665010 / job 114293784117](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38079665010/job/114293784117) completed on the same verified `8097ec69` checkout. All 41 tests ran: **40 PASS / 1 FAIL**, with no UI failure or timeout. The complete CTest took **1774.12 s**; the visual test passed in **851.68 s**. CAB is **65/66 strict p99**, with all 105600 raw rows and 198 paired records validated without evidence errors.

The sole failed condition is **array-v3-6x10 Layout integration, 96k / 64 / mono**: p99 **725.3 us > 666.666667 us**, **18 individual misses**, all worker phase. The p99 callback 482 records **432076 cycles**; max callback 650 is **1248.0 us / 511119 cycles**. Worker-phase p99 is **776.4 us / 446322 cycles** at callback 17. Forced-phase p99 is **178.1 us / 498830 cycles**, with zero misses for that route. Across all 66 conditions, **33 individual misses remain: 27 worker / 6 forced**. These wall/cycle pairs do not justify converting cycles to CPU duration or assigning the failure solely to DSP computation.

All 66 workers start/enter at native low (-1). Four actual timing-process entries and all 66 route records show caller class16384, thread0, affinity15, matching the separately passing Product context. Current VST3 lifecycle 7/7, model exact-bit references, and the 144-route/eight-six-mic oracle pass. **Candidate packaging, Setup, install/repair/uninstall and Defender stages are skipped after the CTest failure**; the Product installer above does not replace those skipped Candidate results.

[Candidate integration artifact 11681462690](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38079665010/artifacts/11681462690) ZIP SHA256 is `dc5e7a7092e690fad5e39fdf9ae75a3fa2e881f90bb7f87991428611d7cfa16d`. This failed first attempt remains preserved while Windows-specific follow-up is investigated.

### Native first attempts for this source

| Workflow | Current state |
|---|---|
| [CAB realtime / three OSes](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38079664995) | Windows 5/5, 66/66 p99 PASS; Linux 5/5, 66/66 PASS; macOS 4/5, 65/66 FAIL |
| [Full Product / three OSes and release gate](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38079665006) | Completed: 117/117 CTests and 198/198 p99 across three OSes; 4 macOS individual misses. All platform packages and Windows lifecycle/security checks complete. Final release gate BLOCKED: 21 PASS / 107 NOT_EXECUTED |
| [Full Windows Candidate and installer validation](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38079665010) | Completed FAIL: 40/41 CTests, 65/66 p99; Layout 96k/64 mono 725.3 > 666.667 us. Setup/install/security stages skipped; no timeout |
| [CAB Panel / state / visual](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38079664978) | PASS on all three OSes: 17/17 each, 51/51 total; exact checkout and full artifact logs verified |
| [Niflheimr measurements](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38079664985) | PASS on all three OSes; each 2/2 tests; actual checkout SHA verified |
| [Niflheimr DI contracts](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38079664991) | PASS on all three OSes, each 3/3 tests; actual checkout SHA verified |

macOS CAB Panel completed all **17/17 tests** on the exact `8097ec69` checkout. [Artifact 11680561152](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38079664978/artifacts/11680561152) ZIP SHA256 is `2ada9a91b61b0b2a7994fc91391b1d718919670a522519c7d1aa38a5308d74e2`. Full LastTest output verifies 864 v1 + 2592 v2/v3 complex comparisons and 144 geometry doubles with exact bits, the 144-route audio oracle (108 prepared models, zero modeled JUCE loaders), all state/UI tests, and worker cancellation/latest-six-mic convergence/resource release (`stop_us=18029.8`). This panel suite excludes the dedicated extended-32 condition and does not override its macOS p99 failure.

Linux CAB Panel also passed **17/17 tests** on the verified `8097ec69` checkout. [Artifact 11680935010](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38079664978/artifacts/11680935010) ZIP SHA256 is `be95c25e4b273d26c66dcb26f51d8c3a781df9b95530b9f304d910be6eab9dea`. Its full LastTest confirms both exact-bit model references, 144-route audio oracle, state/UI tests, and worker cancellation/latest-generation/resource-release checks (`stop_us=2074.29`).

Windows CAB Panel completed the third **17/17 PASS**, making **51/51** across the three OSes. [Artifact 11680672136](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38079664978/artifacts/11680672136) ZIP SHA256 is `36966c9bbeaeea67c8fe78f2349a24c7b31fba11f9859b19fccae89fc7c983da`. Actual checkout SHA, identical 17-test inventory, exact-bit references, 144-route/8-six-mic oracle and cancellation/latest-generation/resource-release checks (`stop_us=3001.7`) were verified from the full log. All three panel ZIP hashes were checked; this completes panel/state/UI scope, while the separate Candidate and macOS performance failures remain open.

Linux dedicated [artifact 11680007007](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38079664995/artifacts/11680007007) has ZIP SHA256 `5809e32b640d028be97a47e045c08d12e80aa287b48692938ef6c60a06fe07c6`. Its 107 indexed files, clean source/tree, 66 raw CSVs / 105600 callbacks, 198 paired records, 132 phase summaries and 66 scheduling records were verified. All 66 p99 conditions pass; four individual misses remain, all in the forced-publication phase of expanded 96k/32 stereo. The separate 12-condition profile passes with zero misses. Same-job release-baseline comparison over the common 36 conditions improves p50 in 36 and p99 in 35; array 44.1k/256 mono p99 worsens 336.228 -> 462.950 us with zero misses. This does not establish Windows/macOS or full-product acceptance.

**Completed dedicated result: failure remains on macOS.** All three ZIP digests and 321 indexed files match; all 316800 raw callbacks and 594 paired records were audited against source `8097ec69` / tree `f12b4312`. Windows has **0** individual misses, Linux **4**, and macOS **62**. Every OS passes the 144-route / eight direct-six-mic audio oracle. Separate instrumented profiles pass all 36 conditions, with only one macOS individual miss.

The sole failed p99 condition is macOS expanded extended **96k / 32 / stereo**: p50 **86.25 us**, p99 **371.5 us** against **333.333333 us**, max **494.209 us**, **29/1600** misses, all in the worker phase. Its wall-p99 callback 313 records **373.542 us thread CPU**; every one of those 29 misses also exceeds budget in the paired CPU measurement. Worker-phase p50/p99 are **132.584 / 397.0 us**; forced-publication p50/p99 are **39.25 / 215.625 us**, zero misses. Native worker low is observed in all 66 records; the caller reports base QoS 17. Equal utility QoS is being investigated and is not asserted as the sole cause.

Windows [artifact 11680237051](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38079664995/artifacts/11680237051): SHA256 `1ae2358f2204fd74ba5736d830df0a6c2b161ebd721bbcbf5386af065a4e4804`. macOS [artifact 11680167188](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38079664995/artifacts/11680167188): SHA256 `61a04139fd998638aee84f22820c6e0bb24efb8f7a7a9d1fc0ee435da4b7421a`. No rerun replaces this failed first attempt; full Product independently passed while Windows Candidate failed the separate Layout condition recorded above.

No merge/release is claimed. Current-source Product packages and lifecycle/Defender results were verified; the failed Candidate skipped its corresponding stages. Previous failures and older installer provenance remain below.

<details>
<summary>Preserved 0f46f9f6 and earlier implementation, CI failures, artifact provenance and validation history</summary>

## Candidate recurrence follow-up

**Current GitHub source:** `0f46f9f6eba7bea38ff256e3a5eafb1ca83b7065`  
**Verified source tree:** `a4587e804aa0cda0d2603593dda512bd26707759`

This follows the full Windows Candidate failure at `4bfddbc8`: [run 38065201856](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38065201856/job/114251441631) had **38 passed, one failed and one interrupted**. Expanded 96 kHz / 64 mono p99 was **2764.9 µs** against **666.666667 µs**, with **33/1600** misses. The job also reached its 60-minute whole-job timeout during the final visual test, so its packaging and installer checks were skipped. The separate Product run at the same old source passed; that does not erase this Candidate failure.

### What this follow-up changes

- Cooperatively abandon obsolete modeled-response work at 64-bin checkpoints, including v1/v2/v3 dispatch. The worker uses the existing source/model freshness conditions and shutdown flag. Synchronous prepare remains independent of the stopped-thread flag.
- Keep incomplete response/spectrum/kernel data private. Commit a new cache entry only after completed preparation and a final request check. Cancellation is separate from an IR error; it does not add work to the audio callback or change normal acoustic calculations.
- Retain all 1600 callbacks in per-condition CSVs, with wall time and CPU observations matched to the **same callback index**. Windows records raw thread cycles without converting them to time. Test-only observations remain outside the unchanged wall interval. All workflow artifact paths preserve these files, and the dedicated driver checks and hashes them.
- Give Candidate a 150-minute job budget, separate bounded build/CTest steps, and a 1200-second default per-test timeout while retaining existing explicit CMake timeouts. Preserve partial CTest logs on interruption and record Windows CPU/OS/power-plan context without changing power, affinity or priority settings.
- Add one small diagnostics CTest and cancellation regression contracts. **No existing test, callback population, automation load, p99 limit or audio tolerance was removed or relaxed.**

### Local validation of the exact tree

- **10/10 relevant CTests**, including **66/66 original p99 timing executions**.
- **144** convolution-oracle routes, **18** mic-post cases, **4** reprepares and **6** independent six-microphone cases passed; latency 0 and watched callback C++ new/delete 0.
- Independently compiled untouched `4bfddbc8` model headers and new default generators produced byte-identical waveforms in **9/9** v1/v2/v3 × 44.1/48/96 kHz comparisons.
- **66 CSVs / 105,600 callback rows** matched the executable summaries. **798** obsolete worker builds were observed as cancelled. **15** evidence tests reject missing/mismatched/altered data.
- The separate instrumented profiler completed **12/12** conditions and all raw-file validation. Its timing does not replace acceptance.

**Remaining local observations are retained:** there were **27/105,600 individual deadline misses** despite passing p99. In one sequential local `4bfddbc8 → follow-up` Expanded 96 kHz / 64 comparison, mono p50/p99 changed **48.203/148.274 → 85.940/151.199 µs** (0 → 0 misses), and stereo **113.301/178.439 → 113.030/221.204 µs** (0 → 1 miss). These include regressions. The extended-suite repeat of the same candidate condition measured mono p50/p99 **49.255/179.391 µs** and stereo **63.977/248.214 µs**. They are separate executions, not interchangeable samples. No universal callback speedup, scheduling-only cause, or zero-dropout claim is made.

The local commit `28bf17b` and GitHub commit `0f46f9f` have identical Git tree `a4587e80`; the commit metadata differs because the remote write used the authenticated GitHub connection. Native CI below checks the final GitHub source.

### Native CI at the follow-up source

Status at **2026-10-10 18:55 UTC**. All first-attempt workflows for this source have finished. Dedicated CAB realtime **[38072841041](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38072841041)** succeeded on all three OSes, but **full Product failed on Windows and macOS, and full Windows Candidate failed**. These failures remain authoritative; no new Windows or macOS installer was produced.

| Platform | Candidate CTests | Strict p99 executions | Individual misses / 105,600 callbacks | Separate profile |
|---|---:|---:|---:|---|
| Windows | **5/5 PASS** | **66/66 PASS** | **0** | 12 conditions, exit 0 |
| Linux | **5/5 PASS** | **66/66 PASS** | **2** | 12 conditions, exit 0 |
| macOS | **5/5 PASS** | **66/66 PASS** | **28** | 12 conditions, exit 0 |

The original strict p99 predicate, 1600 callbacks per execution and automation/forced-publication loads are unchanged. Passing p99 does not mean every callback met its deadline. The Linux misses are both Expanded 96 kHz / 64 stereo: p99 **275.256 µs**, maximum **835.410 µs**, with **835.776 µs thread CPU on that same maximum callback**. macOS has 28 misses across 16 conditions; its largest wall observation is **20,114.334 µs** with **82.542 µs thread CPU on the same callback** (extended Expanded 44.1 kHz / 128 stereo). These paired observations are retained without assigning all outliers to scheduling.

#### Same-runner comparison with the unchanged baseline

All rows below compare `aaa1330 → 0f46f9f` sequentially inside this new run, using the common **36 default conditions** only. Extended-suite repeats, the diagnostic profile and previous `4bfddbc8` runs are excluded. This comparison covers the complete PR and does not isolate the cancellation follow-up.

Expanded CAB, **96 kHz / 64 samples**, deadline **666.666667 µs**:

| OS / channels | p50 µs, baseline → candidate | p99 µs, baseline → candidate | Misses / 1600, baseline → candidate |
|---|---:|---:|---:|
| Windows mono | 60.900 → 45.600 | 132.700 → **91.200** | 0 → 0 |
| Windows stereo | 81.600 → 66.500 | 160.400 → **146.900** | 0 → 0 |
| Linux mono | 107.061 → 99.890 | 178.439 → **156.697** | 0 → 0 |
| Linux stereo | 114.483 → 111.888 | 180.122 → **275.256** | **0 → 2** |
| macOS mono | 40.375 → 20.792 | 226.833 → **80.875** | 3 → 0 |
| macOS stereo | 57.500 → 40.209 | 195.708 → **166.917** | 4 → 0 |

| OS / runner CPU | p50 improved / worsened, of 36 | p99 improved / worsened, of 36 | Default-only misses / 57,600, baseline → candidate |
|---|---:|---:|---:|
| Windows / Xeon Platinum 8573C | 32 / 4 | 24 / 12 | 0 → 0 |
| Linux / EPYC 9V45 | 24 / 12 | 18 / 18 | 0 → 2 |
| macOS / Apple M1 virtual | 30 / 6 | 30 / 6 | 25 → 5 |

**Regressions remain visible:** besides Linux Expanded 96 kHz / 64 stereo above, Windows Original 96 kHz / 256 mono p99 increased **231.3 → 312.5 µs**, Linux Expanded 44.1 kHz / 64 mono **153.582 → 303.088 µs**, and macOS Expanded 44.1 kHz / 256 stereo **225.459 → 283.291 µs** with misses **0 → 2**. All paired rows and remaining increases are preserved in the artifacts. One sequential measurement per condition does not establish statistical confidence or a universal speedup. New test-only thread-clock observations are outside the unchanged acceptance wall interval but add measurement overhead.

#### Raw evidence verification

Each artifact's ZIP SHA-256 and **107 indexed file sizes/hashes** were independently checked. Candidate and profile receipts both match clean source `0f46f9f6`, tree `a4587e80`, before and after execution; baseline receipts match unchanged `aaa1330`. Each artifact contains **66 candidate CSVs + 12 profile CSVs**. All 105,600 candidate Windows cycle observations and all Linux/macOS thread-CPU observations are valid. The **198 candidate + 36 profile** stdout p99/max paired records per OS match their exact CSV callback indices. Windows cycles are not converted into time. Profile misses, separately, were Windows 0 / Linux 0 / macOS 2.

The 66 candidate timing executions recorded **818 / 873 / 764** obsolete worker cancellations on Windows / Linux / macOS respectively; profile runs are excluded. These are cancelled superseded worker builds, not failed audio callbacks.

| OS | Artifact | ZIP SHA-256 |
|---|---|---|
| Windows | [11678230574](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38072841041/artifacts/11678230574) | `ab7ac49232558f341fca7d39e4335fc7f162fb976e13f03c9014205b038a1f70` |
| Linux | [11677432038](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38072841041/artifacts/11677432038) | `dcdb96cd3c59a1cf876928cc49b3a20f8f785d33780fbd616377cd72d93a32d9` |
| macOS | [11678135518](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38072841041/artifacts/11678135518) | `d18388a32f197f90ecfe4144d7e71c6bded457577fa8e5cf1ab6d7e51306d37b` |

#### Full product and installer validation: final failures retained

| Workflow / platform | Run | Completed result |
|---|---|---|
| Windows Candidate | [38072840993](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38072840993) | **40/41 CTests PASS, 1 FAIL**; CAB Layout 96 kHz / 64 mono and stereo p99 failed; Setup skipped |
| Product / Windows | [38072840987](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38072840987/job/114273736639) | **38/41 PASS, 3 FAIL**; Expanded, Extended and Layout timing failed; Setup skipped |
| Product / macOS | [38072840987](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38072840987/job/114273736539) | **37/38 PASS, 1 FAIL**; Extended timing failed; package skipped |
| Product / Linux | [38072840987](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38072840987/job/114273736551) | **38/38 PASS**, packages verified |
| CAB Panel / state / UI | [38072841049](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38072841049) | **SUCCESS on three OSes: 17/17 each, 51/51 total** |

**The Candidate whole-job timeout recurrence is resolved in this attempt:** the job ran **63 min 30.4 s**, all **41/41 inventory tests executed** with no skips, missing or duplicate JUnit entries, and the final visual test passed in **809.78 s**. Full CTest finished in **1749.58 s**, returned its real failure exit code **8**, and saved complete JUnit, LastTest, output and failure-list evidence. Its remaining failure is performance, not timeout or interruption. All four CAB timing CTests are already **RUN_SERIAL=true**; serialization is not a missing fix.

Candidate CAB p99 passed **64/66** conditions, with **44/105,600 individual wall misses**, all in the worker/automation phase and none in the 400-callback forced-publication phases. The two failing Layout conditions were:

| Candidate Layout condition | Wall p99 µs | Budget µs | Misses / 1600 | Same p99 callback / raw thread cycles | Maximum wall µs / same callback cycles |
|---|---:|---:|---:|---|---|
| 96 kHz / 64 / mono | **780.8** | 666.666667 | **19** | 865 / **366260** | 1806.0 / **341321** |
| 96 kHz / 64 / stereo | **876.6** | 666.666667 | **22** | 977 / **414428** | 1579.2 / **474085** |

The previously failing Expanded 96 kHz / 64 mono passed in this Candidate: default p99 **151.9 µs**, extended **156.7 µs**, zero misses in both. That does not remove the new Layout failures. All **66 CSVs / 105,600 callback rows**, **198 paired phase reports**, valid raw cycle deltas and zero clock errors were independently verified. No cycle-to-time conversion was used.

The new Candidate environment record identifies **AMD EPYC 7763**, a VM reporting **2 cores / 4 logical processors**, Windows Server 2025 build **10.0.26100**, image **20260925.250.1**, and **High performance** power plan. The observed **BelowNormal / 0xF** priority/affinity belong to the **collector process**, not a measured CTest/audio thread; no such test-thread priority is inferred. [Candidate integration artifact 11679101217](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38072840993/artifacts/11679101217), **7048918 bytes**, ZIP SHA-256 verified: `ce873ac60a8e675856e831d55b413364801ae9536c96ab89ad0fcce2a817964d`.

**Windows Product independently failed six p99 conditions**, with **60/66 p99 PASS** and **328/105,600 individual misses**:

| Engine / suite / rate / block / channels | Wall p99 µs | Budget µs | Misses / 1600 |
|---|---:|---:|---:|
| Expanded / integration / 96 kHz / 64 / mono | **769.7** | 666.666667 | 20 |
| Expanded / extended / 48 kHz / 32 / stereo | **773.9** | 666.666667 | 19 |
| Expanded / extended / 96 kHz / 32 / mono | **600.0** | 333.333333 | 44 |
| Expanded / extended / 96 kHz / 32 / stereo | **589.8** | 333.333333 | 53 |
| Expanded / extended / 96 kHz / 64 / mono | **949.4** | 666.666667 | 26 |
| Layout / integration / 96 kHz / 256 / stereo | **28839.8** | 2666.666667 | 127 |

The last condition's same wall-p99 callback recorded **1866269 raw thread cycles**. Its maximum callback was **1076579 µs wall / 1524458 raw cycles**. This large wall/cycle difference is retained without treating it as proof of a single cause or translating cycles into CPU time. The Windows Product visual test passed in **782.22 s**. This Product job did not record its CPU model.

**Current Product consolidated gate: BLOCKED, ready=false, 1 PASS / 127 BLOCKED**, with all **30 stage verdicts BLOCKED**. Windows/macOS packaging and installer validation were skipped after CTest failures; assembly evidence is absent and the consolidator returned exit **2**. There is **no new-source Setup, Setup SHA or successful install/repair/uninstall/Defender verdict** to claim. [Final gate artifact 11678642503](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38072840987/artifacts/11678642503), ZIP SHA-256 verified: `8f1cc00616a33696ea4e80340dedcd9e9731b870a6bea6824bbd175e3b22035c`.


**Full macOS Product failure retained at this exact source:** [job 114273736539](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38072840987/job/114273736539) passed **37/38 CTests** but failed `ChimeraCabExtendedTimingTests`. Expanded **96 kHz / 32 samples / stereo** measured p99 wall **398.666 µs** against **333.333333 µs**, with **28/1600 misses**. All 28 occurred in the 1200 worker/automation callbacks; the 400 forced-publication callbacks had zero misses. The same p99 callback had **282.917 µs thread CPU**; the maximum wall callback measured **1795.000 µs wall / 132.417 µs thread CPU**. Overall Product CAB coverage was **65/66 p99**, with **34/105,600 misses**. The paired CPU evidence does not turn the wall failure into a pass or prove that every outlier is scheduling-only.

The macOS visual test (**380.76 s**) and new diagnostics test passed. Its Release universal `arm64;x86_64` build used Apple Clang `17.0.0.17000013` on macOS 15.7.9 (`macos-15-arm64` image `20260907.0337.1`); this job did not record a CPU model, so none is inferred. Packaging was skipped after the test failure. The raw CSV is preserved in [integration artifact 11678481608](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38072840987/artifacts/11678481608), with [DSP logs 11678506494](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38072840987/artifacts/11678506494). A further investigation of worker-active callback CPU cost and wall tails across Windows/macOS is in progress. In this macOS condition, 13 measured thread-CPU observations also exceed the 333.333333 µs budget (including one only 0.084 µs above it); test-clock boundaries include diagnostic overhead, so this is not a count of proven pure-DSP overruns. No p99 limit, callback population or workload has been relaxed, and no CI retry or cancellation has been used to remove this failure.

The full Linux Product job separately passed **38/38 tests**, including the new diagnostics contract, worker/model regressions and the actual visual test (**455.58 s**). Its **66/66 CAB p99 executions** passed with **0/105,600 misses**; Expanded 96 kHz / 64 mono measured **218.825 µs** in the default suite and **243.949 µs** in the separate extended repeat. These are Product-run observations and do not replace the dedicated-CAB measurements above. The Linux [.deb/.tar.gz artifact 11678404314](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38072840987/artifacts/11678404314) was packaged, verified and uploaded.

Both Niflheimr [technical](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38072841004) and [DI contract](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38072841005) workflows have passed on three OSes at this source.

The prior `4bfddbc8` Product run produced a test Setup and passed native tests, but its consolidated release verdict remained **BLOCKED: 21 PASS / 107 NOT_EXECUTED**. At the current source, the failed full Product/Candidate gates above produced no Windows or macOS installer. PR #42 remains **Draft**. No merge, release publication, gate relaxation or rerun-to-green has been performed.

[Follow-up implementation and evidence notes](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/blob/0f46f9f6eba7bea38ff256e3a5eafb1ca83b7065/docs/CAB_REALTIME_PERFORMANCE_1_3.md)

<details>
<summary>Earlier optimization, source-bound native results and historical failures at 4bfddbc8</summary>

## Result and exact source

This PR reduces CAB callback work while retaining the existing audio behavior and strict per-block p99 criterion.

**Final candidate:** `4bfddbc8d3407429353450851b917f4769abd0c2`  
**Tree:** `9afac05f50be48d56b723c78910243973631c94f`  
**Unmodified comparison baseline:** `aaa1330dd5cf7d51faf1c45a7cef633b9b118458` (tree `af81f30e6ec3b340a10a99916a27c86a59e6ef60`).

[Exact-source CAB realtime run 38065201764](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38065201764), attempt 1, completed **SUCCESS on Windows, Linux and native arm64 macOS**. Every platform passed **5/5 candidate CTests and 66/66 timing executions**. Original thresholds, callback counts, automation and six-mic transition loads are unchanged.

**Remaining limitation:** Windows and Linux recorded zero deadline misses in 105,600 candidate callbacks each. macOS recorded 51/105,600, including 13/1600 for Expanded 96 kHz / 32-sample stereo; its p99 was 323.917 µs against 333.333 µs, leaving only 2.8% headroom. All p99 gates pass, but this is not a zero-dropout or complete product-release verdict. Windows Expanded 96 kHz / 64 mono also retains a measured p99 increase, documented below.

## Problem and historical evidence

- Windows Candidate [run 38056482864](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38056482864), source `aaa1330dd5cf7d51faf1c45a7cef633b9b118458`: 37/39 tests passed; Original and Expanded CAB integration failed. Expanded 96 kHz / 64 stereo measured p99 1026.4 µs against 666.667 µs, with 43/1600 deadline misses.
- Separately, macOS Product [run 38057692032](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38057692032), source `07dbfa5b0da24ae7a6000ad6dc215ba2aea67c46`: Expanded 96 kHz / 64 mono measured p99 674.375 µs against 666.667 µs, with 16/1600 misses.

These remain separate source/platform observations. The fresh `aaa1330` baseline passed its 36 p99 cases in each dedicated job, so the historical failures were not reproduced in those new baseline runs. Improvements below compare baseline and candidate measured sequentially on the same native job; they do not treat historical and new runner timings as interchangeable.

## Changes and measured bottleneck

- Small-block modeled mono retains the native real FFT, including macOS vDSP, and processes N/2+1 frequency bins including DC and Nyquist. Stereo retains the packed, independent channel representation.
- Fuse four spectral multiply-accumulate passes into one traversal. MSVC x86/x64 uses explicit four-bin SIMD with unaligned loads/stores and a scalar remainder; multiplication and addition/subtraction order are retained.
- Reuse immutable prepared response spectra in the existing eight-entry worker cache. Each microphone/kernel retains its own FFT, input/history, overlap, partial-block and fade state. Preparing a new processing spec clears the cache.
- Cache unchanged filter coefficient and gain/polarity calculations. The settled zero-delay path retains pre-gain delay history and applies gain without repeating the delay loop; fractional-delay wrapping uses bounded arithmetic.
- Preserve full authored responses, the 4×block FFT / 3×block partition schedule, zero added latency, 20 ms parameter smoothing, serialized 50 ms A/B fades, six active plus up to three fading engines, factory/user-IR behavior, parameter/model identities and serialized preset state.
- Add a separate profiling executable. Timing hooks compile out of normal product and acceptance binaries.

Final diagnostic measurements at Expanded 96 kHz / 64 stereo, during worker activity and automation, show convolution as the principal callback cost. Values are per-callback accumulated stage **p50 in µs**, including six microphones:

| Platform | Parameter updates | Kernel adoption | Forward FFT | Inverse FFT | Spectral MAC | Mic postprocessing |
|---|---:|---:|---:|---:|---:|---:|
| Windows | 0.200 | 0.200 | 9.700 | 10.400 | 22.900 | 0.400 |
| Linux | 0.221 | 0.239 | 61.646 | 61.595 | 37.850 | 0.760 |
| macOS | 0.209 | 0.251 | 2.458 | 2.248 | 57.959 | 0.751 |

The diagnostic executable is separate from acceptance timing. Nested scopes and independently sorted quantiles must not be summed. Worker-side acoustic response generation remains the dominant worker cost; its observed total was 226.165 ms for three responses on Windows, 372.775 ms for three on Linux and 267.723 ms for four on macOS. Those are worker totals across the diagnostic phase, not audio callback durations. Atomic adoption and parameter processing are much smaller costs in these observations; the evidence does not attribute every historical or remaining outlier to one cause.

## Native validation at the final SHA

| Platform and build | Native job | Baseline | Candidate | Candidate callback deadline misses |
|---|---|---:|---:|---:|
| Windows Server 2025 x64, MSVC 19.51, Release, EPYC 9V45 | [114251328710](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38065201764/job/114251328710) | 3/3 CTests; 36/36 p99 | 5/5 CTests; 66/66 p99 | 0/105,600 |
| Ubuntu 22.04 x64, GCC 11.4, Release, EPYC 7763 | [114251328941](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38065201764/job/114251328941) | 3/3 CTests; 36/36 p99 | 5/5 CTests; 66/66 p99 | 0/105,600 |
| macOS 15 arm64, Apple Clang 17, Release, Apple M1 virtual runner | [114251328888](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38065201764/job/114251328888) | 3/3 CTests; 36/36 p99 | 5/5 CTests; 66/66 p99 | 51/105,600 |

The original workload is preserved: 44.1/48/96 kHz, 64/256 samples, mono/stereo, three cabinets and six microphones, with 1200 worker/automation callbacks plus 400 forced six-slot publication callbacks per timing execution. The first 80-callback automation storm is unchanged. Acceptance remains the strict sorted-sample predicate `times[1584] < 1e6 * block / sr`.

Expanded timing adds 32/128/512 samples while repeating 64/256. Thus there are 66 measured timing executions per platform, covering 54 distinct engine/rate/buffer/channel configurations. No reduced test load, relaxed tolerance, fast-math flags or retries were introduced.

### Expanded CAB: same-runner baseline → final candidate

96 kHz / 64 samples, budget **666.666667 µs**. All rows compare `aaa1330 → 4bfddbc8` within the corresponding final native job.

| Platform | Channels | p50 µs | p99 µs | Misses per 1600 |
|---|---|---:|---:|---:|
| Windows | Mono | 41.600 → 35.300 | **80.500 → 165.500** | 0 → 0 |
| Windows | Stereo | 54.700 → 44.100 | 102.400 → 77.500 | 0 → 0 |
| Linux | Mono | 166.964 → 151.073 | 264.527 → 238.306 | 0 → 0 |
| Linux | Stereo | 184.155 → 169.457 | 288.721 → 272.069 | 0 → 0 |
| macOS | Mono | 55.916 → 23.084 | 376.042 → 171.167 | 4 → 0 |
| macOS | Stereo | 76.125 → 38.542 | 369.333 → 156.500 | 4 → 0 |

Windows Expanded mono has a **105.59% p99 increase** despite a lower median and zero misses. No universal p99 improvement is claimed.

Original CAB 96 kHz / 64 p99 also passes:

| Platform | Mono baseline → candidate µs | Stereo baseline → candidate µs |
|---|---:|---:|
| Windows | 110.200 → 65.800 | 105.800 → 89.300 |
| Linux | 278.011 → 246.952 | 286.868 → 277.600 |
| macOS | 408.667 → 160.291 | 643.833 → 219.584 |

Across the 36 paired default cases, p99 improved/worsened in 26/10 cases on Windows, 25/11 on Linux and 29/7 on macOS. All 18 paired 64-sample medians improved on Windows and Linux; their corresponding p99 values improved in 16/18 and 17/18 cases. Other increases remain in the raw comparison evidence, including macOS Layout 96 kHz / 64 stereo (188.542 → 294.459 µs) and /256 stereo (327.458 → 513.375 µs).

### Additional 32-sample coverage and remaining tail

Expanded 96 kHz / 32 samples, budget **333.333333 µs**:

| Platform | Mono p99 µs | Stereo p99 µs | Mono / stereo misses per 1600 |
|---|---:|---:|---:|
| Windows | 50.700 | 77.600 | 0 / 0 |
| Linux | 148.879 | 191.950 | 0 / 0 |
| macOS | 109.458 | **323.917** | **1 / 13** |

The macOS 96 kHz / 32 stereo worker/automation phase had p99 327.292 µs and 10/1200 misses; forced publication had p99 283.416 µs and 3/400 misses. Aggregate p50 was 42.916 µs, thread CPU p99 277.750 µs and maximum 1674.166 µs. Wall and thread CPU quantiles are independently sorted and do not identify the same callback, so these figures do not establish a scheduling-only explanation.

Across matched default cases, macOS misses decreased from 58/57,600 baseline to 14/57,600 candidate; additional extended cases contributed 37/48,000. Its largest observed maximum relative to budget occurred at Expanded 48 kHz / 32 stereo: 8206.292 µs maximum against 666.667 µs, with three misses in that case. The passing p99 result must not hide these individual overruns.

## Audio correctness, independence and compatibility

All three native platforms passed:

- 144 convolution-oracle cases across modeled v1/v2/v3 and user IR, three sample rates, mono/stereo, six maximum buffers, and both fixed and irregular 1/17/63/max/3 chunking.
- 18 mic-post cases and four same-instance reprepare cases.
- Six independent three-cabinet/six-microphone direct-FIR cases, including transitions and reuse; stale, six-ready and reused worker kernels.
- Added latency 0, reset residual 0, unchanged 50 ms fade and zero watched callback C++ new/delete calls. The allocation check is not a universal malloc/lock tracer.

| Platform | Maximum uniform-convolution residual | Maximum six-mic direct-FIR residual | Maximum integration-kernel residual |
|---|---:|---:|---:|
| Windows | 1.25729e-08 | 4.84288e-08 | 7.45058059692e-09 |
| Linux | 1.25729e-08 | 4.84288e-08 | 7.45058059692e-09 |
| macOS | 1.86265e-08 | 4.65661e-08 | 5.58793544769e-09 |

Existing numerical tolerances are retained. Parameter declarations, acoustic model/preset definitions, model version keys, factory/user-IR loading behavior and the `USER_IRS`/`IR` serialized schema remain unchanged. Shared data is immutable response preparation; mutable per-mic/channel histories and transitions remain independent. Kernel retirement/destruction stays off the audio callback.

## First-candidate regression retained

The first candidate `f10f24a102b9fec73c4642810a9b5e13d66f45f4`, tree `e43cdc2d834012555912a68bd16db56bb2e9cb26`, passed all three native p99 gates in [run 38063987771](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38063987771). Its Windows 64-sample median and p99 nevertheless worsened in 17/18 paired cases. Expanded 96 kHz / 64 forced-phase median increased 99.6 → 153.2 µs; diagnostic MAC median reached 103.3 µs during worker activity.

That observed regression motivated the targeted MSVC SIMD follow-up in `4bfddbc8`; acceptance success alone was not treated as evidence of improvement. The final same-runner results show all 18/18 Windows 64-sample medians improving. The first runner used EPYC 7763 and the final runner EPYC 9V45, so raw cross-run timing ratios cannot be assigned wholly to the SIMD change. Original first-run artifacts and measurements are preserved in the documentation and first run. No rerun-to-green was used.

## Evidence integrity

Each native artifact was independently audited for its outer ZIP SHA256, every indexed file digest/size, clean before/after source SHA and tree, full CTest/JUnit population, all 36 paired rows, all 66 candidate rows and 12 diagnostic conditions. Each artifact passed 90 receipt/source/coverage checks. Complete logs, compiler/CPU metadata, binary hashes, source receipts, phase measurements, comparisons and profiles are retained in the run artifacts.

| OS | Artifact ID | ZIP SHA256 |
|---|---:|---|
| Windows | 11674397499 | `ae32de0c3dd8a10892843bce5f25011df35148167cab3a55db6e69cc77cd5c15` |
| Linux | 11674813135 | `764e5e2c4aaaa12f431df22af26462b6c591bb8d440ae43c74e7d67be9b396a0` |
| macOS | 11674878159 | `0119f66f7b4167861d9f962a00ace4a495e47f30a9bb77de882118ccec5e2af0` |

## Release integration and surrounding CI

Base is PR #35's `release/1.3.0-preparation` at `aaa1330dd5cf7d51faf1c45a7cef633b9b118458`. PR #42 is mergeable against that base. PR #41 remains at `07dbfa5b0da24ae7a6000ad6dc215ba2aea67c46`; its current changed-file list has no overlap with this PR. Central horn rendering and the separate CAB navigation UI failure are outside this DSP change. Existing Product/Candidate/CAB workflows are retained.

Status checked **2026-10-10 16:06:45 UTC**, all referring to final `4bfddbc8`:

- [CAB realtime performance 38065201764](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38065201764): **SUCCESS**, three platforms.
- [Niflheimr technical measurements 38065201822](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38065201822): **SUCCESS**.
- [Niflheimr DI contracts 38065201710](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38065201710): **SUCCESS**.
- [CAB Panel contracts 38065201893](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38065201893): **IN PROGRESS**.
- [Windows Candidate 38065201856](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38065201856): **IN PROGRESS**.
- [Product / Open Beta 38065201763](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38065201763): **IN PROGRESS**; its universal macOS build is distinct from the completed native arm64 DSP job.

The first candidate's surrounding Product/Candidate/CAB-Panel runs were cancelled after the follow-up commit, not passed. This PR remains **Draft** for release integration review and the remaining small-buffer tail limitation. No merge or release publication has been performed.

Implementation and first-candidate investigation: [CAB_REALTIME_PERFORMANCE_1_3.md](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/blob/4bfddbc8d3407429353450851b917f4769abd0c2/docs/CAB_REALTIME_PERFORMANCE_1_3.md). Final exact-source results are recorded above without changing the tested commit.


</details>


</details>

