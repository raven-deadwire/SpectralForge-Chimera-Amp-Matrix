# CREATIVE — 1.9

Status: **implemented on `claude/creative-fx`**, stacked on
`claude/post-motion-space`. Roadmap 1.9 "Creative, nine". Covered by
`ChimeraCreativeTests`, with the inventory, native-state and signal-path tests
updated.

Owner's decisions (2026-10-29):
- the working names below, as proposed;
- a new CREATIVE slot in POST, after DELAY and before REVERB.

## The slot

- **Position:** one processor in the POST chain, between DELAY and REVERB
  (`PostFXChain`).
- **Model:** it holds one of nine models at a time.
- **Off (the default):** the signal is sample-exact and the slot costs nothing.
  Mix 0 % is also sample-exact, for every model.
- **Switching:** on/off fades the mix over 20 ms. A model change crossfades the
  old and the new voice over 30 ms.
- **Parameters:** appended at version hint 26. Hints 24 and 25 are Room
  Advanced on its own branch. All eight are automatable:
  - `creativeOn`
  - `creativeModel` (a new choice of nine)
  - `creativeA` / `creativeB` / `creativeC` (0–100 %; the meaning depends on the model)
  - `creativeMix`
  - `creativeSync`: Free, 1/2, 1/4, 1/8, 1/16, 1/8 T, 1/16 T
  - `creativeOutput` (−24…+12 dB)
- **Tempo sync:** the rhythmic models (Gate Slicer, Step Harmonizer, Glitch
  Stutter) follow `creativeSync` and the session tempo. With Free, their rate
  comes from A.
- **UI:**
  - The POST page has a CREATIVE row beside "RACK / AFTER RIG MERGE", with
    model, ON and ALL.
  - ALL opens the full panel. Its A / B / C labels follow the model.
  - The signal path shows a CREATIVE node between DELAY and REVERB. The
    bottom-row nodes after it moved one place left.
- **Artwork:** the slot has no rack artwork of its own; the panel uses the
  model's colour. This is listed under "Not done".

## Models

The references are internal development references, not public names.
HarPeggiator is added to the public-name policy's forbidden terms.

| # | Name | Reference | A | B | C |
|---|---|---|---|---|---|
| 0 | Gate Slicer | Slicer | Rate (sync or 1–20 Hz) | Depth | Duty 10–90 % |
| 1 | Step Harmonizer | HarPeggiator | Step rate (sync or 0.5–16 Hz) | Pattern: octaves / major / minor / fifths | Glide 1–101 ms |
| 2 | Time Bend | Time Warp | Cycle 0.05–4 Hz | Bend 0–±12 st | Shape: sine → tape-stop ramp |
| 3 | Zap Sweep | Laser | Sweep 20–600 ms | Trigger −60…−12 dBFS | Resonance Q 2–20 |
| 4 | Freq Shift | Frequency Shifter | Shift −1000…+1000 Hz (cubic, 0 at 50 %) | Feedback 0–90 % | Spread: right channel to the opposite shift |
| 5 | Freeze Hold | Dynamic Freeze | Hold (60 dB decay) 0.5–30 s | Trigger −60…−12 dBFS | Grain 60–400 ms |
| 6 | Glitch Stutter | Glitch | Step (sync or 1–16 Hz) | Repeat chance | Reverse chance |
| 7 | Bit Crush | Bit Crusher | Word length 16 → 2 bits | Rate 48 kHz → 500 Hz (hold) | Tone 20 kHz → 1 kHz (0 % = no filter) |
| 8 | Grain Cloud | Granular Texture | Density 2–60 grains/s | Pitch scatter 0–±12 st | Grain 20–200 ms |

### How they work

- **Pitch shifting (Step Harmonizer, Time Bend):**
  - Two delay-line heads, 50 ms windows overlapping by half with sin² gains.
  - A restarting head chooses its new delay, within 12 ms, by normalised
    cross-correlation with the playing head.
  - With fixed half-window offsets, the heads cancelled at frequencies where
    25 ms is an odd number of half periods. At 220 Hz the strongest component
    was then a 20 Hz sideband.
- **Freq Shift:**
  - Single sideband: two four-stage all-pass chains with a 90° phase
    difference, then quadrature mixing.
  - Moving the one-sample delay to the other chain took image suppression from
    18 dB to 49 dB.
- **Zap Sweep:**
  - An envelope onset (with 6 dB hysteresis) restarts an exponential
    band-pass sweep from 8 kHz to 150 Hz.
- **Freeze Hold:**
  - One grain after an onset, the last grain-length of audio is captured.
  - It loops through two crossfaded heads and decays by 60 dB over Hold.
  - A new onset recaptures.
- **Glitch Stutter:**
  - At each step, with the repeat chance, the previous step is played again
    (reversed with the reverse chance), with 3 ms edges.
  - The sequence is deterministic: the same input renders the same output.
- **Grain Cloud:**
  - Up to 24 Hann² grains read from the last 0.5 s at random positions, pans
    and pitches.
  - Their level is normalised to −3 dB of the input at any density (sparse
    clouds at most +6 dB of gain).

## Measurements (`ChimeraCreativeTests`, 48 kHz)

- **Off and Mix 0:**
  - Off is sample-exact and idle.
  - Mix 0 % is sample-exact for all nine models.
- **Gate Slicer:**
  - 1/8 at 120 BPM: 4.00 Hz.
  - Duty 0.48 (set 0.5; 2 ms edges).
  - Depth 100 %: closed below −500 dB. Depth 50 %: −6.02 dB.
- **Step Harmonizer:** major pattern on 220 Hz, steps of 0.5 s at 1/4:
  220.1 / 276.9 / 329.6 / 439.7 Hz (design 220.0 / 277.2 / 329.6 / 440.0).
- **Time Bend:** ±12 st on 220 Hz sweeps 109.9–439.8 Hz.
- **Zap Sweep:** spectral centroid 5.19 kHz at 20–50 ms, 1.07 kHz at 140–170 ms,
  and 161 Hz after the sweep. A new onset brings it back to 5.14 kHz.
- **Freq Shift:** +100 Hz on 1 kHz.
  - 1100 Hz at −0.003 dB.
  - Image (900 Hz) at −49.0 dB; the 1 kHz input at −148 dB.
  - Spread 100 % puts the right channel at 900 Hz, with 1100 Hz at −49 dB.
- **Freeze Hold:** after a 0.5 s tone stops, the hold is at −15.5 dB one second
  later. It decays at 16.2 dB/s (design 60 / 3.87 s = 15.5 dB/s).
- **Glitch Stutter:**
  - Chance 100 %: every 1/16 step equals the previous one, exactly (error 0
    over 57,110 samples), forwards and reversed.
  - Chance 0 % passes the input exactly.
- **Bit Crush:**
  - 2 bits give 5 levels; 8 bits give 231 levels on a sine.
  - Rate 100 % holds 96.5 samples (500 Hz = 96).
  - Tone 100 % takes 5 kHz down by 14.0 dB.
- **Grain Cloud:**
  - Scatter 0 %: level −2.9 dB (design −3), and the neighbours of 440 Hz at
    −72 dB.
  - Scatter 100 %: 440 Hz keeps 0.1 % of the power.
  - Density 20 % against 100 %: −0.7 dB.
- **Contract:**
  - A model sweep with extreme settings peaks at 2.79 and stays finite.
  - No allocation in the callback.
- **Processor and editor:**
  - In `ChimeraProcessor`, Bit Crush changes the output (up to 0.119 on a
    220 Hz tone).
  - The strip and the panel are attached, and the panel's labels follow the
    model.
  - The signal path shows the CREATIVE node with the selected model's name.

Test-side corrections made during development:
- The gate rate is counted between the first and last rise.
- Shifted pitch is measured by spectral peak, not zero crossings.
- The hold length is measured on noise, since a sine's consecutive holds can
  quantize to the same value.

Bugs the tests found in the DSP:
- The pitch-shifter cancellation.
- The all-pass delay on the wrong chain.
- An inexact Mix 100 % (`dry + m(wet − dry)`, now `dry(1 − m) + wet·m`).
- Grain levels that fell with density.

## Not done

- **Rack artwork:** a rack face for the CREATIVE slot.
- **Advanced controls:** per-model controls beyond A / B / C, such as custom
  step patterns, freeze on demand, and grain position.
- **Checks:** listening tests, and CPU measurement in a DAW.
