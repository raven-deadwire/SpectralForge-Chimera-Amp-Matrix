# PRE Expansion A 1.4 — six models

Status: **all six models implemented on `claude/pre-paraeq`**, covered by
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
- Later expansions append as their own group after this one (Expansion B:
  models 46 on, version hint 17; see `PRE_EXPANSION_B_1_5.md` §1), so they
  never move an Expansion A index.

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

## 5. Power Wah (model 43, `pre.powerwah`)

Reference: Tel-Ray / Morley Power Wah Boost. Public specification used
(Morley's page for the current reissue via search results, Soundhouse,
effectsdatabase; the original manual was not readable from the container):

| Hardware (published) | Chimera |
|---|---|
| Treadle wah and a clean boost, with separate WAH and BOOST switches, usable alone or together | POSITION (treadle, 0–10 on the panel), WAH (default on), BOOST ON (default off) |
| Boost up to 40 dB (Morley's reissue page; other listings give 20 or 60 dB) | BOOST 0–40 dB (default 10 dB) |
| Electro-optical wah control (no pot to wear) | A 25 ms lag on the treadle's log-frequency stands in for the optical response |
| Sweep range, Q, filter type: **not published** | Chimera choices: 0 dB-peak band-pass, Q 4, 350 Hz–2.2 kHz logarithmic; coefficients every 8 samples; wah then boost |

No added latency, no allocation. An original wah and boost laid out after
the published controls, not a measured model of the pedal.

## 6. Bass Synth (model 44, `pre.basssynth`)

Reference: MXR Bass Synth (MB301). Public description used (retailer
listings: Turramusic, Parkway Music; a settings guide at GuitarChalk; no
numeric specification was found and the manufacturer's pages were not
reachable): MIX, SUB MIX, CUTOFF, RESONANCE, ENVELOPE (filter response, from
fast bounce to slow sweeps), MOD, SHAPE (triangle / sawtooth / square) and
VOICE (additional harmonically tuned oscillators). The hardware notes that
fast playing can make tracking glitch.

Everything below the control names is a Chimera design:

| Part | Chimera |
|---|---|
| Tracking | Mono sum, two-pole 600 Hz low-pass, 20 Hz DC block; positive crossings past a ±30 % hysteresis band measure each period (sub-sample interpolated); 30–1000 Hz; a jump is accepted only when two consecutive periods agree (rejects octave errors); single notes only |
| Oscillators | SHAPE triangle / band-limited saw / band-limited square (PolyBLEP); SUB a square one octave down; VOICE adds the octave (×0.5) and fifth (×0.35) above |
| Filter | TPT state-variable low-pass; CUTOFF 100 Hz–4 kHz, RESONANCE Q 0.7–8; ENVELOPE: a note onset opens the filter up to ×9 and it closes in 30 ms–1 s; MOD: 5 Hz LFO, up to ±1 octave |
| Level | Follows the input (2 ms attack, 80 ms release); a gate closes below −50 dBFS; MIX blends dry and synth |

No added latency (the tracker follows about one period behind a note), no
allocation. An original tracking synth with the published controls, not a
model of the pedal's circuit or tracking.

## 7. Dreadful Peril (model 45, `pre.dreadfulperil`)

Reference: Empress Heavy Menace. Public name **Dreadful Peril**, chosen by
the owner (2026-10-27) as words close in meaning to the reference; it
follows the roadmap rule that it never receives the alias Hammer. Public
specification used (Guitar World,
Andertons, wrdtunes listings):

| Hardware (published) | Chimera |
|---|---|
| GAIN, WEIGHT (a high-pass inside the distortion, not a post EQ), OUTPUT | GAIN: drive 6–42 dB; WEIGHT: pre-distortion 2nd-order high-pass 300 Hz → 30 Hz (more weight, more low end in); OUTPUT −24 to +6 dB |
| LOW, MID, HIGH; MID FREQ 200 Hz–2.5 kHz | LOW shelf 100 Hz, MID bell Q 0.8 at 200 Hz–2.5 kHz, HIGH shelf 3 kHz, each ±12 dB, after the distortion |
| Modes Lite(ish) / Heavy / Heavier | LITE: one soft asymmetric stage; HEAVY: plus a cubic stage; HEAVIER: plus a third; 120 Hz couplers with ×4 gain between stages; all at 4× oversampling |
| Adaptive, switchable gate with its own threshold | GATE THRESHOLD −80 to −20 dBFS on the input before the distortion; opens above it, closes 6 dB below after a 10 ms hold, 0.5 ms attack, 40 ms release; GATE on/off |
| Frequency response 35 Hz–17 kHz (−3 dB) | 2nd-order 35 Hz high-pass and 17 kHz low-pass at the output |
| Gate key input, bypass modes | Not applicable in the plugin |

The oversampling matches the existing drive stage's latency, so the slot
and reported latencies are unchanged. No allocation. An original high-gain
distortion laid out after the published controls, not a measured model of
the pedal.

## 8. Measurements (`ChimeraPedalExpansionTests`)

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
- Power Wah: the response peak sits within 0.44 % of the treadle centre at
  five positions, the sweep is monotonic over 350 Hz–2.2 kHz, measured gain
  equals the analytic response within 0.009 dB; BOOST alone is exactly 40 dB,
  WAH and BOOST off is transparent, both combine; the treadle reaches 63 % of
  a jump in 25.0 ms.
- Bass Synth: tracking at 40/50/80/110/200 Hz with a 2nd harmonic at 0.5
  and 0.9 of the fundamental: largest error 0.00006 %, no octave error.
  Harmonics re the fundamental at 100 Hz: square 2nd −39.2 dB, 3rd −9.5 dB;
  saw 2nd −5.9 dB; triangle 3rd −19.2 dB. SUB MIX 1 brings 50 Hz from −120 to
  0 dB; VOICE adds 200 and 150 Hz; closing CUTOFF lowers the 3rd harmonic by
  16 dB. After a note stops the output is exactly silent 357 ms later.
- Dreadful Peril: small-signal gain (LITE, gate off, three EQ / WEIGHT / MID
  FREQ / OUTPUT settings, 60 Hz–8 kHz) equals the analytic path within
  0.00002 dB; full WEIGHT passes 60 Hz at least 10 dB more than none.
  Harmonics 2–5 re the fundamental (−20 dBFS, 200 Hz): LITE gain 0.1
  −36.0 dB, gain 0.6 −13.8 dB, HEAVY −8.6 dB, HEAVIER −8.2 dB (the last two
  differ by only 0.3 dB on this measure: both are deep in saturation).
  Gate at −60 dB: a −75 dBFS hum is held back completely (−40.4 dBFS with the
  gate off); a −20 dBFS note plays at −9.7 dBFS peak and the output is
  below −200 dBFS 200–300 ms after it stops.
- 225 parameters (5 owners × (bypass + controls) for Para EQ 11, Multi Comp
  5, Tight Boost 1, Power Wah 4, Bass Synth 8, Dreadful Peril 10) at the end of
  the list, version 16; every released
  parameter keeps its index. A project without them restores their defaults.

## 9. Not done

- Listening on real DI; DAW automation and recall.
- Dedicated pedal artwork (the card uses the generic workbench style).
- A clearer HEAVY / HEAVIER difference measure (compression, crest factor
  or higher harmonics) and listening to decide the voicing.
- Bass Synth with real bass DI: fast passages, open strings ringing,
  chords (not supported) and low-B tracking.
- A mode-change click measurement for Multi Comp (the fade is in place but
  not measured).
