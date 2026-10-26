# PRE Expansion A 1.4 — first model: Para EQ

Status: **first model implemented on `claude/pre-paraeq`**, covered by
`ChimeraPedalParaEQTests`. Roadmap: 1.4 PRE Expansion A, six models (Empress
Heavy Menace; Tel-Ray Power Wah Boost; MXR MB301 Bass Synth; EBS MultiComp;
Fortin 33; Empress ParaEQ MKII). These are development references; the
product shows its own working names. Source baseline: main `cfeaa53`.

## 1. Adding PRE models without moving released parameters

The five-slot board's parameter banks were laid out owner by owner, with
every model's bank inside each owner. A new model in that loop would insert
its parameters between owners and shift every later host index. So:

- `releasedPedalModelCount = 40`: models 0–39 keep their banks in the
  released layout.
- Models from 40 on append their banks **after every released parameter**
  (`appendPedalExpansionParameters`, version hint **16**), owner by owner.
  IDs follow the same pattern (`board<owner>_m<model>_<control>`,
  `…_bypass`), so the board's parameter cache, panel and MIDI learn need no
  special case.
- Each further Expansion A model is one catalog entry
  (`Tools/generate_pedal_catalog.py`), one DSP branch and its tests; its
  banks append after the previous model's.

Merge note: `claude/room-core` appends the room parameters at version hint
15 after the extra gates. If both land, the room block must stay before this
one (15 before 16); the parameter-count contract tests on both branches each
add their own count.

## 2. Para EQ (model 40, `pre.paraeq`)

Reference: Empress ParaEQ MKII (standard, not Deluxe). Public specification
used (retailer listings: Perfect Circuit, Woodbrass, wrdtunes; the
manufacturer's site was not reachable from the development container):

| Hardware (published) | Chimera |
|---|---|
| Low band 35–500 Hz, mid 250–5000 Hz, high 1–20 kHz, overlapping | Same ranges; the knob is logarithmic across each band. The high band is held below 0.45 × the sample rate |
| ±15 dB per band | ±15 dB, RBJ peaking filters |
| Three Q settings per band: wide / medium / narrow | WIDE / MEDIUM / NARROW = Q 0.6 / 1.4 / 4.0. **Chimera's values**: the hardware's are not published |
| Up to +30 dB clean boost, footswitch, linkable to the EQ | BOOST 0–30 dB (default 10) with BOOST ON; the slot's own bypass switches the whole pedal |
| No low/high cut on the standard model (Deluxe only) | None |

Order: the three bands in series, then the boost. Neutral settings (all
gains 0, boost off) are exactly transparent. Frequency, gain and Q are
smoothed over 20 ms and the coefficients follow every 32 samples, so
automation does not step; no allocation in the callback; no added latency.
The panel shows the frequency knobs in Hz/kHz and gains in dB.

This is an original parametric EQ laid out after the published controls. It
is not a measured or circuit-level model of the pedal: its filter shapes,
knob tapers, noise and headroom are not matched to the hardware.

## 3. Measurements (`ChimeraPedalParaEQTests`)

- Steady-state sine gain against the analytic response, four settings
  (low +15 dB wide at 35 Hz; mid −15 dB narrow; high +10 dB at 20 kHz,
  clamped, with a 30 dB boost; all bands with a 12 dB boost) at 35 Hz–15 kHz:
  largest error 0.003 dB (44.1 kHz), 0.007 dB (48 kHz), 0.006 dB (96 kHz).
- Centre gains exactly +15 / −15 dB; boost exactly 30 dB; the narrow Q
  changes an octave away at least 3 dB less than the wide one.
- Through the PRE board (owner 2 first in the order): boost 12 dB measures
  11.998 dB.
- 60 parameters (5 owners × bypass + 11 controls) at the end of the list,
  version 16; every released parameter keeps its index. A project without
  them restores their defaults.

## 4. Not done

- Listening on real DI; DAW automation and recall.
- Dedicated pedal artwork (the card uses the generic workbench style).
- The other five Expansion A models.
