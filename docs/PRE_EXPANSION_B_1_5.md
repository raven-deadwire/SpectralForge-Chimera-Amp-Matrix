# PRE Expansion B 1.5 — Seven EQ, Multi Wah, Octave Fuzz, Violet Rage, Fire Drake, Pitch Bend

Status: **all six models implemented on `claude/pre-expansion-b`**
(based on `claude/pre-paraeq`), covered by `ChimeraPedalExpansionTests`.
Roadmap: 1.5 PRE Expansion B, six models (Revv G3; Wampler Dracarys; MXR
Slash Octave Fuzz; Dunlop 535Q; BOSS GE-7; DigiTech Whammy). These are
development references; the product shows its own working names.

## 1. Parameters: one appended group per expansion

The PRE board appends expansion models after every released parameter
(`docs/PRE_EXPANSION_A_1_4.md` §1). Expansion B adds a second **group**:
`pedalExpansionGroups` lists {first model, end model, version hint}, and
`appendPedalExpansionParameters` writes each group owner by owner, one group
after the other:

| Group | Models | Version hint |
|---|---|---|
| 1.4 PRE Expansion A | 40–45 | 16 |
| 1.5 PRE Expansion B | 46 on | 17 |

A later group therefore never sits between an earlier group's banks, so
releasing Expansion A first and B later moves no host index (tested: every
version-16 index precedes every version-17 index). Each further Expansion B
model is one catalog entry, one DSP branch and its tests.

## 2. Seven EQ (model 46, `pre.seveneq`)

Reference: BOSS GE-7. Public specification used (retailer listings: Full
Compass, Fair Deal Music, Musicmatter, Adorama):

| Hardware (published) | Chimera |
|---|---|
| Seven sliders at 100, 200, 400, 800 Hz, 1.6, 3.2, 6.4 kHz, ±15 dB each | Same frequencies and range |
| The first six are peaking filters, 6.4 kHz is a shelving filter | RBJ peaking at Q 1.4 (about one octave) for the six; RBJ high shelf (slope 1) cornered at 6.4 kHz |
| LEVEL slider ±15 dB | LEVEL ±15 dB |
| Bandwidth, shelf slope: **not published** | Chimera choices above |

Smoothed gains with 32-sample coefficient updates, no latency, no
allocation; flat is exactly transparent. The panel shows dB. An original
graphic EQ laid out after the published controls, not a measured model of
the pedal.

## 3. Multi Wah (model 47, `pre.multiwah`)

Reference: Dunlop Cry Baby 535Q. Public specification used (Dunlop spec
sheet PDF and manual text via search results; zZounds, Adorama listings):

| Hardware (published) | Chimera |
|---|---|
| RANGE, six positions: 440 Hz–2.2 kHz, 400 Hz–1.9 kHz, 375 Hz–1.8 kHz, 345 Hz–1.6 kHz, 395 Hz–1.4 kHz, 250 Hz–1.2 kHz | The same spans, swept logarithmically by the treadle. Position 5's 395 Hz is out of sequence (possibly a misprint) and is kept as published |
| Q: band-pass width | Q 2 (wide) to 10 (narrow), logarithmic |
| Filter gain 15 dB (at maximum Q, boost off) | Peak gain 15 dB at maximum Q, 6 dB at minimum Q (Chimera's law between) |
| Boost switch, up to +16 dB (spec sheet 0.1–17 dB) | BOOST 0–16 dB with BOOST ON |
| Filter shape, Q span, gain-versus-Q: **not published** | Chimera choices above; RBJ band-pass, coefficients every 8 samples, 10 ms smoothing |

No latency, no allocation. An original wah laid out after the published
ranges and controls, not a measured model of the pedal.

## 4. Octave Fuzz (model 48, `pre.octavefuzz`)

Reference: MXR SF01 Slash Octave Fuzz. Public specification used (Dunlop
manual text and specifications via search results; Palen Music, Thomann
listings). The manufacturer's PDF host is not reachable from this
environment, so the manual was not read first-hand.

| Hardware (published) | Chimera |
|---|---|
| VOLUME: the direct signal, or the main fuzz with SUB INTO FUZZ engaged; dry maximum gain +1.6 dB | VOLUME −60 to +1.6 dB, same routing |
| FUZZ: fuzz gain, 40 dB range (only with SUB INTO FUZZ) | FUZZ 10–50 dB of drive into an asymmetric tanh clip (the 10 dB offset is Chimera's) |
| TONE: brighter clockwise (fuzz only) | 2-pole low-pass 500 Hz–6 kHz |
| SUB OCTAVE level; sub voice corner 149 Hz | Flip-flop divider on tracked zero crossings at the input envelope, RBJ low-pass at 149 Hz |
| SUB INTO FUZZ switch: the sub voice goes into the fuzz | Switch, 20 ms crossfade between the clean and fuzz routes |
| OCTAVE UP level, own footswitch | OCTAVE UP level and OCTAVE UP ON |
| Internal octave-up GAIN (20 dB range) and TONE trims | UP GAIN 0–20 dB into a soft clip after a full-wave rectifier; UP TONE low-pass 1–8 kHz |
| Divider, clipping, rectifier, tone curves: **not published** | Chimera choices above |

Everything runs at 4x (the polyphase IIR used by the other drives), so the
PRE slot latency is unchanged. The sub is monophonic (single notes). No
allocation. An original octave fuzz laid out after the published controls,
not a measured model of the pedal.

## 5. Violet Rage (model 49, `pre.violetrage`)

Reference: Revv G3. Public specification used (Premier Guitar and
guitar.com reviews, GAK and effectsdatabase listings):

| Hardware (published) | Chimera |
|---|---|
| VOLUME, GAIN | VOLUME −24 to +6 dB; GAIN 6–40 dB of drive |
| BASS, MIDDLE, TREBLE | RBJ low shelf 100 Hz, peak 700 Hz (Q 0.7), high shelf 3 kHz, ±12 dB each, after the clipping |
| AGGRESSION, three positions (off, red, blue), adding gain and volume at two intensities | OFF; RED +6 dB gain, +2 dB volume; BLUE +12 dB gain, +4 dB volume. Which position is stronger is not stated in a spec; BLUE as the stronger follows one review's description and is an interpretation |
| "Tight lows, present highs" (review wording) | 90 Hz 2-pole high-pass before the clipping, 150 Hz coupling between the two stages, 7.5 kHz low-pass after the EQ |
| Amounts, clipping, filter shapes: **not published** | Chimera choices above |

Two soft-clip stages at 4x; PRE slot latency unchanged; no allocation. An
original distortion laid out after the published controls, not a measured
model of the pedal or of the amp channel it follows.

## 6. Fire Drake (model 50, `pre.firedrake`)

Reference: Wampler Dracarys. Public specification used (Wampler 2019
manual excerpts via search results; Thomann, Drummond Musique listings).
The manual PDF host is not reachable from this environment.

| Hardware (published) | Chimera |
|---|---|
| VOLUME, GAIN | VOLUME −24 to +6 dB; GAIN 12–48 dB of drive |
| Active BASS, MIDS, TREBLE; "react differently" per mode | RBJ low shelf 100 Hz, peak 800 Hz (Q 0.8), high shelf 2.5 kHz, ±12 dB each, ahead of the clipping, so they shape what distorts |
| OPEN / TIGHT: two clipping modes; OPEN little compression, TIGHT more compression and sustain | OPEN: 60 Hz pre-cut, second stage at ×2; TIGHT: 150 Hz pre-cut, second stage at ×8 (same small-signal gain, more compression) |
| EQ ranges, clipping, filter shapes: **not published** | Chimera choices above |

Two soft-clip stages at 4x, 8 kHz low-pass after; PRE slot latency
unchanged; no allocation. An original distortion laid out after the
published controls, not a measured model of the pedal.

## 7. Pitch Bend (model 51, `pre.pitchbend`)

Reference: DigiTech Whammy (5th generation). Public specification used
(MusicRadar and Sound On Sound reviews; Adorama, Musik Produktiv, Thomann,
Perfect Circuit listings). The manual PDFs (DigiTech, B&H, manualslib) are
not reachable from this environment.

| Hardware (published) | Chimera |
|---|---|
| One selector: 10 Whammy, 9 Harmony, 2 Detune settings (counts differ between listings; 10/9/2 is the most common) | SETTING with 21 entries in that split |
| Interval names | Commonly published layout: 2 OCT UP, OCT UP, 5TH UP, 4TH UP, 2ND DN, 4TH DN, 5TH DN, OCT DN, 2 OCT DN, DIVE BOMB; harmony pairs 2ND–3RD UP, b3–3RD UP, 3RD–4TH UP, 4TH–5TH UP, 5TH–6TH UP, 5TH–7TH UP, 4TH–3RD DN, 5TH–4TH DN, OCT DN–OCT UP; DETUNE SHALLOW / DEEP. **Not checked against the manual itself** |
| Treadle sets the bend | TREADLE: Whammy heel unison → toe interval (shifted signal only); Harmony heel first → toe second interval over the dry signal; Detune heel 0 → toe the detune, over dry |
| CLASSIC (single notes) / CHORDS switch | Base window 25 ms / 60 ms; the change fades the shifted signal out and in (5 ms) |
| Dive-bomb depth, detune amounts, algorithm: **not published** | DIVE BOMB −36 semitones; detune −10 / −25 cents; two crossfaded delay taps (sine² windows) |

How it shifts: two read taps on a delay line move at (1 − ratio) samples per
sample, crossfaded half a window apart. The input period is tracked (40 Hz–
1.5 kHz, low-passed zero crossings); on a tracked note the window is an even
number of periods (at most one period, 25 ms, beyond the base), so the taps
read the note in phase while they crossfade (no splice sidebands); untracked
input such as a chord keeps the base window. The delay line holds the
longest such window (85 ms); windows are also clamped to it. At exact unison (Whammy heel) the taps drift (at most about 17 cents)
until one sits mid-window alone, so the heel is a clean delayed copy.
Limits: the shifted signal lags by up to one window (25–50 ms CLASSIC,
60–85 ms CHORDS, longer windows on lower notes) and this is not reported
as latency; after a note starts or the interval changes, phase alignment
waits for each tap's next wrap (0.6 s for a 2-semitone shift of a 220 Hz
note in CHORDS); the unison drift takes up to about 2.5 s (CLASSIC) or 4.3 s
(CHORDS) after returning to the heel (computed from the drift rate and the
largest windows, not measured); chords show splice sidebands. No
allocation (the delay lines are sized in `prepare`).

## 8. Measurements (`ChimeraPedalExpansionTests`)

- Flat: transparent (error below 1e−4 dB).
- Each band at ±15 dB gives exactly ±15 dB at its centre (error 6e−13 dB);
  the 6.4 kHz shelf reaches within 1 dB of its gain at 16 kHz.
- LEVEL −15 dB is exact; measured sine gain equals the analytic response
  within 0.013 dB for single bands and a mixed setting.
- Multi Wah: the response peak sits within 0.08 % of each RANGE's published
  low (heel) and high (toe) ends for all six positions; measured gain equals
  the analytic response within 0.0002 dB; peak 15.0 dB at maximum Q, 6.0 dB
  at minimum; one octave above the centre −23.6 dB (max Q) against −10.0 dB
  (min Q); BOOST adds 16 dB.
- Octave Fuzz: VOLUME gives +1.6 dB and −60 dB on the direct signal
  exactly; a 220 Hz note (−12 dBFS) gives its 110 Hz sub at −11.6 dBFS; the
  sub of 880 Hz is 18.8 dB below that of 120 Hz (the 149 Hz low-pass);
  OCTAVE UP turns 440 Hz into 880 Hz at −6.6 dBFS, silent when switched
  off; UP GAIN spans 20.0 dB and FUZZ 40.0 dB (small signal); TONE moves
  3 kHz by 30.9 dB; harmonics 2–5 −50.1 dB (FUZZ 0, −40 dBFS) against
  −8.4 dB (FUZZ max, −20 dBFS); SUB INTO FUZZ puts the 110 Hz sub through
  the fuzz (−14.9 dBFS, none without the sub).
- Violet Rage: BASS/MIDDLE/TREBLE measured through the drive equal the
  analytic tone stack within 3e−7 dB (small signal, 80 Hz–6 kHz); GAIN
  spans 34.0 dB, RED adds 8.0 dB and BLUE 16.0 dB (gain plus volume), VOLUME
  spans 30.0 dB; 40 Hz sits 25.5 dB below 400 Hz; harmonics 2–5 at −30 dBFS
  200 Hz: −45.0 dB (GAIN 0), −22.9 dB (0.5), −16.2 dB (RED), −12.6 dB (BLUE).
- Fire Drake: BASS/MIDS/TREBLE measured through the drive equal the
  analytic EQ within 0.002 dB in both modes (small signal, 200 Hz–6 kHz);
  GAIN spans 36.0 dB, VOLUME 30.0 dB; TIGHT is 13.0 dB lower than OPEN at
  60 Hz; for a 30 dB louder input (500 Hz, GAIN 0.3) the output rises
  13.3 dB in OPEN and 3.6 dB in TIGHT.
- Pitch Bend: all ten Whammy settings at the toe, on 82.4 and 220 Hz, in
  CLASSIC and CHORDS: spectral peak within 0.0053 % of the interval (the
  search grid's resolution), energy centroid within 0.28 %; all nine
  Harmony pairs at heel and toe within 0.0006 % with the dry signal intact
  (±1 dB); OCT UP on a 220 + 330 Hz chord (CHORDS): energy centred at 437.5
  and 658.8 Hz (−0.6 %, −0.2 %); heel after the toe: 220 Hz at −0.001 dB
  re input; a CLASSIC/CHORDS change mid-note has no sample step above the
  steady signal's. OCT UP on 41.2, 52 and 61.7 Hz in both modes: pitch as
  above, level within 1 dB, longest window 76.9 ms inside the 85.1 ms delay
  line. (A first version sized the line for 72.5 ms; at 52 Hz in CHORDS it
  read outside the buffer. The window-against-capacity check fails on that
  version; the pitch and level checks alone did not catch it reliably.) During development a fixed 60 ms CHORDS window measured
  up to 8.1 % centroid error on these single notes, and whole- but odd-
  period windows (taps half a period out of phase) up to 3.8 % peak error;
  hence the even-period windows.
- 440 appended parameters in total (Expansion A 225 at version 16, then
  Seven EQ 45, Multi Wah 30, Octave Fuzz 50, Violet Rage 35, Fire Drake 35
  and Pitch Bend 20 at version 17).

## 9. Not done

- Checking the Pitch Bend interval list and the SF01, Dracarys and Whammy
  details against the manuals themselves (not reachable here).
- Listening on real DI; DAW automation and recall.
