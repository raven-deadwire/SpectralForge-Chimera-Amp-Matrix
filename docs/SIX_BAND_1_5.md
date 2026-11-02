# Six-band multiband dynamics — 1.5

Status: **implemented on `claude/six-band-1-5`**, stacked on
`claude/modulation-1-9`. This is the roadmap's 1.5 "six-band dynamics
expansion" (a separate performance gate). Covered by `ChimeraSixBandTests`,
the existing multiband tests and the common-dynamics UI test.

## Controls

25 parameters are appended at version hint 31, after every other bank. All
are automatable.

| Parameter | Range | Default |
|---|---|---|
| `mbBandCount` | 4 bands, 6 bands | 4 bands |
| `mbCrossover4`, `mbCrossover5` | 40–16000 Hz | 6000 Hz, 11000 Hz |
| `mbBand5_*`, `mbBand6_*` | The same 11 fields as bands 1–4 (bypass, mode, threshold, range, ratio, attack, release, knee, makeup, solo, channels) | As bands 1–4 |

- **Layout:** six bands keep crossovers 1–3 and add crossovers 4 and 5 above
  crossover 3. With the defaults the bands are <120, 120–700, 700–3500,
  3500–6000, 6000–11000 and >11 kHz.
- **Spacing:** the five crossovers keep the 1.4× spacing and the
  16 kHz / 0.4 × sample-rate limit, so crossover 3 is lowered if 4 and 5 need
  the room.
- **Four bands:**
  - Processing is unchanged. A comparison compiled the previous core and this
    one and fed both the same stereo signal while automating M/S, the
    crossovers, solo, on/off, link, mix and the sidechain key. The outputs were
    bit-identical.
  - One deliberate change since (review, 10/30): turning the stage on now also
    resets the sidechain key's crossovers, so stale key state cannot duck the
    first milliseconds. Without a key the outputs stay bit-identical.
  - Bands 5 and 6 and crossovers 4 and 5 are ignored, even a band 5 solo
    (tested).
- **Band-count change:** works like the L/R ↔ M/S change. The stage fades out
  (10 ms), swaps the layout, settles silently for 30 ms and fades back in.
  Switched off, it swaps at once.
  - **Crossover safety (review, 10/30):** the first version could let the
    live crossovers pass through each other. Four and six bands validate
    crossover 3 differently, and only crossovers 4 and 5 jumped at the swap.
    Where two crossovers meet, the synthesis residues are singular, and the
    output reached 1e6 (1e15 at 22.05 kHz, where crossovers 4 and 5 could also
    glide past Nyquist).
  - Now five crossovers always follow the six-band validation of the raw
    values, including while fading out to four. At a swap every crossover
    starts at its new target. All crossovers glide at the same rate, so
    ordered crossovers stay ordered.
  - `ChimeraSixBandTests` repeats the reported cases and random band-count,
    M/S and on/off toggles at 8 and 48 kHz. All peaks are below 3.2 with
    ±24 dB makeup.
- **Page:** the MULTIBAND page gains a 4 BANDS / 6 BANDS choice at the start of
  the crossover row.
  - With six bands the row shows five crossovers, the spectrum shows markers
    X1–X5, and the band selector offers Band 1 / LOW … Band 5 / PRESENCE,
    Band 6 / AIR.
  - Going back to four bands moves a selection of band 5 or 6 to band 4.
  - The page's context text ("6 bands / stereo linked") was hidden behind the
    EXT KEY button and now follows it.
- **Auto Set:** its multiband analysis designs four bands. When it writes the
  multiband it sets four bands, and its Multiband lock keeps the band count.

## Synthesis

The band gains are applied by one minimum-phase filter whose magnitude is
the gain-weighted sum of the phase-aligned LR4 bands, `N(ω⁴)/D(ω⁴)` (see the
comments in `MultibandDynamicsCore.h`).
- **Four bands:** N is a cubic in ω⁴, solved in closed form (unchanged code).
- **Six bands:** N is a quintic.
  - It is solved numerically with the Aberth–Ehrlich iteration, warm-started
    from the previous schedule. It averages 2.8 iterations per schedule (about
    6000 schedules per second).
  - Each root z gives the two fourth roots of z with negative real part as
    zeros.
  - Nearly equal roots stop the iteration at about 1e−14 relative step; that
    stall counts as converged.
  - If the iteration fails, the previous schedule is kept (counted; no failure
    occurred in any test).
- **Same structure:** the filters are fixed parallel poles at the crossovers
  with residue weights, two state-variable sections per crossover. The
  analysis split is the same compensated LR4 cascade with five crossovers
  (5 splits, 10 all-pass compensators).
- **Performance:**
  - Writing the complex arithmetic out (the library operators' special-value
    handling) cut the cost from 19 % to 11 % of a core.
  - Flushing the imaginary part of real roots, which decayed into subnormal
    numbers, brought it to 5.0 %, or 4.5 % with denormals flushed as the plugin
    does (the test's figure).

## Measurements (`ChimeraSixBandTests`)

| Check | Result |
|---|---|
| Neutral six bands | Return the input exactly; impulse energy allpass to 1e−5; zero mix is raw audio |
| Static band gains (±24 dB random, and one band at 0 dB with five at −24 dB) against an independent bilinear LR4 oracle | 240 cases: 4 rates (44.1–192 kHz) × 5 crossover sets (including the tightest spacing at 40 Hz and at 2–7.7 kHz). Worst error 1.9e−5 dB, no solver failure |
| Bands 4–6 at one gain | Same output as four bands (no float sample differed) |
| One band cut by 6 dB | Every tone within 0.019 dB of the oracle; meter 6.0 dB |
| Solo | Each band passes exactly its LR4 band (tolerance 0.002 of a 0.1 tone). Bands 4 and 5 are narrower than an octave, so their centre is not at full level (band 4: 0.058 of 0.1) |
| Sidechain key, 8 kHz | Band 5 meter 12.0 dB, 8 kHz tone cut; bands 1–3 unchanged (bands 4 and 6 also hear the key through their slopes) |
| Automation stress | Modes, M/S channels, solo, crossovers jumping between the tightest low and high sets, mix and on/off every 3 ms: finite, independent of host blocks, no solver failure, no allocation |
| 4 → 6 switch | Swaps 9.3 ms after the change. Second difference equal to the input's. 6 → 4 with 220 Hz alone: 1.09 × the input's (the gain fade), no step |
| CPU, 48 kHz stereo, all bands compressing, one core (`MEASURE` line) | 4 bands 2.5–2.7 %, 6 bands 4.5 % (unlinked channels 5.3–5.5 %). The test fails above 4 × four bands |

## Not done

- **Checks:** listening tests, CPU in a DAW and on the release machines, and
  the roadmap's callback-cost gate on the target hosts.
- **Layouts:** other six-band layouts (bands inserted below crossover 1, or
  redistributed crossovers when switching).
- **Auto Set:** a six-band Auto Set analysis.
- **Other:** free detector bands and band output pan, which remain open from
  [MULTIBAND_STEREO_1_5.md](MULTIBAND_STEREO_1_5.md).
