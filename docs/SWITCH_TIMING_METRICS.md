# Switch timing metrics

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
| `cpu_ms` | `processBlock` wall time: steady p50/p99 over the blocks before t0 (first 20 skipped), worst block from t0 to settle, and the block budget | Whether the switch block risks a dropout. Machine-dependent: reported, not gated |

`click_db` and `overshoot_db` are reported separately on purpose: a louder
first note after a reset raises the sharpness of its attack too, so a high
`click_db` at a pluck time (+42.7, +202.7 ms …) with a matching
`overshoot_db` is overshoot, not a discontinuity.

Gates in CTest (the output must also stay finite):

- channel and amp model switches respond within **20 ms** (their crossfade);
- preset and A/B recalls respond within **60 ms** (UI thread included).

Settle, residual, click, overshoot and CPU are reported in the log and in
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
3. resets the DSP, stays muted for the reported latency (the cleared delay
   lines refill) plus 20 ms while the new sound already runs, so parameter
   smoothers and dynamics detectors have caught up, and fades it in (5 ms).

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

Recall switches, before and after the change (same probe, UI-thread recall):

| Switch | Block | Response ms before → after | Click dB before → after | Overshoot dB before → after |
|---|---|---|---|---|
| preset 0 > 17 | 256 | 395 → 10 | 22.9 → −7.5 | 1.8 → 0.0 |
| preset 17 > 33 | 256 | 410 → 6 | 12.9 → 7.4 | 5.6 → 0.0 |
| A > B | 256 | 426 → 12 | 23.3 → 12.2 | 16.1 → 10.7 |
| preset 0 > 17 | 64 | 435 → 8 | 30.4 → −5.5 | 2.7 → 0.0 |
| preset 17 > 33 | 64 | 481 → 2 | 17.8 → 8.6 | 7.2 → 1.1 |
| A > B | 64 | 451 → 10 | 20.6 → 13.3 | 8.9 → 7.2 |

Before, the state took about 0.4 s to apply while audio played, and the
switch clicked when it landed. After, the remaining `click_db` sits on the
first plucks after the reset (+44 ms, +203 ms) together with the overshoot:
see finding 3.

All switches after the change, 48 kHz, 256 samples (budget 5.33 ms):

| Kind | Switch | Response ms | Settle ms | Click dB | Overshoot dB | CPU p99 / switch ms |
|---|---|---|---|---|---|---|
| channel | model 0 NORMAL > VIBRATO | 1 | 1001+ ¹ | −1.1 | 0.7 | 1.21 / 2.90 |
| channel | model 2 RHYTHM > LEAD | 3 | 358 | −0.1 | −0.1 | 1.93 / 1.76 |
| channel | model 8 NORMAL > TOP BOOST | 2 | 194 | −0.8 | 0.4 | 2.53 / 2.14 |
| channel | model 9 DIRTY > CLEAN | 3 | 139 | −1.6 | 0.0 | 1.48 / 1.80 |
| channel | model 4 LEAD > RHYTHM 1 | 4 | 299 | −4.4 | 0.0 | 1.47 / 1.73 |
| channel | model 10 VINTAGE > OVERDRIVE | 8 | 244 | −4.6 | 0.2 | 1.12 / 2.94 |
| amp model | preset 0, model 0 > 1 | 4 | 156 | 0.2 | 1.1 | 1.27 / 1.89 |
| preset | 0 > 17 | 10 | 445 | −7.5 | 0.0 | 1.61 / 4.16 |
| preset | 17 > 33 | 6 | 1002+ ² | 7.4 | 0.0 | 2.00 / 4.89 |
| A/B | A > B | 12 | 526 | 12.2 | 10.7 | 1.20 / 5.32 |

48 kHz, 64 samples (budget 1.33 ms):

| Kind | Switch | Response ms | Settle ms | Click dB | Overshoot dB | CPU p99 / switch ms |
|---|---|---|---|---|---|---|
| channel | model 0 NORMAL > VIBRATO | 6 | 365 | −1.9 | 0.7 | 0.59 / 0.50 |
| channel | model 2 RHYTHM > LEAD | 4 | 358 | −0.0 | −0.4 | 0.38 / 0.50 |
| channel | model 8 NORMAL > TOP BOOST | 4 | 191 | 0.1 | 0.0 | 0.27 / 0.48 |
| channel | model 9 DIRTY > CLEAN | 3 | 137 | −1.8 | 0.0 | 0.46 / 0.51 |
| channel | model 4 LEAD > RHYTHM 1 | 3 | 316 | −5.3 | −0.0 | 0.40 / 0.54 |
| channel | model 10 VINTAGE > OVERDRIVE | 7 | 259 | −3.2 | 0.1 | 0.29 / 0.37 |
| amp model | preset 0, model 0 > 1 | 2 | 171 | 1.1 | 1.2 | 0.56 / 0.61 |
| preset | 0 > 17 | 8 | 321 | −5.5 | 0.0 | 0.33 / **4.58** |
| preset | 17 > 33 | 2 | 1000+ ² | 8.6 | 1.1 | 0.47 / **1.75** |
| A/B | A > B | 10 | 523 | 13.3 | 7.2 | 0.37 / **1.54** |

¹ The vibrato channel's LFO phase differs from the reference's, so `y` never
matches `r` exactly at 256 samples; at 64 samples it settles in 365 ms.
² Preset 33's sound keeps differing from the fresh reference within 1 s.

CPU figures come from a shared container and are noisy, and the recall
scenarios sleep to keep real-time pace, which disturbs the timing of their own
blocks; repeat them on reference hardware before acting on single values.

## Findings

1. **Channel and amp model switches are clean.** They are heard within
   1–8 ms (a 20 ms linear crossfade from the next block), no switch is sharper
   than a pick attack, and the old channel decays to −40 dB within 140–360 ms:
   the amp's own dynamics settling, not a leftover of the old channel.
2. **Preset and A/B recalls no longer jump.** They respond in 2–12 ms instead
   of about 0.4 s and pass through a short mute (5 ms out, the state change,
   the latency, 5 ms in) instead of a hard cut.
3. **After a recall, the first notes could be louder (overshoot up to
   ~11–16 dB).** Not from the reset itself (skipping each module's reset did
   not remove it): parameter smoothers ramp from the old sound's values for
   about 20 ms. The 20 ms muted settle after the reset removes it for preset
   17 > 33 (8–10 dB → 0) and halves it for A > B (≈4–5 dB left).
4. **The previous cabinet stays for 140–400 ms after a recall.** Modeled CAB
   kernels for the recalled sound are built on the IR worker thread lane by
   lane (here lane 1 after ≈140 ms, lane 2 after ≈270 ms, lane 3 later), each
   then crossfading 50 ms; until then the old cabinet plays. This is the
   remaining A > B overshoot and residual (−0.2 dB at +100 ms, settle
   ≈550 ms). Not changed yet.
5. **Switch blocks can exceed the block budget** in this container,
   especially at 64 samples. Re-measure on reference hardware; if confirmed,
   move the reset work off the switch block.
6. Compared with the benchmark notes: a native channel switch here has no
   separate preparation delay (it installs at the next block and crossfades
   20 ms), so response is a few milliseconds, while the CPU cost lands in
   that block instead.

## Running

```
ChimeraSwitchTimingProbe [--block N] [--rate HZ] [results.csv]
```

CTest runs it at 48 kHz / 256 samples and writes `build/switch-timing.csv`.
