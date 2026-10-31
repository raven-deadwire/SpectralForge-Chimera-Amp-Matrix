# Whole-product checks for 2.0 integration

Status: **on `claude/integration-1-10`**. Covered by `ChimeraWholeProductTests`.

Roadmap 2.0: "Whole-product host/audio/state/performance/preset/installer
integration and RC". Each feature has its own tests. This one puts every bank
appended since the released set (version hint 15 and up, 1109 parameters) to
work at once:
- Room Core / Advanced / spatial stage
- PRE expansion
- multiband stereo and six-band
- saturation
- reference heads
- filter modulation
- POST motion/space
- CREATIVE and SYNTH
- sidechain
- signal order
- the true-peak limiter

It covers the state, audio and performance parts that can be checked here.
Host, installer and target-hardware acceptance stay separate.

## What it checks

1. **State round trip:**
   - 6 seeds of random values for every appended parameter. Bool switches are random, or with "everything on" every `…On` switch is on and every bypass/solo/listen/mute is off. The signal order is a random valid permutation. The routing mode is Classic, Dual or Matrix.
   - After a project save and restore, every parameter matches, and the rendered audio is the same **bit for bit**: the largest difference is 0 for all 6 seeds.
   - An A/B round trip (copy to B, change everything, back to A) restores every parameter.
2. **Everything on in Matrix (3 seeds):**
   - Three rigs, with the Studio limiter on at −1 dBTP.
   - The output stays finite and under the ceiling (peaks 0.15–0.89).
   - It decays below −60 dBFS within 4 s of silence; measured −130 dBFS or lower in the last 100 ms.
3. **Worst-case CPU:** everything on in Matrix with the Studio limiter, on noise; see below.
4. **Sample-rate and block changes:**
   - 44.1 kHz/512, 96/64, 192/256, back to 48/128.
   - No parameter moves.
   - The Studio limiter's share of the latency follows the rate: 1 ms + 8 samples.

`CHIMERA_WHOLE_PRODUCT_PROFILE=1` runs a diagnostic instead of the tests:
- the cost of each switch, by turning it off alone;
- per-block time percentiles;
- the chosen settings.

## Measurements (this CI VM: Xeon 2.1 GHz, 4 cores, Release `-O3`)

| | 48 kHz / 64 | 96 kHz / 128 |
|---|---:|---:|
| Released Matrix defaults (no appended module) | 43% | 87–95% |
| Everything on, Matrix, Studio limiter | 60–71% | 121–133% |
| What the appended modules add | 20–28% | 30–46% |

For reference, Classic defaults measured 17% at 48 kHz and 35% at 96 kHz.

**Gates:** shared-machine speed varies, so these catch gross regressions only:
- everything on must run in real time at 48 kHz / 64;
- the appended modules must add less than three quarters of a core at 96 kHz / 128.

**Per block at 48 kHz / 64:**
- With everything on: median 59%, p99 91%, p99.9 116%, and about 12 of 2250 blocks over their period.
- The released defaults also have isolated blocks over 300%.
- The slow blocks are not periodic: their positions are random, and turning off SYNTH, Room, CREATIVE or Studio saturation does not remove them. They point to scheduling on this shared VM rather than to a module.

## Findings for 2.0

- **96 kHz Matrix exceeds one core here even before any appended module (87–95%).** The three-rig core engine at 96 kHz is the main cost. Everything on adds 30–46% on top. CPU acceptance at high rates on target hardware is still open. This was measured, not optimised.
- **Biggest appended costs** found by turning one switch off at a time (96 kHz):
  - Saturation (Studio 4×): about 6%.
  - Two-filter modulation: about 3%.
  - All others are within the ±10% run-to-run noise.
- **Random gain settings can stack** to +20–30 dBFS when the output limiter is off. That is a reachable session state, not instability: it decays and stays finite. The limiter (Live or Studio) holds it.
- **No state or audio defect found:** every appended bank saves, restores and A/B-switches exactly.

## Limits

- One machine, one thread. No DAW, plugin format, installer or multi-instance check.
- Random settings explore many combinations, but not all. Seeds are fixed, so runs repeat.
- The fixture is a plucked synthetic guitar and noise, not recorded DI.
