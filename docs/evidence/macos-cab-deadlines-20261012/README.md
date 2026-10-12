# macOS CAB deadline investigation — 2026-10-12

## Release boundary and current disposition

The published **1.3.1-beta.1** reference remains
`7b20cd9d1f90e3e2969c250712b199e893cf50cd`, accepted with the limitations in
[release integration PR #48](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/pull/48).
This investigation is separate development work. **The two completed native
experiments do not demonstrate a reproducible caller-QoS benefit. No product
performance fix has been demonstrated or accepted here.**

The development changes add an independent experiment controller, reproducible
archive analysis and preserved evidence. Product DSP, existing CTest assertions,
p99 thresholds, the original callback populations, version, release tags and
published assets are unchanged. Higher caller QoS is an experimental condition;
it is not a new acceptance condition or an audio-thread policy shipped by the
plugin.

## Historical findings

All six original candidate/OS archives were checked against their GitHub
artifact digests, then independently recalculated: **396 populations and
633,600 callbacks**. Every zero-miss population and every original miss field
is retained in the [historical bundle](historical/README.md).

| macOS observation | Earlier `781d03dd` | Published `7b20cd9d` |
|---|---:|---:|
| Original p99 gates passing | 66/66 | 66/66 |
| Measured callbacks | 105,600 | 105,600 |
| Individual wall-deadline misses | 74 | 54 |
| Largest measured wall interval | 27.888 ms | 29.913208 ms |
| Misses with paired CPU at/below deadline | 48 | 29 |
| Misses with paired CPU above deadline | 26 | 25 |
| Misses after the IR worker was stopped | 2 | 8 |

Windows and Linux have zero individual misses in both original candidate runs.
The [full historical audit](historical/historical-audit.md) identifies all 21
affected latest populations, requested cabinet/microphone configurations,
phases, and the two macOS execution environments.

Latest macOS miss counts by sample rate and buffer size:

| Sample rate | 32 | 64 | 128 | 256 | 512 | Total |
|---|---:|---:|---:|---:|---:|---:|
| 44.1 kHz | 0 | 4 | 0 | 0 | 0 | 4 |
| 48 kHz | 0 | 3 | 0 | 1 | 1 | 5 |
| 96 kHz | 4 | 33 | 0 | 8 | 0 | 45 |
| Total | 4 | 40 | 0 | 9 | 1 | 54 |

The 66 entries include separately executed integration and extended populations
at overlapping settings. Each interval processes three independent CAB lane
buffers sequentially on one caller, with two microphones per lane. They do not
measure complete Classic/Dual/Matrix routes, AU/VST3 or CoreAudio callbacks, the
full processor, or editor/UI interference.

The modeled request families are original-v1 bass 4x10, expanded-v2 bass 4x10
with Alloy 10, and array-v3 Bass 6x10 with Foundry 10. The historical audit
retains the exact microphone/unit, rear, tweeter and automation settings.
In the final 400-callback phase, the worker is stopped and the same replacement
key/response derived from Mic A at position 0.85 is published to all six slots.
The outgoing A/B configurations can still contribute during transitions; the
forced steady portion is not six different A/B target responses. Separate
per-slot convolution histories remain.

## What the source and clocks establish

The [source audit](source-audit.md) links immutable Chimera and JUCE sources.
The two candidates differ in opaque UI flags and Windows launcher handling;
their measured CAB DSP and C++ timing source are identical. The macOS image,
architecture and toolchain match, while runner allocation and unrecorded host
conditions differ. **74 → 54 is not evidence of a causal macOS DSP improvement.**

The largest latest wall interval pairs 29,913.208 microseconds wall with
102.25 microseconds thread CPU at 48 kHz/256/mono. Most of that wall interval is
not represented in the recorded thread CPU interval. The responsible
scheduling, waiting, memory or VM event was not traced.
Conversely, 25 misses also exceed the budget in paired CPU, including five after
the worker stopped. The two clock envelopes differ; CPU includes both wall-clock
reads and must not be treated as exclusive DSP execution or subtracted as exact
scheduler time. VM accounting, memory residency and intermittent stage cost
remain to be separated by additional evidence.

No explicit callback allocation, library mutex wait or kernel destruction was
found in the measured custom convolution route. The C++ new/delete guard is
narrower than native malloc, page-fault or lock tracing. Apple vDSP processing
does not use the JUCE fallback FFT's spinlock. UI work and the host wrappers
require a separate full-plugin measurement because this console test does not
instantiate them.

## Prespecified independent controls

The [controller](../../../Tools/CabDeadlineDiagnostic/README.md) builds the
unchanged source separately. Each native run executes the original five CTests
first, then four CAB and four synthetic populations, with 66 configurations and
105,600 intervals in each population set. All conditions on one runner use the
same compiled probe. Source/dependency hashes, binary hashes, compiler settings,
command receipts, scheduling observations and all CSV rows are retained.

- **A:** inherited caller QoS; no priority setter is called.
- **B:** request and verify user-interactive caller QoS; IR worker priority is unchanged.
- **Sentinel:** a separate arithmetic-only program mode with no CAB, worker or UI.
  It follows the same nominal outside-timer sleep cadence, but its shorter work
  and different exposure prohibit direct subtraction or equal-rate assumptions.

ABBA was fixed before measurement. The independently allocated second runner
uses BAAB. The reversing controller commit is timestamped
2026-10-11 23:55:07 UTC; the complete ABBA artifact was created at
2026-10-12 00:09:11 UTC. The reverse order was therefore committed before the
first complete result was available. The [registration receipt](experiments/protocol-registration.json)
retains these GitHub timestamps and identities. This is a bounded pair of
experiments, not a rerun-until-pass procedure. Neither condition requests an
actual CoreAudio real-time policy or certifies DAW behavior.

## Completed native results

Both runs independently verify in full: **1,188 route populations and
1,900,800 intervals**, comprising **1,056,000 CAB callback intervals** and
**844,800 separate synthetic intervals**. All ten CAB population sets retain
their complete 66-route p99 passes. Each run also passes its original five
selected CTests. Every original miss field and every measured zero-miss route
is retained; the two runs contain **170 CAB callback misses and five synthetic
misses** in total. Passing p99 is not equivalent to zero individual misses.

| Native run | Order | Full independent audit | Original artifact |
|---|---|---|---|
| [38185786916](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38185786916) | ABBA | [Report and complete tables](experiments/native-ABBA/README.md) | [11720400815](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38185786916/artifacts/11720400815) |
| [38186063935](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38186063935) | BAAB | [Report and complete tables](experiments/native-BAAB/README.md) | [11719919391](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38186063935/artifacts/11719919391) |

Each CAB or sentinel entry below contains 105,600 intervals. CPU-over counts
refer to wall misses whose paired CPU also exceeds the same budget; they do not
identify exclusive DSP time. Ordinary gates have a different suite order and
are kept separate from the experimental A/B arms.

| Run / population | Caller QoS | CAB misses | CAB CPU-over misses | Worker / forced misses | Sentinel misses |
|---|---:|---:|---:|---:|---:|
| ABBA ordinary gate | 17 | 24 | 9 | 20 / 4 | — |
| ABBA 01-A | 17 | 17 | 0 | 15 / 2 | 2 |
| ABBA 02-B | 33 | 7 | 1 | 7 / 0 | 1 |
| ABBA 03-B | 33 | 6 | 3 | 6 / 0 | 0 |
| ABBA 04-A | 17 | 1 | 1 | 1 / 0 | 1 |
| BAAB ordinary gate | 17 | 34 | 11 | 30 / 4 | — |
| BAAB 01-B | 33 | 49 | 4 | 47 / 2 | 0 |
| BAAB 02-A | 17 | 7 | 2 | 5 / 2 | 1 |
| BAAB 03-A | 17 | 7 | 6 | 7 / 0 | 0 |
| BAAB 04-B | 33 | 18 | 15 | 18 / 0 | 0 |

All 64 diagnostic child processes across the two runs observe the intended
caller class before and after workload: A is 17; B successfully requests 33.
The IR worker policy and frozen product source are unchanged. The
[comparison review](comparison-review.md) checks native compiler commands and
the limits of the intervention. Both allocations record the same M1 virtual
machine class, three virtual cores, 7 GB, runner image, and toolchain, but their
VM nodes differ. Fresh builds have different executable hashes; within each
run all A/B arms use the same probe binary. Cross-run source/flag parity is
not cross-run binary identity or proof of equivalent physical host conditions.

### The apparent miss benefit reverses

Each condition aggregate below contains 211,200 CAB callbacks within its own
run. The experimental repetitions are the process runs, not independent random
draws for every callback.

| Run | A inherited misses | B user-interactive misses | Actual trial sequence |
|---|---:|---:|---|
| ABBA | 18 | 13 | A 17 → B 7 → B 6 → A 1 |
| BAAB | 14 | 67 | B 49 → A 7 → A 7 → B 18 |

This does not reproduce a QoS improvement. It also does not prove that higher
QoS caused the second run's excess misses: order, runner and workload-state
variation remain material. The longest second-run B interval is 37.240292 ms;
raising caller QoS did not eliminate the long tail.

The first run also changes substantially within the same A condition. The
median of 66 matched-route late-A/early-A worker-phase p50 ratios is **4.285062**,
while CAB misses fall from 17 to 1. In the worker-absent forced phase, the median
matched-route B/A p99 ratio is **1.001582**, with exactly 33/66 routes lower
under B. The corresponding BAAB forced-phase ratio is **1.236916**, with only
13/66 routes lower under B. These are descriptive route contrasts, not pooled
percentiles or a causal effect estimate. The accompanying scripts specify their
formulas.

### Anomalies also occur without CAB

The arithmetic-only controls produce four misses in ABBA and one in BAAB.
ABBA includes a burst interval of **4,992 microseconds wall / 13.375
microseconds CPU**, with no scheduled sleep inside the interval. A different
ABBA control interval has **816.459 microseconds wall / 820.417 microseconds
CPU** against a **666.666667-microsecond reference deadline**, despite containing
no CAB, worker or UI. This directly cautions against treating every CPU-over
record as evidence of expensive convolution. It does not identify a specific
clock, hypervisor, scheduler or memory cause, or prove that every CAB outlier
has the control's cause. A clean control arm also does not exclude an external
cause in a separately timed CAB arm.

## Reproduce the evidence checks

The [offline verifier](experiments/verifier/README.md) is separate from the
controller. It imports no controller summary function, verifies original ZIP
and member hashes, source/dependency/controller binding, all raw rows and clock
receipts, original CTest output, actual QoS, and every reported miss. Complete
verified evidence exits 0; retained incomplete/failed execution exits 3;
inconsistent evidence exits 2. None of those statuses converts p99 success into
a zero-miss claim.

Download each original ZIP under the filename in its source-bound manifest.
For example, from this repository root:

```sh
python docs/evidence/macos-cab-deadlines-20261012/experiments/verifier/analyze_diagnostic_artifacts.py \
  --archives docs/evidence/macos-cab-deadlines-20261012/experiments/native-BAAB/archives-manifest.json \
  --archive-base /path/to/original-archive-cache \
  --output /path/to/new-BAAB-analysis
```

The two native reports and historical README contain their complete reproduction
commands. The [native controller instructions](../../../Tools/CabDeadlineDiagnostic/README.md)
describe a fresh macOS execution with a clean pinned checkout and an explicitly
chosen order. New runs are additional evidence; they must not replace these
recorded attempts. The original artifacts have finite retention dates recorded
in their manifests (January 2027 for these two native runs). Durable committed
tables preserve every original miss field and all route summaries; reproducing
the full raw-row audit also requires retaining the original ZIPs.

## Preserved initial failure

[Run 38185416392](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38185416392)
failed at CMake generation before any audio measurement: the parent diagnostic
target had not enabled Objective-C++. The separate controller was corrected by
enabling OBJCXX in the common parent scope. The original failure's complete
[report and logs](experiments/failed-38185416392/report.json) are preserved here;
it is not counted as a passing test or an audio population.

## Criteria for a subsequent product change

A product change needs a reproducible cause and favorable independent controls,
with original response/fade/channel-isolation behavior and unchanged acceptance
checks retained. Caller-QoS sensitivity in this console environment alone is
insufficient justification for changing a DAW audio thread or the IR worker.
The source audit specifies separate stage, scheduling, memory, worker-publication
and real-host experiments for remaining hypotheses. The next attribution work
should record the same callback's stage cost, active/fading kernels, swap/reset
events and native scheduling/fault evidence at the affected 96 kHz small-buffer
settings. Actual AU/VST3/CoreAudio, editor, EQ/tuner and automation effects need
a real host with those paths enabled. Those measurements have not been made by
this investigation.

Only one candidate intervention should differ in a subsequent matched comparison,
with the same actual publication schedule, input, sample count, pacing, complete
IRs and original gates. Its benefit must survive independent repetitions and
be distinguishable from the repeated-baseline drift observed here. A reduction
in ordinary DSP cost, if demonstrated later, must be labeled separately from a
demonstrated resolution of the deadline outliers.

No IR shortening, reduced lane/microphone count, larger buffer, relaxed p99
threshold, discarded callback or suppressed miss is accepted as an improvement.
The beta's existing approval remains limited to its recorded conditions.

## Follow-up: same-callback attribution

The [callback attribution plan](callback-attribution-plan.md) reviews which
explanations are outside the measured console path, insufficient as sole causes,
or still unproven. It specifies guarded callback-stage instrumentation, separate
resource and arithmetic controls, and independently allocated ABBA/BAAB runs.
The new A/B conditions mean recording off/on; they are distinct from this
report's completed caller-QoS experiments. This is diagnostic development, not
an accepted performance fix. Actual AU/VST3/CoreAudio/DAW and editor-interaction
tests remain **NOT_EXECUTED** without separate host evidence.
