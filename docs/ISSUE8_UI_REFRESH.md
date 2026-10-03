# Issue #8 — first native UI CPU optimisation

The baseline is `fe3656521046be79f99a62857976021c07ec398b`. The implementation is
`bf1e7da83614a5260c7eda869a3f26de40e83823` (local measured source). The uploaded
implementation commit is `2eea78c3a08fa5a9c56ee303ded4bdca7d3b02ad`; both have
exactly the same Git tree `7dda4148fd677d5b832d09b34c14fe61a38b1583`. GitHub app
upload changes commit metadata, not the source payload. This work addresses the editor's 25 Hz
refresh path, not Transpose/Gate DSP or the other Issue #8 workstreams. Issue #8
and the Windows DAW release gates remain open.

`ChimeraAmpMatrix` already defines `JUCE_WEB_BROWSER=0`. Removing an embedded
webview is **NOT_APPLICABLE** to this change. TONE3000's implementation/percentage
is not evidence for Chimera.

## Refresh policy

| Work | Before | Candidate |
| --- | --- | --- |
| Editor timer | 25 Hz full refresh and unconditional full repaint | 25 Hz meter sampling and small state-key checks; no unconditional repaint |
| Amp native panels | All three, twice per tick, including hidden lanes | Model/context/channel/route dirty key; visible panel only |
| POST native panels | All three refreshed every tick | Model state only when dirty; visible meters separately |
| ALL windows | Refreshed through owning panel | Independent visibility; live even if owning tab/editor is hidden |
| Cabinet metadata/tooltips | Copies, mutex acquisition and text assembly every tick; metadata also fetched in paint | Non-audio library revision plus existing cabinet activation atomics; cached metadata/tooltip and cabinet artwork data |
| Legacy rack descriptions/styles | Tooltip/text work every tick | Visible model change only; power LED region on bypass change |
| Input/output/LOW/POST/tuner/CPU displays | Full editor invalidation | Changed display value invalidates its own region, rounded outwards for scaling |
| PRE board | 10 Hz complete board repaint | Structural/bypass dirty key; separate GR rectangle |
| Factory preset CUSTOM detection | Full parameter scan at 25 Hz while a preset is selected | Bounded to 5 Hz; up to 200 ms label-detection delay |
| Hide/minimise | Panel work continues | Dirty state deferred until reveal; live detached dialogs still serviced |

The editor owns all dirty caches and timer work. No new APVTS listener, async
updater, UI/network/I/O/lock call, or allocation was added to `processBlock`.
`PluginProcessor.cpp`, DSP implementations, parameter layouts, IDs and ordinals
are byte-for-byte unchanged from the baseline. The processor header only exposes
an IR display-revision getter. IR revision writes occur in existing import,
metadata edit, restore and worker-error paths, not `processBlock`.

Child parameter attachments still update actual parameter changes. This is not
a claim to eliminate all UI polling or every framework/processor timer.

## Measured comparison

See `verification/issue8-ui/baseline.json`, `candidate.json`, `comparison.json`
and `manifest.json`. The table below is generated from those reports.

CPU values are percent of one logical core; medians of 3 repeats.

| Scene | Baseline UI % | Candidate UI % | Change | Full paints, baseline → candidate |
| --- | ---: | ---: | ---: | ---: |
| closed_idle | 1.60 | 1.83 | 14.7% higher | 0 → 0 |
| closed_audio | 1.47 | 1.54 | 4.4% higher | 0 → 0 |
| open_idle_pre | 41.58 | 2.13 | 94.9% lower | 56 → 0 |
| open_idle_rigs | 49.30 | 2.26 | 95.4% lower | 57 → 0 |
| open_idle_post | 57.78 | 2.50 | 95.7% lower | 56 → 0 |
| meter_rigs | 51.23 | 7.53 | 85.3% lower | 106 → 0 |
| meter_post | 55.05 | 10.28 | 81.3% lower | 106 → 1 |
| meter_tuner | 51.62 | 8.94 | 82.7% lower | 106 → 1 |
| panel_switch | 64.86 | 24.56 | 62.1% lower | 66 → 21 |
| idle_rigs_75 | 37.58 | 2.56 | 93.2% lower | 56 → 0 |
| meter_post_150 | 81.36 | 14.57 | 82.1% lower | 76 → 1 |
| hidden_editor | 2.09 | 2.35 | 12.3% higher | 0 → 0 |
| two_idle_rigs | 79.04 | 3.03 | 96.2% lower | 92 → 0 |

**Audio timing non-regression remains BLOCKED.** Output hashes match, but some
candidate timing tails/miss counts increased. In particular, the closed-editor
scene also regressed, so these short shared-runner samples cannot establish
causality. Do not turn output identity or unchanged callback source into a
timing PASS. Compute-time deadline = 2666.67 µs.

| Audio scene | Baseline p99 µs | Candidate p99 µs | Deadline misses, baseline → candidate (4500 callbacks each) |
| --- | ---: | ---: | ---: |
| closed_audio | 1244.4 | 1533.2 | 0 → 7 |
| meter_rigs | 1339.9 | 1384.4 | 10 → 12 |
| meter_post | 1559.1 | 1621.9 | 26 → 15 |
| meter_tuner | 1444.7 | 1070.0 | 9 → 5 |
| meter_post_150 | 1531.0 | 1134.6 | 11 → 11 |

All **30 fixed-length audio renders** (baseline/candidate × 5 audio scenes ×
3 repeats) have FNV64 `9dfe39a2013dd8a3`. All **78 scene runs** preserve every
parameter value. At rest, all visible idle scenes have zero candidate editor
paints after warmup. The meter scenes retain about 25 partial paints/second.
Constructor CPU/wall times, callback mean/p95, absolute UI thread time and all
individual repeats are retained in the raw JSON. Panel-switch reduction is
62.1%, so even this local scene set does not support an unconditional 75% claim.

## Measurement contract and limits

- Release/GCC 13/JUCE 8.0.8, Ubuntu 24.04, Xvfb 2560x1600x24 + Openbox.
- A real desktop peer and successful native paint are required. An initial
  sandbox attempt had no native paints and was rejected; it is not in the
  accepted comparison. X11 socket creation required running the local probe
  outside the restricted sandbox.
- Baseline and candidate execute sequentially, with no build or other test
  running alongside the accepted measurement. Three repeats per scene; median
  UI CPU is reported. This is a shared runner, not a controlled workstation.
- UI CPU is message-thread CPU time / wall time, **one logical core = 100%**.
  It includes processor/APVTS message-thread timers even with the editor closed.
  Display-server/compositor and background-worker CPU are not included.
- Constructor CPU/wall time is recorded separately from steady state. Warmup is
  600 ms; idle/switch scenes run at least 2.2 s, audio scenes at least 4.2 s.
  PRE/RIGS/POST, tuner, hidden editor, 2 instances and 75/150% scaling are covered.
- Audio scenes use a separately paced synthetic 48 kHz / 128-sample callback,
  1500 blocks of stereo 110 Hz with a stepped amplitude envelope, Matrix/default
  processing and filter-only cabinets. Fixed input length permits exact output
  hash comparison with the editor closed/open and before/after the change.
- Callback mean/p95/p99 and compute-time deadline misses are recorded separately.
  These are **not** driver roundtrip, scheduler-wakeup deadline, actual DAW or
  real-instrument measurements. Short-run tails must not certify hard real-time
  acceptance.
- Candidate source was compiled before committing the implementation. The
  embedded configure-time revision string is still `fe36565210`; the source
  payload matches the implementation commit above. Manifest binary/source/
  harness hashes identify the actual measured builds. This is a probe build,
  not a published plugin release.

## Regression and outstanding gates

| Gate | Status | Evidence |
| --- | --- | --- |
| Stable idle: no native state/metadata refresh | PASS | `UIRefreshTests.h`: counters unchanged over multiple timer ticks |
| Hidden panel/editor deferral and reveal | PASS | Hidden model changes deferred; reveal synchronises current state |
| Visible POST meters and detached ALL windows | PASS | Meter counters continue; owning hidden panel counters stay fixed |
| Metadata-only invalidation | PASS | IR tag edit updates tooltip even without a model/short-label change |
| Scaling/state/automation/preset/A-B/IR recall | PASS | Full `ChimeraUITests` and 75/100/125/150% refresh snapshots |
| Audio output identity | PASS | Fixed-length open/closed/baseline/candidate hashes |
| Audio timing non-regression | BLOCKED | Some candidate p99/deadline counts increased; see table above |
| DSP stability | PASS | `ChimeraStabilityTests`; unchanged callback/DSP source |
| Editor/native resource teardown | PASS, synthetic/native harness scope | Existing UI lifetime and active tuner/IR release/reprepare tests |
| Windows VST3 Studio One 8/Cubase/Sonar close/exit + 30-minute soak | BLOCKED | No real host execution in this environment; Issue #3 remains open |
| UI CPU 75% reduction | **STRETCH / BLOCKED** | Local scene measurements do not grant a Windows-host/general product claim |
| Native search/remote I/O scene | BLOCKED, outside this patch | No remote search workload measured |

## Reproduce

Configure the candidate with `-DCMAKE_BUILD_TYPE=Release -DCHIMERA_UI_PROBES=ON`.
Build `ChimeraUIRefreshProbe`, `ChimeraUITests`, and `ChimeraStabilityTests`.
`CHIMERA_UI_PROBES` also enables the existing UI tests on Linux; the existing
Windows build continues to run the new refresh regression without this option.

For the baseline, create a separate worktree at the baseline SHA and copy only
this branch's `CMakeLists.txt` and `Tests/UIRefreshProbe.cpp` into it. Leave every
baseline `Source/` file untouched. Use the same compiler, Release configuration,
JUCE checkout, display, sample rate, block size and harness for both binaries.

Linux (requires `xvfb`, `xauth`, `openbox`; use absolute report paths):

```sh
bash Tools/run_ui_probe_linux.sh /path/to/baseline/ChimeraUIRefreshProbe /absolute/baseline.json BASELINE_SHA 3
bash Tools/run_ui_probe_linux.sh /path/to/candidate/ChimeraUIRefreshProbe /absolute/candidate.json CANDIDATE_SHA 3
python3 Tools/compare_ui_refresh.py /absolute/baseline.json /absolute/candidate.json /absolute/comparison.json
bash Tools/run_ui_probe_linux.sh /path/to/ChimeraUITests /absolute/ui-snapshots
ctest --test-dir build -R '^ChimeraStabilityTests$' --output-on-failure
```

Windows: run each Release `ChimeraUIRefreshProbe.exe` directly on the same
interactive desktop with the same three report/revision/repeat arguments. Run
`ChimeraUITests.exe` and then the real VST3 host/soak matrix separately. A probe
report alone never closes the host gate. Do not compare headless/no-paint output,
different machine/build/scene/instance/scaling settings, or UI CPU with DSP CPU.
