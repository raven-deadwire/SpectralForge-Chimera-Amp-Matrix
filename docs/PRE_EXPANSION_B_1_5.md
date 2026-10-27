# PRE Expansion B 1.5 — Seven EQ, Multi Wah

Status: **two of six models implemented on `claude/pre-expansion-b`**
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

## 4. Measurements (`ChimeraPedalExpansionTests`)

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
- 300 appended parameters in total (Expansion A 225 at version 16, then
  Seven EQ 45 and Multi Wah 30 at version 17).

## 5. Not done

- The other four Expansion B models (Revv G3, Wampler Dracarys, MXR Slash
  Octave Fuzz, DigiTech Whammy).
- Listening on real DI; DAW automation and recall.
