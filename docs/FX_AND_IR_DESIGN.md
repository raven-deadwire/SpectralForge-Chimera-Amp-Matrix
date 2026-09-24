# IR, DI and effects design decision

## IR location

Keep IR controls inline in each active amplifier lane: source, bypass, LOAD/drop, low/high cut, and file/status. The DSP remains **amp -> cabinet -> merge** regardless of which UI page is open. A separate cabinet page does not move IR processing to a global bus.

| Approach | Fit for Chimera | Decision |
| --- | --- | --- |
| Inline amp/cab controls, as in the Odeholm workflow | Compare the MID/HIGH or Dual rigs while keeping their cabinet assignments visible. One IR slot per rig fits the available space. | Implemented. |
| Dedicated cabinet page, as in Neural Archetype | Useful for multiple mic/IR slots, blend, distance, explicit delay and phase tools. Adds navigation for a simple one-slot loader. | Reserve for an expanded cabinet editor; no empty page in this build. |

Matrix LOW is a DI lane. It has no amplifier or cabinet controls in this mode, and dropped IR files are ignored on that card. Its saved amp/cab controls and user IR remain available when switching back to Classic/Dual. The MID/HIGH IRs stay independent. IR loading does not increase the number of amp models.

## Mode responsibilities

| Module | Classic | Dual | Matrix |
| --- | --- | --- | --- |
| Input trim, gate, transpose | Once before rig | Once before split | Once before clean/driven branches |
| Pre Drive | Before rig | Once before both rigs | Before MID/HIGH; aligned dry tap feeds LOW |
| LOW COMP | Inactive | Inactive | LOW DI only, after split and Band Tone |
| Amp and cabinet | One full-range rig | Two full-range rigs, summed 50:50 | MID/HIGH only; LOW remains DI |
| Tone controls | Full EQ | Full EQ per rig | Crossover-relative Band Tone per band |
| Delay then Reverb | Once after rig | Once after merge | Once after merge, including LOW DI |
| Output trim/tuner mute | Global | Global | Global |

The same fixed pre-drive and amp delay is present in the LOW DI path. No extra compression lookahead is used. The LR4 crossover/allpass phase response and the natural onset of loaded IRs are distinct from the reported processing latency. Different IRs are not automatically phase-matched.

## One-knob LOW COMP

An original VCA-style feed-forward gain control with linked stereo RMS detection; not a dbx circuit emulation. COMP x ranges from 0 to 1:

- Threshold: `-12 - 36*x` dBFS RMS; ratio: `1 + 7*x`.
- 6 dB quadratic soft knee; 30 ms RMS smoothing; 10 ms gain attack; 140 ms release.
- COMP 0 is unity. No automatic makeup gain; use LEVEL to match loudness. The card displays gain reduction.
- Stereo detection follows the larger per-sample channel power and applies the same gain to both channels, preserving polarity and balance.

## Pedalboard and rack

PRE displays three stomp modules with independent bypass: **Noise Gate -> Transpose -> Tight Drive**. The fixed order is also the actual module chain; modules are not draggable/reorderable. Gate and transpose have linked quick controls in the global strip. Tight Drive is an original 4x-oversampled saturator with input high-pass, tone low-pass and output level.

POST displays two rack units: **Stereo Delay -> Room Reverb**. They operate once after merge in every mode. Delay exposes milliseconds, feedback and mix; Reverb exposes room size, damping and mix. Current reverb uses JUCE's reverb engine. The rack form does not imply a Lexicon/TC algorithm clone. Current delay has no tempo sync, ducking or modulation.

## Reference review and next acceptance gates

| Role | Reference to study | Current status and next measurable criterion |
| --- | --- | --- |
| Low-band dynamics | dbx 160/560A RMS/VCA and soft-knee behavior; Parallax low-band dynamics | Original one-knob RMS control implemented. Static curve, linked stereo and zero-control unity measured; evaluate bass attack/release with the same recorded DI. |
| Tight boost | Ibanez TS808 Drive/Tone/Level workflow | Generic drive implemented. A measured TS-style model would need input-level calibration, frequency response and clipping/intermodulation references. |
| Aggressive distortion | BOSS HM-2W and MT-2-style character in Slam | Review candidates, not extra labels on the current saturator. Choose one only after matched reamp sweeps/DI expose a useful, repeatable difference. |
| Rack delay | TC 2290 workflow | Basic stereo echo implemented. Ducking, modulation and tempo sync are future behaviors requiring separate acceptance tests. |
| Rack reverb | Lexicon PCM92 workflow | Basic room implemented. Dedicated diffusion/early-reflection/decay controls require impulse/decay references and listening tests. |
| Post tone shaping | Parametric EQ | Defer until DI/IR A/B identifies a correction that cannot be handled by the existing cuts and tone controls. |

## Official sources reviewed

- Slam visible input-to-cab chain and external IR loading: https://odeholm-audio.com/products/slam-amp
- thall workflow: https://odeholm-audio.com/products/thall-amp
- Dedicated cab and pre/post sections: https://neuraldsp.com/manual/archetype-tim-henson-x
- Low-band compression and upper-band distortion: https://neuraldsp.com/plugins/parallax
- VCA and knee reference: https://dbxpro.com/en-US/products/560a and https://dbxpro.com/en-US/compression-quiz
- TS808: https://www.ibanez.com/usa/products/detail/ts808_99.html
- HM-2W: https://www.boss.info/global/products/hm-2w/
- TC2290-DT: https://www.tcelectronic.com/en/products/0815-aak
- PCM92: https://lexiconpro.com/en-US/products/pcm92

References guide control behavior and future validation. No copied artwork, commercial IRs, presets or claims of verified hardware equivalence are included.
