# Auto Set — phase 23 (Cabinet IR)

Phase 23 lets Auto Set choose the factory IR of the Classic cabinet's heard
mic: **V30 / SM57** or **Jensen / SM57**. Phase 9 (`AUTO_SET_PHASE9.md`)
places a modeled mic; a mic that plays a captured IR has no placement, and
that row says "switch on its modeled mic". This step compares the two factory
IRs instead. Earlier phases are described in `AUTO_SET_PHASE1.md` …
`AUTO_SET_PHASE22.md`; locks and strength in `AUTO_SET_LOCKS_STRENGTH.md`.
The rules are fixed, as in the earlier phases.

## Use

The options button next to **AUTO SET** has a **Cabinet factory IR choice**
step, on by default, listed after **Cabinet mic placement**.

- **When it runs:** in the cabinet mic pass, after the cabinet mic step and
  before the cabinet cuts step. It adds no listening time; it alone also
  starts the pass.
- **What it sets:** the heard mic's cabinet source, mic A (`cabtype1`) or
  mic B (`cabBtype1`), between V30 / SM57 and Jensen / SM57.
- **What it leaves alone:** the mic level (the level change is reported; the
  Final EQ + output step sets the level), the other mic, the modeled mics,
  the cuts (the cuts step after it counts the switch), user IRs.
- **The results:** a **Cabinet IR** row, between the **Cabinet mic** and
  **Cabinet cuts** rows: "mic A IR V30 / SM57 > Jensen / SM57; balance off by
  4.2 > 0.2 dB; level +3.2 dB", or why it kept the IR.
- **Strength:** not applied. An IR is switched or kept.
- **Lock → Cabinet IR:** keeps both sources.
- **UNDO AUTO:** restores the source.

## Rules (`analyseCabIR`, `AutoSetCabIR.h`)

- **Heard:** the pass's capture after the cabinet (`Tap::cab`).
- **Prediction:** an IR is a fixed filter in series, so the other IR would
  give the heard third-octave bands plus the other IR's bands minus the
  current IR's. Each IR's bands are taken as the cabinet loads it
  (`juce::dsp::Convolution`, `Normalise::yes`: scaled by 0.125 / the square
  root of its energy), from `IRLibrary::factoryIR`.
- **Choice:** the other IR is chosen when its prediction sits closer to the
  style's Tone EQ target (the amp step's tone error) by more than 0.5 dB, as
  in phase 9.
- **Level:** reported as the heard power with the other IR against the heard
  power now.
- **Low end (verification case IR-01):** the balance counts from 80 Hz, so the
  notes under it are judged on their own. In 44.5–70.7 Hz (a seven-string's
  A1 in Drop A, 55 Hz; an eight-string's F#1, 46 Hz), where the playing has
  it (the 50 and 63 Hz bands within 30 dB of its strongest band):
  - The change is predicted at the notes themselves: the peaks of the heard
    fine spectrum (about 0.7 Hz bins, the loud frames), each scaled by the
    two IRs' power ratio at its frequency (each IR's response from 40 to 75 Hz
    in 0.25 Hz steps, as loaded). A third-octave average is not enough here:
    the factory IRs resonate in this range, and at 55 Hz the V30 is 3.0 dB
    above the Jensen although the 50 Hz third-octave says the opposite.
  - A switch that would cut the notes by more than 3 dB **and** leave the
    50–63 Hz power more than 3 dB under the style's target there is not
    made; the row says "Jensen / SM57 would balance closer (… dB) but cut
    50-63 Hz, the low strings' fundamentals, by … dB, to … dB under the rock
    target". Cutting low end that sits above the target is left to the
    balance.
- **Runs only** for Classic routing, the cabinet on, one mic heard (the A/B
  blend fully on one mic), that mic's modeled mic off and its source a factory
  IR.
- **Kept, with the reason:** locked; the cabinet, a mic or a source changed
  while listening; Dual or Matrix routing; the cabinet off; two mics blended;
  a modeled mic (the cabinet mic step places it); filters only; a user IR;
  bass (both factory IRs are guitar cabinets); no playing heard; the low end
  (above); the current IR already closest; the source edited while applying.

## Testing

`ChimeraAutoSetTests` (also alone: `ChimeraAutoSetTests --cabir`):

- **Rules (`cabIRRules`, 44.1, 48 and 96 kHz):**
  - Target-shaped noise rendered through each factory IR with
    `juce::dsp::Convolution` (as the cabinet): the V30 bands plus the IR
    difference match the rendered Jensen within 0.58 dB per band, 80 Hz–6.3
    kHz.
  - Playing that suits one IR (the target divided by that IR's smoothed
    response), heard through the other, switches to it in every style: at
    48 kHz rock, V30 > Jensen 4.2 > 0.2 dB (rendered 0.4 dB), Jensen > V30 4.2
    > 0.3 dB (rendered 0.3 dB). The predicted error is within 0.5 dB of the
    rendered one and the reported level change within 0.07 dB. Heard through
    the suited IR, it is kept.
  - Bass, filters only, a user IR and silence are kept.
- **Controller (`controllerCabIR`, real processor):**
  - The step runs alone in the cabinet pass and switches mic A V30 > Jensen;
    50% strength still switches; the suited IR is kept; with mic B heard,
    `cabBtype1` moves and `cabtype1` stays.
  - The lock keeps the source (no pass; with the cuts step on, the row says
    locked); the row comes before the cuts row.
  - Kept: the source edited while listening, a modeled mic, filters only, a
    user IR, Dual routing, the cabinet off, two mics blended.
  - UNDO AUTO restores the source.
- **Full run (`controllerIntegration`):** twenty-four result rows, with the
  Cabinet IR row (the default rig's mic A plays V30 / SM57) after the Cabinet
  mic row.
- **Locks (`controllerEveryLock`):** all twenty-six locks at four strengths.

- **Low end (`cabIRLowEnd`, 44.1, 48 and 96 kHz;** also alone:
  `ChimeraAutoSetTests --cabir-low`**):** playing that suits one IR with an
  A1 note (55 Hz, eight harmonics) on top, heard through the other IR:
  - The predicted change at the note is checked against the rendered switch,
    measured independently (projection on a 55 Hz sine): within 0.91 dB.
    Jensen > V30 raises the A1 note by about 3 dB, V30 > Jensen lowers it by
    about 3 dB, and both stay above the target, so the balance decides.
  - Playing whose low end sits at the target, with the other IR made 8 dB
    lower at 50–63 Hz (third octaves) or at 40–75 Hz (fine responses): the
    balance still favours it; it is kept.
  - Playing without low end (high-passed at 160 Hz) is not judged on it.

## Not yet validated

The 3 dB limits and the 30 dB range for the low end are starting values,
checked on synthetic playing through the factory IRs, not on a real Drop A
DI (not recorded yet) or by listening. The controller row for a low-end hold
is not reached with the two factory IRs and the test playing; the rule is
tested on the analysis.


Checked against the factory IRs rendered offline, not with a real DI or a
listening test. The 0.5 dB threshold is phase 9's. The level change is
reported, not compensated: with Final EQ + output off, a switch changes the
loudness by the reported amount.
