# PRE Expansion B 1.5 — first model: Seven EQ

Status: **first of six models implemented on `claude/pre-expansion-b`**
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

## 3. Measurements (`ChimeraPedalExpansionTests`)

- Flat: transparent (error below 1e−4 dB).
- Each band at ±15 dB gives exactly ±15 dB at its centre (error 6e−13 dB);
  the 6.4 kHz shelf reaches within 1 dB of its gain at 16 kHz.
- LEVEL −15 dB is exact; measured sine gain equals the analytic response
  within 0.013 dB for single bands and a mixed setting.
- 270 appended parameters in total (Expansion A 225 at version 16, then
  Seven EQ 45 at version 17).

## 4. Not done

- The other five Expansion B models (Revv G3, Wampler Dracarys, MXR Slash
  Octave Fuzz, Dunlop 535Q, DigiTech Whammy).
- Listening on real DI; DAW automation and recall.
