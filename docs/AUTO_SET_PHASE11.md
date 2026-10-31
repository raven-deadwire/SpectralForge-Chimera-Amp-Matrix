# Auto Set — phase 11 (delay and reverb)

Phase 11 lets Auto Set set the POST delay and reverb from how you play. It
adds no listening time: it reads the playing the input pass already heard
(10 ms windows after input gain). It runs just before the last pass (Final EQ
and output level), once the instrument is known, so that pass hears the wet
sound. Earlier phases are described in `AUTO_SET_PHASE1.md` …
`AUTO_SET_PHASE10.md`; locks and strength in `AUTO_SET_LOCKS_STRENGTH.md`.
The rules are fixed, as in the earlier phases.

## Use

The options button next to **AUTO SET** has a **Delay and reverb** step, on by
default.

- **The results** get a **Delay + reverb** row with what changed and what the
  playing showed: notes per second, the beat, and the share of pauses. Its
  SHOW button opens the reverb.
- **Strength** scales the mix changes.
- **Lock → Delay + reverb** keeps both effects.
- **UNDO AUTO** restores them.

## Measurement (`measurePlayingFeel`, `AutoSetSpace.h`)

- **Onsets:** a window at least 6 dB above the quietest of the three before it,
  within 24 dB of the loudest playing, at most one every 80 ms.
- **Notes per second:** the onsets over the 6 seconds.
- **Beat:** the median gap between onsets, doubled or halved into 250 ms – 1 s.
- **Pauses:** the share of windows more than 30 dB below the loudest playing.
- **Ring:** the median time from a note's peak until it falls 20 dB.

Playing is **dense** above 6 notes per second and **sparse** below 2.5.

## Rules (`analyseSpace`)

| | Clean | Rock | Metal |
|---|---|---|---|
| Reverb mix, guitar | 0.20 | 0.12 | 0.07 |
| Reverb mix, bass | 0.08 | 0.05 | 0.03 |
| Reverb size | 0.50 | 0.35 | 0.25 |
| Reverb damping | 0.45 | 0.55 | 0.65 |
| Delay | mix 0.15, feedback 0.30 | mix 0.10, feedback 0.22 | none |

- Dense playing takes 0.6 × the reverb mix and 0.1 less size, so notes do not
  wash into each other. Sparse playing takes 1.25 × the mix and 0.1 more size.
- The reverb is switched on when its mix is at least 0.03.
- The delay is for guitar, clean or rock, at most 4 notes per second, with a
  measured beat. Its time is the beat, or the host tempo when DELAY SYNC is on.
  Sparse playing takes 1.2 × its mix.
- **Your effects stay yours:** a delay or reverb you already have on keeps its
  type, time, feedback, size and damping. Only its mix moves toward the rule.
  A delay you have on is never switched off; dense playing only lowers its mix.
- An effect that is off is switched on with the whole rule, and its mix is
  scaled by strength.
- Without the input pass (Input gain and Gate both locked) there is no playing
  to judge, and the step is kept with that reason.
- An unknown instrument (Detect without a Tone EQ pass) is treated as guitar.

## Testing

`ChimeraAutoSetTests`:
- **Rules (`spaceRules`), on plucked notes at fixed intervals:**
  - Silence reports no playing.
  - One note a second is measured as sparse, with a 1 s beat, pauses and the
    ring. Clean guitar gets reverb mix 0.25 with size 0.6, and a 1 s delay with
    mix 0.18.
  - Notes 300 ms apart give a 300 ms beat. Notes 200 ms apart fold to a
    400 ms beat and get no delay, because 5 notes per second is too busy.
  - Dense metal (10 notes per second) gets at most 0.05 reverb and no delay.
  - Metal never gets a delay.
  - Bass rock gets 0.05 reverb and no delay.
- **Controller (`controllerSpace`), real processor, fixtures fed to the input
  tap:**
  - The step runs after the input pass and applies the analysis.
  - At 50% strength the mixes are halved.
  - The player's reverb keeps its size and damping, and a synced delay keeps
    the host tempo.
  - The lock keeps both effects.
  - Without the input pass, the step reports why it kept them.
  - UNDO AUTO restores everything.
- **Full run (`controllerIntegration`):** twelve result rows, with Delay +
  reverb before the Final EQ and output pass.
- **Locks (`controllerEveryLock`):** all fourteen locks at four strengths.

## Not yet validated

The style amounts, the dense and sparse limits and the onset rule are starting
values. They have been checked on synthetic plucked notes, not on real
playing or listening tests. The beat comes from the gaps between notes, not
from the host or a tempo detector, so syncopated playing can give a beat that
is half or twice the felt one.
