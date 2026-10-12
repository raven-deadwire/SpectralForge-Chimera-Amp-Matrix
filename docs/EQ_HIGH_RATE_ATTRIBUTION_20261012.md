# EQ high-rate attribution and qualification decision — 2026-10-12

## Result and development decision

The completed attack-loss repair is preserved. A narrow analyzer optimization
and a source-bound attribution fixture are implemented in Draft PR
[#52](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/pull/52).
The final code execution,
[38243938568](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38243938568),
passed all six required tests on Linux, Windows and macOS, including the new
44.1/48/96/192 kHz regressions. These are functional and instrumentation results.
**Physical DAW/audio-interface performance and final October 23 installer
acceptance remain `NOT_ASSESSED`; final acceptance is withheld.**

The earlier low production ratio is not evidence of an FFT-only bottleneck.
The controls identify a source scheduling problem in the synthetic fixture,
substantial full-processor work, and independent rendering/FFT costs. macOS can
underproduce severely with neither DSP nor FFT nor a native window. Windows
can sustain nearly nominal aggregate no-op/EQ-pair production but underproduces
on the full processor with the analyzer OFF. Sleep wake delays, callback work
and the fixture's permanent clock rebasing interact differently on the runners.

Retain the narrow power calculation change as a **development candidate**.
Linux and Windows CI paired update costs improve in every measured cell. macOS
shows overall improvement but repeated non-improving conditions, including
192 kHz / 2048 FFT / a full FIFO. Those results remain unresolved; neither
uniform cost improvement nor macOS performance acceptance is claimed. No
fixture-specific production branch is introduced to hide them. No public-beta
reference, release asset, version, feed or publication path is changed.

## Preserved attack result and exact source identity

The established chronological N/4-hop repair is
[`beb72f520721685f3698883297371be7def92198`](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/commit/beb72f520721685f3698883297371be7def92198),
with completed three-platform verification
[38224717793](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38224717793).
Its [84 evidence files](evidence/eq-transient-fix-20261012/README.md) retain their
original bytes, including 68 LF and 16 CRLF files. Their canonical SHA-256 tree
digest remains `bb47058bd2798bea744da7d61754aefb5b7dbd5d8488086c313a5ae92897bc2e`.
The original missing-attack investigation and negative controls were not
repeated. The new four-rate tests regress that established behavior.

The implementation is
[`ddb0880bd61a4c794236783b869767b96208bf41`](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/commit/ddb0880bd61a4c794236783b869767b96208bf41).
The portability follow-up is
[`72e361ba91ea2c6b8d830921a71275fa25cba493`](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/commit/72e361ba91ea2c6b8d830921a71275fa25cba493).
All canonical Git blob bytes for C++, CMake and workflows are identical between
those commits; raw Windows checkout bytes can differ in line endings. Their
measurements remain separate executions. Source hashes, compiler flags, runner
identity, JUnit, raw CSV, failure evidence and artifact digests are indexed in
[the evidence directory](evidence/eq-high-rate-attribution-20261012/README.md).

| Execution | Tested source | Linux | Windows | macOS |
|---|---|---|---|---|
| [First CI: 38240150849](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38240150849) | `ddb0880` | Six tests passed; 90 attribution cases | Build succeeded; provenance failed; tests **NOT_RUN** | Six tests passed; 90 attribution cases |
| [Second CI: 38243938568](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38243938568) | `72e361ba` | Six tests passed; 90 attribution cases | Six tests passed; 90 attribution cases | Six tests passed; 90 attribution cases |
| Full CMake local execution | `ddb0880` | Four tests passed; 22 direct cases; native UI **NOT_RUN** | Not executed locally | Not executed locally |

The first Windows failure is retained as a failure. Its reference-hash exception
is consistent with the independently reproduced Git checkout line-ending
conversion; that reproduction changes worktree bytes without changing canonical
blobs. The first failed run did not save the actual reference hash or checkout
configuration, so its specific conversion is an inference. The follow-up marks
immutable inputs `-text` and checks both
canonical Git bytes and exact worktree bytes separately. Expected hashes are
unchanged. Three checkout modes and four intentional corruption cases passed
[seven preservation scenarios](evidence/eq-high-rate-attribution-20261012/provenance-portability/validation-result.json).
The second Windows CI then passed the same strict provenance gate and actually
executed all six tests, recording `core.autocrlf=true` and exact immutable-byte
preservation. That success is not applied retroactively to the first
execution. The evidence-only follow-up also protects this new receipt directory
from checkout normalization; it does not change tested runtime behavior.

## Measurement design and boundaries

The historical `SyntheticAudio` source produces 128 frames on an ordinary
`std::thread`, calls the processor and sleeps to its next deadline. At 192 kHz
the nominal period is **666.667 microseconds**. After adding the next period,
if processing has finished more than two periods late, the historical source
sets its deadline to the current time. This discards source-clock debt. Its
old callback timer excludes generation, buffer fills, sleep/wake delay and the
discarded source time, so callback cost alone cannot explain production ratio.

`ChimeraGraphicalEQUITests --attribution <directory>` now provides 90 cases:

| Control | Conditions | Measured distinction |
|---|---|---|
| Producer/audio stage | 192 kHz no-op, two EQs, full processor; FFT OFF | Source pacing and generation versus audio-route work |
| Source clock | Historical sleep/rebase; due-sample catch-up | Permanently discarded source time versus retained debt |
| Signal generation | Per-sample sin/fmod; identical precomputed tone | Generator work outside the processor |
| Consumer/UI | Direct consumer; native panel drawing suppressed; native drawing enabled | FIFO/FFT, event delivery, paths and panel rendering |
| Rate/FFT/cadence | 44.1/48/96/192 kHz × OFF/2048/4096/8192 × 20/40/80 ms | Input-rate, transform-size and timer sensitivity |
| Repeated endpoints | Native FFT-OFF trials repeated at the end | Within-run variability |

The direct subset has 22 cases. Native captures use a real CI desktop/window,
with the existing test-only event pump; they do not use a DAW's event loop or
audio device. Each source capture lasts roughly 1.2 or 2 seconds, with actual
elapsed duration recorded. The new full-route fixture enables both EQs with a
+6 dB first band and default Q. The historical UI sequence restored a different
observation state after Q interactions, so old UI and new attribution captures
are not identical full-process A/B trials.

All waveform caches, audio buffers and bounded record storage are allocated
before the measured source starts. Raw CSV writing and quantiles occur after
source stop/join. New sleeps, joins and recording vectors are confined to the
test executable. Production capture/FIFO/audio processing is unchanged. The
[static code review](evidence/eq-high-rate-attribution-20261012/code-review-72e361ba.json)
found no newly added production callback allocation or wait. Panel observers
are inactive by default and operate on the message thread; production cadence
remains 25 Hz / 40 ms.

Every callback records generation time, processor wall time, entry and original
source-clock lateness, positive sleep requests, actual waits, wake lateness,
rebases and abandoned source time. Update rows record every observed consumer
call, including calls with no FFT window. CPU counters are separate from wall
timers. FFT/update/path/paint/CF/AppKit times are inclusive and can overlap;
they must not be added and called CPU. Raw CPU limitations appear below.

**Due-sample catch-up is a diagnostic source-clock policy, not a real-time
callback implementation.** It can reach nearly 100% aggregate production by
emitting many late blocks in succession. Late starts, execution overruns,
maximum clock debt and consecutive catch-up blocks remain part of each result.

## Where the underproduction occurs

The next tables use only the final `72e361ba` execution. Percentages are
`produced frames / (actual elapsed seconds × nominal rate) × 100`. The platforms
use different CI hardware and architectures, so they are matched configurations,
not an OS-only hardware benchmark. Primary receipts are the
[Linux](evidence/eq-high-rate-attribution-20261012/ci-38243938568/Linux/receipt-manifest.json),
[Windows](evidence/eq-high-rate-attribution-20261012/ci-38243938568/Windows/receipt-manifest.json)
and [macOS](evidence/eq-high-rate-attribution-20261012/ci-38243938568/macOS/receipt-manifest.json)
artifacts.

### 192 kHz stage controls: analyzer OFF, no native UI

| Stage | Linux legacy / due | Windows legacy / due | macOS legacy / due |
|---|---:|---:|---:|
| No-op producer | 99.9941 / 99.9957% | 99.5652 / 99.9069% | **20.6886 / 99.9083%** |
| EQ pair | 99.9958 / 99.9958% | 99.4811 / 99.9014% | **21.1358 / 99.7734%** |
| Full processor | 99.9959 / 99.9957% | **53.5606 / 99.9451%** | **65.0940 / 99.7937%** |

**macOS source wake pacing:** Severe legacy underproduction is reproduced in
no-op and EQ-only controls without FFT, expensive audio DSP or native UI. The
second no-op producer consumes approximately 0.81% of one core yet produces
20.69%. All 312 no-op, 299 EQ-pair and 265 cached-no-op rebases were already
more than three periods late at entry and followed a positive sleep request.
The identical precomputed no-op tone still produces only 18.52%. In the first
execution the corresponding no-op produces 20.07% at 0.85% producer CPU; its
positive-request wake lateness is p50 **5.591 ms**, p99 **8.427 ms**, compared
with a 0.667 ms period. The supported cause is the ordinary-thread source/wake
path in this fixture and runner. A specific timer policy, core assignment,
thermal state or physical-device failure is not identified.

**Windows audio-work and pacing interaction:** No-op and EQ-pair controls have
almost nominal aggregate production, with many catch-up blocks. Adding the
full processor reduces legacy production to 53.56% even with FFT and UI absent.
The legacy full source uses approximately 42.16% of a core while underproducing;
due pacing uses approximately 83.52% while producing almost nominal samples.
Full-route callback p99 is 815.1 microseconds for legacy and 701.7 microseconds
for due, both beyond the 666.7 microsecond nominal budget. Wake lateness p99,
restricted to positive sleep requests, is 1.869 ms for legacy and 2.073 ms for
due. The raw summary's different wake quantiles also include no-sleep catch-up
rows. Audio work consumes much more
of the available time than the EQ-pair control, and late wakeups/rebasing turn
that limited slack into lost simulated source time. Process wall intervals
include any descheduling; they are not proof that every overrun is pure DSP.

Raw trigger classification is more specific: 556 of 557 full-OFF Windows
rebases cross the three-period threshold during the process wall interval,
but all 556 already enter more than one period late. Median entry lateness is
1.803 ms and median process time 0.5266 ms; 551 follow a positive sleep request.
Of these 556 events, **537 processor calls themselves fit the 0.667 ms block
budget**. Approximately 1.8 ms of wake debt plus an otherwise in-budget call
crosses the 2 ms reset threshold. The 927.459 ms of rebased source time explains
99.7833% of the 929.473 ms production-time deficit. Describing this as the
processor continuously exceeding its execution budget would be incorrect.

**Source-clock accounting:** The rebase trigger is equivalently
`entry lateness + generation wall + process wall > 3 × period`. Rebasing
mechanically explains abandoned simulated audio time; it does not, by itself,
identify which interval caused the original lateness. For example, first-run
macOS full OFF loses 1.172230 seconds of source time, of which 1.172136 seconds
(99.9919%) is recorded rebasing. Of its 390 rebases, 326 were already over the
threshold at entry and 64 crossed during the process wall interval. Neither
generation nor FFT can explain the no-op control's late-entry rebases.

**CI execution variability:** In two Linux captures with all 111 recorded
Source/Tests C++ hashes identical, full-OFF producer CPU changes from about
85% to 51% of a core. The same-code paired full-FIFO baseline also runs faster
in the second capture. CPU frequency, physical worker identity and per-trial
core placement are not recorded. Treat these as execution-environment
variation, not an effect of the portability-only commit. Paired measurements
within one executable are the basis for the narrow analyzer cost claim.

### Native ON/OFF comparison at 192 kHz / 40 ms

| Panel and analyzer | Linux legacy / due | Windows legacy / due | macOS legacy / due |
|---|---:|---:|---:|
| Native panel, FFT OFF | 99.9957 / 99.9948% | **67.5141 / 99.8763%** | **65.9890 / 99.8596%** |
| Native panel, FFT 4096 ON | 99.9958 / 99.9914% | **73.9587 / 99.9563%** | **68.0696 / 99.9867%** |

The OFF controls also underproduce. Sequential ON/OFF ratios are not monotonic,
and repeated endpoints expose variability, so an ON-minus-OFF production
percentage is not an isolated FFT cost estimate. The new controlled results
are consistent with the earlier observation that the analyzer's relatively
small CPU-equivalent cost cannot explain the entire source deficit.

### FIFO consumption, FFT batches and UI work

All 90-case attribution matrices completed exact sample accounting. Across
the five native CI datasets there were no FIFO rejections or stale-frame
discards in these short captures. Final pending frames remain pending and are
not described as lost. Sampled high-water is a lower bound on the actual
instantaneous maximum. These observations do not guarantee continuity over
long sessions or arbitrary message-thread stalls.

The first macOS direct 40 ms consumers reached **168–189 ms** between calls,
with the first setup interval excluded, even without a native window. Native
paint-suppressed consumers reached roughly 84–102 ms, and FFT-OFF heartbeat
gaps reached 90–111 ms. Long service gaps therefore do not require FFT or
panel drawing. The test-only CF/AppKit pump also spends inclusive time waiting
and dispatching. It differs from a DAW host loop and has no timestamped trace
that assigns every service gap to one CF or AppKit call.

FFT backlog does impose real message-thread work: all chronological windows
are still consumed. In the second Linux paired benchmark, a full 65,536-frame
FIFO takes candidate update medians of **41.48 / 42.16 / 47.66 ms** for
2048/4096/8192 FFT. These are finite catch-up workloads, not UI scheduling gaps
and not a real-time pass. At 192 kHz the FIFO represents about 341 ms of audio;
that capacity is not a guarantee that the host can tolerate such a stall.

Rendering has an independently measurable cost. In first-run Linux due-paced
192 kHz / 40 ms controls, message CPU falls from **36.9–40.0%** to **22.7–26.1%**
of one core when EQ-panel drawing is suppressed. Direct paint wall equivalent
is **12.2–12.6%**. Other editor controls and spectrum-path work still remain.
For FFT 2048 at 20/40/80 ms, paint equivalent is **25.27 / 12.11 / 5.78%**, while
inclusive analyzer work stays **18.77 / 18.60 / 18.43%**. Other FFT sizes have
the same cadence pattern. The second run's lower absolute paint cost reinforces
the need to avoid comparing unpaired runner timings as code gains.

This supports static background/grid caching and repainting only changed
spectrum regions as future rendering candidates. A longer timer interval
reduces repaint/path frequency but does not remove the scheduled overlap-FFT
work per sample; it increases display delay and queued work per update. A
worker-thread analysis design would require separate lifecycle, continuity,
snapshot and scheduling qualification. Neither timer/window dropping nor a
new audio-callback wait/allocation is part of this candidate. There is no
measured basis for changing the retained N/4 hop, FIFO size or default cadence.

## Implemented analyzer optimization and paired results

The candidate calls the real FFT and computes calibrated power directly from
its interleaved complex bins. It avoids the frequency-only helper's magnitude
square roots followed immediately by squaring, caches coherent power gain,
and removes redundant tail clearing. The backend, every chronological N/4
window, 65,536-frame FIFO, stereo power meaning, smoothing in sample time,
corruption recovery and audio path remain intact. DC/Nyquist endpoint gain
and finite-overflow guards are preserved.

The exact already-fixed analyzer header is compiled beside the candidate in
one executable. Its frozen SHA-256 is
`ad0e9f7dc7fe46579b24707291bf63a5ee09363de701b1606c5cedf98f970f26`.
The fixture uses **ABBA BAAB**, four measurements per implementation for each
of 48 cells: four rates × three FFT sizes × 20/40/80 ms or a separately labelled
full-capacity catch-up. Each run has 384 raw cost trials and 108 parity cases.
Compare complete analyzer update wall time, or separately measured
feed-plus-analysis thread CPU. Baseline `fftMicros` includes magnitude/tail
work that the candidate's real-transform-only interval excludes; a comparison
of those narrower timers is not an identical FFT-kernel comparison.

| Dataset | Cells with lower candidate update median | Median of cell-wise update reductions | Measured thread CPU interpretation |
|---|---:|---:|---|
| Local minimal direct build | 48/48 | 9.327% | Separate local experiment; A/A controls retained |
| Local full CMake `ddb0880` | 45/48 | 8.666% | Median cell-wise reduction 8.628%; three wall counterexamples retained |
| Linux CI `ddb0880` | 48/48 | 3.717% | 48/48 lower; median reduction 3.733% |
| Linux CI `72e361ba` | 48/48 | **2.362%** | 48/48 lower; median reduction 2.338% |
| Windows CI `72e361ba` | 48/48 | **5.632%** | **QUANTIZATION_LIMITED**; no precise headline CPU saving |
| macOS CI `ddb0880` | 39/48 | 14.642% | 40/48 lower; median reduction 11.031% |
| macOS CI `72e361ba` | 38/48 | **14.755%** | 40/48 lower; median reduction 11.945% |

These are descriptive per-cell medians, not weighted production workloads,
confidence bounds, or real-time acceptance. Raw non-improving cells are kept.
The local loaded A/A, idle A/A and A/B runs remain separate in the evidence.
The clean local A/A median apparent gain is about 0.26%, with much larger
individual-cell variation; it is not a calibration of macOS CI uncertainty.

Every native execution passed all **108 spectral parity conditions**, including
DC/Nyquist, bursts, tones/noise, antiphase stereo and smoothing, plus separate
corruption-containment/recovery checks. The latter are not corrupt-input A/B
parity conditions among those 108 CSV rows.
Maximum raw/envelope/display error was **0.0000152587890625 dB**; exact expected
windows were retained, with no cost/parity FIFO drops or parity discontinuity.

### Repeated macOS counterexamples remain open

The [two-run comparison](evidence/eq-high-rate-attribution-20261012/macos-two-run-comparison.md)
retains every raw trial and the conditions that fail to improve twice. In
192 kHz / 2048 FFT / full-capacity replay, all trials process 65,536 frames and
128 windows without drops:

| Metric, ms | First baseline → candidate | Second baseline → candidate |
|---|---:|---:|
| Update wall median | 5.248375 → 7.168792 | 5.356334 → 5.690104 |
| Update cost increase | **36.591%** | **6.231%** |
| Feed + analysis actual thread CPU median | 6.891167 → 8.723229 | 6.451500 → 6.952959 |
| CPU cost increase | **26.586%** | **7.773%** |

Second-run adjacent-pair update reductions are +34.0%, +22.1%, −76.3%, −38.2%.
Both the repeated median increase and the within-run variation matter. Actual
CPU also increases, so this cannot be dismissed as merely an off-CPU wall
outlier. Another cell, 44.1 kHz / 8192 FFT / 80 ms, increases both wall and CPU
in both executions; 48 kHz / 2048 FFT / 40 ms has wall increases in both.
Other non-improving cells are preserved per run. Physical worker/core/frequency
traces are absent, so the precise cause is unresolved. Overall gains justify
continuing development, but these conditions prevent a blanket cost-regression
clearance or final installer acceptance.

## Four-rate functional regressions and timing qualifications

The second run executed all six required CTests on **each** OS, with zero
failed or skipped CTest cases: GateProcessor, GraphicalEQ, GraphicalEQUI,
EQAnalyzer, EQAnalyzerPerformance and EQHighRateAttribution. OS-inapplicable
workflow branches are separately skipped and are not skipped CTest cases.
Windows also built the existing development Standalone target. This is build
evidence; it is not an installer, plugin-host or physical-device qualification.

The dedicated receipts contain the exact 44.1/48/96/192 kHz grids below on
each platform. The full local CMake execution supplies additional separate
headless evidence.

| Contract | Cases per platform | Observed result |
|---|---:|---|
| Early/late attack coverage | 24 | Every bounded attack detected; minimum display −33.5502 dB; exact N/4 windows |
| UI partition and ballistics invariance | 180 | Raw/envelope/pixel differences 0; no drop |
| FIFO stalls/recovery | 48 | Exact accounting; 45 lossless cases and 3 intentional overflow/recovery cases |
| Whole-processor audio ON/OFF invariance | 12 | Bit-identical output, maximum error 0, host latency **42 samples** |
| EQ-pair callback allocation/timing fixture | 48; 28,800 callbacks | Counted allocations **0**; second CI execution overruns 0 in this fixture |
| Analyzer cadence observations | 12; 160 calls each | Every call retained; expected zero-FFT updates allowed; no FIFO drop |

The output-invariance fixture exercises a representative two-EQ processor route
with amp/cab disabled, capture switching/resolution and forced FIFO overflow.
It is not an exhaustive audition of every amp/cab/preset. Production audio
processor, EQ coefficients, parameter layout and latency policy are unchanged.

The intentional 192 kHz × 500 ms stall produces 96,000 frames, exceeding FIFO
capacity. Each FFT setting verifies 30,464 rejected frames plus 65,536 discarded
stale frames and subsequent fresh-window recovery. It would be incorrect to
call all stall tests lossless. Similarly, at 44.1/48 kHz and FFT 8192, the
2048-frame hop exceeds one 40 ms batch (1764/1920 frames); a zero-FFT first call
is legitimate, and bounded attack detection still passes.

The EQ-pair allocation/timing fixture is distinct from the full-processor native
callback measurements below; it does not certify all amp/cab paths allocation-free.
Timing exceptions in other executions are retained: the first macOS EQ-pair callback
fixture has one 192 kHz / FFT 8192 / block 512 overrun, maximum 3345.71 us
against a 2666.67 us budget. The full local execution has one 96 kHz / block 64 /
FFT 8192 overrun and one 192 kHz / block 64 / FFT 4096 overrun. The absence of
these events in the second CI functional fixture does not erase them.

### The native rate grid reaches aggregate rate by catching up

Each rate below has 12 native-painted due-paced cases: FFT OFF/2048/4096/8192
× 20/40/80 ms. These are the second execution's observed production ranges.

| Rate | Linux | Windows | macOS | Callback execution overruns across 12 cases, L/W/M |
|---:|---:|---:|---:|---:|
| 44.1 kHz | 99.8285–99.9940% | 99.8722–99.9416% | 98.7670–99.8804% | 0 / 8 / 13 |
| 48 kHz | 99.9930–99.9941% | 99.8704–99.9326% | 98.9713–99.9624% | 0 / 9 / 20 |
| 96 kHz | 99.9864–99.9940% | 99.8032–99.9693% | 99.4120–99.9112% | 0 / 85 / 134 |
| 192 kHz | 99.9930–99.9961% | 99.7340–99.9393% | 99.5007–99.9631% | **140 / 5631 / 6207** |

There are no FIFO rejects/discards in those captures, but they are plainly
not a no-overrun result. Counts are from short, sequential cases with actual
callback totals retained; they are not normalized long-session failure rates.
The fixture's `NEAR_NOMINAL_DIAGNOSTIC` category has a coarse 95% lower bound;
this table retains the actual deficits instead of calling that label PASS.

For a concrete final-run example, Windows FFT 4096 / 40 ms native due case 34
produces **99.9563%**, yet has **591** callback execution overruns, a **4.805 ms**
callback maximum, **6.713 ms** source-clock lateness and **2843 catch-up blocks**.
The matching macOS case produces **99.9867%** with **870** overruns, a **5.900 ms**
maximum, **29.229 ms** lateness and **2794 catch-up blocks**. In the first-run
macOS native full-OFF case, 99.9048% production coexists with a 26.352 ms callback,
82.382 ms clock lateness and 910 consecutive catch-up blocks. Aggregate rate
alone is insufficient for host stability.

## CPU-counter limitations and inference limits

**Windows paired CPU:** All 384 raw GetThreadTimes observations in this runner
are multiples of **15.625 ms**. One candidate full-FIFO trial records CPU 0 while
update wall is 14.5834 ms and feed-plus-analysis wall is 14.7150 ms. CPU medians
are lower/equal/higher in 15/24/9 cells. The arithmetic median reduction of 0%
is not proof of no CPU saving, and CPU 0 is not zero work. This observed
quantization limits a precise CPU claim; it is not asserted as a universal
GetThreadTimes resolution. Native short CPU captures also need this precision
qualification. Complete-update wall measurements remain available.

**macOS native aggregate inconsistency:** First-run cached no-op due case 14
has process minus producer minus message CPU of −0.304 percentage points.
Second-run native 192 kHz / FFT 8192 / 40 ms case 85 has a much larger mismatch:
process 15.387%, producer 75.439%, message 10.727% over 1.201451 seconds. The
component counters total 1.035237 CPU-seconds while the process counter reports
0.184867 CPU-seconds. That aggregate and its residual cannot support performance
inference. The raw values are retained, not clipped or replaced. Absolute
start/end CPU snapshots were not recorded, so the API/accounting cause cannot
be reconstructed from this artifact. This is separate from steady-clock
production/timing and FIFO-count consistency, and from the paired FFT test's
independent CLOCK_THREAD_CPUTIME_ID measurements. Neither thread counter is
certified as an absolute ground truth by this comparison.

Process CPU remaining after producer/message subtraction elsewhere is unassigned
work, not identified IR workers. Local cgroup CPU additionally includes other
processes and different setup/output windows; its difference from test-process
CPU cannot be attributed to the audio path. CF/AppKit wall durations include
waits and nested callbacks; consumer intervals are not FFT execution times.
These limits prevent unsupported attribution of every delay to one component.

Windows makes this remaining uncertainty material: native full-OFF due has
unassigned process CPU of about 28.87 percentage points with panel drawing
suppressed and 53.11 points with drawing enabled. Native-painted analyzer-ON
40 ms due controls (cases 30/34/38) have residuals of about 67.1–74.2 points;
their paint-suppressed counterparts have about 28.9–39.8 points. Direct full-OFF due has process CPU
approximately equal to producer CPU within the observed quantization. Additional
process-thread work accompanies the native editor, but the raw data do not name
those workers. It cannot all be assigned to the FFT, message thread, IR or the
paint callback; the paint wall equivalent itself is only about 4.2–4.6% in those
Windows controls. A physical-host CPU trace is still needed to identify it.

## October 23 disposition and remaining acceptance work

**Current decision: retain the implementation in the Draft development PR;
withhold final October 23 test-installer acceptance.** The evidence supports
continued internal qualification of the candidate. It does not support a
public-beta update, release promotion or a claim of stable 192 kHz DAW use.

The outstanding work is concrete:

1. Resolve the repeated macOS cost counterexamples with longer matched trials
   and actual worker/core/frequency/CPU-counter observations. Keep numerical
   parity, every overlap window and all non-improving observations visible.
2. Test actual supported Windows/macOS/Linux hosts and interfaces at the four
   supported sample rates and the device's supported buffer sizes. Record the
   DAW, plugin format, driver, interface, device clock, buffer size, source SHA
   and release build. A requested rate unsupported by the interface is NOT_RUN.
3. Measure sustained playback/recording with analyzer OFF and each FFT size,
   editor visible/hidden, native message activity and representative amp/cab
   routes. Record host xruns/dropouts, callback tails, CPU, FIFO continuity,
   automation/state changes and audible transient behavior. A normal-thread
   synthetic source that catches up is not the host's hardware callback.
4. Validate the actual installation packages, signature/permissions where
   applicable, plugin discovery, project/state loading and the final selected
   build. The Windows development Standalone artifact alone does not complete
   this gate.

No physical host/interface or installer session was available in this execution,
so those rows stay `NOT_ASSESSED`. The retained attack fix is complete; these
are separate performance and packaging acceptance tasks. All detailed raw
receipts, non-improving cost cells, failed/skipped scopes and read-only audit
commands are in [the evidence index](evidence/eq-high-rate-attribution-20261012/README.md).
