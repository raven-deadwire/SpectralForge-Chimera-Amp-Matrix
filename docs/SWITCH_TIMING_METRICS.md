# Switch timing metrics

2026-10-27 update: see [CAB recall readiness and comparison protocol](RECALL_CAB_READINESS_20261027.md).
The response gate measures the beginning of a change, not acoustic recovery.
The current probe adds acoustic onset/recovery, process CPU and callback overruns.
Historical results below belong to their recorded source/protocol.

"Gapless" is not "instant", and "no click" is not "no CPU spike". This
document defines what Chimera measures when the sound is switched (amp
channel, amp model, factory preset, A/B), so each property is reported on its
own. `Tests/SwitchTimingProbe.cpp` (`ChimeraSwitchTimingProbe`) measures them
on the real processor.

## Protocol

Every scenario renders the same fixture three times on fresh instances:

| Render | State |
|---|---|
| **o** (old) | the old sound, held throughout |
| **y** (switched) | the old sound, switched at **t0** |
| **r** (new) | the new sound from the start |

- Fixture: A1 (55 Hz) plucks with 6 harmonics, retriggered every 160 ms with
  a 2:1 accent, identical on both input channels (Drop A palm-mute style).
  The next pluck after t0 falls at +42.7 ms.
- 0.6 s pre-roll, then 1 s measured after t0.
- How the switch arrives:
  - **Channel and amp model** changes are parameter writes between blocks on
    the audio thread, as host automation delivers them.
  - **Preset and A/B** recalls run on a second (UI) thread while blocks keep
    coming at real-time pace, as from the editor. This includes the time the
    UI thread needs to build and apply the recalled state. The A/B reference
    `r` is slot B itself selected before playback: a factory recall keeps
    session settings such as input gain, so a freshly loaded preset is not
    the same sound.
- Delay, reverb and chorus are off in all three renders, so effect tails and
  LFOs do not hide the switch. (Keeping effect tails across a switch is a
  separate behaviour, not measured here.)
- Defaults: 48 kHz, 256-sample blocks; `--block` and `--rate` change them.
- Levels are relative to the energy of **r** after t0 (`post`). Windows are
  1 ms unless stated.

## Metrics

| Metric | Definition | What it answers |
|---|---|---|
| `response_ms` | First window after t0 where `y` has moved a tenth (−20 dB in energy) of the way from `o` toward `r`, in a window where `o` and `r` differ by more than −40 dB re `post` | How soon the switch is heard: block boundary, queued switches, preparation delay, UI thread |
| `settle_ms` | End of the last window within the 1 s horizon where `y − r` exceeds −40 dB re `post`; `+` = still above at the horizon | How long the old sound, the switch or the reset state remains audible |
| `residual_db` | `y − r` energy in 5 ms windows at +20/+50/+100/+250 ms, re `post` | Shape of the decay |
| `click_db`, `click_at_ms` | `y`'s sharpest 1 ms (second-difference energy) from t0 − 2 ms to settle + 5 ms, over the sharpest 1 ms anywhere in `o` or `r`, and where it is | Whether the switch is sharper than the hardest pick attack either sound makes on its own. ≤ 0 dB: no click |
| `overshoot_db` | Worst 5 ms window where `y` is louder than both `o` and `r` | A passage louder than either sound, e.g. dynamics restarting without gain reduction |
| `gap_ms` | Longest run of windows after t0 where `y` is silent (below −60 dB re `post`) while `r` is not | How long a recall mutes the output |
| `cpu_ms` | `processBlock` wall time: steady p50/p99 over the blocks before t0 (first 20 skipped), worst block from t0 to settle, and the block budget | Whether the switch block risks a dropout. Machine-dependent: reported, not gated |

`click_db` and `overshoot_db` are reported separately on purpose: a louder
first note after a reset raises the sharpness of its attack too, so a high
`click_db` at a pluck time (+42.7, +202.7 ms …) with a matching
`overshoot_db` is overshoot, not a discontinuity.

Gates in CTest (the output must also stay finite):

- channel and amp model switches respond within **20 ms** (their crossfade);
- preset and A/B recalls respond within **60 ms** (UI thread included).

Settle, residual, click, overshoot, gap and CPU are reported in the log and in
`build/switch-timing.csv` (written even when a gate fails). A modulated
channel (vibrato, tremolo) never settles against a reference whose LFO
started earlier, and wall time varies by runner.

Scenarios are chosen automatically: up to six factory presets whose first
native amp has more than one channel (one per model), a native amp model
change on preset 0, two factory preset changes and an A/B switch to another
preset. Names use Chimera model numbers and channel labels only
(`docs/PUBLIC_PRODUCT_NAMING.md`).

## How a recall switches

A recalled sound (factory preset, A/B, project load, pedal board on/off)
replaces thousands of parameters and resets every DSP stage. While audio is
running on another thread, Chimera now:

1. asks the audio thread to fade the old sound out (5 ms, raised cosine) and
   waits for that, at most 30 ms (`ChimeraProcessor::beginSwitch`);
2. builds and applies the new state while the output stays muted (a hold that
   gives up after 0.5 s);
3. resets the DSP and stays muted for the reported latency (the cleared delay
   lines refill) plus 20 ms while the new sound already runs, so parameter
   smoothers and dynamics detectors have caught up;
4. stays muted until every cabinet that can be heard (lanes in the routing
   mode, not muted, cabinet on; a blended second mic too) has the recalled
   IR or model installed, at most 0.5 s (`Engine::audibleCabinetsSettled`
   in `Source/ChimeraDSP.h`), then fades in (5 ms). While muted, a recalled
   cabinet installs at once instead of crossfading 50 ms from the previous
   one (`Cab::setInstantSwap`); if one arrived after step 3, the 20 ms settle
   runs again so the dynamics hear it before the fade-in. The IR worker
   generates the recalled cabinet models of several mics side by side
   (`IRLibrary::pregenerate`: one on the worker, the others on short-lived
   low-priority helpers), and the recall wakes it as soon as the requests are
   set instead of at its next 20 ms poll.

Without step 1, the new parameters already reach the old DSP on the next
block and the jump is audible however the output is faded afterwards. Offline
loads, a stopped host, the first block after `prepareToPlay` and calls on the
audio thread itself apply at once, as before. A host `reset()` stays
immediate.

Building the recalled state used to take most of a second: every parameter
was looked up by a linear scan of about 5000 children
(`ValueTree::getChildWithProperty`). `ParamIndex` (`Source/ParamIndex.h`)
indexes them once. Measured in this container:

| Operation | Before | After |
|---|---|---|
| Factory preset load | 0.78–1.20 s | 17–35 ms |
| A/B switch | ≈0.47 s | 16–20 ms |
| Project restore (`setStateInformation`) | — | ≈40 ms |

## Results (2026-10-26, Linux container, Release)

Recall switches across the three changes (same probe, UI-thread recall). The
first column is the code before any of them; the A > B row there used the
earlier reference (the preset loaded fresh), the later columns slot B itself.

| Switch | Block | Response ms | Click dB | Overshoot dB | Gap ms (after) |
|---|---|---|---|---|---|
| | | before → fade → +settle/cabinet | before → fade → +settle/cabinet | before → fade → +settle/cabinet | |
| preset 0 > 17 | 256 | 395 → 10 → 10 | 22.9 → −7.5 → −17.2 | 1.8 → 0.0 → 0.0 | 193 |
| preset 17 > 33 | 256 | 410 → 6 → 6 | 12.9 → 7.4 → 0.7 | 5.6 → 0.0 → 0.0 | 256 |
| A > B | 256 | 426 → 12 → 12 | 23.3 → 12.2 → 1.0 | 16.1 → 10.7 → 0.0 | 443 |
| preset 0 > 17 | 64 | 435 → 8 → 8 | 30.4 → −5.5 → −17.2 | 2.7 → 0.0 → 0.0 | 217 |
| preset 17 > 33 | 64 | 481 → 2 → 2 | 17.8 → 8.6 → −0.4 | 7.2 → 1.1 → −0.5 | 264 |
| A > B | 64 | 451 → 10 → 10 | 20.6 → 13.3 → −0.4 | 8.9 → 7.2 → 0.0 | 393 |

Before, the state took about 0.4 s to apply while audio played and the
switch clicked when it landed. With the fade alone, the first notes after
the reset were louder (smoothers, then the old cabinet until the new one was
built). With the settle and the cabinet wait, nothing is louder or sharper
than the recalled sound itself; the price is a 0.2–0.45 s mute while the
cabinets are built (finding 4).

All switches with every change, 48 kHz, 256 samples (budget 5.33 ms):

| Kind | Switch | Response ms | Gap ms | Settle ms | Click dB | Overshoot dB | CPU p99 / switch ms |
|---|---|---|---|---|---|---|---|
| channel | model 0 NORMAL > VIBRATO | 1 | 0 | 1001+ ¹ | −1.1 | 0.7 | 1.58 / 1.34 |
| channel | model 2 RHYTHM > LEAD | 3 | 0 | 358 | −0.1 | −0.1 | 1.19 / 1.50 |
| channel | model 8 NORMAL > TOP BOOST | 2 | 0 | 194 | −0.8 | 0.4 | 1.42 / 1.28 |
| channel | model 9 DIRTY > CLEAN | 3 | 0 | 139 | −1.6 | 0.0 | 1.13 / 1.55 |
| channel | model 4 LEAD > RHYTHM 1 | 4 | 0 | 299 | −4.4 | 0.0 | 2.20 / 1.37 |
| channel | model 10 VINTAGE > OVERDRIVE | 8 | 0 | 244 | −4.6 | 0.2 | 1.27 / 1.29 |
| amp model | preset 0, model 0 > 1 | 4 | 0 | 156 | 0.2 | 1.1 | 3.91 / 1.17 |
| preset | 0 > 17 | 10 | 193 | 209 | −17.2 | 0.0 | 1.38 / 3.08 |
| preset | 17 > 33 | 6 | 256 | 1002+ ² | 0.7 | 0.0 | 1.45 / 5.85 |
| A/B | A > B | 12 | 443 | 526 | 1.0 | 0.0 | 1.03 / 8.06 |

48 kHz, 64 samples (budget 1.33 ms):

| Kind | Switch | Response ms | Gap ms | Settle ms | Click dB | Overshoot dB | CPU p99 / switch ms |
|---|---|---|---|---|---|---|---|
| channel | model 0 NORMAL > VIBRATO | 6 | 0 | 365 | −1.9 | 0.7 | 0.37 / 0.45 |
| channel | model 2 RHYTHM > LEAD | 4 | 0 | 358 | −0.0 | −0.4 | 0.46 / 0.40 |
| channel | model 8 NORMAL > TOP BOOST | 4 | 0 | 191 | 0.1 | 0.0 | 0.41 / **1.61** |
| channel | model 9 DIRTY > CLEAN | 3 | 0 | 137 | −1.8 | 0.0 | 0.34 / 0.45 |
| channel | model 4 LEAD > RHYTHM 1 | 3 | 0 | 316 | −5.3 | −0.0 | 0.48 / 0.52 |
| channel | model 10 VINTAGE > OVERDRIVE | 7 | 0 | 259 | −3.2 | 0.1 | 0.30 / 0.35 |
| amp model | preset 0, model 0 > 1 | 2 | 0 | 171 | 1.1 | 1.2 | 0.37 / 0.92 |
| preset | 0 > 17 | 8 | 217 | 229 | −17.2 | 0.0 | 0.35 / **1.64** |
| preset | 17 > 33 | 2 | 264 | 1000+ ² | −0.4 | −0.5 | 0.54 / **4.64** |
| A/B | A > B | 10 | 393 | 523 | −0.4 | 0.0 | 0.37 / **1.54** |

¹ The vibrato channel's LFO phase differs from the reference's, so `y` never
matches `r` exactly at 256 samples; at 64 samples it settles in 365 ms.
² Preset 33's sound keeps differing from the fresh reference within 1 s.

CPU figures come from a shared container and are noisy, and the recall
scenarios sleep to keep real-time pace, which disturbs the timing of their own
blocks; repeat them on reference hardware before acting on single values.

## Recall mute

`gap_ms` needs the recall to run against wall time: the probe paces its
blocks in real time from the recall until the end of the render (before,
pacing stopped once the UI thread had applied the state, and the cabinet
wait was rendered faster than real time, which inflated `gap_ms` to
0.2–0.45 s). Five runs each, 48 kHz, 256 samples, same paced probe:

| Recall | Mics with a new model | Gap ms, before | Gap ms, after |
|---|---|---|---|
| preset 0 > 17 | 1 | 135–161 | 91–139 |
| preset 17 > 33 | 2 (third cached) | 166–198 | 95–132 |
| A > B | 3 | 203–225 | 90–117 |

At 64 samples: 124–127, 100–106 and 93–111 ms. Overshoot stays 0 dB (at most
0.1) and click ≤ 1 dB. Where the time goes after the change (wall clock,
17 > 33): about 50 ms while the message thread fades out and applies the
state, about 50 ms generating the models (the slowest one, 35–80 ms each),
then the 20 ms settle and the fade-in.

## Findings

1. **Channel and amp model switches are clean.** They are heard within
   1–8 ms (a 20 ms linear crossfade from the next block), no switch is sharper
   than a pick attack, nothing mutes, and the old channel decays to −40 dB
   within 140–360 ms: the amp's own dynamics settling, not a leftover of the
   old channel.
2. **Preset and A/B recalls no longer jump.** They respond in 2–12 ms instead
   of about 0.4 s, and the output fades instead of cutting.
3. **The first notes after a recall are no longer louder.** They were, by up
   to ~11–16 dB: not from the reset itself (skipping each module's reset did
   not remove it) but from parameter smoothers ramping from the old sound's
   values (about 20 ms) and from the old cabinet playing until the new one
   was built. The 20 ms settle and the cabinet wait remove both (overshoot
   0 dB, click ≤ 1 dB).
4. **A recall mutes for about 0.1 s** (90–140 ms in this container, see
   "Recall mute" below) while the recalled cabinets are generated and
   installed. It was 135–225 ms with the cabinets built mic after mic and
   each crossfaded 50 ms behind the mute. Keeping both A/B slots' kernels
   ready would remove most of what remains for A/B; not done.
5. **Switch blocks can exceed the block budget** in this container,
   especially at 64 samples and for recalls. Re-measure on reference
   hardware; if confirmed, move the reset work off the switch block.
6. Compared with the benchmark notes: a native channel switch here has no
   separate preparation delay (it installs at the next block and crossfades
   20 ms), so response is a few milliseconds, while the CPU cost lands in
   that block instead. A recall, by contrast, now trades a short mute for
   never playing a half-switched sound.

## Running

```
ChimeraSwitchTimingProbe [--block N] [--rate HZ] [results.csv]
```

CTest runs it at 48 kHz / 256 samples and writes `build/switch-timing.csv`.
