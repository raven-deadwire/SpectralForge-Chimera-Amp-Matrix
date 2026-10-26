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
- 0.6 s pre-roll, then 1 s measured after t0. The switch is applied between
  blocks, as a host or the editor delivers it.
- Delay, reverb and chorus are off in all three renders, so effect tails and
  LFOs do not hide the switch. (Keeping effect tails across a switch is a
  separate behaviour, not measured here.)
- Defaults: 48 kHz, 256-sample blocks; `--block` and `--rate` change them.
- Levels are relative to the energy of **r** after t0 (`post`). Windows are
  1 ms unless stated.

## Metrics

| Metric | Definition | What it answers |
|---|---|---|
| `response_ms` | First window after t0 where `y` has moved a tenth (−20 dB in energy) of the way from `o` toward `r`, in a window where `o` and `r` differ by more than −40 dB re `post` | How soon the switch is heard: block boundary, queued switches, preparation delay |
| `settle_ms` | End of the last window within the 1 s horizon where `y − r` exceeds −40 dB re `post`; `+` = still above at the horizon | How long the old sound or the switch transient remains audible |
| `residual_db` | `y − r` energy in 5 ms windows at +20/+50/+100/+250 ms, re `post` | Shape of the decay; old cabinet or amp state left behind |
| `click_db` | Worst 1 ms window, from t0 − 2 ms to settle + 5 ms, of `y`'s second-difference energy over the larger of `o`'s and `r`'s | Discontinuity beyond what either sound contains. ≈0 dB: no click; a linear crossfade stays within a few dB; a hard cut reads +20 dB and more |
| `cpu_ms` | `processBlock` wall time: steady p50/p99 over the blocks before t0 (first 20 skipped), worst block from t0 to settle, and the block budget | Whether the switch block risks a dropout. Wall time depends on the machine: reported, not gated |

Gate in CTest: every switch responds within **20 ms** (the native channel
crossfade length) and the output stays finite. Settle, residual, click and
CPU are reported in the log and in `build/switch-timing.csv` for review:
a modulated channel (vibrato, tremolo) never settles against a reference whose
LFO started earlier, and wall time varies by runner.

Scenarios are chosen automatically: up to six factory presets whose first
native amp has more than one channel (one per model), a native amp model
change on preset 0, two factory preset changes and an A/B switch to another
preset. Names use Chimera model numbers and channel labels only
(`docs/PUBLIC_PRODUCT_NAMING.md`).

## First results (2026-10-26, Linux container, Release)

48 kHz, 256 samples (budget 5.33 ms):

| Kind | Switch | Response ms | Settle ms | Residual dB @50 / @250 ms | Click dB | CPU p99 / switch ms |
|---|---|---|---|---|---|---|
| channel | model 0 NORMAL > VIBRATO | 1 | 1001+ ¹ | −25.9 / −40.4 | 9.9 ¹ | 0.88 / 1.10 |
| channel | model 2 RHYTHM > LEAD | 3 | 358 | −18.2 / −35.2 | 0.0 | 1.66 / 1.89 |
| channel | model 8 NORMAL > TOP BOOST | 2 | 194 | −42.7 / −54.5 | 0.9 | 1.13 / 1.13 |
| channel | model 9 DIRTY > CLEAN | 3 | 139 | −29.7 / −63.3 | 0.0 | 1.28 / 1.45 |
| channel | model 4 LEAD > RHYTHM 1 | 4 | 299 | −25.5 / −47.3 | 0.1 | 1.25 / 1.41 |
| channel | model 10 VINTAGE > OVERDRIVE | 8 | 244 | −28.2 / −50.0 | 0.1 | 0.88 / 1.08 |
| amp model | preset 0, model 0 > 1 | 4 | 156 | −23.5 / −59.9 | 2.4 | 1.21 / 1.37 |
| preset | 0 > 17 | 4 | 194 | −3.0 / −60.3 | **31.3** | 0.92 / 3.25 |
| preset | 17 > 33 | 0 | 1002+ ² | −5.9 / −21.4 | **50.9** | 1.71 / **5.48** |
| A/B | A > B | 0 | 526 | −7.4 / −4.8 | **45.9** | 0.89 / 4.19 |

48 kHz, 64 samples (budget 1.33 ms):

| Kind | Switch | Response ms | Settle ms | Click dB | CPU p99 / switch ms |
|---|---|---|---|---|---|
| channel | model 0 NORMAL > VIBRATO | 6 | 365 | 1.8 | 0.34 / 0.42 |
| channel | model 2 RHYTHM > LEAD | 4 | 358 | 0.0 | 0.33 / **2.41** |
| channel | model 8 NORMAL > TOP BOOST | 4 | 191 | 0.4 | 0.37 / 0.74 |
| channel | model 9 DIRTY > CLEAN | 3 | 137 | 0.0 | 0.46 / 0.54 |
| channel | model 4 LEAD > RHYTHM 1 | 3 | 316 | 0.0 | 0.37 / 0.42 |
| channel | model 10 VINTAGE > OVERDRIVE | 7 | 259 | 0.0 | 0.49 / 0.64 |
| amp model | preset 0, model 0 > 1 | 2 | 171 | 1.8 | 0.24 / 0.33 |
| preset | 0 > 17 | 2 | 307 | **24.7** | 0.45 / **1.57** |
| preset | 17 > 33 | 0 | 1000+ ² | **52.8** | 0.51 / **2.53** |
| A/B | A > B | 0 | 524 | **46.6** | 0.26 / **3.04** |

¹ The vibrato channel's LFO phase differs from the reference's, so `y` never
matches `r` exactly at 256 samples; at 64 samples it settles in 365 ms. Its
click reading at 256 samples (1.8 dB at 64) most likely comes from the same
phase difference rather than a cut; check by ear before treating it as one.
² Preset 33's sound keeps differing from the fresh reference within 1 s.

CPU figures come from a shared container and are noisy; repeat them on
reference hardware before acting on single values.

## Findings

1. **Channel and amp model switches are clean.** They are heard within
   1–8 ms (a 20 ms linear crossfade from the next block), click readings are
   0–2.4 dB, and the old channel decays to −40 dB within 140–360 ms. That time
   is the amp's own dynamics settling, not a leftover of the old channel.
2. **Factory preset and A/B switches click (+25 to +53 dB).** They restore the
   whole state and reset every DSP stage at the next block with no fade, so
   the output jumps. A short output fade around the reset (for example 5–10 ms
   out, reset, 5–10 ms in) would remove it. Not changed here; proposed as a
   follow-up.
3. **Switch blocks cost more than steady blocks**, most for presets and A/B
   (reset and state install on the audio thread): up to 5.5 ms against a
   5.33 ms budget at 256 samples, and 1.6–3.0 ms against 1.33 ms at 64
   samples in this container. One channel switch at 64 samples took 2.4 ms.
   Re-measure on reference hardware; if confirmed, prepare the new state off
   the audio thread before the switch.
4. Compared with the benchmark notes: a native channel switch here has no
   separate preparation delay (it installs at the next block and crossfades
   20 ms), so response is a few milliseconds, while the CPU cost lands in
   that block instead.

## Running

```
ChimeraSwitchTimingProbe [--block N] [--rate HZ] [results.csv]
```

CTest runs it at 48 kHz / 256 samples and writes `build/switch-timing.csv`.
