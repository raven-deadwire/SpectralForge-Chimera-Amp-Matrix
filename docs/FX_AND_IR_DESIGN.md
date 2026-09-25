# IR, routing and effect design

## Permanent controls and navigation

Gate and Transpose stay in the top strip on **RIGS, PRE and POST**. They are global input utilities, not pedals in the PRE page. The top strip also exposes input/output gain and meters, stereo/Mono L input, factory preset selection/previous/next, reference Open/Save As, Doubler and A/B. The bottom strip exposes Tuner, MIDI Learn, Tap, BPM, Host tempo, practice Click and Settings. Enabling Tuner opens a display in the routing bar while the top controls remain available; A4 and auto-mute live in that tuner display.

The five factory starting points are Clean Sustain, Tight Rhythm, Bass Matrix, Filter Lead and Fuzz Texture. They are original settings for different use cases, not reference-product presets. Save As/Open writes/loads `.chimera` files containing both A/B states and user IR bytes. A/B does not automatically match loudness.

Input `MONO L` duplicates the left input before processing; `STEREO` preserves the input channels. Doubler adds a small modulated wet delay to decorrelate a stereo output; it does not alter the dry-path latency and is inactive on mono buses. MIDI Learn assigns the next received CC to the selected control and saves that map with the project/reference. Route MIDI to the plugin in the host, or select a MIDI input in the standalone application's native audio/MIDI Options dialog.

TAP averages up to four recent intervals and returns to manual tempo. HOST follows the host BPM when available, with manual BPM as fallback. Delay SYNC uses a quarter note; manual delay time remains available when SYNC is off. The practice metronome is a free-running 4/4 click at that tempo, **not a DAW transport/bar-position synchronizer**. It is off by default and passes through output trim and tuner mute. Settings includes reference saving/loading, A4 reset, MIDI reset and credits; hardware device settings remain in the standalone application's native Options or the host.

## IR decision

IR stays inline with its amplifier lane: source, bypass, LOAD/drop, cuts and filename/status. The DSP stays **amp -> IR -> merge** regardless of visible page. This keeps two cabinet assignments visible when comparing Dual rigs or Matrix MID/HIGH. A separate cabinet page, as in the Neural Archetype workflow, becomes useful with multiple mic/IR slots, blend and explicit delay/phase tools; it adds navigation without a benefit for the present single-slot loader. No empty cabinet page is included.

Matrix LOW has COMP, LEVEL, BAND TONE and a reduction meter, plus the selected amp/cab and a DI/AMP blend. Its clean low band is compressed before branching into DI and amp/cab paths. The blend uses constant-sum gain and defaults to 0% AMP. The head Drive control is hidden and fixed at zero in this mode; stored Classic/Dual drive remains intact. The selected head can still color the low band at zero drive. Amp/cab loading remains available for this optional branch.

The DI branch is delayed by the head's algorithmic latency at every oversampling setting. IR onset is retained; distinct IRs are not automatically time/phase matched. A cabinet's captured phase or onset can therefore change the result of DI/AMP blending. User IR loading expands cabinet choice without expanding the amp model list.

The lane CAB menu lists installed IRs directly. IR LIBRARY exposes both factory assets and 13 personal reference captures with installed/missing status; a missing WAV cannot be loaded. The private full installer ZIP carries the actual WAVs and sidecars in a companion folder, which Setup installs to the shared library. The public bare installer provides only the two factory IRs and reference metadata. The ZIP importer accepts catalog-matched SHA-256 audio, ignoring NAM files and archive paths, and writes canonical metadata. Added folder locations persist outside presets and A/B. The expected complete personal collection is 15 available: two factory plus 13 personal IRs, including two bass captures and seven Karnivore variants. See [MODELS_AND_REFERENCE.md](MODELS_AND_REFERENCE.md) for capture details and unknown metadata.

## Mode responsibilities

| Stage | Classic | Dual Blend | Dual Crossover | Matrix |
| --- | --- | --- | --- | --- |
| Input trim, input mode, Gate, Transpose | Once | Once | Once | Once |
| PRE compressor and envelope | Once | Once | Once | Shared by clean/driven paths |
| PRE Fuzz, Boost, Overdrive | One rig | Before both rigs | Before split | MID/HIGH; LOW uses aligned dry tap |
| Split | None | Two full-range copies | Two LR4 bands, 60-4000 Hz crossover | Three LR4 bands |
| Rig processing | Amp/IR, full EQ | Two amp/IR rigs, full EQ | Two amp/IR rigs, Band Tone | LOW COMP then DI/amp-cab blend; MID/HIGH amp/IR; Band Tone per lane |
| Merge | One rig | Adjustable linear 0:100-100:0 balance | Sum low + high, no 50% attenuation | Sum three bands |
| POST rack, Doubler, output | Once | Once after merge | Once after merge | Once after merge |

Oversampled nonlinear stages have latency-aligned bypass. PRE Fuzz and Overdrive and the POST preamp run at fixed 4x oversampling; amplifier quality selects 1x, 2x, 4x or 8x. LOW DI includes the fixed delays of the Fuzz, Overdrive and amplifier stages it bypasses. Bus preamp delay applies globally even when bypassed. Amplifier quality changes leave the host-reported delay constant; enabling Transpose adds its displayed FFT-window delay. Allpass crossover phase and an IR's natural onset are not extra fixed processing latency.

## Pedalboard and rack models

New-instance PRE order is **Envelope -> Compressor -> Fuzz -> Boost -> Overdrive** (TOUCH). Compressor first remains selectable as SUSTAIN, and older states without the new order parameter restore that previous order. Gate and Transpose precede both choices and remain in the top strip. POST order remains Bus Comp -> Preamp -> EQ -> Modulation -> Delay -> Reverb. Every module has independent bypass and a model selector. See [MODELS_AND_REFERENCE.md](MODELS_AND_REFERENCE.md) for all **46 implemented variants**, hardware reference boundaries, CPU and A/B behavior, and the IR collection workflow.

The five PRE roles each have five model choices. Envelope before compression retains more picking-level information at the filter detector; compression first supplies a more level-controlled detector input. Fuzz, Boost and Overdrive keep that relative order, with the latter two shaping the fuzz output and amp input. Arbitrary reordering is not implemented. A physical Fuzz Face's pickup loading and input impedance are not modeled, so this order does not claim to reproduce an unbuffered analog pedalboard.

Compressor/Envelope are shared with Matrix LOW in either order; all three gain pedals are excluded from its clean tap. That same clean source feeds both branches of LOW's DI/AMP blend. Rack effects process the entire merged signal. The POST model counts are 3 compressor, 3 preamp, 3 EQ, 6 modulation, 3 delay and 3 reverb.

Fuzz and POST preamp tone-filter poles use 20 ms ramps at the oversampled rate, shared by both channels. This prevents abrupt filter changes during tone automation without changing the settled model response. Local DSP checks cover all five fuzz and three preamp voices, including stereo agreement, identical 17- and 511-sample block results and return to the original tone response; platform build and UI verification remain separate checks.

## LOW COMP control law

An original VCA-style feed-forward RMS controller inspired by the dbx 160/560A role. COMP x in [0,1] sets threshold `-12-36*x` dBFS and ratio `1+7*x`. It uses a 6 dB soft knee, 30 ms RMS detector, 10 ms gain attack and 140 ms release. COMP 0 is unity. No automatic makeup: use LEVEL for loudness matching. Stereo follows the larger channel power and applies the same gain to both channels.

## Official references reviewed

- Visible signal flow and user IR: https://odeholm-audio.com/products/slam-amp and https://odeholm-audio.com/products/thall-amp
- Archetype permanent utilities, pedal/cab sections: https://neuraldsp.com/manual/archetype-tim-henson-x
- Split-band bass processing: https://neuraldsp.com/plugins/parallax
- dbx RMS/VCA/knee: https://dbxpro.com/en-US/products/560a and https://dbxpro.com/en-US/compression-quiz
- MXR compressor controls: https://www.jimdunlop.com/content/manuals/M102.pdf and https://www.jimdunlop.com/mxr-super-comp/
- EHX filter/fuzz: https://www.ehx.com/products/micro-q-tron/ and https://www.ehx.com/products/big-muff-pi/
- Ibanez TS808: https://www.ibanez.com/usa/products/detail/ts808_99.html
- SSL bus control: https://store.solidstatelogic.com/plug-ins/ssl-native-bus-compressor-2 and https://www.solidstatelogic.com/products/big-six
- Neve preamp/output architecture: https://www.ams-neve.com/outboard/1073spx/ and https://www.ams-neve.com/wp-content/uploads/2026/06/1073SPX_1.2_User_Manual.pdf
- Console EQ scope: https://www.solidstatelogic.com/products/e-series-eq-module
- TC delay: https://www.tcelectronic.com/en/products/0815-aak
- Lexicon rack: https://lexiconpro.com/en-US/products/pcm92

Original generated artwork interprets the references within fixed rectangular pedal and rack layouts; source product photos and manufacturer logos are not bundled. The private user's downloaded IRs are separate from the public code and factory data. NAM captures supply repeatable digital reference baselines without owning the hardware. See [NAM_REFERENCE_RESULTS.md](NAM_REFERENCE_RESULTS.md); those fixed-capture comparisons do not establish hardware equivalence or validate every newly added pedal algorithm.
