# Eight additional amplifier designs

The eight appended models run native C++ audio processing in `NewAmpDSP.h`, reached by `Amp::process` and the regular lane engine. They are original circuit-inspired algorithms, not NAM models, hardware captures, component-level reconstructions, or verified reproductions of the named references. A numerical distinction between algorithms does not establish audible fidelity to hardware.

The first 15 model indices and their nonlinear arithmetic/EQ coefficients remain on the existing path. The new models bypass that Legacy tanh chain and its capture-derived contours. The old tables retain their catalog-sized storage only for compatibility; unused appended entries do not produce the new sounds.

| Index / product name | Design reference (not a fidelity claim) | Implemented original signal path | Selectable routes (default in bold) |
|---|---|---|---|
| 15 / Cinder 120 | ZUTA GBG120 | Two-stage open route; extra cascaded stages for crunch and higher gain; a tighter final coupling stage on CH4; reactive output feedback | **CH1**, CH2, CH3, CH4 |
| 16 / Iron Compact | ENGL Ironball E606 | First-stage local feedback; clean route with early tone shaping; lead route adds cold-biased and recovering gain stages; more compliant output supply | CLEAN, **LEAD** |
| 17 / Fourfold | Diezel VH4 | Clean bypasses later preamp stages; crunch adds a second, MEGA adds a third, and LEAD adds a post-tone stage; stiff output feedback | CLEAN, CRUNCH, **MEGA**, LEAD |
| 18 / Classic Tube | Ampeg SVT-CL, independently from SVT-VR | Low fundamental split before asymmetric preamp saturation, recombination before driver and a strongly damped output stage | **CLASSIC** |
| 19 / Monolith | 1970s SUNN Model T; exact revision unverified | Parallel normal and brilliant input stages; their outputs feed shared coupling, tone and driver stages; slower output supply recovery | NORMAL INPUT, BRILLIANT INPUT, **JUMPED INPUTS** |
| 20 / Night Harvest | Fortin Evil Pumpkin | GAIN I blends separately saturated low/full and high-frequency branches; GAIN II uses two serial gain sections with tone shaping between them; CLEAN bypasses both high-gain arrangements | **GAIN I**, GAIN II, CLEAN |
| 21 / Hot Lead | Soldano SLO-100 OD; exact LTD revision unverified | OD-only cascade with cold-biased clipping and cathode-feedback compression ahead of tone shaping | **OVERDRIVE** |
| 22 / Blue Storm | Bogner Uberschall Rev Blue; matching original documentation unverified | Separate clean and lead paths; lead retains wider low-frequency coupling into late gain stages; low-frequency output feedback | CLEAN, **LEAD** |

Monolith's three choices are input-routing experiments, not three hardware channels. Hot Lead implements only an OD design; the Normal channel and its modes are not added by implication. Iron Compact retains the Ironball E606 study scope; it is not relabeled as another ENGL amplifier.

Each nonlinear stage has an algebraic asymmetric transfer, grid-loading memory, cathode feedback, an interstage coupling state, and a bandwidth state. The output stage uses complementary nonlinear halves, frequency-dependent delayed negative feedback, transformer bandwidth, and separate supply depletion/recharge rates. Coefficients and connections are authored software choices. They are not extracted component values. Stereo signal states and supply reservoirs are independent.

All four existing oversampling settings and latency compensation paths are used. A new-model channel change crossfades for 20 ms. Repeated changes during a crossfade retain the most recent requested route until the current fade completes. State is installed at a block boundary, and no heap allocation occurs in the new DSP processing path. `reset()` immediately installs the final requested route with empty filter and nonlinear memories.

## API and control boundary

`Amp::set(model, drive, nativeChannel)` selects a route. `setNativeChannel(int)` and `nativeChannel()` expose the same state. `newAmpChannelCount`, `newAmpDefaultChannel`, and `newAmpChannelName` describe its bounds and presentation. The existing two-argument `Amp::set` remains supported, choosing the new model's default on model changes and retaining a manually selected route when the model stays the same.

The existing six-band lane controls remain global software EQ. Their frequencies differ by new design, but they are not exact native hardware tone stacks or knob tapers. Per-channel hardware knobs, gain-boost/voicing switches, power soak, onboard reverb, gate controls, MIDI behavior, and physical input/loop/output behavior are outside this DSP change unless separately connected and tested. In particular, the SVT-CL study does not claim to implement its physical five-position mid selector through the existing generic mid controls.

## Reference boundary

Manufacturer material was used to delimit product and channel scope, not to assert that the algorithms reproduce a circuit:

- [ZUTA GBG120](https://zutagroup.com/products/gbg120-tube-amp-by-zuta): four channel sections, per-channel controls, and global presence/depth.
- [ENGL Ironball E606](https://www.engl-amps.com/shop/heads/ironball-e606/): Clean/Lead gain controls, shared EQ, and Gain Boost. The current page describes four resulting sounds; this implementation selects the two base channel paths and does not claim a separate boost switch.
- [Diezel VH4](https://www.diezelamplification.com/vh4/): Clean, Crunch, Mega, and Lead with individual controls and global Presence/Deep.
- [Ampeg SVT-CL](https://ampeg.com/products/classic/heads.html): a separate bass-head target from the existing SVT-VR, including a documented five-way midrange selector that remains outside the generic EQ implementation.
- [Fortin Evil Pumpkin](https://fortinamps.com/products/evil-pumpkin%C2%AE-3-channel-midi-100w-free-hydra-midi-pedal): two gain channels and one clean channel. GAIN I/II routing algorithms are authored designs, not verified schematics.
- [Soldano SLO-100 Classic](https://www.soldano.com/products/classic/amplifiers/slo-100-classic/): Normal and Overdrive scope. Only OD is implemented here; the exact earlier LTD reference revision and newer Depth feature are not conflated.

For the 1970s SUNN Model T and Bogner Uberschall Rev Blue, matching original manufacturer revision documentation has not been verified in this change. Monolith and Blue Storm are explicitly design studies; neither a reissue, pedal, clone, nor another revision is substituted as verified hardware evidence.

## Validation

`Tests/NewAmpTests.cpp` runs synthetic signal tests for all 20 routes: finite bounds, independent stereo state, silence, sample-rate and oversampling variants, block partitioning, reset, drive response, channel differences after RMS matching, and signal-history/recovery behavior. It also checks repeated host parameter updates and rapid channel changes. The history test combines filter and nonlinear state; it is not an isolated sag measurement.

Numerical results and the independent 720-case Legacy before/after comparison are recorded separately with source hashes. Real DAW operation, listening evaluation, calibrated reamping, and original-hardware comparison are separate, uncompleted validation stages.

The final Linux headless run passed 160 route/rate/oversampling combinations, each in three stereo/block arrangements (48/96 kHz, 1x/2x/4x/8x, 64/257 samples). Peak magnitude was 0.947821; maximum block-partition residual was 1.2517e-6; identical-stereo residual was zero. Minimum RMS-matched differences were 0.348632 between default models, 0.399384 between native routes, and 0.0809263 between drive settings. These prove that the algorithms are not just level changes, not that they match hardware.

An independent immutable-header baseline compared all 15 Legacy voices at 44.1/48/96 kHz, blocks of 64/257, all four oversampling modes, and drive 0.2/0.8: all 720 output-byte hashes and energy values matched exactly after the local-feedback correction. The before/after result files share SHA-256 `89cc26b30754887dc26aee9c447edb82abf864d1775666adddcf34931169d75f`. A subsequent gain-staging correction changed only four appended routes; Legacy arithmetic was untouched, and the 720-case comparison was not repeated. Its original source hashes and execution record are preserved separately from the later new-route regression. This preservation result covers the specified render conditions; it is not a claim about every possible host session.

At drive 0.8 and 1.0, all 20 routes reset exactly and returned to silence after conditioning; worst relative quiet-probe difference after a two-second recovery interval was 3.9208e-5. An initial full-cascade feedback design in Iron Compact's lead route failed the silence test. It was corrected to feedback around the first stage only, with maximum nominal small-signal loop gain below 0.54. The original silence/recovery bounds were retained.

The existing integrated dynamics test exposed excessive steady-state compression in Blue Storm LEAD at maximum drive. An expanded probe found the same issue in Cinder CH4, Fourfold LEAD, and Night Harvest GAIN II. Their distributed stage gains were reduced without changing Legacy processing or the acceptance bounds. The new regression covers all 20 routes using the same 997 Hz, 48 kHz, 4x oversampling fixture: loud input peak 0.3 versus quiet peak 0.015, after discarding the first 64 of 192 blocks of 256 samples. Every route passed the unchanged `1.05 < loud/quiet output RMS < 21` gate; observed ratios range from 1.07909 to 13.1344. Maximum loud-output DC was 9.66613e-5 against the unchanged 0.001 bound. This checks retained input dynamics under sustained excitation in addition to the transient/multitone tests.
