# PR #68 macOS CAB failure: source-bound diagnosis

Status: investigation, no demonstrated product-performance fix or release approval.

Original source: `c2263e4c90bffb9a56cc41285ebf8207e641b3aa`.
Run: https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38572999676
Artifact: `11899426013`, verified ZIP SHA-256
`1a1b1de2748c497a980b5e2101b0c8be709db01cc7d262abf640c22456e5a39d`.
All 107 internal manifest entries verify. `original-audit.json` preserves all 66
route decisions and all 66 individual misses across 105,600 candidate callbacks.
Reproduce using `python Tools/audit_pr68_cab.py ARTIFACT_DIR AUDIT.json`.

## Two different observations

The actual failed CTest is **ChimeraCabExtendedTimingTests**, expanded-v2,
96 kHz / 32 / stereo: p99 363.667 us against 333.333333 us, 18/1,600 wall
misses. All 18 paired CPU times also exceed the budget. Its worst callback is
907: 1,613.208 us wall / 1,616.541 us paired CPU. CPU is measured around a wider
envelope than wall, so CPU-over-wall is preserved rather than clamped.

The highlighted array-v3-6x10 96 kHz / 256 / stereo interval is in the **passing
ChimeraCabLayoutIntegrationTests**. Callback 539 is 21,269.25 us wall /
114.5 us paired CPU, a ratio of **0.53833586%**. The numerical difference is
21,154.75 us, **not an exact descheduling measurement**. That route has 2/1,600
misses but p99 309.917 us against 2,666.666667 us. The existing gate uses wall
p99 despite its historical “runner CPU gate” label. Neither the long miss nor
the actual p99 failure is waived.

Overall: 66 misses, 46 with paired CPU above budget, and 9 in the forced phase
after worker shutdown. The runner reports Apple M1 (Virtual), three cores,
macOS 15 / Darwin 24.6.0. Console caller QoS is 17; worker priority is low.

## Comparison with PR #49

The measured CAB files (`Cabinet.h`, `ModeledCabConvolution.h`, `IRLibrary.h`,
`IRLibrary.cpp`, `OriginalCabIntegrationTests.cpp`) have no diff from released
`7b20cd9` to this failed source. A source-level DSP regression in those files
therefore does not explain this run; this does not clear the product or runner.

PR #49's source-bound attribution at `35617a2` recorded ABBA 02-B expanded-v2
stereo callback 910: 15,477 us wall / 145.917 us CPU; nested overlap_add was
15,340 us wall / 12.375 us CPU, worker waiting, no fading/pending slots. This
shows a similar divergence can occur outside kernel replacement and active
worker preparation. It does not establish scheduler, VM, cache or fault cause.
Its independent QoS comparison reversed the apparent benefit (A/B misses
18/13 versus 14/67), and higher QoS retained a 37.24 ms miss. No QoS change is
justified. Enabled tracing itself increased misses, so observer controls remain
mandatory. Historical results are from PR #49's committed audit, not a fresh
download/reverification of its original ZIPs in this investigation.

## What can and cannot be separated now

| Factor | Evidence and remaining limit |
|---|---|
| DSP execution | Actual failed 32-sample route has paired CPU-over-budget. Original rows lack stage timings; no FFT/MAC cause is established. Same geometry and unchanged source are insufficient to dismiss CPU spikes. |
| OS scheduling / VM | 256-sample wall/CPU divergence strongly supports time outside recorded thread execution. Exact runnable/wait/preemption/VM intervals need an OS scheduler trace. Clock-envelope subtraction cannot identify them. |
| IR worker | 539 occurred in worker phase, but no same-callback worker state was recorded. Nine misses occur after stop/join. These phases differ in publication/pacing and are not a causal worker A/B. |
| Kernel replacement | Requests stop at index 79, but asynchronous publication can arrive later. Index 539 alone does not prove no swap/fade. Add six-slot before/after and publication/retirement event joins. |
| Memory | C++ new/delete counters are zero. They do not cover malloc, page faults, cache stalls or OS locks. Add explicit copy/clear/reset scopes, separately sampled process fault/page-in counters and memory controls. Process counters include the worker. |

## Implemented diagnostic change

Port PR #49's opt-in recorder and its independent verifier to the integration
source. Normal product and original acceptance targets compile hooks out.
Extend the separate probe to 32, 64 and 256 samples; retain 1,600 callbacks,
full IRs, six microphones, 50 ms serialized fades, allocation and response
checks. Keep unmeasured response/drain duration in samples when changing block
size. Frozen baseline is the actual failed integration source, not beta.1.

The new workflow registers six separate allocations: each block size in ABBA
and BAAB order. A is hooks compiled out, B is recording on; this measures the
observer effect, **not a product optimization**. Each allocation runs both
unchanged five-test gates, semantic/trace contracts, arithmetic/memory/clock
controls and a separate resource condition. Failures remain failures; all arms
continue and raw artifacts upload even when a gate fails. No rerun-until-pass.

Local validation: original Python evidence contracts 20/20; expanded verifier
contracts 24/24; Windows native trace recorder contract PASS. These do not
establish native macOS performance. Native workflow results must be reviewed
before selecting any product fix.

## Minimum product change decision

No product optimization is currently cause-supported. Preserve the DSP until
native joins identify repeatable CPU work or lifecycle interference. If repeated
MAC cost dominates, inspect AppleClang vectorization before changing only that
loop, retaining arithmetic order and independent FIR/channel oracles. If reset
or cold publication dominates, test worker-side preparation/prefaulting with
fixed publication schedules and full readiness/fade checks. If worker overlap
dominates, use matched prebuilt publications in worker-present/absent arms;
this experiment is still outstanding. Never raise deadlines or replace wall
acceptance with CPU acceptance.

Scheduler traces (Instruments/System Trace or permitted native equivalent)
remain separate and may require runner privileges. Missing traces must be
reported unavailable, never inferred from process counters. Actual AU/VST3,
CoreAudio device, sustained DAW playback, variable blocks, UI/tuner/MIDI
interference and physical hardware acceptance remain **NOT_EXECUTED**.
