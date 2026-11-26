# SYNTH: preset browser

Status: implemented on the `claude/synth-presets` branch, stacked on
`claude/synth-effects`. Measured by `ChimeraSynthTests` on Linux. Not tried in
a DAW, and not on Windows or macOS (file names, the preset folder and
OPEN FOLDER were run on Linux only).

## Why

The owner's direction for SYNTH is a synth in the class of Serum or ArgWave 3.
Until now the page had a menu with four built-in patches and no way to keep a
sound. This step adds presets the user saves, and a browser for them.
Wavetable import is not in this change.

## What a preset is

- Every synth setting a patch may change: oscillators, wavetables, filters,
  distortion, envelopes, LFOs, the step sequencer, macros, both halves of the
  matrix and the effect rack. 184 parameters in this version.
- **Not in a preset:** SYNTH ON, SOURCE, INSERT, LEVEL, THRESH and each
  oscillator's sample ROOT. These are the eight parameters a built-in patch
  already leaves alone. Loading a preset does not change them.
- **A loaded sample is not in a preset.** A preset that uses Sample mode
  plays whatever sample the project has in that oscillator.
- Loading a preset sets each parameter that differs, as one host gesture each.
  A parameter the preset does not name returns to its default.

## Built-in and own presets

- The four built-in patches are the factory presets: Init (Other), Dvergr
  Analog (Bass), Seidr Dynamic (Pad), Mimir Impact (Pluck).
- A preset the user saves is one file, `<name>.chimerasynth`, in
  `SpectralForge/Chimera/Synth Presets` under the user's application data
  folder. The folder is created the first time a preset is saved (or OPEN
  FOLDER is pressed). The file's name is the preset's name.
- Copying a file into or out of that folder is how presets are shared.
- The file is JSON: a format name, a format version (1), a type and the
  parameters by id with their real values (Hz, %, dB, list positions).
- A file is refused when it is over 256 KB, is not this format, is from a
  newer format version, or holds a value that is not a finite number. Refused
  files are counted under the list, not shown.
- When a file is read, ids that presets do not carry and ids this version
  does not know are ignored. A value beyond a control's range is held at the
  nearest limit.
- Names: up to 48 characters. Characters a file name cannot hold
  (`\ / : * ? " < > |` and control characters) are removed, as are leading
  dots. Names Windows reserves (CON, NUL, COM1 ...) are refused. Two names that
  differ only in upper and lower case are the same preset.
- Types: Bass, Lead, Pad, Pluck, Keys, FX, Other.
- At most 1000 files are read from the folder.

## On the page

- **Top bar:** previous, the loaded preset's name, next, in place of the
  PATCH menu.
  - The name is followed by `*` once a setting differs from the preset.
  - `PRESETS` is shown when no preset is loaded.
  - Previous and next step through the browser's list as it is filtered, and
    wrap round.
- **Pressing the name** opens the browser in the lower row, in place of the
  envelopes, LFOs, matrix and sub / noise (as FX does; one of the three views
  shows at a time). The oscillators, filters and master stay.
  - Left: ALL PRESETS / FACTORY / USER, the type, and a search on the name.
  - Middle: the list - name, type, FACTORY or USER. Choosing a row loads it;
    the arrow keys move through the list.
  - Right: NAME, the type to save under, SAVE, DELETE, OPEN FOLDER, and a
    line that says what happened.
- **SAVE** writes the settings as they are. If one of the user's presets has
  that name, the button turns to REPLACE? and a second press overwrites it.
- **DELETE** removes the loaded preset's file after a second press (SURE?).
  The sound stays as it is. Built-in presets cannot be deleted.
- The folder is read when the browser opens, after SAVE and DELETE, and on
  the first previous / next. Opening the plugin window reads at most one file
  (the loaded preset's).
- To make room, CHARACTER in the top bar is narrower.

## The loaded preset's name

- It is kept in the project, next to the parameters (not as a parameter), so
  a reopened project shows it. It is part of each A/B slot's state.
- The `*` mark is worked out by comparing the settings with the preset. If
  the preset's file is not on this computer the name is shown without a mark.

## Parameters and compatibility

- No parameter was added, removed or changed.
- A project saved before this change shows `PRESETS` and sounds as before.
- The `synthPatch` menu is gone from the page; its four patches are in the
  list.

## Not done

- No favourites, ratings, tags, author or notes.
- No sub-folders: every file is directly in the one folder.
- No rename in the browser (rename the file, or save under the new name and
  delete the old one).
- No undo for loading a preset over unsaved settings, and no warning.
- Presets do not carry samples (see above).
- The list shows about nine rows at the default size; it scrolls.
- No more factory presets were made: there are still four, and none uses the
  sequencer, a macro or an effect.
- The preset name following an A/B switch was not tested.
- DE / KO manual text is not written.

## Measurements

From `ChimeraSynthTests` on Linux (x86-64), this branch.

- A capture of every parameter at a random value, written and read back:
  184 parameters carried, 8 kept out, file 5477 bytes, largest difference 0.
- Refused, each with a message and without changing anything: a missing
  file; text that is not JSON; another format name; version 0, 2 and "1";
  no parameter list; a value that is text, true / false, a list or 1e300; a
  file over the size limit.
- A file naming `synthOn`, `synthLevel`, an amp parameter, `synthCutoff` and
  an unknown synth id: read with the two it may carry.
- List order with files `zeta`, `Alpha 10`, `alpha 2`, `Beta`, `Round Trip`:
  factory first, then alpha 2, Alpha 10, Beta, Round Trip, zeta.
- Reading a folder of 304 presets: 45 ms (one run, this machine).
- Page at 1140 x 438: the list is 476 x 162 px, nine rows without scrolling;
  no browser or top-bar control overlaps another.
- SAVE wrote no parameter. Loading the saved preset after Init changed the
  three parameters that differed, two gesture messages each, and left LEVEL
  and SYNTH ON.
- A hand-written file with cutoff 1e8 Hz, resonance -50 % and filter type 99:
  loaded as the highest cutoff, 0 % and the last type.
- Opening the page and pointing it at a folder that does not exist did not
  create the folder.
- CPU was not measured (nothing here runs on the audio thread).

Screenshot: `docs/evidence/synth-presets-20261126/synth-page-presets.png`.
