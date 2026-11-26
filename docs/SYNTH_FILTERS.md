# SYNTH: two filters and a distortion

Status: implemented on the `claude/synth-filters` branch, stacked on
`claude/synth-wavetable`. Measured by `ChimeraSynthTests` on Linux. Not heard
through a real instrument or a keyboard yet, and not compared with Serum,
ArgWave or any other synth.

## Why

The owner's direction for SYNTH is a synth in the class of Serum or ArgWave 3.
After the wavetable oscillators (docs/SYNTH_WAVETABLE.md) the next gap was the
filter section: one filter, with a drive as the only distortion. This step adds
a second filter, a routing choice and a distortion. The step sequencer, macros,
effect rack, preset browser and wavetable import are not in this change.

## What it is

The voice is now: oscillators, sub and noise, then **filter 1**, **filter 2**
and a **distortion**, then the amp envelope. All three run in every voice.

### Filter 2

- Its own on switch, type, CUTOFF, RES, KEY (key tracking) and ENV 2 amount.
  Off by default.
- Eight types: the six of filter 1 (low-pass 12 / 24, high-pass 12 / 24,
  band-pass, notch) and two combs.
- **Comb + / Comb -:** a feedback comb tuned to the cutoff. Comb + rings at the
  cutoff and its multiples; Comb - at the odd multiples of half of it, a hollow
  tone. RES sets the feedback from 0.40 to 0.98, so a comb is never neutral.
  With KEY at 100 % the comb follows the note (tuned at C4).
- Filter 1 keeps its six types. A longer list would change what its saved
  automation values mean.

### Routing

- **Serial 1 > 2:** filter 1 feeds filter 2.
- **Parallel 1 + 2:** both filters hear the same signal and are mixed. The
  balance bar appears: filter 1 alone on the left, both at full level in the
  centre, filter 2 alone on the right.
- With only one filter on, the routing makes no difference.

### Distortion

- On switch, type, position, DRIVE and MIX. Off by default.
- Six types:

| Type | What it does |
|---|---|
| Soft clip | tanh: rounds the peaks |
| Hard clip | cuts the peaks flat |
| Wavefold | folds the peaks back (triangle fold) |
| Tube | a biased soft clip: even and odd harmonics at any drive |
| Sine fold | a sine shaper: folds smoothly |
| Bit crush | fewer bits: 16 at 0 % down to 3 at 100 % |

- **DRIVE** is 0 to +36 dB into the shaper. Every shape stays inside full
  scale, so more drive is denser, not louder than full scale.
- **Position:** Before filters, After filter 1, After filters. In Parallel,
  "After filter 1" distorts filter 1's branch only.
- It is **per voice**: a chord is distorted note by note and stays clear
  (see Measurements). A distortion after the mix would add the tones between
  the notes; POST has those.
- Filter 1's DRIVE knob is unchanged. It is a soft clip in front of filter 1.

## On the page

The FILTER panel is now FILTERS / DIST.

- Top line: the routing menu and, in Parallel, the balance bar.
- The curve draws both filters: filter 1 in amber, filter 2 in green (only
  while it is on). Each has a handle; a press takes the nearer one. Left /
  right is cutoff, up / down is resonance. A comb draws its teeth.
- One row per filter: its switch and type on the left, its knobs beside.
- Bottom line: the distortion's switch, type, position, DRIVE and MIX bars.
- The curve is lower than before (64 px high at the page's normal size; it was
  about 114), and the filter knobs are smaller than the oscillators' knobs.
  The page is the same size.

## Modulation

The matrix targets Cutoff and Resonance move filter 1, as before. Filter 2
has its own KEY and ENV 2 amount but is not a matrix target, and neither is
the distortion: new targets would change the released target list. This needs
its own step.

Seidr Dynamic opens both filters with the playing level.

## Parameters and compatibility

- Thirteen parameters appended at version hint 39, after the wavetable bank:
  `synthFilter2On`, `synthFilter2Type`, `synthFilter2Cutoff`,
  `synthFilter2Resonance`, `synthFilter2KeyTrack`, `synthFilter2Env`,
  `synthFilterRouting`, `synthFilterBalance`, `synthDistOn`, `synthDistType`,
  `synthDistPlace`, `synthDistDrive`, `synthDistMix`. All automatable.
- Defaults: filter 2 off (High-pass 12 at 200 Hz when switched on), Serial,
  distortion off (Soft clip, Before filters, DRIVE 30 %, MIX 100 %).
- With filter 2 and the distortion off the voice is the released one, sample
  for sample (tested). An older project has no values for them and sounds the
  same.
- No released parameter changed.

## Sound quality and cost

- The distortion is not oversampled. Hard clip, Wavefold, Sine fold and Bit
  crush make harmonics above half the sample rate that fold back; this is
  audible on high notes at high drive. It was not measured. The plugin's
  OVERSAMPLING setting does not cover the synth.
- Tube is biased, which makes DC. A 10 Hz high-pass in each voice removes it
  (measured: 5e-11 left).
- A comb reads its delay with linear interpolation, which dulls the top of
  high-tuned combs slightly.
- Memory: comb delay lines for 20 Hz, per voice and channel (about 0.2 MB at
  48 kHz). Allocated in prepare; nothing is allocated while playing (tested).
- CPU was not measured.

## Not done

- Filter 2 and the distortion as matrix targets.
- More filter types (formant, phaser, ladder models), filter 1 combs.
- Oversampling in the distortion.
- No factory patch uses filter 2 or the distortion.
- DE / KO manual text is not written.

## Measurements

From `ChimeraSynthTests` on Linux (x86-64), 48 kHz, this branch.

- Filter 2 and the distortion off, with all their other settings changed:
  largest sample difference 0 against the released voice.
- Filter 2, each of the eight types, against the curve the page draws
  (55 Hz saw, resonance 30 %, harmonics from 110 Hz to 4 kHz): largest error
  below 0.001 dB. The comb was tested tuned to 480 Hz, a delay of exactly 100
  samples; other tunings read between samples and were not compared.
- Serial against the product of both curves: 8e-7 dB. Parallel: balance 0 is
  filter 1 alone and 100 is filter 2 alone (difference 0); 50 is their sum
  (6e-8).
- Comb + on noise, resonance 90 %: the peak at 262 Hz is 26.6 dB over the dip
  a fifth above. With KEY 100 % and A3 played, 220 Hz is 21.3 dB over 330 Hz.
- Distortion on a sine at A3, drive 70 %, harmonics 2 / 3 relative to the
  fundamental: Soft clip none / -9.7 dB, Hard clip none / -9.6 dB, Wavefold
  none / +13.8 dB, Tube -12.1 / -12.2 dB, Sine fold none / +8.4 dB, Bit crush
  none / -66.2 dB (-37.7 dB at drive 100 %).
- Hard clip at drive 0 % and any type at mix 0 %: difference 0 against off.
- Per voice: A3 + E4 through the hard clipper, the 2 f1 - f2 product is
  -98 dB re f1.
- Position: harmonic 9 with a 400 Hz low-pass after the clipper -71.5 dB,
  before it -20.7 dB.
- Page at 1140 x 438: every filter-panel control is on the page, at least
  30 x 16 px, none overlap; dragging filter 2's handle moves only filter 2,
  as one gesture per parameter.

Screenshot: `docs/evidence/synth-filters-20261126/synth-page-filters.png`.

