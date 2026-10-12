# SpectralForge Chimera — factory and signature presets

The 48 factory presets are starting points for an instrument DI: 31 original presets, three bass signatures, four guitar signatures, five Náströnd presets and five Niflheimr presets. Their stored ordinals and category navigation remain unchanged; built-in display names now consistently use Chimera naming. ID30 is displayed as Modern Clean. The menu uses a fixed taxonomy: Guitar (Clean & Ambient / Edge & Rock / High Gain / Lead & Texture), Bass (Clean & Dynamics / Drive & Texture), Dual (Blend / Crossover), and Matrix (Bass / Experimental). Names identify Chimera's own DSP voicings and intended musical roles.

Set interface gain first. Aim for normal playing peaks around **−12 to −6 dBFS** with Input at 0 dB; lower interface gain if the input clips. In 1.3, all factory presets start with global **OUTPUT at 0 dB**. Their cabinet microphone levels provide the measured CAB compensation without changing the authored amp drive. Pickup output, playing and added gain can still change the final level. Match Output when comparing sounds; the presets are not a promise of identical loudness or a mastering limiter.

Every active factory CAB path now uses a modeled cabinet, compatible speaker and microphone. Factory recall does not load the two embedded IRs or search for private capture files. The six intentionally dry Matrix LOW paths retain CAB bypass. Use CABINET to inspect the stored cabinet and microphone settings for each preset. User-imported IRs remain available for manual selection and saved-project recall; see [IR distribution](IR_DISTRIBUTION.md).

## Guitar presets

| Preset | Category | Main sound | Cabinet / use |
|---|---|---|---|
| Clean Sustain | Clean & Ambient | Glass, optical compression, short plate | 1×12 / Silver 12; controlled arpeggios and sustained clean lines |
| Bell Clean | Clean & Ambient | Chime 30, restrained spring | 2×12 / Silver 12; bright clean picking |
| Ambient Clean | Clean & Ambient | Glass, Spatial Chorus, tape delay, hall | 2×12 / Silver 12; spacious layered parts |
| Edge Chime | Edge & Rock | Chime 30, mild Echo Lift | 2×12 / Verdant 25; touch-sensitive breakup |
| Prism Edge | Edge & Rock | Prism Chime, light optical control, spring | 2×12 / Verdant 25; articulate boutique chime |
| Classic Crunch | Edge & Rock | Brit Edge, Treble Lift, short spring | 2×12 / Verdant 25; classic rock rhythm |
| Tight Rhythm | High gain | Tight 515 RHYTHM 7.6 / POST 6.3, BRIGHT/CRUNCH, Optical, Green Drive 10/10/10 | 4×12 / Ember 30; dense, controlled metal rhythm |
| Amber Heavy | High gain | Amber Crown DIRTY 7.6, Studio FET, Treble Lift, Green Drive 10/10/5 | 4×12 / Crimson 12; dense heavy rhythm |
| Melodic Death Rhythm | High gain | Tight 515 LEAD 8.6 / POST 4.0, Variable Mu, Treble Lift, Green Drive 3/6/5 | 4×12 / Ember 30; saturated low-tuned rhythm |
| Melodic Lead | Lead & Texture | Liquid Lead, dark delay, plate | 2×12 / Ember 30; melodic solos |
| Silk Smooth Lead | Lead & Texture | Silk Lead, Gold Drive then boost, dark delay | 2×12 / Ember 30; smooth, expressive lead |
| Filter Lead | Lead & Texture | Brit Edge, pick-responsive filter, Treble Lift and stronger Green Drive | 2×12 / Ember 30; saturated filter lead with tempo-following delay |
| Fuzz Texture | Lead & Texture | Micro Lift before Big Sustain, Glass, chorus, plate | 4×12 / Crimson 12; sustaining fuzz layers |

## Bass presets

These presets select modeled bass cabinets, with the authored low/high cuts retained for each sound. Finger Round and Vintage Bass DI use 1×15, Modern Clean uses 1×12, Pick Punch and Slap Studio use 4×10, Modern Grind and Wool Bass Fuzz use 6×10, and Bass Envelope uses 2×10. They do not depend on an imported IR. A user IR can be selected afterwards; save a user preset to retain that choice.

| Preset | Category | Main sound | Use |
|---|---|---|---|
| Finger Round | Clean & Dynamics | Vintage Valve, optical levelling | Warm fingerstyle foundation |
| Modern Clean | Clean & Dynamics | Taste Punch, mild VCA | Broad, clear modern bass foundation |
| Pick Punch | Clean & Dynamics | Solid Punch, FET control, upper mids | Clear picked rock bass |
| Slap Studio | Clean & Dynamics | Metro Clean, VCA control, mild low-mid scoop | Slap with controlled peaks |
| Modern Grind | Drive & Texture | Modern Bass and Micro Bass | Aggressive upper harmonics with retained lows |
| Vintage Bass DI | Drive & Texture | Iron Tube and Bass DI, gentle Variable Mu | Warm rock DI character |
| Wool Bass Fuzz | Drive & Texture | Wool Bass fuzz into Metro Clean | Gated sustain with a filtered top |
| Bass Envelope | Drive & Texture | Bass Envelope followed by VCA | Pick-responsive sweep with a dry-low blend |

The envelope precedes the compressor so its detector can follow playing dynamics. Reduce **Sense** if the filter opens too easily; increase it for a quieter instrument. Fuzz sustain also depends on the instrument level.

## Dual presets

Dual means **one input signal feeding two amplifier rigs**. “Guitar + Bass” identifies the models used; it does not assign separate guitars and basses to the left and right inputs. The table lists the low-band rig first in Crossover mode.

| Preset | Pair | Routing | Starting point |
|---|---|---|---|
| G+G Clean / Crunch | Glass + Brit Edge | Blend, 35% Rig B | Clean definition with crunch underneath |
| G+G Tight / Wide | Tight 515 LEAD + Wide Rect CH3/SOLO | Blend, 35% Rig B | Shared Variable Mu, Treble Lift and Green Drive; dense dual rhythm |
| G+B Low Anchor | Metro Clean + Tight 515 | Crossover, 220 Hz | Clean bass foundation and guitar-head upper grit |
| G+B Air / Weight | Vintage Valve + Chime 30 | Crossover, 320 Hz | Warm weight below the split and brighter upper articulation |
| B+B Warm / Definition | Vintage Valve + Metro Clean | Blend, 40% Rig B | Round bass body plus clean definition |
| B+B Clean / Grind | Metro Clean + Modern Bass | Crossover, 250 Hz | Clean low B with a driven upper band |

Each guitar and bass rig uses its assigned modeled CAB, including the 6×10 low anchor and bass-grind paths. Select each rig in CABINET to inspect or adjust its assignment. Rig levels retain the authored blend roles; adjust the second level and blend to suit the instrument.

## Matrix presets

The LOW band in the four original Matrix presets below is a **clean DI foundation**: LOW drive is zero, DI/amp blend is zero, CAB is bypassed, and only the chosen LOW compression is applied. Blackhearted and Dark Matters of Throne retain the same intentional dry LOW role. Other Matrix signatures retain their authored LOW amp blends; Slam Impact has an amplified LOW path. The other two rigs process their respective bands through their assigned modeled CABs. The visible crossover values describe the input bands; distortion can generate harmonics beyond a band's input boundary.

| Preset | Splits | MID / HIGH models | Use |
|---|---|---|---|
| Bass Matrix | 180 Hz / 1.2 kHz | Modern Bass / Solid Punch | Clean LOW with modeled 6×10 MID and 4×10 HIGH |
| Low B Foundation | 140 Hz / 1.4 kHz | Modern Bass / Solid Punch | Five- and six-string bass with a restrained top |
| Pick Attack Matrix | 180 Hz / 1.6 kHz | Iron Tube / Modern Bass | Warm mids and clearer pick attack |
| Spectral Texture | 200 Hz / 1.8 kHz | Chime 30 / Amber Crown | Experimental guitar/bass texture with slow phase and hall |

## Deadwire Signature presets

These three presets are mix-role reconstructions derived from the Deadwire stereo masters, not claims of exact source-track recovery. They use Matrix routing and the production five-slot PRE/native amp/native POST state.

| Signature | Splits | LOW / MID / HIGH | Default modeled CABs: LOW / MID / HIGH |
|---|---|---|---|
| Crom Cruach | 200 Hz / 1.8 kHz | Taste Punch / Modern Bass / Tight 515 | Bass 2×12 / Bass 6×10 / Guitar 4×12 |
| Wild Hunt | 165 Hz / 1.5 kHz | Taste Punch / Solid Punch / Tight 515 | Bass 4×10 / Bass 4×10 / Guitar 4×12 |
| Azhi Dahaka | 135 Hz / 1.2 kHz | Classic Tube / Modern Bass / Night Harvest | Bass 6×10 / Bass 6×10 / Guitar 4×12 |

These signatures no longer search for named external or private IR targets. Their modeled cabinets are available on every installation, with no missing-file fallback. The existing imported files remain in the user's library. Select a personal WAV/AIFF with **IR LIBRARY → OPEN IR** or **ADD FOLDER** and save a user preset to keep a custom capture in that rig.

## Recall behavior

Loading a factory preset initializes all sound parameters, then applies its sound: amp and effect models, bypass states, tone controls, cabinet source, routing, crossover, gate, oversampling and output. A delay, fuzz, solo, polarity flip or transpose from the preceding sound cannot stay enabled accidentally. The gain section normally returns to **Fuzz → Boost → Drive**; **Silk Smooth Lead** selects **Fuzz → Drive → Boost**, and **Fuzz Texture** puts its Micro Lift before the fuzz. The detector order follows the authored recipe. A boost before a driven amplifier adds saturation as well as level.

The original 31 presets and three bass signatures preserve input gain/mode, tempo/host-follow, click, tuner settings and MIDI assignments. Full-rig snapshot banks also restore their authored performance settings, as described below for the guitar signatures. All factory presets turn the doubler and transpose off. **Filter Lead** uses the retained current tempo; other delays use their stored millisecond value. Imported IR assets remain available, but a factory preset never selects User IR. Save a user preset if you want to keep a custom IR, parameter changes and chosen routing. Loading an existing saved project does not apply the new factory CAB recipes to that project.

For compatibility, the original five menu IDs remain Clean Sustain, Tight Rhythm, Bass Matrix, Filter Lead and Fuzz Texture. Their categories can move in the menu without changing their IDs; factory parameter improvements are part of the beta revision. Saved user presets and DAW states store the actual parameters rather than relying on these factory menu IDs.

## Validation scope

`Tests/FactoryPresetTests.h` retains the original factory-template regressions. The production recall path in `Tests/IntegratedProcessorTests.cpp` checks all **48** selectable presets through their actual PRE, native amps, modeled CABs and POST. The 1.3 checks cover valid CAB assignments, dirty/clean recall, OUTPUT at 0 dB, audible RMS and bounded nominal/+6 dB-input peaks, including initial samples. Prepared-state and saved-state round trips are checked separately. Source-specific fixtures, limits and measured evidence accompany release validation. These are regression checks; listening with real instruments and adjusting input gain remain necessary.

## Signature validation gate

The machine-readable targets and scoring rules live in `Validation/signature-benchmark-policy.json`. The evaluator `Tools/evaluate_signature_benchmark.py` reports PASS, REVISE or INVALID from same-DI render metrics. A high aggregate score cannot override a failed Signature hard gate. These automated scores do not replace source-specific release validation or listening acceptance.

## Raven Guitar Signature bank

A separate bank uses stored ordinals 34–37 after Factory31/Bass3; all original IDs
remain stable. Version 1.3 retains their PRE/AMP drive and agreed song roles while
assigning the new modeled CABs. Isolated DI/listening and actual DAW acceptance are pending.

| Song | Routing | PRE | Native amps | POST |
|---|---|---|---|---|
| A Path To Alsatia | Dual Blend, 38% B | Studio FET → Gold Drive | Hot Lead overdrive + Liquid Lead lead | FET Limiter → Inductor EQ → Precision Delay → Plate |
| Feel My Wrath | Dual Blend, 30% B | Yellow Asym | Fourfold CH3 + Tight 515 | Dry; Console VCA saved bypassed |
| Blackhearted | Matrix 140 / 1800 Hz | Yellow Asym after LOW tap | Clean LOW + Fourfold CH3 MID + Night Harvest GAIN I HIGH | Console Four EQ; dry space |
| Dark Matters of Throne | Matrix 160 / 1500 Hz | Obsession after LOW tap | Clean LOW + Blue Storm lead MID + Night Harvest GAIN I HIGH | Iron Colour → Passive Tube EQ; dry space |

Recall builds a complete native-state snapshot, including inactive model
banks, with canonical bool/choice host values. INPUT/tempo/tuner reset too.
Session MIDI mappings, imported assets and alternate A/B slots remain session
resources. Metadata includes the stable song ID, format v2 and the modeled-CAB
policy; private IR target fields are empty. Binary state and A/B retain it. XML
snapshots are produced by ChimeraIntegratedProcessorTests and uploaded by CI.
Each active rig uses the cabinet/speaker/microphone assignment in the 1.3 CAB
table, with Blackhearted and Dark Matters of Throne keeping dry LOW. Actual DI
loudness matching is still a release requirement.

## Náströnd and Niflheimr banks

The remaining ten presets keep their identities and full-rig snapshots. They
use the same factory CAB policy and are included in the 48-preset production
recall and level checks.

| Bank | Stored ordinals | Presets |
|---|---|---|
| Náströnd | 38–42 | Thall Rhythm, Molten Lead, Rotten Grind, Sludge Mass, Slam Impact |
| Niflheimr | 43–47 | Clear Attack, Death Grind, Clean Low Drive, Slam Punch, Doom Fuzz |

The 6×10 assignments use six sources in two columns and three rows. Existing
user-loaded 8×10 capture audio is retained; the change applies to the unshipped
modeled 8×10 cabinet and the new factory recipes.
