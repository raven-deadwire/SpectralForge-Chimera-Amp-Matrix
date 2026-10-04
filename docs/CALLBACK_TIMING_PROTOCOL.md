# 1.1.1 callback timing diagnosis protocol

The first UI-refresh benchmark ran complete baseline and candidate batches
sequentially on a shared runner.  Its closed-editor control also moved, so the
observed p99 and deadline-miss differences cannot be attributed to editor work.
The release gate therefore remains `BLOCKED`.

`Tools/run_paired_callback_probe.py` alternates the two independently built
`ChimeraUIRefreshProbe` executables in A/B then B/A order.  Each process performs
one repeat of every scene.  Five or more pairs are aggregated with pair number,
execution order, binary SHA-256 and source revision.  Builds, downloads and
other test jobs must not run concurrently on the measurement machine.

```sh
python3 Tools/run_paired_callback_probe.py \
  /path/to/baseline/ChimeraUIRefreshProbe \
  /path/to/candidate/ChimeraUIRefreshProbe \
  /absolute/callback-pairs BASELINE_SHA CANDIDATE_SHA --pairs 7

python3 Tools/analyze_callback_pairs.py \
  /absolute/callback-pairs/baseline.json \
  /absolute/callback-pairs/candidate.json \
  /absolute/callback-pairs/diagnosis.json
```

The analyzer requires identical OS, CPU, JUCE version, sample rate, block size,
fixture, scope, callback counts and synthetic output hashes.  For each pair it
divides the scene's candidate/baseline p99 ratio by the same pair's
`closed_audio` ratio and subtracts the control's miss-rate delta.  This turns a
machine-wide slowdown into a control observation instead of attributing it to
the editor.

`LIKELY_REGRESSION`, `LIKELY_NONREGRESSION` and `INCONCLUSIVE` are diagnostic
labels only.  The generated `release_gate_status` is always `BLOCKED`: the
synthetic desktop-peer probe does not exercise a commercial DAW, audio driver,
scheduler wake-up deadline, real project, or 30-minute soak.  Studio One,
Cubase and Sonar acceptance must remain separate evidence.
