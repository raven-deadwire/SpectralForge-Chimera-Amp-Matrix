# POST reverb key — the reverb rings in a key

The reverb of every POST slot can be passed through resonators tuned to the
notes of a key, so that it rings on those notes and is quiet between them.
At no amount, the default, the reverb renders as it did. The dry sound is
not touched.

## The controls

| Parameter | ID (slot 1) | Choices or range | Default | What it does |
|---|---|---|---|---|
| Reverb key | `reverbkey` | 0 to 1 | 0 | Crosses from the reverb as it is to the reverb through the key's resonators. |
| Reverb key root | `reverbkeyroot` | C to B | C | The key's root. |
| Reverb key scale | `reverbkeyscale` | Major, Minor, Dorian, Minor pentatonic, Major pentatonic | Major | The key's scale. |
| Reverb key tune | `reverbkeytune` | -50 to +50 cents | 0 | Moves every note; 0 is A at 440 Hz. |
| Reverb key ring | `reverbkeyring` | 0 to 1 | 0.5 | How narrow each note is: broad at 0, narrow and long-ringing at 1. |

Slots 2 to 4 have the same five under `slot2_`, `slot3_` and `slot4_`.

## How it works

There is one two-pole resonator for each note of the scale from A1 (55 Hz)
to A6 (1760 Hz): up to 36 for a scale of seven notes, up to 26 for one of
five. Each is
as wide as its frequency over `4 x 62.5^ring`: a quarter of it at RING 0, a
250th at 1. The reverb, after its low cut and tone, goes through all of
them; their sum replaces the reverb by the amount.

**Level.** The resonators pass only part of the reverb, so their sum is
raised to keep the level. The gain takes the reverb to fall off above 5 kHz
with one pole, counts what each resonator passes of that, and allows for
their overlap at a low RING (figures computed from the resonators' own
response, in `Source/ReverbControls.h`). A reverb brighter than that comes
out a little lower and a darker one a little higher: see the measurements.

The plugin's latency does not change.

## Compatibility

- **Parameters.** 20 parameters appended after every earlier one, at version
  hint 49, slot 1 first. No released parameter moves.
- **Sound.** At no amount the reverb takes the path it took before, whatever
  the other four say.
- **Projects.** A project saved before this change opens with no key.

## The ALL window

A fifth row: AMOUNT, ROOT, SCALE, TUNE and RING, under the small line KEY.
A double-click returns AMOUNT and TUNE to 0 and RING to 0.5. The window is
now 740 by 926; on a screen too low for it the window scrolls, as the other
large windows do.

## Verification (Linux)

- **`ChimeraReverbControlsDSPTests --key`** (new): every scale holds seven or
  five notes with its root, the same in every octave; in five models the
  reverb in A major is at least 12 dB higher on A and C sharp than on the A
  sharp and C between them, against the plain reverb; Minor moves C sharp to
  C; A sharp as the root moves B to A sharp; TUNE +50 cents moves the A a
  quarter tone; RING 1 leaves less between the notes than RING 0; half the
  amount lies between none and all; the level stays within 6 dB; at no
  amount the render is unchanged, sample for sample; after it is switched
  off a new note's reverb is the plain one; every control moving under loud
  noise stays finite and bounded in every model, mono and stereo.
- **`ChimeraSignalOrderTests`** (one new case): the bank of 20 parameters at
  version 49; each of the five acts through the processor; at no amount, and
  in a switched-off slot, nothing changes, bit for bit; saving, loading and
  an older project.
- **`ChimeraUITests`**: the three knobs write their parameters and return to
  their defaults; ROOT and SCALE list their choices and write theirs; the
  window shows every row without scrolling.

Measured on the module with noise, A major at full, RING 0.8, against the
plain reverb (levels within 4 Hz of each note):

| Model | On the notes | Between them | Whole level |
|---|---|---|---|
| Room | +11.1 dB | -14.8 dB | -2.6 dB |
| Hall | +11.2 dB | -14.6 dB | +0.2 dB |
| Spring | +11.0 dB | -14.3 dB | +0.8 dB |
| Hall Prime | +11.4 dB | -11.3 dB | +2.9 dB |
| Void Space | +11.6 dB | -17.0 dB | +4.0 dB |

Over two scales and RING 0, 0.5, 0.8 and 1 the whole level moved by -4.3 to
+3.8 dB.

## Not in this change

- The key from MIDI notes, and finding the key from the playing.
- Other scales and tunings, and notes outside A1 to A6.
- Auto Set does not set these controls. Factory presets do not use them.

## Not yet validated

No listening test: every measurement is on synthetic input. At the default
RING a note is not well clear of the semitone beside it; how clearly the
key is heard there was not checked. The resonators are retuned at once when
ROOT, SCALE, TUNE or RING change: that was checked to stay bounded, not to
be free of clicks. Not run on macOS or Windows yet.
