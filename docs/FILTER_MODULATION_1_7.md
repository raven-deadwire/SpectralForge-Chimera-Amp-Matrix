# Two-filter modulation — 1.7

Status: **implemented on `claude/filter-modulation`**, stacked on
`claude/reference-amps`. Roadmap 1.7: "a common two-filter modulation
processor". The Volcano 3 reference (two-filter LP/HP/BP/notch with
LFO/envelope follower) is in scope; advanced modulation is left to 1.9. Covered
by `ChimeraFilterModulationTests` and the common-processor UI tests.

## Placement and controls

The stage sits after the three-band saturation and before the de-esser. Its
editor is the FILTER page of the common-processor workspace: a header entry and
a tab. The spectrum shows draggable A and B cutoff markers, and a selector picks
which filter's controls are shown.

The parameters append after every released parameter, at version hint 21 (25
parameters).

| Parameter | Range | Default |
|---|---|---|
| `fltOn` | off / on | off |
| `fltMix` | 0–100 % | 100 % |
| `fltOutput` | −24 to +12 dB | 0 dB |
| `fltRouting` | Serial A > B, Parallel A + B | Serial |
| `fltLfoShape` | Sine, Triangle, Square, Saw down | Sine |
| `fltLfoRate` | 0.05–20 Hz | 1 Hz |
| `fltLfoSync` | Free, 1/1, 1/2, 1/4, 1/8, 1/16, 1/4 T, 1/8 T, 1/4 D, 1/8 D | Free |
| `fltLfoStereo` | 0–180° right-channel phase offset | 0° |
| `fltEnvAttack` / `fltEnvRelease` | 0.1–200 ms / 5–2000 ms | 10 / 150 ms |
| `fltEnvSensitivity` | −24 to +24 dB | 0 dB |
| `fltA_*`, `fltB_*`: on, type, slope | LP / HP / BP / Notch; 12 or 24 dB/oct | A on, LP; B off, HP |
| `fltA_*`, `fltB_*`: cutoff, resonance | 20 Hz–20 kHz; 0–100 % | A 1 kHz, B 80 Hz; 20 % |
| `fltA_*`, `fltB_*`: LFO amount, envelope amount | ±4 octaves each | 0 |

## Signal design

- **Filters:** zero-delay-feedback state-variable filters, prewarped at the
  cutoff. They take per-sample coefficient changes without instability.
  - Resonance maps 0–100 % to Q 0.707–20 on a log scale.
  - Band-pass has unity gain at its centre.
  - 24 dB/oct cascades a fourth-order Butterworth pair (Q 0.541, 1.307) at zero
    resonance; resonance raises the second stage's Q.
- **Routing:**
  - **Serial:** A feeds B.
  - **Parallel:** both hear the input and are averaged. With only one filter
    on, that filter alone is heard.
- **Modulation:** each cutoff is `cutoff × 2^(LFO amount × lfo + envelope amount × env)`,
  clamped to 10 Hz – 0.45 fs.
  - The LFO is bipolar. The square passes a 2 ms one-pole so its edges do not
    click.
  - The envelope follower tracks each channel's own level times the sensitivity,
    capped at full scale (1.0).
  - Synced rates follow the host tempo (or Chimera's tempo). The LFO is
    **not locked to the bar position**: it free-runs at the synced rate.
- **Off:** the input is untouched, sample-exact.
- **On/off:** 10 ms crossfade. Turning on from fully off starts from cleared
  filter state.
- **Mix 0:** also sample-exact.
- **Latency:** none.

## Measurements (`ChimeraFilterModulationTests`, 48 kHz)

- **Response:**
  - Measured against the analytic prototype at the warped frequency: 4 types × 2
    slopes × 2 resonances × 5 frequencies. The largest error is 0.0004 dB.
  - LP12 at the cutoff: −3.01 dB.
  - LP12 2 kHz re 1 kHz: −9.36 dB. LP24 4 kHz re 2 kHz: −24.7 dB.
  - Band-pass peak: 0.0003 dB. Notch at its centre: −134 dB.
  - Resonance 100 % on LP12: +26.0 dB at the cutoff (Q 20).
- **Routing:**
  - Serial LP 1 kHz into HP 200 Hz: −12.3 dB at 100 Hz, −0.34 dB at 450 Hz,
    −28.6 dB at 5 kHz.
  - Parallel: −6.6 dB at 50 Hz and −6.1 dB at 8 kHz (one filter of two passes).
- **Off and Mix 0:** sample-exact.
- **Switching on**, with a 220 Hz tone:
  - The largest second difference is 3.4e−4, against 2.5e−4 settled.
  - A version that switches instantly measured 0.070, and the test fails on it.
- **LFO:**
  - Envelope cycles in 3.9 s: 11 (free, 3 Hz), 5 (1/4 at 90 BPM, 1.5 Hz) and
    17 (1/8 T at 90 BPM, 4.5 Hz). These are whole crossings of 11.7, 5.9 and
    17.6 expected cycles.
  - Exact rate: 1/8 D at 100 BPM advances 0.222222 cycles past 2 in 1 s, as expected.
- **Stereo 180°:** the left and right modulation envelopes correlate at −0.94.
- **Envelope:** with +3 octaves on LP 250 Hz, a loud 2 kHz tone passes 32.6 dB
  more (relative to its input) than a quiet one.
- **No allocation** while processing.

## Not done

- LFO phase locked to the host's bar position and a retrigger option.
- More LFOs, envelope-follower sidechain, filter drive and further filter types
  (planned for 1.9 "advanced modulation"). **Done in 1.9:** see
  [MODULATION_1_9.md](MODULATION_1_9.md).
- Listening tests, and CPU measurement in a DAW.
