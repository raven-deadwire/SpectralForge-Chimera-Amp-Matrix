# Niflheimr genre references and original rigs

Research date: 2026-10-07. Scope: the five original bass channels and their
complete factory rigs. The owner's listening result is authoritative: Modern
Tight and Industrial Bite were accepted; Death Grind, Slam Impact and Sludge
Mass required revision because they blurred the notes.

Full-rig routing revision: 2026-10-12. The configurations below describe the
current authored source. For native recall/audio test status and level-calibration
evidence for these revised routes, see the
[revision validation report](CAB_ROUTING_AND_NIFLHEIMR_20261012.md#verification)
and [PR #54](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/pull/54).
The historical channel acceptance above does not approve the new full rigs.

## Channel identity

| Index (unchanged) | Display name | Previous role | Design target |
|---|---|---|---|
| 0 | Hrímfaxi | Modern Tight | Precise contemporary metal, stable lows and clear syncopation |
| 1 | Garmr | Death Grind | Coarse serial grind with intelligible fast fingerstyle |
| 2 | Nidavellir | Industrial Bite | Hard-edged, controlled industrial pulse |
| 3 | Ymir | Slam Impact | Dense brutal-death/slam distortion and a percussive first hit |
| 4 | Hel | Sludge Mass | Dark asymmetric fuzz, slow recovery and sustained weight |

Serialized keys and parameter ordinals are unchanged. Skadi is excluded because
the owner already uses that name for a bass design. Hrímfaxi and Nidavellir's
accepted channel algorithms are unchanged; their optional full-rig presets
are separately authored configurations.

## What the artists actually described

The following sources are direct musician interviews, interview-host reports,
or manufacturer documentation. These are historical practices from the cited
source, not assertions about the artists' current touring rigs. No recordings
were downloaded, analyzed or used as a hidden training target.

### Hrímfaxi: contemporary technical/progressive metal

- [Amos Williams / TesseracT, 2023 interview](https://www.guitarworld.com/features/how-amos-williams-found-his-sound-with-tesseract):
  describes a parallel distortion branch high-passed between 400 and 600 Hz
  and low-passed between 2 and 5 kHz. His live chain separates input control,
  EQ/compression, parallel overdrive, amp/cab and final tone shaping. Percussive
  dead notes and precise unison parts matter to the musical role.
- [Adam "Nolly" Getgood / Periphery, 2012 interview](https://www.notreble.com/buzz/2012/12/20/getting-heavier-an-interview-with-peripherys-adam-nolly-getgood/):
  describes low-F-sharp overdrive becoming fuzzy and losing definition, and
  uses a controlled distortion low end with clean blending. His then-current
  Axe-Fx configuration split and recombined clean and distorted SVT paths.

**Authored preset inference:** retain the accepted amplifier algorithm, keep
sub weight stable, use restrained compression,
and use upper-mid definition without a wide treble/fizz boost. The published
filter range describes Williams's parallel distortion, not a filter to apply
to the entire bass signal or an exact Niflheimr knob equivalent.

### Nidavellir: industrial metal

- [Paul D'Amour / Ministry, 2024 interview](https://www.guitarworld.com/features/paul-d-amour-tool-ministry):
  describes supporting samples and layered arrangements, and names Harmonic
  Booster and Microtubes X as central tone elements, including control of the
  low end. His amplifier brand is not adopted as a Niflheimr modeling target.
- [Paul Barker, Sound On Sound, February 1993](https://www.muzines.co.uk/articles/ministry-of-sound-ideas/10503?theme=1):
  the authorized archive reproduces his discussion of tight unison parts and
  avoiding clashes between bass, drums and vocals. The sub part discussed for
  "Just One Fix" uses an Oberheim OB8 synthesizer; it is not evidence of an
  enormous bass-amplifier low shelf.

**Authored preset inference:** preserve the accepted bite engine, shape a
repeatable dry pulse and forward midrange, and control excessive sub energy.
Do not infer synthesizer layers from a bass-only amp path. Optional full-rig
compression and tone contour must not replace the established channel voice.

### Garmr: death metal and grindcore

- [Alex Webster, No Treble, 2014](https://www.notreble.com/buzz/2014/03/06/profiles-in-tone-alex-webster/):
  too much overdrive can obscure fingerstyle notes. He describes reducing the
  level into a B3K, taking separate clean and distorted DI feeds, and keeping
  low-frequency weight in the clean feed while the dirty feed supplies growl.
- [Shane Embury, Gitarre & Bass](https://www.gitarrebass.de/stories/shane-embury-napalm-death-der-blick-aufs-grosse-ganze/):
  even distortion-led grindcore needs a balanced clean/overdriven recording.
  He describes SansAmp pedal and rack use, Ampeg live amplification, and
  prioritizing rhythmic accuracy while tracking.
- [Webster on Violence Unimagined, Bass Player/Guitar World](https://www.guitarworld.com/features/cannibal-corpse-alex-webster-5-best-bass-albums):
  clean DI and B7K were supplied for recording; Erik Rutan reamped using a
  1971 Ampeg and Microtubes X7. This is not evidence that Webster used X900,
  or that the record's control values are known.

**Authored design inference:** retain strong serial asymmetric saturation but
reduce low-frequency drive and detector pumping before distortion. Keep the
clean low anchor, midrange grind and audible onset separate. A quieter or
cleaner version alone does not satisfy the brief.

### Ymir: slam and brutal death metal

- [Chris Andrews, Devourment interview, 2008](https://slam-minded.blogspot.com/2008/02/interview-with-chris-of-devourment.html):
  describes striking the strings with his fingers, substantial distortion and
  chord playing to give the band maximum thickness. His stated influences
  include Dan Lilker and Peter Steele. The interview does not identify his
  amplifier, a crossover frequency or a blend percentage.
- [Derek Boyer, Eternal Terror, 2010](https://eternal-terror.com/2010/04/18/derek-boyer-suffocation-swat-it-like-you-mean-it/):
  prioritizes punch, glassy attack and a percussive connection to the drummer.
  This is a brutal-death articulation reference alongside Devourment's direct
  slam example; the genres are not treated as identical.
- [Boyer, Seymour Duncan interview](https://www.seymourduncan.com/blog/artist/voices-of-metal-derek-boyer-of-suffocation):
  describes a SansAmp DI feeding compressed lows and a Sennheiser 421 cabinet
  microphone carrying finger attack and dynamics. Speaker-size preferences in
  that interview are personal preferences, not a general physical rule.

**Authored design inference:** keep dense hard/round parallel saturation and
strong transient transmission. Reduce the bass-driven envelope suppression
that smears adjacent hits; do not remove the distorted mass that differentiates
slam from a clean technical-metal sound. Include dyads/chords as well as single
notes in subsequent real-DI acceptance.

### Hel: sludge and doom

- [Damnation Audio's original Bloop documentation](https://damndamndamn.bigcartel.com/product/true-bypass-looper-clean-blend-pedal):
  developed for Bongripper's Ron Petzke to restore clarity in a dense mix,
  rather than to remedy insufficient bass quantity. A full-range distortion
  loop is mixed with a clean-low path; its documented clean-low rolloff begins
  around 1 kHz and a polarity switch addresses cancellation.
- [Dixie Dave Collins / Weedeater, Bass Nerds host report, 2025](https://www.notreble.com/buzz/2025/12/26/bass-rig-breakdown-dixie-dave-collins-of-weedeater-on-volume-weight-and-everything-on-10/):
  describes two Sunn heads, separately powered 2x15 cabinets, a Morley JD 110
  preamp and very dark settings. The head's exact Sunn model is not established
  by this source. Old strings and instrument setup also contribute to the
  result; they cannot be replicated by an EQ value alone.

**Authored design inference:** preserve the asymmetric body fuzz and original
30 ms / 240 ms envelope timing while protecting the low anchor. Darker upper
bandwidth and sustained midrange density remain intentional. Cleaner gaps
between notes must not turn Hel into another fast modern channel. Neither
Weedeater's extreme hardware settings nor Bloop's 1 kHz rolloff are copied
literally into Chimera.

## Original / Niflheimr full-rig bank

The five entries remain at preset indices 43-47; previous 0-42 identities and
category navigation remain intact. The 2026-10-12 revision gives them direct
genre/use names and two Classic, one Dual and two Matrix arrangements. The amp
still has five independently selectable channels. A full-rig name describes
the complete configuration, and does not replace a native channel name.

| ID | Current full-rig preset | Previous display name | Routing and channel roles |
|---|---|---|---|
| 43 | Progressive Clank | Frostline Precision | Classic: Hrímfaxi (0), full range |
| 44 | Death Grind | Carrion Barrage | Dual crossover at 180 Hz: restrained Hrímfaxi (0) below the split, Garmr (1) above |
| 45 | Industrial Pulse | Foundry Pulse | Matrix at 140 / 1600 Hz: clean LOW DI, Nidavellir (2) MID, Hrímfaxi (0) HIGH |
| 46 | Slam Punch | Jötunn Hammer | Matrix at 110 / 900 Hz: Ymir (3) LOW and MID, Garmr (1) HIGH |
| 47 | Doom Fuzz | Mirebound Monolith | Classic: Hel (4), full range |

Every lane in the selected routing mode explicitly recalls its native model,
channel and all 14 native controls in the matching Classic/Dual/Matrix bank.
PRE, modeled cabinet, POST, gate, crossovers and output are also authored.
Changing only the mode would leave unrelated native banks in use; this builder
sets the banks for the actual routing contexts.

### Lane and cabinet settings

All active cabinets are Chimera's own bass models. The active microphone is
Mic A; the alternate Mic B is stored but disabled, with the mic blend at A.

| Preset / lane | Channel | Cabinet · speaker · Mic A | Native BLEND | Native Amp OUT | Mic A level |
|---|---|---|---:|---:|---:|
| Progressive Clank / full range | Hrímfaxi | Bass 4x10 · Vector 10 · Anchor 20 | 66% | +7.5 dB | +4.8 dB |
| Death Grind / below 180 Hz | Hrímfaxi | Bass 2x12 · Chimera Depth 12 · Anchor 20 | 28% | +5.0 dB | +4.0 dB |
| Death Grind / above 180 Hz | Garmr | Bass 6x10 · Foundry 10 · Hammer 421 | 72% | −2.0 dB | +4.4 dB |
| Industrial Pulse / LOW | Hrímfaxi, parked | Bass 2x12 · Chimera Depth 12 · Anchor 20, parked; AMP and CAB off | 28%, stored | 0 dB, inactive | 0 dB, inactive |
| Industrial Pulse / MID | Nidavellir | Bass 4x10 · Alloy 10 · Spear 441 | 70% | +1.0 dB | +5.6 dB |
| Industrial Pulse / HIGH | Hrímfaxi | Bass 1x12 · Clarity 12 · Prism 414 | 65% | +3.0 dB | +4.0 dB |
| Slam Punch / LOW | Ymir | Bass 6x10 · Foundry 10 · Anchor 20 | 30% | +3.0 dB | +3.3 dB |
| Slam Punch / MID | Ymir | Bass 6x10 · Foundry 10 · Anchor 20 | 66% | +1.0 dB | +3.3 dB |
| Slam Punch / HIGH | Garmr | Bass 4x10 · Alloy 10 · Spear 441 | 68% | −2.0 dB | +2.5 dB |
| Doom Fuzz / full range | Hel | Bass 1x15 · Monolith 15 · Anchor 20 | 73% | −4.5 dB | +10.1 dB |

The native **BLEND** column is internal to each Niflheimr channel; its protected
LOW is common to the native clean and driven signals. It is separate from the
Matrix **LOW DI / AMP + CAB** blend. Industrial Pulse sets the latter to
**100% DI / 0% AMP + CAB**, with LOW compression at **0.25**. Its parked native
channel and cabinet remain editable but contribute no sound. Slam Punch sets
**75% DI / 25% AMP + CAB**, with LOW compression at **0.50**. Its LOW cabinet
affects only the 25% amplified share. Both Matrix clean taps follow the first
PRE compressor, before the Graphic EQ; the MID/HIGH input uses the complete
PRE chain. Death Grind uses crossover Dual, so the stored 50% parallel blend
does not weight its complementary low/high bands.

| Full-rig preset | PRE | Cabinet HP / LP | POST / space |
|---|---|---|---|
| Progressive Clank | Bass Comp, Graphic EQ | 30 Hz / 6.7 kHz | Console Four contour; dry |
| Death Grind | Graphic EQ, Bass Comp | 32 Hz / 5.9 kHz | Console Four midrange; dry |
| Industrial Pulse | Parallel Studio FET, Graphic EQ | 32 Hz / 6.1 kHz, stored but inactive on LOW | Console Four pulse definition; dry |
| Slam Punch | Bass Comp, Graphic EQ | 27 Hz / 5.1 kHz; LOW cuts affect its amplified share | Console Four impact contour; dry |
| Doom Fuzz | Gentle Diamond, Graphic EQ | 28 Hz / 3.9 kHz | Dark Passive Tube contour; dry |

PRE uses compression and EQ without a full-band drive before the native clean
branch. Compressor positions are Chimera parameter positions, not copied
artist settings or hardware time-constant measurements. Global **OUTPUT**
recalls at **0 dB** in all five presets. The native Amp OUT and microphone
levels above are visible authored makeup after distortion. Legacy lane LEVEL
remains 0 dB; native processing does not use it for level correction.

The stored captured-source selectors use ordinal 0, labeled **Filters only**
when that source is selected. Those selectors are inactive while a modeled
microphone owns the path. They do not enable an extra Filters-only source
alongside the modeled cabinet. Cabinet HP/LP cuts remain the explicit cut
controls of that cabinet path. No factory recall resolves a personal IR path,
loads a guitar IR in place of a bass model, or requires an external bass IR.
Selecting an IR later is an explicit source change. Factory presets and
`.chimera` exports use the production state path.

### Compatibility and verification

The stable IDs remain `original.rig.niflheimr.hrimfaxi.v1`,
`original.rig.niflheimr.garmr.v1`, `original.rig.niflheimr.nidavellir.v1`,
`original.rig.niflheimr.ymir.v1` and `original.rig.niflheimr.hel.v1`.
Only a new factory recall selects the revised arrangements and display names.
Loading an existing project, user `.chimera` file or A/B snapshot restores its
stored parameters and metadata, including its previous Classic route, output
trim and old display name. It does not reauthor the saved sound from its ID.

The revised-route tests are defined in
[NiflheimrPresetTests.h](../Tests/NiflheimrPresetTests.h). They check contaminated
state recall, the correct native contexts and all active controls, an audible
contribution from each route, the inactive Industrial LOW hardware, binary
restore, A/B audio equality and legacy Classic snapshot preservation. The
unchanged production-fixture limits are **RMS > −30 dBFS**, **peak < 0.95** at
both nominal and +6 dB input, with **OUTPUT = 0 dB**. Consult the
[revision validation report](CAB_ROUTING_AND_NIFLHEIMR_20261012.md#verification)
for source-specific native test results and level measurements. Earlier five-Classic measurements in
[Factory CAB voicing — historical calibration](FACTORY_CAB_VOICING_1_3.md#historical-local-calibration--2026-10-09)
are retained as history; they are not measured level matches or acceptance
evidence for the new Dual/Matrix arrangements.

## Technical cross-check

[Darkglass X900's official feature reference](https://www.darkglass.com/products/x900)
documents compressed clean lows, distorted highs, a pre-distortion HPF from
100 Hz to 1 kHz and a clean-path LPF from 50 to 500 Hz. It explicitly associates
lower drive HPF settings with thicker/fuzzier saturation. This supports testing
the amount of low energy entering the nonlinear path. It is a structural
reference, not a claim of X900 circuit fidelity or proof that each artist used it.

## Acceptance boundaries

All Chimera control values, EQ curves, filter cutoffs and envelope adjustments
are authored design choices. Unpublished artist settings are not inferred or
presented as measurements. The presets are original rigs, not artist signatures
or verified album-tone recreations. No Ashdown design or rejected Avalanche
capture is used as a model target.

The core regression verifies fundamental retention, intermodulation, transient
behavior and distortion strength using synthetic signals. The accepted-channel
comparison checks sample-identical behavior at varied rates and moving controls.
Those checks do not establish musical acceptance. Final acceptance still needs
the owner's real bass DI, level-matched comparison through the modeled bass
cabinets, and listening in a dense mix. A separate user-IR comparison is optional.
Include low B shifted down two semitones,
fast repeated notes, strong/soft attacks, dyads and long decays. Keep the protected
low fundamental audible while evaluating each channel's distinct texture.
