# Reference Model Development Policy

Chimera model names remain original/non-infringing. Real hardware is used as an engineering reference for topology, control behaviour, gain staging and frequency-response targets; UI artwork, trademarks and trade dress are not copied.

## Amp references

### Glass
Reference family: classic Fender Twin/Reverb clean architecture.
Targets:
- high clean headroom and gradual 6L6-style power compression
- passive Bass/Mid/Treble interaction
- bright voicing option later
- restrained preamp clipping; power-stage feel is more important than distortion
Reference facts: Twin Reverb family uses four 6L6 output tubes; classic controls include Volume/Treble/Middle/Bass and Bright.

### Tight 515
Reference family: Peavey 6505/6505 II lead architecture.
Targets:
- cascaded 12AX7-style high-gain stages
- passive 3-band tone stack after major gain generation
- 6L6-style power stage
- Presence and Resonance feedback controls
- tight asymmetric clipping rather than generic tanh
Reference facts: 6505 II is 120 W, 6x12AX7 + 4x6L6GC; Presence is specified around +10 dB @ 2 kHz and Resonance around cabinet resonance.

### Iron Tube
Reference family: Ampeg SVT-CL.
Targets:
- large low-frequency headroom with progressive tube compression
- selectable mid-frequency character
- Ultra-Lo / Ultra-Hi style voicing concepts
- 6550-like power-stage saturation
Reference facts: 2x12AX7 preamp, 12AX7/12AU7 driver complement, 6x6550, 300 W; tone reference points Bass 40 Hz, selectable mids 220/450/800/1600/3000 Hz, Treble 4 kHz.

### Solid Punch
Reference family: Gallien-Krueger RB architecture.
Targets:
- very fast solid-state transient response
- active serial 4-band EQ
- contour/presence voicing
- controlled upper-mid growl without tube sag
Reference facts: GK documents proprietary active serial variable-Q four-band EQ and contour/presence voicing in the RB family.

## Effect references

### Green Drive
Reference family: Ibanez Tube Screamer.
Controls: Drive / Tone / Level.
DSP direction: pre-emphasis -> soft/asymmetric clipping -> characteristic mid-focused post filtering. Bass variant can later add Mix/Bass/Treble inspired by TS9B control philosophy.

### Pi Fuzz
Reference family: Electro-Harmonix Big Muff Pi.
Controls: Sustain / Tone / Volume.
DSP direction: cascaded high-gain clipping stages -> passive-style bass/treble tone blend -> output level. Preserve long sustain and strong compression.

### Blue Chorus
Reference family: BOSS CE-2 / CE-2W.
Controls: Rate / Depth.
DSP direction: BBD-inspired modulated short delay, filtering/noise-bandwidth constraints, mono CE-2 character first; stereo extension later.

## Validation
Models should be validated against published manuals/specifications plus legally obtained reference recordings or hardware measurements. Do not claim component-exact emulation unless the implementation is actually derived and measured to that level.
