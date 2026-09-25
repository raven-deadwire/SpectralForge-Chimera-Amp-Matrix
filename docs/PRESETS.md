# SpectralForge Chimera — Open Beta 1.0 factory presets

The 31 factory presets are starting points for an instrument DI. The menu groups them by instrument, use and routing. Names identify Chimera's own DSP voicings; the amplifier and effect reference names are design influences, not a claim of an exact circuit or captured model.

Set interface gain first. Aim for normal playing peaks around **−12 to −6 dBFS** with Input at 0 dB; lower interface gain if the input clips. Factory output is usually −9 to −11 dB to leave mix headroom. Pickup output, playing and added gain can still change the final level. Match Output when comparing sounds; the presets are not a promise of identical loudness or a mastering limiter.

## Guitar and texture presets

| Preset | Category | Main sound | Cabinet / use |
|---|---|---|---|
| Clean Sustain | Clean | Glass, optical compression, short plate | Jensen; controlled arpeggios and sustained clean lines |
| Bell Clean | Clean | Chime 30, restrained spring | Jensen; bright clean picking |
| Ambient Clean | Clean | Glass, Dimension, tape delay, hall | Jensen; spacious layered parts |
| Edge Chime | Edge | Chime 30, mild EP Lift | Jensen; touch-sensitive breakup |
| Matchless Edge | Edge | Match Chime (Matchless DC-30 influence), light optical control, spring | Jensen; articulate boutique chime |
| Classic Crunch | Rock | Brit Edge, Treble Lift, short spring | V30; classic rock rhythm |
| Tight Rhythm | High gain | Tight 515, low-cut clean boost | V30; dry, controlled metal rhythm |
| Orange Heavy | High gain | Orange Crown, full low mids | V30; heavy rock and slower riffs |
| Melodic Death Rhythm | High gain | Tight 515, low-drive Green 808, focused upper mids | V30; fast low-tuned rhythm |
| Melodic Lead | Lead | Liquid Lead, dark delay, plate | V30; melodic solos |
| Dumble Smooth Lead | Lead | Silk ODS (Dumble Overdrive Special influence), Gold Drive then boost, dark delay | V30; smooth, expressive lead |
| Filter Lead | Envelope texture | Brit Edge, pick-responsive filter, mild drive | V30; filter lead with a quarter-note delay at the current tempo |
| Fuzz Texture | Fuzz texture | Big Sustain, Glass, chorus, plate | V30; sustaining fuzz layers |

## Bass presets

These presets select **Filters only**, with a 28–35 Hz low cut and an upper cut appropriate to the sound. They do not load a bass speaker capture or depend on an imported IR. A bass IR can be selected afterwards; save a user preset to retain that choice.

| Preset | Category | Main sound | Use |
|---|---|---|---|
| Finger Round | Clean | Bassman Valve, optical levelling | Warm fingerstyle foundation |
| EICH Modern Clean | Clean | Taste Punch (EICH T900 influence), mild VCA | Broad, clear modern bass foundation |
| Pick Punch | Pick | Solid Punch, FET control, upper mids | Clear picked rock bass |
| Slap Studio | Slap | Subway Clean, VCA control, mild low-mid scoop | Slap with controlled peaks |
| Modern Grind | Drive | Modern Bass and Micro Bass | Aggressive upper harmonics with retained lows |
| Vintage Bass DI | Drive | Iron Tube and Bass DI, gentle Variable Mu | Warm rock DI character |
| Wool Bass Fuzz | Fuzz | Wool Bass fuzz into Subway Clean | Gated sustain with a filtered top |
| Bass Envelope | Envelope | Bass Envelope followed by VCA | Pick-responsive sweep with a dry-low blend |

The envelope precedes the compressor so its detector can follow playing dynamics. Reduce **Sense** if the filter opens too easily; increase it for a quieter instrument. Fuzz sustain also depends on the instrument level.

## Dual presets

Dual means **one input signal feeding two amplifier rigs**. “Guitar + Bass” identifies the models used; it does not assign separate guitars and basses to the left and right inputs. The table lists the low-band rig first in Crossover mode.

| Preset | Pair | Routing | Starting point |
|---|---|---|---|
| G+G Clean / Crunch | Glass + Brit Edge | Blend, 35% Rig B | Clean definition with crunch underneath |
| G+G Tight / Wide | Tight 515 + Wide Rect | Blend, 35% Rig B | Tight metal rhythm with a broader second rig |
| G+B Low Anchor | Subway Clean + Tight 515 | Crossover, 220 Hz | Clean bass foundation and guitar-head upper grit |
| G+B Air / Weight | Bassman Valve + Chime 30 | Crossover, 320 Hz | Warm weight below the split and brighter upper articulation |
| B+B Warm / Definition | Bassman Valve + Subway Clean | Blend, 40% Rig B | Round bass body plus clean definition |
| B+B Clean / Grind | Subway Clean + Modern Bass | Crossover, 250 Hz | Clean low B with a driven upper band |

Guitar rigs use the embedded V30 or Jensen IR. Bass rigs use Filters only. Rig levels compensate for the different voicings; adjust the second level and blend to suit the instrument.

## Matrix presets

The LOW band in every factory Matrix preset is a **clean DI foundation**: LOW drive is zero, DI/amp blend is zero, and only the chosen LOW compression is applied. The other two rigs process their respective bands. The visible crossover values describe the input bands; distortion can generate harmonics beyond a band's input boundary.

| Preset | Splits | MID / HIGH models | Use |
|---|---|---|---|
| Bass Matrix | 180 Hz / 1.2 kHz | Modern Bass / Solid Punch | Original three-band bass starting point; cabinets bypassed |
| Low B Foundation | 140 Hz / 1.4 kHz | Modern Bass / Solid Punch | Five- and six-string bass with a restrained top |
| Pick Attack Matrix | 180 Hz / 1.6 kHz | Iron Tube / Modern Bass | Warm mids and clearer pick attack |
| Spectral Texture | 200 Hz / 1.8 kHz | Chime 30 / Orange Crown | Experimental guitar/bass texture with slow phase and hall |

## Recall behavior

Loading a factory preset initializes all sound parameters, then applies its sound: amp and effect models, bypass states, tone controls, cabinet source, routing, crossover, gate, oversampling and output. A delay, fuzz, solo, polarity flip or transpose from the preceding sound cannot stay enabled accidentally. The gain section normally returns to **Fuzz → Boost → Drive**; **Dumble Smooth Lead** selects **Fuzz → Drive → Boost** to place its mild lift after Gold Drive. The detector section returns to **Envelope → Compressor**. A boost before a driven amplifier can still add saturation rather than a pure loudness increase.

Input gain/mode, tempo/host-follow, click, tuner settings and MIDI assignments are preserved. All factory presets turn the doubler and transpose off. **Filter Lead** uses the retained current tempo; other delays use their stored millisecond value. Imported IR assets remain available, but a factory preset never selects User IR. Save a user preset if you want to keep a custom IR, parameter changes and chosen routing.

For compatibility, the original five menu IDs remain Clean Sustain, Tight Rhythm, Bass Matrix, Filter Lead and Fuzz Texture. Their categories can move in the menu without changing their IDs; factory parameter improvements are part of the beta revision. Saved user presets and DAW states store the actual parameters rather than relying on these factory menu IDs.

## Validation scope

`Tests/FactoryPresetTests.h` checks metadata, complete sound initialization, valid effect/model selections, portability of cabinet sources and the clean Matrix LOW contract. Its deterministic plucked-string fixture runs all 31 presets through PRE, amplifiers, embedded cabinet convolution, POST and preset output gain at 44.1 and 48 kHz with unequal host block sizes. It requires finite, audible, unclipped output for that calibrated fixture. This is a regression check; listening with real instruments and adjusting input gain remain necessary.
