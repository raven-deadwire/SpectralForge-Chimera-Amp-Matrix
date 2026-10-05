# Náströnd channel character and full-rig revision

2026-10-05. Initial revision based on PR #20 head `e8f3887`;
level/clarity follow-up based on `5e65009`.

## Level and clarity follow-up

Every one of the 43 factory entries now recalls OUTPUT at 0 dB. This means
unity gain at the output knob, not normalization to 0 dBFS. Existing saved
projects retain their stored controls; recall loads the revised factory values.

Fenrir's DSP is unchanged. Surtr / Níðhöggr / Fimbulvetr / Ragnarök receive
+3 / +2.5 / +8 / +8 dB at the modern channel output stage. The same bundled
V30 and default knobs produce -16.75 / -16.71 / -16.59 / -16.67 / -17.80 dBFS
RMS: a 1.21 dB spread. The common hot PRE/POST rig spans 1.83 dB.
The extreme-output soft rail remains in place; preamp gain is not reduced.

Fimbulvetr retains its slower supply response, while wider interstage
bandwidth, less low-mid emphasis and less treble attenuation reduce the
muffled balance. Paired 4x renders show 4.25–6.93 dB more 1.5–4 kHz energy
relative to 200–600 Hz across six synthetic probes. Fenrir's six renders are
byte-identical to `5e65009`. The modern channel pair residual stays above
0.416; these are synthetic measurements, not a listening approval.

Requested starts: Thall Rhythm BLOOM 7.5; Molten Lead ROT 7.5;
Slam Impact Low DI Comp 0.5 and DI/AMP mix 75%.
Thall/Slam POST VCA makeup is now 0 dB to leave space for the stronger
channel levels; Rotten Grind uses a visible -1.5 dB POST EQ output level.

Ambient Clean keeps the low-drive Glass amp and receives an enabled POST
Console VCA: threshold -24 dB, 2:1, attack 10 ms, release 0.6 s, makeup
+12 dB. With OUTPUT 0 dB, it measures -25.70 dBFS RMS / -10.83 dBFS peak;
the +6 dB input probe peaks at -7.78 dBFS. Its previous RMS was -25.99 dBFS
with OUTPUT +11.5 dB. Dimension, tape delay and hall remain in the rig.

Other quiet rigs gain level at their visible native amp output: Clean Sustain
+6 dB, Bass Envelope +7 dB, G+G Clean/Crunch +9 dB, Modern Clean +4 dB.
Peak-heavy bass rigs reserve headroom there: Finger Round -4 dB, Modern
Grind -2 dB, Vintage Bass DI -4 dB, G+B Air/Weight -2 dB and B+B Clean/Grind
-1 dB. Wild Hunt uses -3.5 dB at the visible POST EQ output. These are
specific stage adjustments, not the previous master trim table relocated.
The high-gain presets keep their PRE and amp drive settings.

Regression checks require all 43 recalls at OUTPUT 0 dB, nominal and +6 dB
input peaks below 0.95 linear, RMS above -30 dBFS, the requested knob values,
and cabinet-referenced channel spreads below 2 dB bare / 2.5 dB driven.
The initial revision measurements below are historical and precede this
louder unity-output follow-up.


The owner reported that the five channels were too similar, asked for a
clear channel-knob policy, and requested replacement PRE / amp / POST presets.
This revision keeps the existing channel banks and replaces the five menu
examples in place (indices 38–42). User-saved presets are not removed.

## Channel character

The modern response now varies stage-drive distribution, interstage coupling,
pre-distortion emphasis, bias/recovery, power-supply timing, feedback,
transient response and fixed tone contours. Channel output trims follow the
nonlinear stages so level alignment does not weaken the drive.

| Channel | Intended distinction |
|---|---|
| Fenrir | Lean lows, pronounced pick bite, quick recovery |
| Surtr | Forward singing mids, rounded top, dense compression |
| Níðhöggr | Uneven clipping stages, asymmetric decay, coarse grind |
| Fimbulvetr | Broad low mids, slower supply recovery, softer attack |
| Ragnarök | Firm supply, late-stage saturation, deep transient hit |

All 13 controls remain available. The existing 5.0 gain/master starting point
and adjustment above noon remain. No host parameter ID or ordering changes.
Each channel retains its own last settings. First visit uses defaults;
returning restores that channel's edits. RESET CHANNEL remains explicit.
A CHANNEL tooltip and the EN/KO/DE manual explain this behavior.

## Owner reference and replacement rigs

The two new owner-supplied files have identical parameter values except mode:
reference 1 uses Matrix and reference 2 uses crossover Dual. Their active PRE
is TS808 (drive 0.5, tone/level 1.0) then Diamond Compressor with hi-cut enabled;
Rangemaster is bypassed. POST is Console VCA, Iron Colour and Console Four EQ.
The EQ cuts 80 Hz by 8.89 dB and 600 Hz by 3.36 dB, and boosts 2 kHz by 6.02 dB.
Delay/reverb are off. Matrix divides at 350/1200 Hz with Fenrir / Fimbulvetr /
Ragnarök; Dual divides at 350 Hz with Fenrir / Ragnarök. Most active gain/macros
are at maximum. This informed the hotter dry rigs and sharper POST contour,
while individual channels retain distinct adjustment ranges.

| Preset | Routing | PRE | POST |
|---|---|---|---|
| Thall Rhythm | Fenrir Classic | Hot TS808 → Diamond | Console VCA + focused Console Four EQ, dry |
| Molten Lead | Surtr Classic | Cali76 → Klon | Inductor EQ, short delay and plate |
| Rotten Grind | Níðhöggr / Ragnarök crossover Dual | OCD → Diamond | Iron Colour + sculpted Console Four EQ, dry |
| Sludge Mass | Fimbulvetr Classic | Tone Bender → Graphic EQ | Passive Tube EQ + small hall |
| Slam Impact | Fenrir / Fimbulvetr / Ragnarök Matrix | Hot TS808 → Diamond; LOW taps after TS808 | Console VCA + Iron Colour + reference POST EQ, dry |

These are adaptations, not exact copies: factory rigs use the bundled V30
cabinet and measured output trims. The private embedded IRs and full uploaded
presets are not added to the repository or redistributed.
The frozen v1 snapshots remain only as compatibility/measurement fixtures.

## Verification

Paired before/after JUCE 4x renders use the same wrapper/compiler, five channel
midpoints, six synthetic probes (pluck/chord/generic pre-drive at two levels),
and all ten channel pairs. RMS normalization removes overall loudness from
the pairwise comparisons. The minimum waveform residual increased from
0.0891 to 0.4304. The minimum 14-band spectral distance increased from
0.211 to 2.780 dB; the mean increased from 2.646 to 7.347 dB.
See [measurement data](measurements/nastrond-channel-revision/channel-separation.json)
and `Tools/measure_original_channel_separation.py` for the method.

The owner's uploaded Dual chain was also rendered privately, retaining its
PRE/POST/cabinets and setting both amp lanes to each channel's midpoint.
Its minimum RMS-matched pairwise residual is 0.5081 with the revised DSP.
This is a synthetic full-chain check, not a listening comparison.

Seventeen legacy v1 states produce byte-identical before/after JUCE renders.
Core and wrapper checks pass across 12 sample-rate/oversampling routes,
including simultaneous control changes with zero audio-callback allocations.

All 43 factory entries pass the synthetic gain/recall bank checks. The five
new rigs measure -25.93 to -23.35 dBFS RMS with 10.85–14.61 dB nominal peak
headroom; the +6 dB input probe remains below -10.26 dBFS peak.
Their binary state restores are sample-identical within 1e-6, A/B retains the
authored gain, and 30 independent channel banks / 390 knobs pass recall.
See [factory levels](measurements/nastrond-channel-revision/factory-levels.json).

These measurements show separation on the stated fixtures; they do not
establish preferred tone, real-instrument acceptance, NAM matching or hardware
fidelity. Final voicing remains an owner listening decision.
