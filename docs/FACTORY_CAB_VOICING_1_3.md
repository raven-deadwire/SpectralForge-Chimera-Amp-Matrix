# Factory CAB voicing — 1.3

Release 1.3 keeps all 48 factory preset ordinals, names and category navigation. Each factory recall now supplies an explicit modeled cabinet, compatible speaker, microphone, pickup unit and visible microphone level. The four snapshot builders share [FactoryCabVoicing.h](../Source/FactoryCabVoicing.h); loading a saved project or user preset does not call that factory revoicing function.

## Recall and routing

- Factory recalls use the independently authored Chimera CAB model. They do not scan for or select private/reference IR filenames. No factory preset requires an external IR file.
- Existing user-imported IR audio, paths, sidecars and A/B project resources retain their normal recall behavior. Factory recall selects its own modeled CAB without deleting the user's files.
- Classic uses one rig, Dual uses two, and Matrix uses three bands. The authored dry LOW in Bass Matrix, Low B Foundation, Pick Attack Matrix, Spectral Texture, Blackhearted and Dark Matters of Throne remains bypassed. Signature LOW amp blends and Slam Impact's amplified LOW keep their existing routing roles.
- Mic A is active on every modeled path. Mic B has an authored alternate microphone and neutral delay/polarity but starts disabled, with the A/B blend at A. This avoids introducing an unmeasured second-microphone phase blend into the existing presets.
- CAB low/high cuts, PRE drive, native amp voicing, crossovers and spatial effect settings retain each preset's existing musical role. Only the explicit CAB recipe and measured level compensation are owned by this revision. The global OUTPUT contract remains 0 dB.

## Cabinet assignment

Numbers below are stored preset ordinals, not positions in the category-sorted menu. Each active cell is `cabinet · speaker · Mic A`. A dry cell means CAB bypass; it is not an IR dependency.

| ID | Preset | Routing | Rig A / LOW | Rig B / MID | HIGH |
| --- | --- | --- | --- | --- | --- |
| 0 | Clean Sustain | Classic | Guitar 1x12 · Silver 12 · Prism 414 | — | — |
| 1 | Tight Rhythm | Classic | Guitar 4x12 · Ember 30 · Needle 57 | — | — |
| 2 | Bass Matrix | Matrix | Dry / CAB bypass | Bass 6x10 · Vector 10 · Spear 441 | Bass 4x10 · Alloy 10 · Needle 57 |
| 3 | Filter Lead | Classic | Guitar 2x12 · Ember 30 · Focus 160 | — | — |
| 4 | Fuzz Texture | Classic | Guitar 4x12 · Crimson 12 · Veil 7 | — | — |
| 5 | Bell Clean | Classic | Guitar 2x12 · Silver 12 · Pencil 184 | — | — |
| 6 | Edge Chime | Classic | Guitar 2x12 · Verdant 25 · Spear 441 | — | — |
| 7 | Classic Crunch | Classic | Guitar 2x12 · Verdant 25 · Needle 57 | — | — |
| 8 | Orange Heavy | Classic | Guitar 4x12 · Crimson 12 · Veil 7 | — | — |
| 9 | Melodic Death Rhythm | Classic | Guitar 4x12 · Ember 30 · Needle 57 | — | — |
| 10 | Melodic Lead | Classic | Guitar 2x12 · Ember 30 · Focus 160 | — | — |
| 11 | Ambient Clean | Classic | Guitar 2x12 · Silver 12 · Prism 414 | — | — |
| 12 | Finger Round | Classic | Bass 1x15 · Monolith 15 · Anchor 20 | — | — |
| 13 | Pick Punch | Classic | Bass 4x10 · Alloy 10 · Needle 57 | — | — |
| 14 | Slap Studio | Classic | Bass 4x10 · Vector 10 · Anchor 20 | — | — |
| 15 | Modern Grind | Classic | Bass 6x10 · Foundry 10 · Hammer 421 | — | — |
| 16 | Vintage Bass DI | Classic | Bass 1x15 · Monolith 15 · Anchor 20 | — | — |
| 17 | Wool Bass Fuzz | Classic | Bass 6x10 · Foundry 10 · Hammer 421 | — | — |
| 18 | Bass Envelope | Classic | Bass 2x10 · Foundry 10 · Anchor 20 | — | — |
| 19 | G+G Clean / Crunch | Dual | Guitar 1x12 · Silver 12 · Prism 414 | Guitar 2x12 · Verdant 25 · Needle 57 | — |
| 20 | G+G Tight / Wide | Dual | Guitar 4x12 · Ember 30 · Needle 57 | Guitar 4x12 · Granite 55 · Hammer 421 | — |
| 21 | G+B Low Anchor | Dual | Bass 6x10 · Vector 10 · Spear 441 | Guitar 4x12 · Ember 30 · Needle 57 | — |
| 22 | G+B Air / Weight | Dual | Bass 1x15 · Monolith 15 · Anchor 20 | Guitar 2x12 · Verdant 25 · Spear 441 | — |
| 23 | B+B Warm / Definition | Dual | Bass 1x15 · Monolith 15 · Anchor 20 | Bass 4x10 · Vector 10 · Anchor 20 | — |
| 24 | B+B Clean / Grind | Dual | Bass 2x12 · Chimera Depth 12 · Anchor 20 | Bass 6x10 · Foundry 10 · Hammer 421 | — |
| 25 | Low B Foundation | Matrix | Dry / CAB bypass | Bass 6x10 · Foundry 10 · Hammer 421 | Bass 4x10 · Vector 10 · Anchor 20 |
| 26 | Pick Attack Matrix | Matrix | Dry / CAB bypass | Bass 6x10 · Foundry 10 · Hammer 421 | Bass 4x10 · Alloy 10 · Needle 57 |
| 27 | Spectral Texture | Matrix | Dry / CAB bypass | Guitar 2x12 · Verdant 25 · Spear 441 | Guitar 4x12 · Crimson 12 · Veil 7 |
| 28 | Matchless Edge | Classic | Guitar 2x12 · Verdant 25 · Spear 441 | — | — |
| 29 | Dumble Smooth Lead | Classic | Guitar 2x12 · Ember 30 · Focus 160 | — | — |
| 30 | Modern Clean | Classic | Bass 1x12 · Clarity 12 · Prism 414 | — | — |
| 31 | Crom Cruach | Matrix | Bass 2x12 · Chimera Depth 12 · Anchor 20 | Bass 6x10 · Foundry 10 · Hammer 421 | Guitar 4x12 · Nocturne 100 · Needle 57 |
| 32 | Wild Hunt | Matrix | Bass 4x10 · Vector 10 · Anchor 20 | Bass 4x10 · Alloy 10 · Spear 441 | Guitar 4x12 · Nocturne 100 · Needle 57 |
| 33 | Azhi Dahaka | Matrix | Bass 6x10 · Foundry 10 · Anchor 20 | Bass 6x10 · Foundry 10 · Hammer 421 | Guitar 4x12 · Steel 75 · Needle 57 |
| 34 | A Path To Alsatia | Dual | Guitar 2x12 · Ember 30 · Focus 160 | Guitar 4x12 · Ember 30 · Needle 57 | — |
| 35 | Feel My Wrath | Dual | Guitar 4x12 · Ember 30 · Needle 57 | Guitar 4x12 · Steel 75 · Needle 57 | — |
| 36 | Blackhearted | Matrix | Dry / CAB bypass | Guitar 4x12 · Ember 30 · Needle 57 | Guitar 4x12 · Nocturne 100 · Needle 57 |
| 37 | Dark Matters of Throne | Matrix | Dry / CAB bypass | Guitar 4x12 · Steel 75 · Needle 57 | Guitar 4x12 · Nocturne 100 · Needle 57 |
| 38 | Thall Rhythm | Classic | Guitar 4x12 · Chimera Ruin 12 · Chimera Strike | — | — |
| 39 | Molten Lead | Classic | Guitar 2x12 · Ember 30 · Focus 160 | — | — |
| 40 | Rotten Grind | Dual | Guitar 4x12 · Crimson 12 · Veil 7 | Guitar 4x12 · Chimera Ruin 12 · Chimera Strike | — |
| 41 | Sludge Mass | Classic | Guitar 4x12 · Granite 55 · Hammer 421 | — | — |
| 42 | Slam Impact | Matrix | Guitar 4x12 · Chimera Ruin 12 · Chimera Strike | Guitar 4x12 · Crimson 12 · Veil 7 | Guitar 4x12 · Nocturne 100 · Needle 57 |
| 43 | Frostline Precision | Classic | Bass 4x10 · Vector 10 · Anchor 20 | — | — |
| 44 | Carrion Barrage | Classic | Bass 6x10 · Foundry 10 · Hammer 421 | — | — |
| 45 | Foundry Pulse | Classic | Bass 4x10 · Alloy 10 · Spear 441 | — | — |
| 46 | Jötunn Hammer | Classic | Bass 6x10 · Foundry 10 · Anchor 20 | — | — |
| 47 | Mirebound Monolith | Classic | Bass 1x15 · Monolith 15 · Anchor 20 | — | — |

## Pickup and enclosure choices

Open-back 1×12/2×12 Silver and Verdant designs serve the clean/chime roles. Closed 2×12/4×12 designs serve lead, crunch and high-gain roles. Bass recipes use the corresponding 10-, 12- or 15-inch speaker family; a layout never stretches an incompatible driver to fit.

Bass 6×10 uses six sources in two columns and three rows, with the same 63 cm width and the 94 cm enclosure height authored for the 1.3 model. Its 178.5-litre net volume preserves the former preview's volume per driver while the source positions and enclosure modes follow the six-driver geometry. This is reuse of the existing authored speaker/response model, not a newly trained or measured hardware capture. The factory pickup faces unit 3 (middle left), so it never selects one of the removed 8×10 bottom-row units.

Factory Mic A distances are 6–8 cm. Cone positions are 25–42%, with Thall Rhythm (38) using a centred 0% Strike pickup at 6 cm and −5.0 dB. Its Ruin 12 / closed 4×12 assignment is retained. Alternate Mic B starts at 10 cm / 50%, disabled. Tweeters are off for warm/vintage/heavy recipes and low-level for the selected modern/clean bass recipes. These are starting settings; both microphones, drivers and cabinet controls remain editable.

## Level evidence

Mic-stage compensation is measured through the production processor and its actual factory-recall function. The previous 48-preset levels at source `40d5e8a5ea466eaccc1044ccefce1bd33e6ac222` provide a reference for each preset's relative loudness; the presets are not all normalized to one RMS target. Peak headroom takes priority where matching the old RMS would overload the new full rig. PRE drive, amp gain and the global OUTPUT setting are not used for this compensation.

Final acceptance retains the existing 48-preset synthetic-fixture gates: OUTPUT = 0 dB, RMS above −30 dBFS, peak below 0.95, and +6 dB input peak below 0.95. Measurements include the first output sample at 48 kHz, 128 samples per block and 450 blocks per render. No warmup interval is discarded. The dirty-state/clean-state audio comparison and preset/A-B round trips must also pass. Synthetic fixtures establish signal safety and deterministic recall; real instrument/DAW listening remains a separate acceptance item.

Preparation now installs the saved CAB gain, polarity, delay, blend and bypass before initializing its smoothers. It also supplies the complete current native amp state before initializing the native/legacy engine mix. This makes a fresh processor and a re-prepared processor start from the same selected rig. Parameter values and live 20 ms gain/engine transitions retain their existing contracts.

### Completed local calibration

The complete strict default `ChimeraIntegratedProcessorTests` run finished with **exit 0** in **871.09 seconds** at **2026-10-09 16:41:26 UTC**, without diagnostic flags or a preset subset. The production-recall render completed **48/48** nominal and **48/48** hot-input preset checks with all limits unchanged. The lowest RMS is **−27.7529 dBFS** (Slap Studio). The largest nominal and +6 dB-input peaks are **−0.734963 / −0.899584 dBFS** (Orange Heavy), both below the 0.95 linear peak limit (approximately −0.4455 dBFS).

The strict default five-channel CAB comparison also passed with the centred Thall Rhythm pickup. K-weighted spread is **2.32495 dB** without the PRE/POST drive path and **1.54410 dB** with it, within the unchanged **2.5 / 1.6 dB** limits. The largest peak across these ten channel renders is **−0.987072 dBFS**. Only preset 38's microphone position and compensation were changed for this correction; the amp channels and test limits retain their previous definitions.

All five Niflheimr presets also passed binary state and A/B audio round trips with a maximum sample difference of **0**. The separate first-sample CAB regression passed for both microphones, including −12 dB/inverted/4 ms startup, stop/switch/reprepare at −6 dB/7 ms, and the live 20 ms gain ramp.

This local executable compiles the real production audio/state code against JUCE 8.0.8; its only processor substitutions omit the editor include and return no editor. It does not establish native UI, installer, host performance or musical listening acceptance. The final native three-platform CI must execute the committed source; its results are recorded in the integration PR.

The microphone gain below applies to each active modeled CAB lane of that preset; bypassed LOW and inactive lanes retain 0 dB. Values shown are actual full-processor measurements, with output trim 0 dB throughout.

| ID | Mic gain (dB) | RMS (dBFS) | Peak (dBFS) | Peak at +6 dB input (dBFS) |
| --- | ---: | ---: | ---: | ---: |
| 0 | +1.7 | -26.4747 | -9.5248 | -6.2102 |
| 1 | -1.2 | -9.8286 | -1.7810 | -1.8129 |
| 2 | +3.7 | -20.9107 | -7.3387 | -3.6232 |
| 3 | -3.6 | -16.0875 | -6.1241 | -6.1170 |
| 4 | -9.6 | -23.6992 | -15.0183 | -14.9994 |
| 5 | +0.3 | -24.4630 | -10.0176 | -7.8755 |
| 6 | -0.9 | -20.3793 | -10.8246 | -10.2989 |
| 7 | -5.6 | -24.8098 | -10.0015 | -8.8552 |
| 8 | +2.4 | -12.9652 | -0.7350 | -0.8996 |
| 9 | +2.3 | -14.1351 | -4.4813 | -4.3963 |
| 10 | -5.7 | -21.2758 | -11.7710 | -11.7559 |
| 11 | +1.6 | -24.1670 | -9.3547 | -6.7330 |
| 12 | +9.5 | -19.6977 | -3.6896 | -2.0393 |
| 13 | +3.6 | -22.7917 | -6.8227 | -5.4873 |
| 14 | +5.8 | -27.7529 | -14.3242 | -11.8000 |
| 15 | -0.7 | -17.5552 | -4.6590 | -3.6953 |
| 16 | +10.4 | -21.0958 | -4.9820 | -2.8095 |
| 17 | +0.6 | -18.5220 | -7.4877 | -6.2354 |
| 18 | +6.8 | -25.6302 | -10.7999 | -5.0034 |
| 19 | -9.0 | -26.3702 | -12.8613 | -11.4096 |
| 20 | +2.9 | -14.4200 | -5.1453 | -5.0983 |
| 21 | -1.5 | -20.2564 | -8.8964 | -5.8325 |
| 22 | +7.1 | -20.0800 | -4.4850 | -2.0488 |
| 23 | +9.9 | -22.2420 | -6.1917 | -5.0209 |
| 24 | +4.7 | -20.0188 | -6.2186 | -2.3635 |
| 25 | +0.7 | -19.7621 | -6.9647 | -3.6027 |
| 26 | +1.9 | -19.7521 | -4.6294 | -2.1263 |
| 27 | +3.7 | -25.2307 | -10.0444 | -6.8129 |
| 28 | -0.7 | -23.4388 | -9.3284 | -6.9955 |
| 29 | -5.7 | -19.9016 | -8.4367 | -8.5921 |
| 30 | +9.8 | -26.7277 | -8.8477 | -7.3501 |
| 31 | +4.3 | -23.7159 | -11.8416 | -12.2500 |
| 32 | +6.2 | -20.7825 | -4.2911 | -2.5622 |
| 33 | +4.7 | -22.9594 | -8.0198 | -5.1028 |
| 34 | -1.4 | -16.6865 | -4.2073 | -5.1897 |
| 35 | -7.3 | -20.2428 | -8.5835 | -8.3011 |
| 36 | -4.8 | -19.4629 | -7.5506 | -5.9557 |
| 37 | -4.1 | -22.6936 | -10.7589 | -9.3662 |
| 38 | -5.0 | -15.9636 | -0.9595 | -2.4328 |
| 39 | -6.5 | -17.3265 | -3.7617 | -3.7078 |
| 40 | -5.7 | -11.9118 | -1.2764 | -1.3732 |
| 41 | -13.7 | -17.9364 | -8.3666 | -8.8306 |
| 42 | -5.0 | -18.1373 | -3.3106 | -3.3269 |
| 43 | +4.8 | -24.2670 | -9.6474 | -6.3875 |
| 44 | +4.4 | -24.6704 | -11.3063 | -9.6680 |
| 45 | +5.6 | -24.5100 | -9.2882 | -5.3355 |
| 46 | +3.3 | -24.4357 | -11.4629 | -8.8159 |
| 47 | +10.1 | -24.7331 | -13.1380 | -7.2432 |

Measured source SHA-256:

- `Source/PluginProcessor.cpp`: `7e7db793b3c7ea079258b133bbf14627e20c3ca807514d87079a3a06e5e5084b`
- `Source/ChimeraDSP.h`: `ee35949cdf33f3d47b37f98da93ebde0d2df85f7041cea210f083d9030b5aadd`
- `Source/Cabinet.h`: `af4add9ab5ad3c5da475acfe093e1d3a0f484208578c2124af6c1b14156f157a`
- `Source/FactoryCabVoicing.h`: `4b4fc32933ecd22dbb7168a74da24dfb532b1fc09cc9da1d875afc5c447a23e2`

The complete 48-row calibration comparison (`pass3-full-level-comparison.csv`) has SHA-256 `7c4671c9ceca65dbf19b1acd26c19923e0cd4cddbf832cf633181ea1d1e4323c`. The unchanged historical level reference is the Linux product fixture from [run 37903125012](https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/actions/runs/37903125012), source `40d5e8a5ea466eaccc1044ccefce1bd33e6ac222`. It is a comparison baseline, not acceptance evidence for the new source.

The complete default-suite log (`full-default-pass3.log`) has SHA-256 `9fba2f4f5c4bed35977a3f31cdd65bf9965e3e11a3769eaa9ecfadbbec8945f6`. Its completion record (`full-default-pass3-pass.json`, SHA-256 `7d4351303a87dd491648da01a4709fe1640ca40d120fc7ac8a7268d76e3dff4d`) records exit 0, the measured source header, all 48 nominal / 48 hot / 10 channel rows and five final Niflheimr passes.

## Source contracts

- [FactoryNativeVoicing.h](../Source/FactoryNativeVoicing.h): original 31 + three Deadwire snapshot recalls, including preserved performance controls and inactive PRE banks.
- [GuitarSignaturePresets.h](../Source/GuitarSignaturePresets.h): four Raven guitar full-rig snapshots.
- [OriginalPresets.h](../Source/OriginalPresets.h): five Náströnd full-rig snapshots.
- [NiflheimrPresets.h](../Source/NiflheimrPresets.h): five Niflheimr full-rig snapshots.
- [IntegratedProcessorTests.cpp](../Tests/IntegratedProcessorTests.cpp): actual production processor recall and 48-preset level fixtures.
- [NiflheimrPresetTests.h](../Tests/NiflheimrPresetTests.h): Niflheimr binary export/restore and A/B checks.
- [CabPreparedStateTests.h](../Tests/CabPreparedStateTests.h): first-sample microphone gain/polarity/delay, stop/switch/reprepare and live gain-ramp checks.
