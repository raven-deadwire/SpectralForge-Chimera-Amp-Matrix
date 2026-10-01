# Models and reference workflow

SpectralForge Chimera current `main` exposes **60 selectable effect variants: 39 PRE pedal DSPs (plus the Empty slot) and 21 POST rack models**, alongside **23 amplifier voices**. These are original algorithms with hardware-informed controls and artwork, not licensed circuit models or verified replicas. Choosing a model changes processing as well as its appearance. The PRE board can host up to five independent owners at once; A/B and project recall include model-specific banks. See the compatibility note below for the boundary between the preserved legacy parameter set and appended structural/model parameters.

| Module | Choices | Actual processing differences |
|---|---|---|
| PRE compressor | Studio VCA / Red OTA / Optical / Studio FET / Variable Mu | Linked detection; distinct knee, attack and recovery; FET parallel dry contribution; progressive Mu ratio and recovery memory |
| Envelope | Q Sweep / Tron Band / Reverse Sweep / Bass Envelope / Dynamic Wah | Rising low-pass, rising band-pass, falling low-pass, band-pass with retained dry lows, fast vocal-band sweep |
| Fuzz | Big Sustain / Round Face / Bender / Wool Bass / Gated Factory | Dense clipping, asymmetric soft clipping, gated edge, bass-weighted gating, stronger asymmetric gating |
| Boost | RC Clean / Treble Lift / Micro Lift / EP Lift / Linear Power | Broad shelves, 650 Hz low-cut, full-band lift, low-mid emphasis with softened top, mild broadband contour |
| Drive / distortion | Green 808 / Gold Drive / Rodent / Bass DI / Micro Bass / Yellow Asym / Obsession / Plus Drive / Dual Circuit | The original five voices plus four independent oversampled drive circuits; OCD peak mode changes the algorithm and JB-2 exposes solo/toggle/serial/parallel routing |
| PRE wah | Manual Wah | Smoothed swept band-pass with range and resonance controls |
| PRE equalizer | Graphic EQ | Ten peak bands plus independent input/output gain |
| PRE modulation | Classic Chorus / Spatial Chorus / Stone Phase / Tidal Flange / Drift Vibrato / Pulse Tremolo | Six independent PRE modulation instances using the product's original modulation algorithms |
| PRE pitch / octave | Mono Octaver / Spectral Octaver | Tracking-divider mono sub-octaves and STFT spectral octave processing; spectral mode adds frame latency |
| Bus compressor | Console VCA / FET 76 / Opto Level | Linked RMS, fast peak, program-dependent recovery |
| Preamp | N73 Colour / V5 Pure / ISA Blue | Asymmetry, headroom and bandwidth at 4x oversampling |
| EQ | Console E / N73 Shelves / Passive Tube | 80/8k, 110/12k, 60/10k shelves; broad passive-style mid |
| Modulation | Classic Chorus / Dimension / Stone Phase / Mistress Flange / Eddy Vibrato / Pulsar Tremolo | Delay chorus, dual stereo voices, four allpasses, short feedback comb, pitch modulation, amplitude modulation |
| Delay | Digital 229 / Tape Echo / Analog Memory | Clear repeat, filtered wow/flutter, dark modulated repeat |
| Reverb | Studio Plate / Concert Hall / Spring Tank | Short dense diffusion, predelayed hall, dispersive feedback spring |

The controls are deliberately consistent within each module. No claim is made that every knob range or circuit topology matches the referenced hardware. The six rack modules run exactly once after merge. Matrix LOW keeps its clean tap before Fuzz/Boost/Drive, compresses the low band, and blends the DI with the selected amp/cab. Head drive is fixed at zero; the DI delay matches the head algorithmic latency. Gate and Transpose remain global on every page.

## PRE references, board inventory and order

The current five-slot PRE board has **40 catalog entries including Empty, of which 39 are implemented DSP models**. The original five families still account for 25 models and retain their existing model banks. Four additional drive references, Manual Wah, PRE Graphic EQ, six PRE modulation variants and two octave processors are appended as independent owner/model banks rather than by renumbering the legacy families.

| Family | Reference 1 | Reference 2 | Reference 3 | Added reference 4 | Added reference 5 |
|---|---|---|---|---|---|
| Compressor | MXR M87 | MXR Dyna Comp | Diamond Compressor | Origin Effects Cali76 / 1176 | Manley Variable Mu |
| Envelope | EHX Q-Tron | Mu-Tron III band-pass | Mu-Tron III Down mode | MXR M82 Bass Envelope Filter | Boss AW-3 |
| Fuzz | EHX Big Muff Pi | Dunlop Fuzz Face | Sola Sound Tone Bender | ZVEX Woolly Mammoth | ZVEX Fuzz Factory |
| Boost | Xotic RC Booster | Dallas Rangemaster | MXR Micro Amp | Xotic EP Booster | EHX LPB-1 |
| Drive | Ibanez TS808 | Klon Centaur | Pro Co RAT | Tech 21 SansAmp Bass Driver DI | Darkglass Microtubes B3K |

Additional current PRE references are:

| Product alias | Hardware/reference target | Current implementation boundary |
|---|---|---|
| Yellow Asym | BOSS SD-1 | Independent oversampled asymmetric-clipping path with Drive/Tone/Level; hardware/capture calibration is not established |
| Obsession | Fulltone OCD v2 | Independent oversampled drive path with Volume/Drive/Tone and HP/LP peak switch; exact revision response is not capture-verified |
| Plus Drive | MXR Distortion+ M104 | Independent oversampled hard-clipping path with Distortion/Output; not a renamed RAT/DOD path |
| Dual Circuit | BOSS/JHS JB-2 | Separate BOSS/JHS paths with solo selection, both serial orders and parallel sum; physical circuit correspondence remains unverified |
| Manual Wah | Original DSP / wah reference | Smoothed swept band-pass; no hardware certification |
| Graphic EQ | Original DSP | Ten bands from 31.25 Hz to 16 kHz plus input/output gain |
| PRE modulation | CE-2 / Dimension / Small Stone / Electric Mistress / Eddy / Pulsar reference families | Six PRE instances of the original modulation algorithms |
| Mono Octaver / Spectral Octaver | Tracking-divider / experimental spectral pitch references | Mono /2 and /4 tracking plus STFT octave processing; not circuit replicas |

New instances default to **TOUCH: Envelope -> Compressor -> Fuzz -> Boost -> Overdrive**. The envelope detector then receives the playing dynamics before PRE compression. **SUSTAIN: Compressor -> Envelope -> Fuzz -> Boost -> Overdrive** remains selectable for a more level-controlled detector input. Both follow global Gate/Transpose, so those utilities can still affect the signal that reaches the envelope. Old states without the order parameter restore Compressor first.

Fuzz precedes Boost/Overdrive so those stages can shape its output and subsequent amp drive. This is an in-plugin signal-order decision. Pickup loading, pedal input impedance and the guitar-volume interaction of a physical early-chain Fuzz Face are not simulated; moving a digital module cannot establish that circuit behavior. A separate gain-order selector switches Fuzz → Boost → Overdrive (default) to Fuzz → Overdrive → Boost. After-drive boost can lift level if the downstream amp has headroom; it does not guarantee a clean solo lift. Use lane LEVEL or OUTPUT after the amp for final level. General drag-and-drop reordering is not implemented.

The new Bass DI and Micro Bass algorithms preserve more low-frequency content than the guitar-tightening drive variants. Their fixed blend/contour choices do not reproduce every SansAmp or B3K control. Variable Mu uses an original progressive gain-control law; it is not a vacuum-tube circuit simulation. NAM head comparisons do not validate these pedal algorithms.

## Amplifier reference boundary

The original eight voices are Glass, Brit Edge, Tight 515, Wide Rect, Liquid Lead, Iron Tube, Solid Punch and Modern Bass. Their fixed NAM reference captures, input-calibration gaps and held-out comparison results are documented in [NAM_REFERENCE_RESULTS.md](NAM_REFERENCE_RESULTS.md). Modern Bass uses a B7K Ultra **plus Aguilar DB751** reference chain, not an isolated Darkglass head.

Seven earlier appended voices — Chime 30, Orange Crown, Vintage Valve, Metro Clean, Prism Chime, Silk Lead and Taste Punch — bring the preserved legacy host-choice bank to **15 models**. Eight later models are appended without rescaling that legacy choice parameter: **Cinder 120 / ZUTA GBG120, Iron Compact / ENGL Ironball E606, Fourfold / Diezel VH4, Classic Tube / Ampeg SVT-CL, Monolith / SUNN Model T, Night Harvest / Fortin Evil Pumpkin, Hot Lead / Soldano SLO-100 and Blue Storm / Bogner Uberschall Rev Blue**.

All 23 models now have native panel definitions and autho## Test-project compatibility

The original amplifier host choice parameters remain exactly **15 legacy choices** with their existing normalized positions and automation meaning. Models 15–22 are selected through appended structural/native parameters, so old amp automation is not silently rescaled to 23 choices. Channel/input-route selectors for appended/native amps are structural and non-automatable; native control IDs are appended under stable `nativeAmp_c{context}_m{model}_...` namespaces.

The five-slot PRE board similarly uses fixed owner/model/control IDs. Board enable, model selection, order and LOW-tap structure are non-automatable; model-specific control and bypass banks remain separate, and binary project/A/B tests preserve inactive banks. A pre-board legacy project keeps its compatibility audio path and raw legacy PRE parameters until the user deliberately activates/edits the board.

These are processor/APVTS compatibility guarantees exercised by the repository tests. They do **not** establish correctness of every pre-existing automation lane in an external DAW, and they do not substitute for current Windows host/session validation.

## CPU meter

The footer reports CPU average and PK. Both are percentages of the available audio block time: wall-clock time inside this plugin's processBlock divided by samples/sample-rate. Average has a one-second exponential response; peak decays with the same time constant and immediately captures spikes. 100% means this callback consumed its entire nominal deadline. This is not total computer usage or the host's aggregate CPU meter. File decoding, UI painting and the background tuner/IR worker are outside this measurement. Actual readings depend on processor, block size, routing, oversampling and enabled effects.

## A/B checked behavior

A and B are **two complete sound snapshots**, not left/right audio channels. Switching stores the outgoing state and recalls the other state's parameters, module models and embedded IRs/tags. Both stereo input channels run through the selected sound. COPY duplicates the active sound to the other slot; it does not mix or pan them. An unused slot initially copies the current sound. Factory presets are separate starting points, and Save As/Open preserves both A/B snapshots. MIDI assignments remain global.

Switching resets DSP histories/tails and is not gapless tail-preserving preset morphing. A/B does not automatically match perceived loudness. Stereo channel isolation/equal gain, model recall, IR recall and deletion-independent project restoration are covered by the Windows processor tests.

## IR collection

IR remains on its amplifier lane. The CAB menu directly lists installed files, grouped into bass and guitar/other, in addition to the factory sources. IR LIBRARY and each lane's IRs button open the persistent cabinet collection, with factory and missing/installed reference rows. A catalog entry without its WAV is not an available IR and cannot be loaded. The collection indexes up to 512 WAV/AIFF files, filters by speaker diameter, and searches speaker/microphone/position/creator text. TAGS shows twelve independent capture fields. User tags are editable and are embedded with audio in projects and A/B. Factory tags are read-only.

A sidecar named exactly `filename.wav.json` supplies confirmed fields. The examples in `reference/ir-tags` match the selected TONE3000 files. Copy the matching sidecar beside your downloaded WAV. Recognized filenames use the source catalog with a filename-association warning; verify the recorded SHA256 for exact identity. Other files receive only identifiable filename hints. The selected reference exposes its source page. Unknown distance, off-axis angle, unit model or diameter stays blank. A 4x12 means four 12-inch speakers; it says nothing about mic distance. Close-mic descriptions without a number are not converted into invented centimeters.

The reference catalogs currently describe **30 external/private capture files** in addition to the two embedded factory IRs: 26 entries in `reference/ir-catalog.json` plus four entries in `reference/raven-ir-catalog.json`. The Raven set contains three Celestion G12-100 Raven positions (SM57 In / Out / Ref) and one Marshall 1960 V30 comparison file from The other John Browne.

A catalog row or sidecar is **not** audio availability. Public source/CI include the two redistributable factory IRs and metadata; external/private captures become Ready only when the matching WAV is present and passes the recorded SHA-256. The collection and importer reject corrupt/silent files and hash-mismatched catalog imports. Generic embedded-user-IR save/restore is tested, while Raven-specific private-pack import is only fully exercised when the external ZIP is supplied to the test harness.

The Raven metadata records 48 kHz mono PCM24, 9,601-frame files and exact SHA-256 values, but the public repository does not contain those WAVs. The recorded source does not include public-product redistribution permission, so Raven remains a private-import/reference asset unless separate rights are obtained. Unknown cabinet suffix, exact mic distance/angle, source normalization and phase processing remain explicitly unfilled rather than inferred.

Personal/user IR audio that is actually loaded is embedded in saved projects and A/B snapshots. Metadata, hashes and source URLs document provenance; they do not by themselves grant redistribution rights.

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
