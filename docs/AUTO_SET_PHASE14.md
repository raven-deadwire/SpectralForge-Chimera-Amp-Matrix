# Auto Set — phase 14 (stereo doubler)

Phase 14 lets Auto Set set the stereo **DOUBLER** (on or off, and its spread)
from the rig sound and the playing it has already heard. It adds no listening
time. It runs just before the Final EQ and output pass, after the delay and
reverb step (the doubler follows POST in the chain). Earlier phases are
described in `AUTO_SET_PHASE1.md` … `AUTO_SET_PHASE13.md`; locks and strength
in `AUTO_SET_LOCKS_STRENGTH.md`. The rules are fixed, as in the earlier phases.

## Use

The options button next to **AUTO SET** has a **Stereo doubler** step, on by
default.

- **The results:** a **Stereo doubler** row shows what changed ("doubler on:
  spread 6.0 ms", or a spread change). It also shows the width and how rough
  the mono sum is. Its SHOW button opens the DOUBLER / CLICK block.
- **Strength:** scales a spread change of a doubler that is already on.
  Switching the doubler on is not scaled.
- **Lock → Stereo doubler:** keeps it as it is.
- **UNDO AUTO:** restores it.
- **Never switched off:** a doubler you have on is never turned off. At most
  its spread moves.

## What it hears

- **The rig sound:** the last sample pass that heard the rig before the Tone
  EQ. That is the Tone EQ pass if it runs, else the cabinet mic pass, else the
  amp pass. The first 3 seconds, after a 0.25 s ramp, are rendered offline
  through the plug-in's own doubler (`PerformanceUtilities`) for each
  candidate spread.
- **The playing:** the input pass's 10 ms windows, read with the phase 11
  onset detector. They show how fast the notes come.

If none of the sample passes ran, the row says so and nothing changes. The
output must be stereo; the doubler does nothing in mono.

## Rules (`analyseWidth`, `AutoSetWidth.h`)

**What the measures do**
- **Width:** 1 − the correlation of left and right. The doubler's mix is
  fixed, so above a few ms the spread changes the width little (about 0.10–0.19
  on synthetic material). A candidate must widen by at least 0.08. Width is
  checked, not targeted.
- **Mono:** summed to mono, the delayed copy combs. The mono sum's roughness
  against the dry sound (third-octave bands, 80 Hz – 6.3 kHz, band against its
  neighbours) is what the spread changes most.

**Choosing a spread**
- **The style's range** (2 ms steps). A longer spread is heard as a second
  player:

  | Metal | Rock | Clean |
  |---|---|---|
  | 4–10 ms | 6–14 ms | 8–18 ms |

- **The choice:** the least rough mono sum among the candidates that widen
  enough. If every candidate is rougher than 1.5 dB, the doubler stays as it
  is.
- **A doubler that is on:** keeps its spread if the spread is inside the range
  and its mono sum is within 0.3 dB of the best. Otherwise the spread moves.

**When it stays as it is**
- **Fast playing** (more than 6 detected notes in any second): an off doubler
  stays off, because a delayed copy smears quick notes. One that is on uses
  the shorter half of the range.
- **Bass:** the doubler stays as it is, so the low end stays centred.

## Testing

`ChimeraAutoSetTests` (also alone: `ChimeraAutoSetTests --width`):

- **Rules (`widthRules`), 44.1, 48 and 96 kHz, on plucked notes:**
  - The doubler widens more at 12 ms than at 1 ms.
  - For each style, an off doubler is turned on inside the range, at the
    smoothest mono sum among the candidates. The result widens enough and
    stays within the mono limit.
  - A doubler already at that spread is kept.
  - Metal shortens a 20 ms spread.
  - Fast playing keeps a doubler off, or shortens one that is on.
  - Bass and silence change nothing.
- **Controller (`controllerWidth`), real processor, fixtures fed to the input
  and Tone EQ taps:**
  - The doubler is turned on with the analysed spread.
  - The lock keeps it.
  - 50% strength moves a 20 ms spread half-way.
  - Bass keeps it.
  - UNDO AUTO restores it.
- **Full run (`controllerIntegration`):** fifteen result rows, with the
  Stereo doubler row after the delay and reverb row.
- **Locks (`controllerEveryLock`):** all seventeen locks at four strengths.

## Not yet validated

The spread ranges, the 1.5 dB mono limit, the 0.08 width minimum and the
6 notes/s limit are starting values. They have been checked on synthetic
plucked notes, not on real guitars or listening tests.

**Approximations in the render**
- The doubler sits after POST, but Auto Set renders it on the rig sound before
  the delay and reverb.
- The render feeds the same mono rig to both channels. A stereo source
  upstream (a stereo reverb or chorus) would decorrelate further than measured.
- The width and roughness measures do not tell when a delayed copy is heard as
  a separate echo. The style's spread range stands in for that.
