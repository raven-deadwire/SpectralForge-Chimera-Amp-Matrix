# Paired conclusion: native ABBA and BAAB diagnostics

**The two independently verified native allocations do not demonstrate a reproducible caller-QoS improvement or a product performance fix.** The direction of the miss contrast reverses. Both runs also exhibit changes within unchanged QoS conditions, and the worker-absent forced-publication phase supplies no consistent p99 improvement under higher caller QoS.

## Evidence retained

| Run/order | Controller | Artifact SHA-256 | Verified scope |
| --- | --- | --- | --- |
| [38185786916/1, ABBA](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38185786916) | `5bb5b65e4ae706b0e4c532be824108bd36485b91` | `97a35a4fccbf1a1e19059a9d2a377a3844035c0bbb8a0dce6c3064e0614e24b9` | 594 route populations / 950,400 intervals |
| [38186063935/1, BAAB](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38186063935) | `6345092ddefd11d8833eda8c794b7943e0195ef1` | `e35401f002edb8bb7bbe6fbd91b68534e22137ea456c66cf710b337b905b64cd` | 594 route populations / 950,400 intervals |

Both pin clean frozen product source `7b20cd9d1f90e3e2969c250712b199e893cf50cd` and JUCE `d6181bde38d858c283c3b7bf699ce6340c050b5d`. Probe and CMake source hashes agree. Runner/controller documentation changes are bound separately to each declared order. Reported probe binary hashes differ (`7f89446d…` versus `29b607d2…`); each allocation builds once and uses its own single executable across its four conditions. The VM nodes are distinct, with matching declared image/toolchain/virtual hardware properties, but physical host conditions are untraced.

The ordinary gates remain separate, passing five CTests and 66 p99 criteria in each run, while recording 24 and 34 individual misses. Every diagnostic CAB p99 comparison also passes. Those p99 passes do not erase the original deadline misses. The earlier configuration failure [38185416392](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/38185416392) remains an explicitly incomplete run with no measurements, not a successful zero-miss population.

## The effect does not reproduce

Every A or B trial below contains 105,600 callbacks. Each per-condition total has the same 211,200-callback count within its run; equal counts do not establish equal elapsed-time exposure.

| Run | CAB misses in measured order | A inherited total | B user-interactive total | A forced-phase misses | B forced-phase misses |
| --- | --- | ---: | ---: | ---: | ---: |
| ABBA | A17 → B7 → B6 → A1 | 18 | 13 | 2 | 0 |
| BAAB | B49 → A7 → A7 → B18 | 14 | 67 | 2 | 2 |

ABBA's lower aggregate B count does not recur under the independently declared reverse order. BAAB has 49 misses in its first B and 18 in its last B; this does not by itself prove a QoS-caused regression either. Trial-to-trial context and time effects remain unresolved. Counting the callback records as independent observations would overstate the evidence.

Matched-route p99 comparisons reinforce the absence of a repeatable improvement. For each route, the mean B p99 is divided by the mean A p99 within that run; the table is the median of those 66 ratios, not a pooled quantile.

| Subset | ABBA median B/A p99 | BAAB median B/A p99 | BAAB routes lower under B |
| --- | ---: | ---: | ---: |
| All 1,600 callbacks | 0.958055 | 1.031000 | 26 / 66 |
| Worker/automation 1,200 | 0.965802 | 1.053596 | 24 / 66 |
| Forced publication 400 | 1.001582 | 1.236916 | 13 / 66 |

Under repeated inherited A in ABBA, worker-phase median latency increases by a median route factor of 4.285062 despite total misses dropping from 17 to 1. In BAAB, repeated B worker medians increase by 1.770229 and repeated A by 1.351934; their forced-phase p99 factors instead fall to 0.874302 and 0.540544. These are different changes in different portions of the workload, rather than one stable latency improvement attributable to a QoS setting.

The forced phase removes the IR worker and publishes one common Mic-A-derived target response into six separate kernel instances, with serialized transitions and distinct per-slot histories. It offers a more closely matched publication comparison than the worker phase, but it is not a trace proving identical runtime state or fade timing across trials.

## What the arithmetic controls establish

The controls contain no CAB, IR worker, UI, or audio callback. ABBA records synthetic misses 2/1/0/1; BAAB records 0/1/0/0. Four of the five combined synthetic misses have paired CPU below their deadline. One ABBA miss has **816.459 µs wall / 820.417 µs CPU** against **666.666667 µs**, while executing fixed arithmetic only.

This demonstrates that the diagnostic environment can produce both low-CPU and CPU-over excursions without CAB processing. It prevents classifying all low-CPU excursions as one particular wait, or all CPU-over excursions as convolution cost. It does not prove that every historical CAB miss has an environmental cause, identify a clock defect, or rule out a separate product bottleneck. Sentinel exposure differs from CAB and cannot be subtracted from it.

No synthetic miss across the two runs occurs at the immediate next first-phase interval after a scheduled sleep (index modulo 8 equal to 1). The 4.992 ms maximum synthetic excursion occurs in the unpaced burst phase. Therefore a simple immediate-wakeup position explanation is insufficient for these records. Actual wake-up latency, scheduling events, memory faults, host activity, and individual active-kernel/fade states remain unrecorded.

## Consequence for the development PR

Keep the controller, independent verifier, immutable manifests, original failures, complete raw-derived tables, and narrowly stated findings. **No product caller-priority, worker-priority, DSP, allocation, or UI patch is supported by these experiments.** The beta's source and original 54 macOS misses remain separate from this later diagnostic work.

A future product claim needs an identified and reproducible path, a single targeted intervention, preserved original gates and miss records, and independent repeated controls whose effect can be distinguished from the same-condition variation observed here. A claim about DAW/CoreAudio scheduling, complete plug-in routing, or UI interference additionally needs those actual contexts. Ordinary CAB cost reduction and elimination of rare deadline excursions are different claims and should be evaluated separately.

Source data and reproducible formulas are retained in the [ABBA bundle](../native-ABBA/README.md), [BAAB bundle](README.md), and their respective contrast scripts. The paired conclusion does not replace either original run or pool away any contrary result.
