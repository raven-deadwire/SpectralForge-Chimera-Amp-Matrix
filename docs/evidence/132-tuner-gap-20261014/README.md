# Expired input-window recovery

The three-platform worker suite passed at `b993771e65333de21ca9a24150eeb828c959f524`.
A further controlled case showed that **one fresh queued block after a long gap
could revalidate the previous complete analysis window**, or a mixed old/new
queued window. The block-end timestamp correctly dated the last block, but did
not certify the age of the earlier samples in that window.

`control.log` reproduces the failure against that exact source with the extended
test. `fixed.log` passes all three pause/resume scenarios: an old block alone,
an old block plus one fresh block, and 40 old blocks (more than a complete
analysis window) plus one fresh block. Every case also reacquires a different
fresh tone after resuming.

Before consuming queued input, the worker now invalidates its history and
pending FIFO snapshot when its previous input is older than 250 ms. The first
worker observation also discards any pre-start backlog. The next published
samples must fill a complete new analysis window. This deliberately discards
the potentially fresh tail of a mixed pending snapshot; the worker then
reacquires from subsequent input. It does not renew old history using one new
block's timestamp.

No audio callback code, storage, publication, priority, latency/accuracy limit,
DSP output, or tuner UI behavior is changed by this follow-up. The added age
check and FIFO discard run only on the worker. Reader expiry and retained
arrival timestamps remain in effect.

The standalone worker workflow also builds/runs `ChimeraPostNativeTests` on all
three platforms so exact POST cache parity and the existing POST contracts are
checked before the full UI/package build. No existing test is removed.

This is synthetic regression evidence, not real-driver/DAW callback clearance.
Prior evidence remains historical and is not relabeled as this source's result.

## Full local route run: FAIL, retained

The expanded pause/recovery tests passed, as did the 108 kernel comparisons,
but the full local native run failed three of 32 routes:

| Rate / block / tone | Acquire | Settle | Final error | Max push wall |
| --- | ---: | ---: | ---: | ---: |
| 48 kHz / 64 / 30.867706 Hz | 180.08 ms | not acquired | invalid | 34949.29 us |
| 48 kHz / 256 / 82.406889 Hz | 187.00 ms | not acquired | 5.41929 ct | 115.46 us |
| 96 kHz / 64 / 440 Hz | 174.09 ms | 364.88 ms | -0.00001921 ct | 17336.37 us |

These failures are **not waived, relabeled as PASS, or discarded by retry**.
The shared local host showed large wall stalls, but the run does not establish
their cause. `native.csv` and `native.log` preserve every baseline/candidate row.
Fresh platform results are required to distinguish a reproducible regression
from environment-specific timing; full callback/DAW qualification stays pending.
