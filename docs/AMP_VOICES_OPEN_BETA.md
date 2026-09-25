# Open Beta 1.0 amplifier voices and gain-stage order

## Amplifier catalog

Chimera has fifteen original voiced nonlinear amplifier algorithms. Hardware names identify the reference or design influence; they do not denote licensed models, component-level reproductions, or a verified accuracy claim. Cabinet processing remains a separate lane stage.

| Raw index | Chimera voice | Hardware reference | Intended role |
|---:|---|---|---|
| 0 | Glass | Fender '65 Twin Reverb | Open clean guitar |
| 1 | Brit Edge | Marshall JTM45 | Dynamic British breakup |
| 2 | Tight 515 | Peavey 6505 / 5150 family | Tight high-gain rhythm |
| 3 | Wide Rect | Mesa Dual Rectifier | Broad saturated rhythm |
| 4 | Liquid Lead | Mesa Mark IV Lead | Focused sustained lead |
| 5 | Iron Tube | Ampeg SVT-VR | Weighty bass and tube drive |
| 6 | Solid Punch | Gallien-Krueger 800RB | Articulate punchy bass |
| 7 | Modern Bass | Darkglass B7K Ultra + Aguilar DB751 | Bass drive with a clean component |
| 8 | Chime 30 | VOX AC30 Top Boost | Bright clean-to-edge guitar |
| 9 | Orange Crown | Orange Rockerverb 50 MKIII | Dense low-mid guitar gain |
| 10 | Bassman Valve | Fender Super Bassman | Rounded bass fundamentals and soft breakup |
| 11 | Subway Clean | Mesa Subway D-800+ | Broad, high-headroom bass |
| 12 | Match Chime | Matchless DC-30 | Complex chime with firm mids |
| 13 | Silk ODS | Dumble Overdrive Special | Rounded, singing overdrive |
| 14 | Taste Punch | EICH T900 | Clear bass mids and open dynamics |

Indices 0–7 retain the earlier beta's voice data. Their one-capture comparisons, including Modern Bass's combined pedal/head chain, remain documented in [NAM_REFERENCE_RESULTS.md](NAM_REFERENCE_RESULTS.md). The new voices 8–14 have original coupling, gain, bias, envelope-sag, dry contribution, bandwidth and EQ parameters. Six voices (8–13) now have fixed NAM-file comparisons and a held-out chord check in [OPEN_BETA_NAM_VALIDATION.md](OPEN_BETA_NAM_VALIDATION.md). EICH (14) has no exact NAM reference, and calibrated hardware listening remains pending for all new voices. The previous eight-voice NAM result must not be presented as a validation of all fifteen voices.

### Primary design references

These sources establish the hardware controls and broad design context. The resulting DSP settings are our design choices, not measured hardware constants.

- [VOX AC30 Custom](https://voxamps.com/product/ac30-custom-amplifier/) and [owner's manual](https://voxamps.com/wp-content/uploads/2014/11/AC15C1_AC30C2_X_OM_EFGS3.pdf): Normal and Top Boost paths; interactive bass/treble and master Tone Cut. Chime 30 uses a bright, responsive nonlinear voice; it does not reproduce the complete interactive Top Boost tone stack.
- [Orange Rockerverb 50 MKIII](https://orangeamps.com/en-us/products/rockerverb-50-mkiii) and [series manual](https://orangeamps.com/wp-content/uploads/2017/05/Rockerverb-MKIII-Series-Manual-%E2%80%93-Orange-Amps.pdf): separate Clean and Dirty channels with dedicated gain/EQ controls. Orange Crown selects a thick gain-oriented voicing; its internal stage count is not a claim about the amplifier's circuit.
- [Fender Super Bassman](https://www.fender.com/products/super-bassman): tube bass head with Vintage and Overdrive channels, drive Blend, and Deep/Bright switches. Bassman Valve retains a clean contribution and low fundamental content. It is one continuous voice, not an implementation of both hardware channels and every switch.
- [Mesa Subway D-800+](https://subway.mesaboogie.com/amplifiers/bass/subway-series/subway-d800-plus/index.html): solid-state preamp, four-band active EQ, variable flat-to-scooped voicing and a Deep switch. Subway Clean uses a low-gain, broad-bandwidth voice; the hardware Voicing, variable high-pass, sweepable mids and Deep/Bright switches are not separately modeled.

- [Matchless C-30](https://www.matchlessamplifiers.com/amplifiers-and-cabinets/c-30): distinct 12AX7 and EF86 preamp channels, with a six-position tone switch on the EF86 side and a high-frequency Cut. Match Chime uses our own harmonically rich mid-forward voice; it does not implement either full channel circuit or the six switch positions. Any comparison against a Ceriatone DC-30 clone must identify that clone, not claim an actual Matchless capture.
- [Dumble Preservation Society](https://dumble.com/): official brand archive and Overdrive Special identity. Silk ODS is an original smooth-drive interpretation; no particular original Dumble serial number or component circuit is claimed. An ODS #102-style clone capture is a clone reference, not authenticated Dumble hardware.
- [EICH T900](https://www.eich-amps.com/t900) and [T-series manual](https://www.eich-amps.com/media/61/manual-t300-t500-t900.pdf): broad-range input stage, four-band EQ centred at 30 Hz, 250 Hz, 800 Hz and 8 kHz, plus the Taste contour. Taste Punch follows those EQ frequencies with an original low-gain voice; there is no separate Taste control and no exact T900 capture validation. A TecAmp capture must not be relabeled EICH.

Vanderkley remains a research candidate. We did not add an Aurora voice without an adequate primary reference or an actual capture.

### Validation and compatibility

`Tests/AmpVoiceTests.h` exercises all fifteen voices at 44.1, 48 and 96 kHz, 1x/4x/8x oversampling, 17- and 512-sample block partitions, identical and silent stereo channels, extreme tone settings, and invalid model indices. New voices are compared with all preceding voices after RMS normalization so a level change alone cannot satisfy the distinction check. The existing amplifier tests also exercise DC, input dynamics and oversampling alias rejection. These are numerical DSP checks, not a substitute for playing and listening.

Saved raw amplifier indices remain stable. Increasing the host choice count from 8 to 15 changes normalized automation mapping. An older DAW automation lane containing amp selections may therefore target different voices; review or re-record that automation when upgrading an earlier test build. Ordinary state recall and normalized host automation are different compatibility cases.

## Fuzz, boost and overdrive order

The default **Fuzz → Boost → Overdrive** is intentional: fuzz provides the first clipped texture, while boost controls the level and spectral balance entering overdrive. Increasing boost into an already clipping overdrive tends to add saturation and compression instead of producing a proportional volume lift.

The alternate **Fuzz → Overdrive → Boost** gives boost direct control over the overdrive's output level and tone. It is useful for a solo lift into a sufficiently clean amp. A saturated amp farther downstream can still turn this increase into more distortion, so it is not a guaranteed post-amp volume boost. Use lane Level or master Output when the intended adjustment is output level after amplifier processing.

Neither order is universally correct. [BOSS's boost/preamp guide](https://articles.boss.info/the-complete-guide-to-boost-and-preamp-pedals/) describes the before-drive saturation and after-drive level-lift roles. [BOSS's distortion-stacking guide](https://articles.boss.info/pedal-partners-combining-distortion-with-other-effects/) describes the effect of the downstream gain stage on stacked textures. [EarthQuaker Devices' EQ/boost guide](https://www.earthquakerdevices.com/blog-posts/eq-boost) likewise explains how a preceding booster pushes gain pedals.

These are digital audio modules. Moving their order does not recreate a physical Fuzz Face's pickup loading, impedance or guitar-volume interaction. The independent Envelope/Compressor choice remains useful: Envelope first follows the incoming playing dynamics; Compressor first gives the envelope detector a more controlled input.
