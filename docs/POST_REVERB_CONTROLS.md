# POST reverb controls — pre-delay, low cut, tone, width and freeze (engine and parameters)

The POST reverb had three controls: SIZE, DAMPING and MIX. This adds five to
the reverb of every POST slot. This change is the engine, the parameters and
their tests. **The controls are not on the POST page yet**: until the page
change follows they are reachable from the host's parameter list and
automation only.

## The controls

| Parameter | ID (slot 1) | Range | Default | What it does |
|---|---|---|---|---|
| Reverb pre-delay | `reverbpredelay` | 0 to 200 ms | 0 | The reverb starts this much after the note. |
| Reverb low cut | `reverblowcut` | 20 to 500 Hz | 20 (off) | Takes the lows out of the reverb, 12 dB per octave. The dry sound is not touched. |
| Reverb tone | `reverbtone` | -1 to +1 | 0 | Tilts the reverb around 1.5 kHz, up to 6 dB each way: dark below 0, bright above. |
| Reverb width | `reverbwidth` | 0 to 2 | 1 | Scales the difference between the reverb's two channels: 0 is mono, 2 doubles it. |
| Reverb freeze | `reverbfreeze` | off / on | off | Holds the tail and takes no new playing into the reverb. |

Slots 2 to 4 have the same five under `slot2_`, `slot3_` and `slot4_`.

## Compatibility

- **Parameters.** 20 parameters appended after every earlier one, at version
  hint 43, slot 1 first. No released parameter moves.
- **Sound.** At the defaults the reverb is not touched: the pre-delay line is
  not read, the low cut, tone and width are skipped, and freeze is off. A
  render of all six reverb models before and after this change, at the
  defaults, was compared sample by sample on Linux: see Verification.
- **Projects.** A project saved before this change has no values for the
  controls and loads them at their defaults.

## How it works

`SpaceModule::process` takes a `ReverbControls`.

- **Pre-delay and freeze** act on what the reverbs hear. The input goes
  through a delay line (linear interpolation, 50 ms glide when the time is
  changed) and is faded out over 50 ms when freeze is switched on.
- **Freeze** also stops each model's decay: the two JUCE reverbs use their
  freeze mode; the three feedback delay networks (Hall Prime, Void Space,
  Gated Room) stop decaying and their loops are not damped, and the Gated
  Room's gate stays open; the spring's loop is closed (feedback 0.9995) with
  its filter open. Freeze holds only while the reverb is on.
- **Low cut, tone and width** act on the reverb's sum before it is mixed with
  the dry sound. Low cut is two one-pole high-passes; tone splits the sum at
  1.5 kHz with one pole and scales the two parts; width scales half the
  difference of the two channels. With one channel, width does nothing.

There is no added latency: only the reverb is delayed.

## Verification (Linux)

- **The plain reverb is unchanged.** One program rendered noise through the
  reverb module built from the source before this change and from the source
  after it, in 108 configurations (44.1, 48 and 96 kHz; blocks of 64, 256 and
  480; mono and stereo; all six models), with the reverb switched off and on,
  a model change and SIZE, DAMPING and MIX moving on the way, then a reset
  and a second prepare. The 108 outputs are bit-identical. That program is
  not part of the repository.
- **`ChimeraReverbControlsDSPTests`** (new, on the module, all six models):
  pre-delay, low cut and tone, width, freeze, every control moving at its
  extremes in mono and stereo, and a new note after the controls return to
  their defaults.
- **`ChimeraSignalOrderTests`** (three new cases): the bank of 20 parameters,
  each slot's own controls through the processor (a switched-off slot's
  controls change nothing, bit for bit), and saving, loading and an older
  project.
- The whole test suite: 106 of 106 pass.

Measured on the module, the same in all six models to the figure shown:

| Control | Setting | Result |
|---|---|---|
| Pre-delay | 100 ms | the reverb 100 ms later, silent until then |
| Low cut | 500 Hz | 60 Hz in the reverb -37 dB, 4 kHz -0.7 dB |
| Tone | +1 | 150 Hz in the reverb -5.9 dB, 8 kHz +5.6 dB |
| Width | 2 | the difference of the channels +6.02 dB, their sum kept |
| Freeze | 6.5 s | the tail within 0 to -2.5 dB of where it was held; new playing not taken in |

Two existing checks in `SignalOrderTests.cpp` assumed every `slotN_`
parameter sits at version 35, 36 or 37; they now know the reverb controls'
version. The parameter-count checks of thirteen test files count the new
bank.

## Not in this change

- The knobs and the freeze switch on the POST page.
- Tempo-synced shapes, the older-digital character, key tracking and shimmer.
- Auto Set does not set these controls and its Space lock does not cover them.
- Factory presets do not use them.

## Not yet validated

No listening test: every measurement is on synthetic input. The spring's
freeze is a long hold, not an endless one, and its sound while frozen is
brighter than its tail, because its loop filter is opened. Moving the
pre-delay while notes ring glides the pitch of the reverb, as a tape delay
does; whether that is wanted was not asked. Not run on macOS or Windows yet.
