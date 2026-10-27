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
   | GAIN / BASS / MIDDLE / TREBLE | The selected head's own front-panel control on its current channel and input (for example `PRE GAIN` on Tight 515, `VOLUME` on a non-master head). GAIN changes drive and the amp's loudness. The knob follows amp, channel and input changes, and its tooltip names the panel control it edits. A head without a matching control shows the knob disabled. |
   | POST REVERB | The POST reverb (after the amp and cabinet). It reads 0 while that reverb is off; turning it up switches it on and back to 0 switches it off. The stored mix is kept. A reverb built into the amp head is not changed. Host automation sees two parameters: `reverbon` and `reverbmix`. |
   | OUTPUT | The final output level (`output`). The output limiter follows it, so very loud settings may be held back. |
3. **Get ready**: AUTO SET with its options (instrument and style, the same
   choice as the full editor), CANCEL, UNDO and the tuner. CANCEL only stops
   listening: steps Auto Set already finished stay applied, and UNDO restores
   the settings from before the run.

Between the preset row and the knobs, Easy view shows the same cabinet room
as the full editor, so players always see which amps and cabinets they are
using. The room is view-only here: amplifier and cabinet windows open from the
full editor. While the tuner is open, it takes the place of the room and the
knobs.

In Dual mode the amp knobs edit the first rig. In Matrix mode they edit the
MID rig, because LOW is the compressed DI band. Easy view frames that rig in
the room and names the head, channel and input it is editing. When the target rig cannot be heard (muted,
another rig soloed, its amp switched off, or Dual BLEND fully on amp 2), a note
next to the amp name says so.

### Heads with several inputs or sections

| Head | Input / section | GAIN edits |
| --- | --- | --- |
| Brit Edge | HIGH TREBLE HIGH / LOW, JUMPED | LOUDNESS I / HIGH TREBLE |
| Brit Edge | NORMAL HIGH / LOW | LOUDNESS II / NORMAL |
| Monolith | NORMAL | NORMAL |
| Monolith | BRILLIANT, JUMPED | BRILLIANT |
| Modern Bass | Amplifier section | GAIN, BASS, MID and TREBLE of the amplifier section; the drive section stays in the full editor |

With JUMPED inputs both volumes are heard; Easy GAIN edits the bright input and
the other volume stays in the full editor.

Projects that still use the classic amp engine (head panel off) edit the
classic lane controls: GAIN is `drive`, BASS `bass`, MIDDLE the low mid
`lowmid` and TREBLE `treble`. A newer head whose panel is off shows its knobs
disabled with the reason.

### Presets

Easy view uses the full editor's preset loading. When you change a preset's
sound, the menu shows the preset name followed by *(edited)*. Using the tuner
does not count as an edit.

## What it does not change

- No host parameter was added. Every Easy knob is the same parameter as the
  matching control in the full editor.
- Switching between Easy view and the full editor writes no parameter and
  sends no host gesture (checked by the tests). If the routing changes while a
  knob is held, the knob keeps editing its parameter until it is released,
  so every host gesture is closed on the parameter it began on.
- Dropping an IR file works only on a cabinet shown in Easy view's room.

Not yet verified: audio continuity measured across a view switch in a running
host, host automation read / touch / latch in DAWs, Windows scaling and macOS
Retina / AU / VST3 editor lifecycles. These need checks in real hosts.

## Which view opens

The first time Chimera opens on a computer, it shows Easy view. After that it
opens in the view the player last chose with **EASY VIEW** (bottom bar of the
full editor) or **FULL EDITOR** (top right of Easy view). The choice is stored
for that computer in `SpectralForge/Chimera/interface.json` under the user
application-data folder. It is never saved in a project.

## Tests

`ChimeraEasyModeUITests` (`ChimeraUITests <folder> --easy-mode-only`) checks:

- the stored choice and the `createEditor()` start path;
- for every head, channel and input route, that each amp knob maps to a
  visible knob that changes the real amp DSP output, plus the explicit
  input-path cases above;
- first-open binding (missing knobs disabled) and classic-engine projects;
- immediate rebinding on a route change, no rebinding during a drag, balanced
  host gestures (also for POST REVERB and when the editor closes mid-drag);
- the Dual / Matrix target lane, its name and the "not heard" notes;
- IR drops on the cabinet versus the knobs;
- an Auto Set run started before Easy view existed, CANCEL and the options;
- preset loading, tuner use without CUSTOM, the *(edited)* name;
- no parameter writes or gestures when switching views.

It saves screenshots at 75%, 100% and 150%.
