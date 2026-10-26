# Auto Set — phase 4 (multiband compressor)

Phase 4 adds a multiband pass to Auto Set (phases 1–3:
`AUTO_SET_PHASE1.md` … `AUTO_SET_PHASE3.md`). It compresses only the
frequency bands whose level jumps around more than the rest of the signal:
booming low notes, a honky mid on certain notes, pick or fizz peaks up top.
It is rule-based like the earlier phases.

## Use

The options button next to **AUTO SET** has a **Multiband** step, on by
default. With every step on, Auto Set listens in five passes of about 6 s
each, in signal order:
1. Input and gate.
2. Tone EQ.
3. Compressor.
4. Multiband: play low notes and pick hard too, so every band is heard.
5. De-esser and limiter.

**UNDO AUTO** also restores every multiband parameter.

## Order

The multiband sits after the clean compressor and before the de-esser, so
pass 4 listens at the multiband input. By then the input gain, Tone EQ and
compressor settings are already in place. The audio thread copies the mono
(channel average) signal into the shared lock-free sample ring while pass 4
listens; it is allocation-free and idle otherwise.

## Rules (`analyseMultiband`)

| Step | Rule |
|---|---|
| Crossovers | Guitar rock 150 / 700 / 3500 Hz, clean 150 / 600 / 3000, metal 120 / 800 / 4500; bass 80 / 300 / 2000 |
| Band levels | Four bands through 24 dB/oct splits; RMS per 10 ms window, playing windows only (within 15 dB of the 90th-percentile level) |
| Swing | Each band's 95th − 20th percentile level over 100 ms blocks, minus the median swing of the *other* three bands. The shared note decay cancels out, and a band that dominates the mix cannot hide its own swing. 100 ms blocks keep low-frequency noise from looking like swing |
| Engages when | Swing ≥ 4 dB, and the band is within 40 dB of the full signal (an empty band is left alone). Static balance is the Tone EQ's job, so excess over the style target alone does not engage a band |
| Threshold | 75th-percentile band level (acts on the loudest quarter) |
| Ratio | Swing < 6 dB → 2:1, < 10 dB → 3:1, else 4:1 |
| Range | 0.5 × swing + 0.5 × excess over the style target + 2 dB, 3–9 dB |
| Attack / release | Low 30 / 200 ms, low-mid 20 / 150, high-mid 10 / 100, high 5 / 80; release × 0.7 for metal, × 1.3 for clean |
| Other | Downward compression, knee 6 dB, no makeup (balance, not loudness), RMS detector, 100 % mix, no solo |
| Applied | Bands that do not engage are bypassed. If no band engages, the multiband is left as it was |

## Verification

`ChimeraAutoSetTests`:
- Crossovers follow the instrument and style.
- Silence is rejected.
- A steady signal shaped like the target engages no band.
- A low band jumping about 18 dB every quarter second engages only the low
  band, with its attack, metal release and ratio, at 44.1/48/96 kHz.
- A five-pass run on the real processor: multiband on/off and each band's
  bypass match the analysis, host gestures are balanced, undo restores
  everything, and the optional steps off give two passes.

`CommonDynamicsUITests` checks the new step's default and that turning every
optional step off gives two passes.

## Not yet validated

The crossovers, the 4 dB swing limit, the ranges and the time constants are
starting values. They have not been tuned against reference recordings or
listening tests.

On the synthetic DI in the tests, the high-mid and high bands engage (3:1 and
4:1). There, distortion harmonics flare on each pick attack and fade with the
note. Whether that is welcome on real playing needs a listening check.
