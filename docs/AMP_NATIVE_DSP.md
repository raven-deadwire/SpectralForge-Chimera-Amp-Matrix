# Native amplifier panels and six routing contexts

This revision connects the 23 amplifier panel definitions in `Preparation/amp_controls.py` to product parameters and authored DSP. The earlier fixed Drive/six-band editor is no longer the product's amplifier panel. The full original DSP path remains available internally for older project-state playback until a native panel/model/control is selected.

## Parameter and state contract

- 348 model controls are retained by their named panel keys. Channel-specific controls keep distinct values; physically shared controls have one shared value. Changing channel does not overwrite its neighbour or duplicate a shared EQ.
- Six independent banks represent Classic, Dual A, Dual B, Matrix Low, Matrix Mid and Matrix High. Every context retains its chosen model, that model's selected channel/input route, and all 23 model control banks.
- `nativeAmp_c{context}_m{model}_{control-key}` parameters are appended after every previously released parameter. Existing legacy choices, extension parameters and their normalized host positions are not repurposed.
- Native model/channel/input route selection reserves fixed raw integer ranges. Unsupported stored values are sanitized; incomplete older state is migrated by the processor rather than interpreted as arbitrary control values.
- Input Trim and Output Level are separately labelled software controls. Their range is -24 to +24 dB. They are not presented as controls found on every original amplifier.
- A software SOLO footswitch activates the extra Solo Level control on the three-channel rectifier and Cinder panels. It is not an extra gain knob on their hardware panels.

## Panel inventory

| Product model | Stored controls | Channel or input structure |
|---|---:|---|
| Glass | 13 | Normal / Vibrato; per-channel tone and brightness; onboard effects |
| Brit Edge | 6 | High Treble / Normal / jumped inputs; two input volumes |
| Tight 515 | 11 | Rhythm / Lead gain and volume; shared EQ and power controls |
| Wide Rect | 24 | Three independent panels and modes; shared output and solo |
| Liquid Lead | 36 | R1 / R2 shared bass-mid; separate Lead; five-band graphic and power modes |
| Iron Tube | 12 | Two channels; channel-one selectable mid; bright inputs |
| Solid Punch | 14 | Four-band EQ; voicing switches; separately controlled bi-amp bands |
| Modern Bass | 20 | B7K-style drive, blend and frequency switches into a separate bass preamp |
| Chime 30 | 10 | Normal / Top Boost; Cut, master, onboard reverb and tremolo |
| Orange Crown | 11 | Clean / Dirty panels; reverb, attenuator and power |
| Vintage Valve | 17 | Vintage / Overdrive; OD blend and variable mid; shared master |
| Metro Clean | 14 | High-pass, voicing, two variable mids and input type |
| Prism Chime | 9 | Three-knob channel one / six-position channel two; Cut and master bypass |
| Silk Lead | 11 | Clean / OD; separate OD Drive and Ratio; shared tone and voicing |
| Taste Punch | 8 | Gain, Taste contour, four-band EQ, Master and Mute |
| Cinder 120 | 42 | Four independent panels and buttons; common power, gate and solo |
| Iron Compact (legacy recall) | 12 | Clean / Lead; shared EQ; boost, reverb and power soak |
| Fourfold | 23 | Four independent gain/EQ/volume banks; shared power section |
| Classic Tube | 8 | Gain / Master; five-position mid selection and ultra switches |
| Monolith | 6 | Normal / Brilliant / jumped routes with separate input volumes |
| Night Harvest | 18 | Two distinct gain paths and clean; gain-channel shared Sweep EQ |
| Hot Lead | 11 | Normal Clean/Crunch and Overdrive; shared tone and power controls |
| Blue Storm | 12 | Clean / Lead, separate panels and lead presence; shared master |

The counts include controls stored for inactive channels; the editor displays the current channel's controls together with shared controls.

The 1.1.1 candidate appends **Special Edition / E670FE**, 32 controls and five paths.
The catalog is now **24 serialized / 23 active models, 380 controls**; previous
measurements below remain historical. See `E670FE_IMPLEMENTATION.md` for new
evidence, append-only parameter ordering and pending reference gates.

## Audio integration

`AmpNativeDSP` runs inside the amplifier's existing oversampling and latency-compensation paths. Its native controls replace the old generic Drive/six-band layer when a native bank is active. The host's selected oversampling remains effective, and the amplifier bypass retains the same compensated delay.

The reference panel inventory includes channel gain/volume stages, pre- or post-distortion tone stacks, input attenuation and jumped inputs, power feedback/depth/presence, shared equalizers, onboard reverb/tremolo, multi-band bass preamps and the documented panel switches. Input and output levels are separate from these panel controls. The native implementation is an authored digital interpretation of those signal-flow roles, not a collection of renamed six-band EQ controls.

Input-channel Volume controls can reach silence, whereas Gain controls retain the model's minimum gain. Channel/master controls precede power saturation; attenuators and software Output Level follow it. The two input branches, bass clean/drive blends, four- or five-stage gain routes, variable-mid filters and onboard effects are separate signal-flow operations.

Gain and level values slew over 12 ms. Input/output and tone biquad coefficients interpolate over 20 ms with independent stereo state and a 16-sample update clock; inserted/removed filters transition through identity. Model/channel/input-route selection crossfades two prepared circuits over 20 ms. The first edit that leaves an older project's internal engine also uses a 20 ms blend, with independent oversampling state and matched algorithmic delay on each side.

The reverb is a dark four-line feedback delay network evaluated near 24 kHz, with static delay lengths. It is a spring-like approximation, not a captured spring IR or a mechanical spring model. Turning the mix to zero keeps the tank advancing, avoiding a frozen tail reappearing later. Tremolo is separate amplitude modulation. The GK-style bi-amp branches are recombined into the plugin's existing output; separate physical high/low speaker jacks are not additional output buses.

All processing storage is prepared in advance. Amplifier circuit paths are constructor-owned heap storage rather than large stack members; no model/control update or processing call allocates new storage. The measured Linux ABI object sizes are 1,680 bytes for `Amp` and 11,248 bytes for `Engine`, compared with 66,112 and 204,544 before moving their prepared path storage. These are ABI-specific measurements, not Windows structure-size claims.

## Scope of reference claims

The panel target definitions retain their original evidence status and source links in `Preparation/amp_controls.py`. “Connected control” means the control affects the digital audio path. It does not mean the real component values, taper, channel response or hardware output have been measured.

These are original amplifier designs informed by the declared reference panels. They are not new NAM model weights or an assertion of circuit-identical reconstruction. Several panel revisions still need matching primary manuals or captures: the exact Multi-Watt rectifier revision, SUNN Model T generation, the ODS #102 clone manufacturer, B7K capture revision, SVT-CL numeric mid-frequency labels, and older Uberschall Rev Blue panel documentation. Their uncertainties are not silently resolved by adding the controls.

Physical mains/standby, service bias, physical speaker loads, wiring, MIDI hardware and effects-loop connectors are outside this audio plugin. Controls such as speaker-off/silent-recording select the plugin's audible speaker-output path; the physical unit's separate recording jacks are not supplied as additional plugin buses.

## Verification status

`Tests/AmpNativeTests.cpp` and `Tests/AmpNativeTransitionTests.h` passed the focused Linux/JUCE build on 2026-09-28:

| Check | Result |
|---|---|
| Catalog and defaults | 23 models, 348 controls, six contexts |
| Control-response fixtures with their route/enable conditions engaged | 348/348; minimum output RMS difference 0.0000142221 |
| Inactive channel controls | 364 isolation cases; no influence on another channel |
| Native channels at 44.1/48/96 kHz | 138 cases; finite output; peak 0.826977 |
| Amplifier integration with 1x/2x/4x/8x oversampling | 92 paths |
| Native state updates, sample processing and old/native engine transition | 0 monitored C++ heap allocations after prepare |
| Software +6 dB output level | RMS ratio 1.99526; distinct from input drive |
| Old/native first-sample transition fixture | Absolute difference 0.000458091 from continuing old audio |
| Gate with internal versus equivalent external input trim | Residual 0 at 44.1/48/96/384 kHz |
| Reverb hidden at zero mix then restored | Tail residual 0 |
| EQ edit first 15 samples | Residual 0 before coefficient update; changed response thereafter |
| Repeated filter insertion/removal during automation | Finite; peak 0.172631 |

These are synthetic numerical and state-isolation checks. They verify functioning controls, routing, bounded processing and regressions; they do not measure hardware matching or establish Studio One behavior. Processor state restoration, UI and Windows integration are separate release gates.

Final focused source hashes and command/output logs are recorded in the build evidence. The native DSP header tested here has SHA256 `24c8f25063e3625655bae071c4d6a224e915b86eeccd965f913598472d8e7df2`.
