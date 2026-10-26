# Auto Set — phase 1 prototype (input gain, gate, clean compressor)

Auto Set listens to the player and proposes settings, in the spirit of an
"assistant". Phase 1 is rule-based (no machine learning) and only touches
input gain, the gate and the clean compressor.

## Use

Dynamics → **COMPRESSOR** or **GATE / EXPANDER** → **AUTO SET**. Play
normally for about 12 seconds and include a short pause so the gate can hear
the noise floor. The header shows progress and, at the end, the applied
values. **CANCEL** stops listening without changing anything; **UNDO AUTO**
restores every touched parameter to its value before the run.

## How it works

- Audio thread (`Source/AutoSet.h`, `Capture`): when listening, reduces the
  signal to 10 ms windows (peak, RMS of the louder channel) into a lock-free
  ring per tap. Allocation-free; nothing is computed when not listening.
  - Tap `input`: after input gain, where the gate detector runs.
  - Tap `compressor`: the clean compressor input (after Tone EQ).
- Message thread (`Source/AutoSetController.h`), two passes of 6 s audio:
  1. **Input + gate** from the input tap.
  2. **Compressor** from the compressor tap, re-measured *after* the new input
     gain because the amp/cab path is nonlinear (first 300 ms discarded).
- Values are written as host gestures (begin/set/end), so DAW automation and
  undo see ordinary edits. Starting or cancelling writes nothing.

## Rules (`analyseInputGate`, `analyseCompressor`)

| Value | Rule |
|---|---|
| Input gain | Move the 99th-percentile playing peak to −9 dBFS; ignore deviations < 2 dB; at most ±12 dB per run; 0.5 dB steps |
| Gate threshold | Noise peak (90th percentile of quiet windows, shifted by the new gain) + 9 dB (6 dB hysteresis + 3 dB margin); never above the quietest playing level − 6 dB, otherwise midway |
| Gate range | Full when playing is ≥ 30 dB above the noise, else gap − 6 dB (12–60 dB) |
| Gate | Left unchanged when no pause was heard |
| Comp threshold | Median RMS of the playing windows (compresses the louder half) |
| Comp ratio | Playing dynamic range (p90 − p20 RMS): < 6 dB → 2:1, < 12 dB → 3:1, else 4:1 |
| Comp attack | Crest factor > 16 dB → 15 ms, > 10 dB → 8 ms, else 3 ms (keeps pick attack) |
| Comp makeup | 75 % of the estimated average gain reduction, 0–12 dB |
| Comp other | Release 120 ms, knee 6 dB, RMS detector, 100 % mix, enabled |

Silence, no audio callbacks for 3 s, or no playing above −60 dBFS fail with a
message and change nothing.

## Verification

`ChimeraAutoSetTests`: rule cases (silence, quiet DI, steady input, noisy
input, picked vs sustained compressor), capture at 44.1/48/96 kHz without
audio-thread allocation, and a controller run on the real processor with a
synthetic plucked DI (−18 dBFS notes, −72 dBFS noise): both passes, host
gestures balanced, exact undo. `CommonDynamicsUITests` checks the entry is
shown only on the compressor/gate pages and that listen/cancel writes nothing.

## Not yet validated

The targets (−9 dBFS peak, thresholds, ratios) are starting values chosen
from the detectors' behaviour, not tuned against reference recordings.
Listening tests with real DI captures across guitars/basses, pickups and
interfaces are needed before release. Phase 2 (Tone EQ toward per-instrument
target curves) is described in `AUTO_SET_PHASE2.md`; phase 3 (de-esser,
limiter, genre targets) is not part of this prototype.
