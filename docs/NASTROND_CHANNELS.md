# Náströnd five-channel high-gain revision

Historical first revision. The later channel-separation and rebuilt-rig update is documented in [NASTROND_CHANNEL_FEEDBACK.md](NASTROND_CHANNEL_FEEDBACK.md).

2026-10-05. Built on `be4df6d8ebe7f28d7e140bc8254963e6dd42f1a7` in draft PR #20.

The owner supplied a strong Dual preset and clarified that this should be an
ordinary operating point, with useful adjustment above it. Fenrir, Surtr,
Níðhöggr, Fimbulvetr and Ragnarök now select five circuits inside Náströnd.
Each channel remembers its own 13 controls in each of the six amp contexts.
Selecting a channel leaves PRE, cabinets, POST and other channel banks alone.
The five full-rig factory examples remain optional ways to start a session.

**Engineering checks and synthetic measurements do not establish hardware
matching, audible high-gain acceptance or final voicing. Those remain pending
recorded instrument-DI comparison and owner listening.**

## Channel behavior and midpoint

| Channel | Character | Circuit changes relative to Fenrir |
|---|---|---|
| Fenrir | Tight rhythm, CLANK | Reference interstage network, stronger noon macro response |
| Surtr | Dense sustain, CRUSH | Earlier serial drive, slightly lower coupling/bandwidth, more supply sag |
| Níðhöggr | Asymmetric decaying grind, ROT | More bias asymmetry, later drive and lower coupling |
| Fimbulvetr | Broad low-mid sustain, BLOOM | Lowest coupling/bandwidth and strongest sag |
| Ragnarök | Heavy transient impact, IMPACT | More late drive, tighter coupling and firmer supply |

All channels retain GAIN, BASS, MID, TREBLE, MID FREQ, PRESENCE, DEPTH, MASTER,
CLANK, CRUSH, IMPACT, ROT and BLOOM. GAIN, MASTER and the five macros start at
5.0; MID starts at 5.5 and MID FREQ at 850 Hz. Noon GAIN maps to the former
maximum preamp drive. Noon MASTER maps to the owner's 0.695 operating level.
The upper GAIN half increases the input and later serial-cell drive; it is
not a post-output volume boost. The macro tapers retain zero and reserve
additional drive, asymmetry and power response above noon. The stage changes
make channels distinct even when all visible controls have the same values.

The stronger noon state exposed excess upper-shelf motion in IMPACT/ROT.
Modern IMPACT therefore emphasizes its 100 Hz shelf and 180 Hz cut; modern
ROT uses a gentler upper shelf. The original response remains frozen for old
sessions. BLOOM still changes coupling, sag and the bounded LF resonator;
CRUSH still controls the serial cells; CLANK still controls input/interstage
tightness. There is no reference-file-specific correction in the DSP.

The uploaded preset is a Dual A/B chain, with Treble Lift at 0.6 and Green
Drive at 0.5 in front. A uses GAIN 1.0, MASTER 0.695 and ordinary macros;
B uses GAIN 0.72, MASTER 0.695, all five macros at 1.0 and MID FREQ 1800 Hz.
Its inactive Classic bank is not used as the target. Factory examples use
the same PRE starting chain and built-in cabinets; the owner's private
embedded IRs are not redistributed. Níðhöggr's example uses Dual A/B.
Factory output trims provide headroom after the drive; they do not reduce
preamp saturation.

## Recall and use

Choose **Náströnd → CHANNEL** to switch among the five channels. Their knobs
are separate memories. **ALL → RESET CHANNEL** restores only the selected
channel's new factory midpoint. For a legacy imported Náströnd preset, its
original channel initially retains the old response and saved values; use
RESET CHANNEL when intentionally adopting the new response. Switching to
another channel immediately accesses that channel's new response.

The 90 existing Original parameters retain their IDs, ordinals and version
hints. Four new 13-control banks plus five internal response flags per context
are appended: 342 parameters. The full session schema is 9; the isolated
Original codec accepts legacy schema 1 and writes schema 2. Binary project
recall and A/B comparison slots retain every selected channel and bank.
Response changes and channel coefficients use the existing 20 ms ramp.

## Measurement and limits

The same SHA-256-pinned 26 NAMs and the unchanged 37-second, 48 kHz / JUCE 4x
protocol are used. All 17 frozen states are rerendered, with 22 additional
states: five channel midpoints, five GAIN maxima, each channel's role macro
at 0/1, and the owner's two isolated amp settings. Cached NAM outputs are
reused only after checking model, renderer, input and output hashes; this
pass does not claim a second fresh NAM inference. No calibration convention
or capture selection changes. Meshuggah still includes cabinets and cannot
establish an amp-only CLANK direction verdict.

The pluck/chord probes informed the adjustments; they are not held-out data.
Spectral distance uses output RMS matching. THD is a harmonic-ratio descriptor,
not a quality or gain score. The owner's full PRE/Dual/IR chain is separately
rendered using a synthetic stimulus; it is not an instrument-DI listening test.

| Macro / channel / reference | Probe | Previous v1 0 → 1 | New channel 0 → 1 |
|---|---|---:|---:|
| CRUSH / surtr | pluck | 15.253 → 7.942 | 14.040 → 5.553 |
| CRUSH / surtr | chord | 10.449 → 3.735 | 8.186 → 3.865 |
| IMPACT / ragnarok | pluck | 9.118 → 7.575 | 5.234 → 5.094 |
| IMPACT / ragnarok | chord | 4.927 → 3.660 | 3.579 → 3.434 |
| ROT / nidhoggr | pluck | 10.895 → 9.470 | 6.276 → 5.331 |
| ROT / nidhoggr | chord | 5.407 → 4.038 | 3.835 → 3.290 |
| BLOOM / fimbulvetr | pluck | 5.733 → 3.768 | 5.637 → 4.229 |
| BLOOM / fimbulvetr | chord | 3.630 → 2.941 | 5.117 → 3.441 |

These compare different operating points: the new channels use stronger noon
settings and channel-specific networks. The old 17 states remain bit-identical.
All 22 selected capture/probe endpoints improve in sign. IMPACT has one
pluck case improving only **0.035 dB**, below the prior pass’s 0.1 dB
direction threshold; **21/22** clear that threshold. It is not an unqualified
macro-direction PASS. Endpoint direction is an observation; intermediate and combined settings are
not asserted to be monotonic. No hardware or listening PASS is assigned.

| Channel | 400 Hz THD at GAIN 5 / 10 | Full owner rig RMS at noon | GAIN 5→10 matched waveform residual, full owner rig |
|---|---:|---:|---:|
| fenrir | 64.67% / 65.00% | -16.611 dBFS | 0.03472 |
| surtr | 62.48% / 62.27% | -16.645 dBFS | 0.02149 |
| nidhoggr | 62.41% / 62.15% | -16.868 dBFS | 0.02958 |
| fimbulvetr | 57.19% / 56.99% | -17.084 dBFS | 0.02287 |
| ragnarok | 65.18% / 64.85% | -16.675 dBFS | 0.03099 |

The original owner chain is -15.971 dBFS RMS on this stimulus.
With its two strong PRE stages and Dual/IR chain, added GAIN above noon changes
the RMS-matched waveform only about 2–4%; this is a limitation, not evidence of
a dramatic audible increase. The isolated core stimulus produces larger
normalized changes (0.498–0.912). Saturation density and playing feel still
require real DI at the owner’s normal input level.

Local regressions: Original core/integration/renderer **3/3**; complete integrated
processor suite **PASS**, including 43 factory recalls, 30 independent Original
banks / 390 controls, exact legacy audio recall, channel-only switching and A/B.
UI checks cover 112 native channel cases and 2,106 control-visibility cases,
all five Original channels at 100%/75%, reset and actual callbacks.
Nominal Original factory peaks are −10.986 to −11.662 dBFS; the +6 dB input
probe remains below −11.095 dBFS. These are synthetic gain-staging checks.

## CI contract correction

The first CI revision (`540ff7c8a4`) passed 18 of 19 Linux CTests. The gate
processor test still expected a total of 3,981 parameters, before the new
342 channel parameters. Its expected total is now 4,323. The frozen 3,686-entry
host contract, gate ordinal 3,890, AU version hint and automation checks remain
strict. This correction changes the regression expectation only; the measured
DSP, channel mappings, UI and artwork remain unchanged. The rebuilt local gate
processor test passes, including the released host contract, state/automation,
all three routing modes, high-gain hiss reduction and POST tails.

## Head artwork

The original ornate Norse head design is retained: serpents, wing/root relief,
bronze borders, thirteen knobs and five channel switches. The square NAM cover
uses the same design with ordinary room lighting and a cabinet background.
Both images are generated artwork, not photographs or proof of physical
hardware. The overly elaborate fantasy backdrop and the later simplified head
are excluded.

The requested photographic reference review used the ordinary gear framing in
[TONE3000's Fortin listing](https://www.tone3000.com/tones/fortin-meshuggah-86564)
and [Orange OR15 listing](https://www.tone3000.com/tones/orange-or15-amp-head-39238).
Their photos are not copied into the repository. Assets:
`Assets/Artwork/anastrond.png` and `Assets/Promo/nastrond-nam-cover.png`.

## Reproduce

Build `ChimeraOriginalAmpTests`, `ChimeraOriginalAmpIntegrationTests`,
`ChimeraOriginalAmpRender`, `ChimeraIntegratedProcessorTests` and `ChimeraUITests`.

```sh
ctest --test-dir build -R Original --output-on-failure
python Tools/compare_original_nam.py \
  --models /private/Nastrond-20261005 \
  --manifest docs/reference/nastrond-nam-manifest.json \
  --nam-render /path/to/nam-core/build/tools/render \
  --original-render /path/to/ChimeraOriginalAmpRender \
  --states docs/evidence/nastrond-channels-20261005/channel-states.json \
  --out /private/channel-measurements --workers 3
ChimeraIntegratedProcessorTests --owner-original-reference /private/owner.chimera /private/owner-renders
```

The last command requires the owner's private preset and does not fetch or
publish its IRs. Full local evidence is in
[evidence/nastrond-channels-20261005](evidence/nastrond-channels-20261005).
