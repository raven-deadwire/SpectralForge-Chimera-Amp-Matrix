# Models and reference workflow

This development build has **46 selectable effect variants: 25 PRE pedals and 21 POST rack models**, alongside eight amplifier voices. These are original algorithms with hardware-informed controls and artwork, not licensed circuit models or verified replicas. Choosing a model changes processing as well as its appearance. A/B and project recall include the selection; see the automation compatibility note below when upgrading older test projects.

| Module | Choices | Actual processing differences |
|---|---|---|
| PRE compressor | Studio VCA / Red OTA / Optical / Studio FET / Variable Mu | Linked detection; distinct knee, attack and recovery; FET parallel dry contribution; progressive Mu ratio and recovery memory |
| Envelope | Q Sweep / Tron Band / Reverse Sweep / Bass Envelope / Dynamic Wah | Rising low-pass, rising band-pass, falling low-pass, band-pass with retained dry lows, fast vocal-band sweep |
| Fuzz | Big Sustain / Round Face / Bender / Wool Bass / Gated Factory | Dense clipping, asymmetric soft clipping, gated edge, bass-weighted gating, stronger asymmetric gating |
| Boost | RC Clean / Treble Lift / Micro Lift / EP Lift / Linear Power | Broad shelves, 650 Hz low-cut, full-band lift, low-mid emphasis with softened top, mild broadband contour |
| Drive | Green 808 / Gold Drive / Rodent / Bass DI / Micro Bass | Soft clip, clean/asymmetric blend, hard clip, bass-preserving drive with mid scoop, clean lows with upper-band clipping |
| Bus compressor | Console VCA / FET 76 / Opto Level | Linked RMS, fast peak, program-dependent recovery |
| Preamp | N73 Colour / V5 Pure / ISA Blue | Asymmetry, headroom and bandwidth at 4x oversampling |
| EQ | Console E / N73 Shelves / Passive Tube | 80/8k, 110/12k, 60/10k shelves; broad passive-style mid |
| Modulation | Classic Chorus / Dimension / Stone Phase / Mistress Flange / Eddy Vibrato / Pulsar Tremolo | Delay chorus, dual stereo voices, four allpasses, short feedback comb, pitch modulation, amplitude modulation |
| Delay | Digital 229 / Tape Echo / Analog Memory | Clear repeat, filtered wow/flutter, dark modulated repeat |
| Reverb | Studio Plate / Concert Hall / Spring Tank | Short dense diffusion, predelayed hall, dispersive feedback spring |

The controls are deliberately consistent within each module. No claim is made that every knob range or circuit topology matches the referenced hardware. The six rack modules run exactly once after merge. Matrix LOW keeps its clean tap before Fuzz/Boost/Drive, compresses the low band, and blends the DI with the selected amp/cab. Head drive is fixed at zero; the DI delay matches the head algorithmic latency. Gate and Transpose remain global on every page.

## PRE references and order

Each PRE family has five choices. The first three retain their existing raw project indices; the final two are appended.

| Family | Reference 1 | Reference 2 | Reference 3 | Added reference 4 | Added reference 5 |
|---|---|---|---|---|---|
| Compressor | MXR M87 | MXR Dyna Comp | Diamond Compressor | Origin Effects Cali76 / 1176 | Manley Variable Mu |
| Envelope | EHX Q-Tron | Mu-Tron III band-pass | Mu-Tron III Down mode | MXR M82 Bass Envelope Filter | Boss AW-3 |
| Fuzz | EHX Big Muff Pi | Dunlop Fuzz Face | Sola Sound Tone Bender | ZVEX Woolly Mammoth | ZVEX Fuzz Factory |
| Boost | Xotic RC Booster | Dallas Rangemaster | MXR Micro Amp | Xotic EP Booster | EHX LPB-1 |
| Drive | Ibanez TS808 | Klon Centaur | Pro Co RAT | Tech 21 SansAmp Bass Driver DI | Darkglass Microtubes B3K |

New instances default to **TOUCH: Envelope -> Compressor -> Fuzz -> Boost -> Overdrive**. The envelope detector then receives the playing dynamics before PRE compression. **SUSTAIN: Compressor -> Envelope -> Fuzz -> Boost -> Overdrive** remains selectable for a more level-controlled detector input. Both follow global Gate/Transpose, so those utilities can still affect the signal that reaches the envelope. Old states without the order parameter restore Compressor first.

Fuzz precedes Boost/Overdrive so those stages can shape its output and subsequent amp drive. This is an in-plugin signal-order decision. Pickup loading, pedal input impedance and the guitar-volume interaction of a physical early-chain Fuzz Face are not simulated; moving a digital module cannot establish that circuit behavior. General drag-and-drop reordering is not implemented.

The new Bass DI and Micro Bass algorithms preserve more low-frequency content than the guitar-tightening drive variants. Their fixed blend/contour choices do not reproduce every SansAmp or B3K control. Variable Mu uses an original progressive gain-control law; it is not a vacuum-tube circuit simulation. NAM head comparisons do not validate these pedal algorithms.

## Amplifier reference boundary

The eight voices are Glass, Brit Edge, Tight 515, Wide Rect, Liquid Lead, Iron Tube, Solid Punch and Modern Bass. Their fixed NAM reference captures, input-calibration gaps and held-out comparison results are documented in [NAM_REFERENCE_RESULTS.md](NAM_REFERENCE_RESULTS.md). Modern Bass uses a B7K Ultra **plus Aguilar DB751** reference chain, not an isolated Darkglass head. Reference artwork does not establish circuit accuracy or a hardware-fidelity pass.

## Test-project compatibility

Saved raw model indices 0-2 continue to select the original choices. Expanding PRE choice parameters from three to five values changes the conversion between a normalized host automation value and a choice index. Older DAW automation lanes may therefore select a different model even when ordinary project-state recall is correct. Review and re-record model-selection automation after upgrading an older test build. This is a compatibility limitation of the current test parameter layout, not a claim of complete automation compatibility.

## CPU meter

The footer reports CPU average and PK. Both are percentages of the available audio block time: wall-clock time inside this plugin's processBlock divided by samples/sample-rate. Average has a one-second exponential response; peak decays with the same time constant and immediately captures spikes. 100% means this callback consumed its entire nominal deadline. This is not total computer usage or the host's aggregate CPU meter. File decoding, UI painting and the background tuner/IR worker are outside this measurement. Actual readings depend on processor, block size, routing, oversampling and enabled effects.

## A/B checked behavior

A and B are **two complete sound snapshots**, not left/right audio channels. Switching stores the outgoing state and recalls the other state's parameters, module models and embedded IRs/tags. Both stereo input channels run through the selected sound. COPY duplicates the active sound to the other slot; it does not mix or pan them. An unused slot initially copies the current sound. Factory presets are separate starting points, and Save As/Open preserves both A/B snapshots. MIDI assignments remain global.

Switching resets DSP histories/tails and is not gapless tail-preserving preset morphing. A/B does not automatically match perceived loudness. Stereo channel isolation/equal gain, model recall, IR recall and deletion-independent project restoration are covered by the Windows processor tests.

## IR collection

IR remains on its amplifier lane. The CAB menu directly lists installed files, grouped into bass and guitar/other, in addition to the factory sources. IR LIBRARY and each lane's IRs button open the persistent cabinet collection, with factory and missing/installed reference rows. A catalog entry without its WAV is not an available IR and cannot be loaded. The collection indexes up to 512 WAV/AIFF files, filters by speaker diameter, and searches speaker/microphone/position/creator text. TAGS shows twelve independent capture fields. User tags are editable and are embedded with audio in projects and A/B. Factory tags are read-only.

A sidecar named exactly `filename.wav.json` supplies confirmed fields. The examples in `reference/ir-tags` match the selected TONE3000 files. Copy the matching sidecar beside your downloaded WAV. Recognized filenames use the source catalog with a filename-association warning; verify the recorded SHA256 for exact identity. Other files receive only identifiable filename hints. The selected reference exposes its source page. Unknown distance, off-axis angle, unit model or diameter stays blank. A 4x12 means four 12-inch speakers; it says nothing about mic distance. Close-mic descriptions without a number are not converted into invented centimeters.

The personal collection contains **13 WAV captures**, in addition to the two embedded factory IRs:

| Capture group | WAV files | Classification | Known distinctions |
|---|---:|---|---|
| Mesa Traditional 4x12 / Celestion V30 | 3 | Guitar, 12-inch | SM57 plus two M201 grille distances; documented unit/cone positions |
| Peavey Delta Blues 1x15 / Celestion Fullback | 1 | Guitar, 15-inch | AT4050, center, filename states 1 inch with unspecified distance datum |
| Ampeg 8x10 | 1 | Bass, 10-inch | MD421; unit model and mic geometry undocumented |
| 1970 Fender Bassman 2x15 / CTS | 1 | Bass, 15-inch | SM57, upper unit, cone position; numerical distance and angle undocumented |
| Mesa standard-size slant 4x12 / Eminence Karnivore | 7 | Guitar, 12-inch | Filename labels Fredman, V7x, SM57, M160, 421, 906 Flat and i5 |

The Fullback is not labeled a bass cabinet merely because it is 15 inches. For Karnivore, the creator identifies a speaker in the slanted section of a standard-size Mesa 4x12; microphone identities are filename-derived. The Fredman label does not confirm a particular microphone pair or angle. Exact cone location, distance and off-axis angle remain unknown. `reference/ir-catalog.json` records source, creator, license, exact file hash, frame count and known capture conditions.

TONE3000 files were obtained through the user's signed-in browser. They are not embedded in the public plugin binary or repository. The **private full installer ZIP** includes the actual 13 WAVs and sidecars in `Chimera-Personal-IRs`; extract the entire ZIP and keep that folder beside Setup for automatic installation. A bare public CI installer contains only the two redistributable CC BY factory IRs plus reference metadata. Catalog names and sidecars alone do not supply the other audio files.

Existing installations can import the updated personal ZIP through IMPORT PERSONAL ZIP, or add the extracted IR folder through ADD FOLDER. The importer accepts catalog-matched SHA-256 audio, ignores NAM files, and writes canonical metadata. All 13 personal WAVs plus the two factory assets should be available after the full personal pack is installed. Personal IRs are embedded in saved projects; creator permission is required before including restricted capture data in a public distribution or factory preset.

## Official design references

- Neve 1073: https://www.ams-neve.com/outboard/1073spx/
- Avalon V5: https://avalondesignarchive.com/v5.html
- Focusrite ISA: https://us.focusrite.com/products/isa-one
- MXR M87: https://www.jimdunlop.com/mxr-bass-compressor/
- MXR Dyna Comp: https://www.jimdunlop.com/mxr-dyna-comp-compressor/
- Diamond optical: https://www.diamondpedals.com/products/comp-eq
- Cali76 FET: https://origineffects.com/product/cali76-fet-compressor/
- Variable Mu: https://www.manley.com/products/pro-audio/dynamics/variable-mu
- MXR M82: https://www.jimdunlop.com/mxr-bass-envelope-filter/
- Boss AW-3: https://www.boss.info/global/products/aw-3/
- ZVEX Woolly Mammoth: https://www.zvex.com/guitar-pedals/woolly-mammoth-guitar-effects-pedal
- ZVEX Fuzz Factory: https://www.zvex.com/guitar-pedals/fuzz-factory-guitar-effects-pedal
- Xotic RC: https://xotic.us/manuals/pdf/RC_Booster_manual.pdf
- Xotic EP: https://xotic.us/effects/ep-booster/
- EHX LPB-1: https://www.ehx.com/products/lpb-1/
- SansAmp Bass Driver DI: https://www.tech21nyc.com/products/sansamp-2/bassdriver-di/
- Darkglass B3K: https://www.darkglass.com/en-int/products/microtubes-b3k
- SSL bus compressor: https://store.solidstatelogic.com/plug-ins/ssl-native-bus-compressor-2
- FET/opto dynamics: https://www.uaudio.com/blogs/ua/1176-vs-la-2a-understanding-two-legendary-compressors
- Chorus: https://www.boss.info/ca/products/ce-2w/
- Dimension: https://www.boss.info/uk/products/dc-2w/
- Phaser: https://www.ehx.com/products/small-stone/
- Flanger: https://www.ehx.com/products/stereo-electric-mistress/
- Vibrato: https://www.ehx.com/products/eddy/
- Tremolo: https://www.ehx.com/products/nano-pulsar/

Artwork uses individually generated model surfaces with native controls and readable labels, within the RavenForge workbench design. Pedals and racks use consistent rectangular layouts; distinctive materials, grille patterns, panel details and knob treatments identify the reference instead of reusing one enclosure in different colors. The Mu-Tron Up/Down variants intentionally share one hardware body. Cabinet images illustrate cabinet style and are not photographs of the captured unit. Generation prompts and asset provenance are recorded in `ARTWORK_PROMPTS.json`. Equipment names identify references; manufacturer logos and source product photography are not bundled.
