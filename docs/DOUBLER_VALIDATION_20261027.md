# Doubler follow-up validation — 2026-10-27 KST

## Decision and scope

The #192 design materially improves Drop A fundamental and synthetic palm-mute
preservation. It is **not** a flat-response or constant-loudness doubler. Keep the
remaining comb coloration, waveform-dependent headroom and real-DI audition as
explicit acceptance items. The follow-up fixes bit-pattern bypass and redundant
coefficient evaluation without redesigning the sound or silently enabling the
limiter in existing presets.

Source snapshots:

- Measurement PR #190: `a553b9d359ab72efba28c07c53b09668387be7cf`.
- Audio change PR #192: `b46dab01548b8c86a9801c4c2ff643877bd742d6`.
- Test baselines in `Tests/DoublerBaselines/` preserve those headers, changing only
  the namespace. Product source under test is `Source/PerformanceUtilities.h`.
- JUCE 8.0.8, `d6181bde38d858c283c3b7bf699ce6340c050b5d`; Linux x86_64,
  Intel Xeon Platinum 8573C; GCC 13.3.0. Focused DSP build: `-O3 -DNDEBUG`,
  no fast-math, production `juce::dsp::DelayLine`/smoothers, ScopedNoDenormals.

No real instrument DI was supplied or auditioned in this validation. This
run generated its own synthetic inputs; repository WAV filenames were not
treated as evidence of an independent real Drop A DI listening set. No claim of Neural DSP-equivalent quality,
Windows/macOS success, real DAW/device deadline safety or release approval follows.

## What was fixed

1. Compute normalization/dry correction/wet gain only when the smoothed mix
   changes, shared by both channels. Settled mix no longer evaluates a square
   root/reciprocal per sample/channel. A ramp retains the original sample rate
   and operation order. The HPF coefficients were already calculated only in
   `prepare()` and are left there.
2. Cache the metronome envelope decay at `prepare()` instead of recomputing an
   unchanged exponential inside the sample loop.
3. At fully dry mix with metronome off, leave the input buffer untouched while
   continuing HPF/delay history. The old `x + ... + 0` path changed negative
   zero to positive zero; the old `==` assertion could not detect that. Tests
   now compare IEEE bit patterns. Turning off still has the existing **20 ms
   fade**; immediate bit identity during that fade is not promised.
4. Correct the measurement interpretation: the old `mean_db` was an arithmetic
   mean of 1/3-octave dB values, not broadband RMS or perceived loudness. Preserve
   that field for compatibility and add a separately measured `rms_db`.
5. Correct the documentation: finite HPF roll-off is not a brick-wall low-band
   bypass; the output limiter is optional/off by default; upstream compressor,
   multiband and De-esser cannot catch peaks generated later by Doubler.

## Actual DSP render protocol

`ChimeraDoublerValidationTests --measure` runs 3 versions × 3 sample rates ×
20 integer spread settings × 17 fixtures = **3,060 renders**, with L/R/(L+R)/2
statistics, giving **9,180 CSV rows**. Rates: 44.1, 48 and 96 kHz; block 128;
spread 1, 2, …, 20 ms. Every render is 8 s and discards the first 100 ms,
covering at least one full 0.13 Hz modulation period after settling. Full
fractional-spread automation is exercised in the regression; the frozen-response
supplement samples spread at 0.25 ms steps.

Fixtures: pure 27.5/40/55/80/110/160/200/250/400/1000/3000/6000 Hz tones;
A0/A1 synthetic palm mutes (0.5 ms rise, 28 ms decay, 160 ms repetition,
12 declining harmonics); A0 bass sustain; 880 Hz harmonic lead; seeded uniform
white noise. Each input is normalized to **−2 dBFS sample peak**, intentionally
stressing headroom. RMS ratios are unweighted energy ratios over the same
samples. Attack is the mean first-3-ms peak ratio of settled palm-mute events.
Correlation and side/mid energy describe these fixtures, not a universal width. CSV channel 0/1/2 means L/R/mono.

Raw evidence: [measurements.csv](evidence/doubler-validation-20261027/measurements.csv).
All reported metrics for the optimized version exactly match #192 to CSV
precision; sample-by-sample parity is separately gated by the regression.

### Default 6 ms at 48 kHz, mono sum

| Fixture / metric | #190 original | #192 / optimized |
|---|---:|---:|
| A0 27.5 Hz RMS | −0.728 dB | −0.022 dB |
| A1 55 Hz RMS | −2.969 dB | +0.190 dB |
| A1 palm-mute RMS | −2.598 dB | +0.174 dB |
| A1 palm-mute first 3 ms peak | −3.751 dB | +0.131 dB |
| A0 harmonic bass RMS | −1.061 dB | +0.026 dB |
| Harmonic lead RMS | −2.382 dB | +0.487 dB |
| White-noise RMS | −3.335 dB | −0.684 dB |

The original 8-second Gaussian-noise/old-pluck probe is also rerun in
[original-probe-fixed.txt](evidence/doubler-validation-20261027/original-probe-fixed.txt).
Its band averages reproduce the original report; the additional broadband RMS
field prevents interpreting its approximately −0.3 dB band mean as an overall
level guarantee. Different pluck envelopes are intentionally kept identifiable.

### Across all measured spreads and rates

| Signal / metric | Optimized L/R/mono range |
|---|---:|
| A0 pure-tone RMS | −0.056 to +0.097 dB |
| A1 pure-tone RMS | −0.241 to +0.374 dB |
| Synthetic palm-mute RMS | −0.178 to +0.227 dB |
| Synthetic palm-mute 3 ms attack | +0.018 to +1.308 dB |
| Harmonic bass RMS | −0.064 to +0.078 dB |
| Harmonic lead RMS | −2.763 to +1.573 dB |
| White-noise RMS | −0.700 to −0.302 dB |

For comparison, #190 reaches −10.44 dB at A0 and −10.41 dB at A1 across
the same grid. The new fundamental preservation is not just a default-spread
improvement.

Noise L/R correlation is 0.836–0.843, similar to the old noise result. Bass
correlation is 0.9986–0.9999 and palm-mute correlation 0.9909–0.9995: protecting
low fundamentals deliberately reduces width there. Harmonic lead correlation
varies 0.7025–0.8719 with spread. A single “0.84, width unchanged” statement
therefore does not describe every input.

## Frequency response, peaks and limiter

The dense analytical supplement uses the actual HPF transfer and linear
fractional-delay response at 4,101 frequencies, 64 frozen modulation phases,
77 spreads (1–20 ms by 0.25), and all three rates. It is explicitly a
**frozen-phase frequency calculation**, not an audio render or a transient
peak bound. Run `Tools/doubler_response_bounds.py output.csv` (NumPy).
See [frozen-response.csv](evidence/doubler-validation-20261027/frozen-response.csv).

- A0: −0.056 to +0.097 dB; A1: −0.242 to +0.375 dB.
- Individual frequencies within 40–160 Hz: approximately **−2.32 to +2.12 dB**.
  The old “within ±0.7 dB” statement concerned averages and hid this variation.
- Upper-band comb minima reach approximately **−7.82 dB**. Around 318 Hz,
  HPF phase permits a sine response of **+2.816 dB**, above the old +2.6 dB
  asymptotic high-frequency estimate.
- Actual rendered maximum sample peak: **+1.546 dBFS** from a −2 dBFS bounded
  noise input (44.1 kHz, spread 14 ms, L), a **+3.546 dB peak increase**. The
  worst harmonic-lead output is +0.955 dBFS. The transient figures need not
  respect a sine-response bound.
- These are floating-point values above unity, not proof of hard clipping
  inside Doubler. An integer export, DAC or later nonlinear stage can clip.
  The measured fixture maximum is not a guaranteed bound for every possible DI.

Production order, verified in `PluginProcessor.cpp`:
`Doubler → Final EQ → output trim/tuner mute → CommonDynamics::limit`.
The default output trim is −6 dB, but presets/user edits can change it and Final
EQ can add gain. The limiter's default is **off**, ceiling −1 dBFS when enabled.
It is stereo-linked, zero-lookahead **sample-peak** limiting, not true-peak
protection. Do not enable it silently or cap Doubler peaks as an optimization:
that would change existing preset dynamics and the sound being compared.

## Performance

[cpu.csv](evidence/doubler-validation-20261027/cpu.csv) contains 486 observations:
48/96 kHz × 64/128/512 frames × off/on/automated mix (mode 0/1/2) × 9 repetitions ×
#190/#192/fixed. Version order reverses each repetition. Preparation and
warm-up are outside timing. Per-thread CPU includes input copy and clock
bookkeeping equally; maximum callback wall time and every deadline overrun
remain in the raw data. Other compilation was active on this shared host.
This is an observational microbenchmark, not an isolated realtime session.

The added [A/A control](evidence/doubler-validation-20261027/cpu-control.csv)
runs the **same #192 code twice** plus fixed, 15 times at 48 kHz/128/on.
A/A differences range **−49.9% to +260.5%**, median +5.44%. Medians are
101.8 ns/frame for #192 A, 113.7 for #192 B and 106.4 for fixed. Five callback
overruns also occurred in the control; the broader run contains more.
Thus neither the HPF+normalization increment nor a stable optimization speedup
can be attributed reliably here. Do **not** publish a CPU-saving percentage
from the more favorable rows. The demonstrated change is fewer redundant
normalization/decay evaluations with audio parity; native quiet-host paired
measurement remains required for a quantitative CPU/deadline acceptance.

The new filter adds two double-precision transposed-DFII recurrences per
stereo frame. Old-vs-#192 timings measure that combined with normalization;
#192-vs-fixed isolates the caching/bypass changes at the source level, but
this host's A/A spread prevents quantitative attribution. No extra allocation,
lock, file operation, engine construction or oversampling was added.

## Regression coverage and limits

[contracts.txt](evidence/doubler-validation-20261027/contracts.txt) gates:

- 44.1/48/96/192 kHz; mono/stereo; block 1/64/128/257/1024; on/off and spread
  automation; metronome toggles/restart; reset; sample-for-sample #192 parity
  except corrected signed-zero bypass; complete bypass after the 20 ms ramp.
- Immediate sample-0 dry feedthrough on both channels at every integer spread;
  the original probe independently checks strongest input/output correlation
  lag = 0. Wet delay remains intentional. No host latency or parameter added.
- A0/A1 response within ±0.5 dB across integer spreads, plus a complete
  modulation-period measurement; block partition invariance for 1/64/128/257/1024.

`ChimeraCommonDynamicsTests --doubler-only` adds actual processor state save/load,
legacy missing-field defaults, A/B recall, unchanged 1–20 ms/default 6 ms and off
parameter definitions, actual production limiter ceiling/GR and callback
allocation tests. Native assertions passed; the final process result is recorded in the log below. Parameter
IDs, ranges, defaults, state serialization and preset implementations are
unchanged by the product optimization. Presets with Doubler on intentionally
sound different from #190 because of #192; compatibility does not imply old
on-state audio identity.

### Actual processor/limiter execution

The added native functional test executed against the actual compiled
`ChimeraProcessor`, its APVTS/serializer/A-B implementation and production
`CommonDynamics::limit()`. Project save/load, missing legacy defaults and A/B
recall passed. Across **60 limiter cases** (44.1/48/96 kHz, all integer spreads),
the maximum sample magnitude after limiting was 0.891251 (−1 dBFS within float
tolerance), maximum incoming magnitude 1.35852, and maximum observed reduction
3.66133 dB. Audio-callback allocations were zero.

Evidence: [native-state-limiter.txt](evidence/doubler-validation-20261027/native-state-limiter.txt).
The integration build used `Functional`, `-O0 -DNDEBUG`, with repository JUCE
patches and actual artwork/IR/manual resources. The test translation unit used
the repository target's compiler flags and was linked directly to the actual
shared-code archive, reusing its JUCE objects instead of compiling the same
modules twice. No processor, limiter or serialization stubs were substituted.
This functional build is not the build used for CPU claims.

The existing CommonDynamics regression also printed all PASS results, including
stereo link, limiter bypass, final-EQ/output-trim ordering, oversized blocks,
legacy state and unchanged latency; see
[common-dynamics-regression.txt](evidence/doubler-validation-20261027/common-dynamics-regression.txt).
The stock local `xvfb-run` returned 1 during cleanup even for `true`. The runner
was corrected locally to preserve the child exit status during cleanup, and the
new native test was repeated successfully: **NATIVE_RUN_EXIT=0** is logged explicitly. No product
change was made to hide a test failure. The original CommonDynamics log is
assertion evidence from the affected wrapper, not a clean CTest process result.

## Merge dependency and release boundary

At inspection, #190 targets main and #192 targets `claude/doubler-mono-measure`.
#192's parent is exactly #190's head; #190 supplies the probe/CMake/documentation,
not another product DSP change. Merge #190 first, then retarget #192 to main
and rerun current-source checks. If #190 is squash-merged, verify the resulting
#192 diff/rebase so measurement changes are not duplicated. This work does not
merge either PR or publish a build. Passing Linux DSP contracts is not equivalent
to the historical 64/65 complete Linux suite, Windows/macOS tests, or release
acceptance; the NativeFocus history is not reclassified here.

## Real DI listening still required

Level-matched listening must cover actual 5-string Drop A bass A0, 7-string
Drop A guitar A1, repeated palm mutes, long decays and high lead notes at
1/3/6/10/15/20 ms, both stereo and mono, with limiter off and on. Listen for
metallic/flanging coloration, delayed pick smearing, slow pitch beating,
left/right balance on sustained notes, loss of palm-mute upper-bass weight and
limiter pumping. Synthetic peak/RMS/correlation results cannot decide those
musical tradeoffs or certify the effect as two independent performances.

## Reproduction

Build `ChimeraDoublerMonoProbe`, `ChimeraDoublerValidationTests` and
`ChimeraCommonDynamicsTests` with the repository CMake/JUCE configuration.
Run the two new CTest entries `ChimeraDoublerValidationTests` and
`ChimeraDoublerStateLimiterTests`. Run the validation executable with `--measure`
and `--benchmark` to capture the CSVs; do not use an unoptimized build for CPU
comparisons. `Tools/summarize_doubler_validation.py <evidence-directory>` rejects
incomplete or duplicate grids and checks #192/optimized metric parity.
