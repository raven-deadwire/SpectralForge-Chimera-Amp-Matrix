# Models and reference workflow

This development build adds 36 selectable pedal/rack variants. These are original algorithms with hardware-informed controls and artwork, not licensed circuit models or verified replicas. Choosing a model changes DSP and the panel finish; host automation, A/B and project recall include the selection.

| Module | Choices | Actual processing differences |
|---|---|---|
| PRE compressor | Studio VCA / Red OTA / Optical | RMS vs fast peak detector; knee, ratio and recovery |
| Envelope | Q Sweep / Tron Band / Reverse Sweep | Rising low-pass, rising band-pass, falling low-pass |
| Fuzz | Big Sustain / Round Face / Bender | Dense clipping, asymmetric soft clipping, gated edge |
| Boost | RC Clean / Treble Lift / Micro Lift | Two shelves, 650 Hz low-cut plus treble emphasis, broad lift |
| Drive | Green 808 / Gold Drive / Rodent | Soft clip, clean/asymmetric blend, hard clip; different input cuts |
| Bus compressor | Console VCA / FET 76 / Opto Level | Linked RMS, fast peak, program-dependent recovery |
| Preamp | N73 Colour / V5 Pure / ISA Blue | Asymmetry, headroom and bandwidth at 4x oversampling |
| EQ | Console E / N73 Shelves / Passive Tube | 80/8k, 110/12k, 60/10k shelves; broad passive-style mid |
| Modulation | Classic Chorus / Dimension / Stone Phase / Mistress Flange / Eddy Vibrato / Pulsar Tremolo | Delay chorus, dual stereo voices, four allpasses, short feedback comb, pitch modulation, amplitude modulation |
| Delay | Digital 229 / Tape Echo / Analog Memory | Clear repeat, filtered wow/flutter, dark modulated repeat |
| Reverb | Studio Plate / Concert Hall / Spring Tank | Short dense diffusion, predelayed hall, dispersive feedback spring |

The controls are deliberately consistent within each module. No claim is made that every knob range or circuit topology matches the referenced hardware. The six rack modules still run exactly once after merge. Matrix LOW keeps its clean tap before Fuzz/Boost/Drive, plus the alignment delay. Gate and Transpose remain global on every page.

## CPU meter

The footer reports CPU average and PK. Both are percentages of the available audio block time: wall-clock time inside this plugin's processBlock divided by samples/sample-rate. Average has a one-second exponential response; peak decays with the same time constant and immediately captures spikes. 100% means this callback consumed its entire nominal deadline. This is not total computer usage or the host's aggregate CPU meter. File decoding, UI painting and the background tuner/IR worker are outside this measurement. Actual readings depend on processor, block size, routing, oversampling and enabled effects.

## A/B checked behavior

A and B are **two complete sound snapshots**, not left/right audio channels. Switching stores the outgoing state and recalls the other state's parameters, module models and embedded IRs/tags. Both stereo input channels run through the selected sound. COPY duplicates the active sound to the other slot; it does not mix or pan them. An unused slot initially copies the current sound. Factory presets are separate starting points, and Save As/Open preserves both A/B snapshots. MIDI assignments remain global.

Switching resets DSP histories/tails and is not gapless tail-preserving preset morphing. A/B does not automatically match perceived loudness. Stereo channel isolation/equal gain, model recall, IR recall and deletion-independent project restoration are covered by the Windows processor tests.

## IR collection

IR remains on its amplifier lane. LOAD opens a single-file loader or folder collection. The collection indexes up to 512 WAV/AIFF files, filters by speaker diameter, and searches speaker/microphone/position/creator text. TAGS shows twelve independent capture fields. User tags are editable and are embedded with audio in projects and A/B. Factory tags are read-only.

A sidecar named exactly `filename.wav.json` supplies confirmed fields. The examples in `reference/ir-tags` match the selected TONE3000 files. Copy the matching sidecar beside your downloaded WAV. Recognized filenames use the source catalog with a filename-association warning; verify the recorded SHA256 for exact identity. Other files receive only identifiable filename hints. LOAD also provides direct links to the bass/guitar source pages. Unknown distance, off-axis angle, unit model or diameter stays blank. A 4x12 means four 12-inch speakers; it says nothing about mic distance. Close-mic descriptions without a number are not converted into invented centimeters.

The expanded reference collection includes guitar V30/SM57, two V30/M201 grille distances, a 15-inch Fullback guitar speaker, **Ampeg 8x10/MD421 bass** and **Fender Bassman 2x15/CTS/SM57 bass**. The Fullback is not mislabeled a bass cabinet simply because it is 15 inches. `reference/ir-catalog.json` records source, creator, license, exact file hash, frame count and known capture conditions.

TONE3000 T3K files were obtained through the user's signed-in browser and used locally for validation. They are not embedded in the public plugin binary/repository/package. The package includes the source catalog and tags; these do not contain IR samples or NAM weights. The two redistributable CC BY factory IRs remain available out of the box. Personal IRs load through the collection and are saved in personal projects; do not redistribute those projects as factory presets without the creator's permission.

## Official design references

- Neve 1073: https://www.ams-neve.com/outboard/1073spx/
- Avalon V5: https://avalondesignarchive.com/v5.html
- Focusrite ISA: https://us.focusrite.com/products/isa-one
- MXR M87: https://www.jimdunlop.com/mxr-bass-compressor/
- MXR Dyna Comp: https://www.jimdunlop.com/mxr-dyna-comp-compressor/
- Diamond optical: https://www.diamondpedals.com/products/comp-eq
- Xotic RC: https://xotic.us/manuals/pdf/RC_Booster_manual.pdf
- SSL bus compressor: https://store.solidstatelogic.com/plug-ins/ssl-native-bus-compressor-2
- FET/opto dynamics: https://www.uaudio.com/blogs/ua/1176-vs-la-2a-understanding-two-legendary-compressors
- Chorus: https://www.boss.info/ca/products/ce-2w/
- Dimension: https://www.boss.info/uk/products/dc-2w/
- Phaser: https://www.ehx.com/products/small-stone/
- Flanger: https://www.ehx.com/products/stereo-electric-mistress/
- Vibrato: https://www.ehx.com/products/eddy/
- Tremolo: https://www.ehx.com/products/nano-pulsar/

Artwork uses original vector enclosures, material shading, machined knobs, footswitches and speaker grilles. Equipment names identify references; no manufacturer logos or proprietary product artwork are included.
