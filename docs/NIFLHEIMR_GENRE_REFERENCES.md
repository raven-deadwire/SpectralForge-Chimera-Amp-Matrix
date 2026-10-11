# Niflheimr genre references and original rigs

Research date: 2026-10-07. Scope: the five original bass channels and their
complete factory rigs. The owner's listening result is authoritative: Modern
Tight and Industrial Bite were accepted; Death Grind, Slam Impact and Sludge
Mass required revision because they blurred the notes.

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

Five entries are appended at preset indices 43-47; all previous 0-42 identities
and display navigation remain intact. The actual amp still has five channels:
these are optional complete rigs around those channels, not substitutes for
the channel selector. Full-rig preset names differ from the native channel
names; the historical serialized preset IDs and indices remain unchanged.
Each rig explicitly sets all 14 active native controls,
PRE, cabinet filters, POST, gate and output, so an earlier rig cannot leak in.

| Full-rig preset | Native amp channel (index) | PRE | Amp BLEND | Amp OUT dB | Cabinet HP / LP | POST / space |
|---|---|---|---|---|---|---|
| Frostline Precision | Hrímfaxi (0) | Bass Comp, Graphic EQ | 66% amp | +7.5 | 30 Hz / 6.7 kHz | Console Four contour; dry |
| Carrion Barrage | Garmr (1) | Graphic EQ, Bass Comp | 72% amp | -2.0 | 32 Hz / 5.9 kHz | Console Four midrange; dry |
| Foundry Pulse | Nidavellir (2) | Parallel Studio FET, Graphic EQ | 70% amp | +1.0 | 32 Hz / 6.1 kHz | Console Four pulse definition; dry |
| Jötunn Hammer | Ymir (3) | Bass Comp, Graphic EQ | 66% amp | +1.0 | 27 Hz / 5.1 kHz | Console Four impact contour; dry |
| Mirebound Monolith | Hel (4) | Gentle Diamond, Graphic EQ | 73% amp | -4.5 | 28 Hz / 3.9 kHz | Dark Passive Tube contour; dry |

These are authored starting values. Each channel also retains its internal
protected LOW path; BLEND is not a simple dirty-band-only percentage. PRE uses
compression and EQ, not full-band drive ahead of the amp's clean branch.
Compressor positions are Chimera parameter positions, not copied artist
settings or claims of hardware time-constant calibration. Global **OUTPUT**
still recalls at **0 dB**. The visible native **Amp OUT** trims apply linear
makeup after distortion, targeting approximately **-24.5 dBFS RMS** on the
48 kHz synthetic bass-pluck fixture. This is a synthetic level-consistency
target, not perceived-loudness matching or musical acceptance; complete-rig
headroom is also checked with the fixture input raised by 6 dB.

The bundled IRs are guitar cabinets. These bass rigs deliberately use the
available **Filters only** cabinet mode, without resolving a personal path or
silently substituting a V30. A user may select a suitable bass IR after recall.
Factory presets and `.chimera` exports use the same production state path.
User-added IRs can now be classified as Bass, Guitar or Unspecified and removed
from the list without changing the source file or already-loaded session audio.

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
the owner's real bass DI, level-matched comparison, suitable bass IR when one is
available, and listening in a dense mix. Include low B shifted down two semitones,
fast repeated notes, strong/soft attacks, dyads and long decays. Keep the protected
low fundamental audible while evaluating each channel's distinct texture.
