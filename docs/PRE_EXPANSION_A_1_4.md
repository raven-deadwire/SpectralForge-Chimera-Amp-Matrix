# PRE Expansion A 1.4 — Para EQ, Multi Comp, Tight Boost

Status: **three of six models implemented on `claude/pre-paraeq`**, covered by
`ChimeraPedalExpansionTests`. Roadmap: 1.4 PRE Expansion A, six models (Empress
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

## 3. Multi Comp (model 41, `pre.multicomp`)

Reference: EBS MultiComp. Public specification used (retailer and
manufacturer listings via search results: Woodbrass, Thomann, equipboard; the
manufacturer's site and manual were not reachable from the development
container):

| Hardware (published) | Chimera |
|---|---|
| COMP: ratio 1:1 (minimum) to 5:1 (maximum) | COMP 1:1–5:1 (default 3:1) |
| GAIN: 0–15 dB make-up | GAIN 0–15 dB (default 3 dB) |
| SENS: threshold −25 to +6 dB | SENS −25 to +6 dB (default −18 dB), as dBFS of the linked peak |
| Attack under 10 ms (to 80 %) | 4 ms detector attack; measured 7.96 ms to 80 % of a 14.8 dB step |
| Modes: Normal (one band), Multiband (independent bass and treble compressors), TubeSim (Normal plus tube-style harmonics) | NORMAL / MULTI-BAND / TUBESIM, same meanings |
| Blue Label: internal band balance in Multiband | BALANCE ±6 dB on the treble band, Multiband only |
| Crossover, release, knee, TubeSim curve: **not published** | Chimera choices: Linkwitz–Riley 250 Hz split, 160 ms release, 6 dB soft knee, gently asymmetric `tanh` stage that is unity at low level |

Stereo-linked peak detection per band, smoothed controls; a mode change
dips through a 10 ms fade so the band split and TubeSim stage do not switch
mid-wave. It is a compressor pedal in the menu, so its detail view is the
compressor spectrum workspace and the slot reports its gain reduction. No
added latency, no allocation. This is an original bass compressor laid out
after the published controls, not a measured model of the pedal.

## 4. Tight Boost (model 42, `pre.tightboost`)

Reference: Fortin 33 (Fredrik Thordendal signature boost). Public
specification used (Andertons, effectsdatabase listings; the manufacturer's
pages were not reachable):

| Hardware (published) | Chimera |
|---|---|
| Single knob, up to +22 dB of boost; true-bypass footswitch | BOOST 0–22 dB (default 11 dB); the slot bypass |
| More boost adds harmonic content and midrange focus; the low end tightens (maker and user descriptions, no curve published) | As BOOST rises, a 2nd-order low cut moves from 60 to 220 Hz and a 1 kHz bell (Q 0.8) grows to +4 dB; the knob's dB is exactly the small-signal gain at 1 kHz |
| Amp channel-switching remote jack | Not applicable in the plugin |
| Frequency curve, clipping behaviour: **not published** | Chimera choices: the curves above and a soft-knee stage at 4× oversampling, linear below 0.5 FS, slightly asymmetric above |

The oversampling has the same latency as the existing drive stage, so the
slot's fixed latency and every reported latency are unchanged. No
allocation. An original boost laid out after the published behaviour, not a
measured model of the pedal.

## 5. Measurements (`ChimeraPedalExpansionTests`)

- Steady-state sine gain against the analytic response, four settings
  (low +15 dB wide at 35 Hz; mid −15 dB narrow; high +10 dB at 20 kHz,
  clamped, with a 30 dB boost; all bands with a 12 dB boost) at 35 Hz–15 kHz:
  largest error 0.003 dB (44.1 kHz), 0.007 dB (48 kHz), 0.006 dB (96 kHz).
- Centre gains exactly +15 / −15 dB; boost exactly 30 dB; the narrow Q
  changes an octave away at least 3 dB less than the wide one.
- Through the PRE board (owner 2 first in the order): boost 12 dB measures
  11.998 dB.
- Multi Comp static curve (SENS −20 dB; ratio 1, 2.5, 5; gain 0 and 6 dB;
  −40 to −3 dBFS at 1 kHz): exact below threshold (3.7e−7 dB); the measured
  ratio (slope) is exact (2e−6); above threshold the curve sits up to 0.39 dB
  high, because the 4 ms peak detector settles about 0.5 dB below a 1 kHz
  sine's peak.
- Multi Comp modes: a −6 dBFS 60 Hz note with a −36 dBFS 2 kHz overtone, 5:1
  at −20 dB: Normal pulls the overtone to −46.8 dB, Multiband leaves it at
  −36.0 dB while the note's band compresses (−16.7 dB). Balance +6 dB lifts
  the treble band by 6 dB. TubeSim: 2nd harmonic −23.7 dBc at −3 dBFS
  (Normal: none), unchanged at −40 dBFS. Through the PRE board the slot
  reports 7.7 dB of gain reduction on a −6 dBFS tone at the defaults.
- Tight Boost: small-signal gain equals the analytic curve within 9e−7 dB
  (knob 0/11/22 dB, 60 Hz–4 kHz); 80 Hz sits 1.2 dB below 1 kHz at 0 dB and
  21.6 dB below at 22 dB. Largest 2nd/3rd harmonic for a −20 dBFS 1 kHz input:
  −151 dBc at 0 dB, −19.5 dBc at 22 dB.
- 100 parameters (5 owners × (bypass + controls) for Para EQ 11, Multi Comp
  5, Tight Boost 1) at the end of the list, version 16; every released
  parameter keeps its index. A project without them restores their defaults.

## 6. Not done

- Listening on real DI; DAW automation and recall.
- Dedicated pedal artwork (the card uses the generic workbench style).
- The other three Expansion A models (Heavy Menace, Power Wah Boost, Bass
  Synth).
- A mode-change click measurement for Multi Comp (the fade is in place but
  not measured).
