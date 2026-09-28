# Production POST native panels and DSP

This change connects all nine previously prepared compressor/preamp/EQ panels to
production APVTS parameters and actual audio processing. Each section has three
independent model banks, bypass, software input trim and output level. Selector
names are aliases. Hardware references remain panel information.

| Section | Alias | Reference | Connected behavior |
|---|---|---|---|
| Dynamics | Console VCA | SSL XLogic G Series, 2005 Rev 0A | Stepped attack/release/ratio, threshold/makeup, compressor IN, timed autofade and recovery |
| Dynamics | FET Limiter | UA 1176LN D/E-style reissue | Input/output, clockwise-faster attack/release, attack OFF, four ratios plus nonlinear All Buttons response, GR/output/OFF meter |
| Dynamics | Optical Level | UA Teletronix LA-2A reissue | Gain, peak reduction, compress/limit, program-dependent release memory, HF-emphasis detector and output/GR meter |
| Preamp | Iron Colour | Neve 1073 preamp section | MIC/LINE gain positions, OFF mute, polarity, independent transformer-inspired saturation/filtering |
| Preamp | Pure DI | Avalon V5, provisional panel | Input voicings, 21 boost positions, ten authored tone curves, tone IN, high cut, polarity and pad |
| Preamp | Blue Channel | Focusrite ISA One | MIC/LINE/INST gain routing, gain-range switch, trim, separate instrument gain, polarity, HPF, input/output meter source |
| EQ | Console Four | SSL E Series 500 EQ | Four independent bands, HF/LF shelf-to-bell, both mid-band Qs, Brown/Black bandwidth/gain behavior and IN |
| EQ | Inductor EQ | Neve 1073 complete EQ section | 12 kHz HF shelf, six MF choices, four LF choices, four HPF frequencies plus OFF, EQL |
| EQ | Passive Tube | Pultec EQP-1A | Simultaneous low boost/attenuation, high boost bandwidth, independent high attenuation frequency, IN |

These are authored digital implementations of the reviewed control topology,
not measured component models or calibrated hardware transfer curves. In
particular V5 input/tone curves, transformer coloration, All Buttons behavior,
optical memory, EQ bandwidth laws and Pultec interaction are approximations.
Pultec tube-amplifier distortion is not claimed. Continuous parameter ranges
are explicit software ranges, not verified hardware potentiometer tapers.

## Digital gain and meter conventions

The software trim and level controls are separate from original panel controls.
Preamp microphone gains use nominal digital input offsets to avoid directly
applying a physical microphone's 80 dB gain to a line-level track: Iron Colour
MIC positions have a −40 dB digital reference offset; Blue Channel MIC has
−30 dB, LINE uses its −20…+10 dB range, and INST uses gain relative to 10 dB.
Pure DI's 21-position Boost is authored at 2 dB per step (0…40 dB), not a claim
about V5 measured gain. Its MIC +48 V position shares a MIC voicing and supplies
no phantom power.

Console Four's common gain position range is ±18; Brown maps its endpoints to
±15 dB and Black to ±18 dB. This permits one stable host parameter range while
preserving the mode-specific gain span. EQ filter frequency is limited below
Nyquist at low sample rates.

Compressor meters read actual applied gain reduction or output. The digital
VU convention is −18 dBFS = +4 reference; +8 and +10 modes offset that readout by
4 and 6 dB. It does not establish an interface's physical dBu calibration.
FET meter OFF smoothly bypasses its processing and blanks the meter, serving
as a software power proxy. Attack OFF disables compression but preserves the
amplifier path. Blue Channel's POST INSERT switch changes input/output metering;
there is no physical external insert.

## Physical-only controls

Console VCA external sidechain and Blue Channel microphone/instrument impedance,
phantom power and physical insert controls are visible hardware information,
disabled and explicitly annotated. No external sidechain audio bus, microphone
source impedance model or phantom-power generator is pretended. The processor
uses its internal stereo-linked detector. Cue/headphone routing, ADC clocks,
hardware stereo calibration, meter-zero screws and mains switches are omitted.

## State, timing and realtime contract

`pn_bus_*`, `pn_preamp_*` and `pn_eq_*` are appended host IDs. Each model owns its
controls, bypass and trims. Previously shipped parameter IDs, order and normalized
ranges are untouched. New instances use native sections with every model bypassed.
Projects lacking native parameters keep the previous signal path internally;
selecting or editing a current native panel activates it directly. No legacy
control panel or bank-selection dialog is needed.

Sections switch models with a 20 ms crossfade. Knobs/gain/bypass smooth over a
12 ms one-pole response; EQ coefficients interpolate without heap allocation.
Native/compatibility transitions crossfade for 20 ms. Inactive models reset their
filter history before re-entry. The native preamp carries the same fixed
compensation delay as the existing preamp; host latency does not jump. MOD,
DELAY and REVERB retain their existing twelve DSP models and audio placement.

Nonlinear preamp coloration currently operates at the project rate. Its bounded
algebraic transfer is not oversampled; high-frequency alias performance has not
been certified. POST's existing reported delay is retained for alignment rather
than being misrepresented as new oversampling. Output guards only bound extreme
states; large positive EQ/trim settings can still overload downstream stages.

## Validation

`ChimeraPostNativeTests` checks all 64 audio controls and three meter controls with
context-appropriate min/max renders, nine-model stereo isolation, host-block
partition invariance, exact settled bypass, fixed reported preamp latency and
288 extreme automation/model-switch blocks over 44.1/48/96 kHz. It verifies
signal connection, not physical-hardware fidelity. `PostNativeAllocationTests`
intercepts malloc/calloc/realloc/free and C++ new/delete around the complete
POST callback, including compatibility/native transitions and all nine models.

Actual command outputs belong in the build evidence; Windows editor/DAW claims
require their separate jobs and user-host checks.

## Primary sources reviewed

- [SSL XLogic G Series compressor manual](https://www.solid-state-logic.co.jp/docs/XLogic_G-Comp.pdf)
- [UA 1176LN manual](https://media.uaudio.com/assetlibrary/1/1/1176ln_manual.pdf)
- [UA LA-2A manual](https://media.uaudio.com/assetlibrary/l/a/la-2a_manual.pdf)
- [Neve 1073](https://www.ams-neve.com/outboard/1073-range/1073-mic-preamp-equaliser/)
- [Focusrite ISA One controls](https://userguides.focusrite.com/hc/en-gb/articles/17525637995538-ISA-One-Controls-and-Features)
- [SSL E Series 500 EQ guide](https://www.solidstatelogic.com/assets/uploads/downloads/SSL_500_Series_E_EQ_Module_User_Guide.pdf)
- [Pultec EQP-1A specifications](https://pulsetechniques.com/products/tube-equalizers/eqp-1a/)
- Avalon V5 historical PDF remains unavailable through the retrieval service;
  its panel remains provisional as documented in `Preparation/RACK_PREPARATION.md`.
