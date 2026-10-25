# Easy view

Easy view is a simpler start screen for players who are new to Chimera. It sits
next to the existing editor, which is unchanged and is now called the *full
editor* in the interface.

## What it shows

1. **Pick a sound**: the same preset menu as the full editor, with previous and
   next buttons and the preset's description.
2. **Shape it**: six large knobs.
   | Knob | Edits |
   | --- | --- |
   | GAIN / BASS / MIDDLE / TREBLE | The selected head's own front-panel control on its current channel (for example `PRE GAIN` on Tight 515, `VOLUME` on a non-master head). The knob follows amp and channel changes. A head without a matching control shows the knob disabled. |
   | REVERB | The POST reverb mix. It reads 0 while the reverb is off; turning it up switches the reverb on and back to 0 switches it off. The stored mix is kept. |
   | VOLUME | Output gain. |
3. **Get ready**: AUTO SET (with CANCEL and UNDO) and the tuner.

Between the preset row and the knobs, Easy view shows the same cabinet room
as the full editor, so players always see which amps and cabinets they are
using. Clicking a head or cabinet opens the same amplifier or cabinet window
as in the full editor. While the tuner is open, it takes the place of the
room and the knobs.

In Dual mode the amp knobs edit the first rig. In Matrix mode they edit the
MID rig, because LOW is the compressed DI band. Easy view shows which head and
channel it is editing.

## What it does not change

- No host parameter was added, so projects, presets, reference files and
  automation are unchanged. Every Easy knob is the same parameter as the
  matching control in the full editor.
- Switching between Easy view and the full editor never changes the sound.

## Which view opens

The first time Chimera opens on a computer, it shows Easy view. After that it
opens in the view the player last chose with **EASY VIEW** (bottom bar of the
full editor) or **FULL EDITOR** (top right of Easy view). The choice is stored
for that computer in `SpectralForge/Chimera/interface.json` under the user
application-data folder. It is never saved in a project.

## Tests

`ChimeraEasyModeUITests` (`ChimeraUITests <folder> --easy-mode-only`) checks
the stored choice, the knob mapping for every head and channel, that the
knobs edit the shared parameters, reverb on/off behaviour, preset loading,
the tuner, and that switching views leaves the sound unchanged. It saves
screenshots at 75%, 100% and 150%.
