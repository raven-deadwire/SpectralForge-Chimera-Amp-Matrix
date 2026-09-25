# Amp and global DSP validation

This build is a development instrument, with eight **voiced algorithmic amp models**. It is not a circuit simulation or a neural capture of the named hardware families. Synthetic measurements establish behavior and regression safety; they do **not** establish perceptual or electrical equivalence to a physical amplifier.

## Changes from the previous test build

The four guitar distortion voices previously used the same two waveshapers. Every voice now has separate input/interstage high-pass filtering, saturation gains/stage counts, asymmetry, output bandwidth, dry blend and envelope-dependent supply sag. A post-distortion DC blocker removes the offset produced by asymmetric saturation. Drive, rig level, global gains and mute/gate gain changes are smoothed.

The full-range EQ remains available in Classic/Dual Blend. Matrix and Dual Crossover bypass it and use the crossover-relative preamp Band Tone. The amplifier creates harmonics beyond its input band by design; the crossover is an input split, not a brick-wall restriction on the distorted output.

## Automated evidence

`Tests/ChimeraTests.cpp` and `Tests/FeatureTests.h` cover:

- Existing band-tone response, coefficient updates and routing isolation at 44.1, 48, 96 and 192 kHz.
- Eight distinct responses, bounded finite output, persistent DC below 0.001 FS, and positive compressed input/output dynamics.
- A coherent 10.006 kHz high-drive sine, measuring the folded fifth harmonic near 2.032 kHz relative to the fundamental. 4x and 8x must improve this component by at least 12 dB versus 1x. This is one deliberately severe aliasing probe, not a complete perceptual score.
- Stereo-linked gate closure, fast reopening and no chatter on a 30.87 Hz bass fundamental.
- Monophonic tuner frequency/cents error on synthetic harmonic-rich tones from 25.96 to 1390 Hz, and silence rejection.
- Transpose zero-semitone dry-path identity with the reported delay, rate-dependent latency, upward/downward intervals and rejection of the unshifted component.
- Factory IR decoding, invalid-file rejection, convolution stereo isolation, independent lanes, and embedded IR restoration after deleting the original file.

Windows additionally runs the actual processor/state/editor and saves screenshots at multiple scales. CI retains its measured values in `Testing/Temporary/LastTest.log`; Windows packages include that log as `Verification.txt`.

## Latency and operating limits

- Amp oversampling: 1x/2x/4x/8x, default 4x. All choices are aligned to the maximum oversampling delay, so quality changes do not change host compensation. Switching uses a short crossfade.
- Transpose: Chimera STFT, polyphonic, -12 to +12 semitones. A power-of-two FFT window (2048 samples at 48 kHz) with 8x overlap uses instantaneous-frequency scaling and peak phase locking. Exact algorithm delay is one FFT window and is shown in the UI/reported to the host when enabled. Bypassed pitch adds no delay. Transpose is not claimed to match the reference commercial product's latency or sound.
- Tuner: original input after input trim, before gate/pitch; monophonic, 25-1400 Hz; A4 430-450 Hz. Analysis runs off the audio callback. The rolling analysis window trades display response for reliable low-note detection. Auto-mute is optional.
- IR: WAV/AIFF, mono or stereo, 8-384 kHz, <=1 second, <=4 MB. Speaker IRs are normalised but their leading delay is retained. Loading/preparation/destruction occur outside the audio callback. New engines crossfade over 50 ms. Natural IR onset delay is not automatically removed or phase-matched between different captures.
- No output limiter is applied. Watch the output meter and use the master trim; clipping is indicated in red.

## NAM reference validation

A meaningful reproduction test needs a matched dry DI plus reamped hardware outputs, exact gain/EQ/channel/cabinet settings and level calibration. For each intended reference, compare clean-to-distorted sweeps, pickup-volume cleanup, single notes, bass transients, palm mutes, chords/intermodulation, attack/recovery and decay, at matched perceived loudness. The user has no reamp hardware. Eight third-party amp-head NAM captures now provide digital reference outputs instead; see NAM_REFERENCE_RESULTS.md for exact files, calibration limits and measured improvements. This still does not independently establish physical hardware fidelity.

The previous bass listening report remains a useful first check. Guitar listening, especially low-tuned chords under transpose and high-gain pick attack through IRs, remains a listening acceptance step rather than an automated claim of fidelity.

## Crossover, merge and LOW DI regression gates

`Tests/AlignmentTests.h` compares the static three-band sum against an independent analytic cascade of two bilinear allpasses over 160 frequencies at 44.1/48/96/192 kHz. It requires <0.03 dB magnitude error and <0.15 degree phase error. Flat magnitude does not imply linear phase or a zero-delay dry null.

Automated crossover motion compares parallel bands to a serial time-varying allpass reference, including the same smoothing schedule, and compares 127- vs 511-sample blocks. Gates are <0.00005 FS reference residual, <0.01 dB level difference and <0.000001 FS block-size residual. This catches split/allpass coefficient disagreement and block-boundary timing errors; it does not claim that time-varying filters preserve every instantaneous waveform amplitude.

Identical Dual rigs must null against Classic after the 50:50 merge, with post effects both on and off, at every oversampling choice. Dry impulse positions must equal summed module latency at all four tested rates/factors. At 0% AMP, Matrix LOW must null when only Pre Drive, amp, cabinet and oversampling settings change, including moving crossovers. COMP 0 must be unity; steady RMS reduction must match its control law within 0.35 dB and preserve linked stereo balance.

LOW also blends its compressed DI with the selected head/cab using constant-sum
weights. At 44.1/96 kHz and every oversampling factor, the midpoint must null against
the endpoint average; both hidden drive changes and a previous Classic high-drive
history must have no effect on Matrix LOW's fixed-zero drive. The DI branch matches
the head's algorithmic latency. Natural IR phase/onset delay is deliberately retained.

## Repeatable A/B workflow

1. Record one dry DI without effects. Keep its exact audio, start time, interface input setting, sample rate, block size and measured peak/RMS fixed; do not replay the part for the B pass.
2. Save reference A using MENU > Save reference. It includes parameter state, both A/B slots and the original user IR bytes. Record the plugin commit and IR source/hash externally alongside the DI.
3. COPY A to B, then change one amp, cabinet or control. Switch A/B to compare. Active slot changes and project/reference recall clear DSP histories/tails; allow at least one second of preroll before measuring a sustained section, and include sufficient silence to measure tails.
4. Match output loudness using LEVEL or OUTPUT. A/B switching does not automatically match loudness. Compare spectra, transient envelope, dynamic cleanup, chord intermodulation and decay on the same region.
5. Re-render A from a fresh instance and null A/A first. Log metrics and accept/reject a change against the fixed reference. NAM validation uses the exact same generated DI and fixed capture metadata; physical hardware fidelity remains a separate claim.

`Tests/ReferenceTests.h` creates a versioned deterministic **synthetic** pluck/chord fixture, repeats an amp+factory-IR render (A/A null gate <1e-7 FS), and generates a different-drive B with RMS matching to A (level error gate <0.001 dB). Windows packages contain `reference-audio/DI-synthetic-v1.wav`, `A-tight035-v30.wav`, `B-tight065-v30-RMS-matched.wav`, and `metrics.csv`. These original generated signals are regression/listening fixtures, not real instrument or hardware recordings. The test requires a remaining waveform difference after loudness matching so a gain-only change cannot masquerade as voicing.

Windows state tests also verify A/B amp, COMP and embedded user IR recall after deleting the original IR file, and a byte-exact `.chimera` export/import fixture. UI screenshots cover all routing modes, PRE pedalboard, POST rack and scaling.

## Expanded module and Dual gates

`Tests/StudioTests.h` requires a measurable enabled/bypass difference for each of the eleven pedal/rack modules and bounded finite output. The identical-Dual vs Classic test also runs the complete post rack, protecting its once-after-merge scope. The Matrix clean-path null toggles all three gain pedals. Both PRE detector orders must affect dynamics while preserving that clean tap and fixed latency. Dual Blend endpoints must match the corresponding Classic rig, and a 75:25 setting must match that exact weighted sum. Moving Dual Crossover must match a serial allpass reference and remain independent of 127/511-sample blocks. Windows tests check Dual Crossover controls, five-pedal/six-rack pages, global tuner view, MIDI Learn and persisted CC assignments.

All four tested sample rates/factors retain fixed dry-path delay. The current full processor adds the fixed pre Fuzz, Overdrive, amplifier and post Preamp delays (24 samples at 48 kHz, plus Transpose when enabled). Compression, EQ and wet-only time effects do not add dry-path delay. Distinct saturation/IR frequency-dependent phase responses still differ by design; fixed sample alignment does not make different amp/cab transfer functions identical.

The current suite checks all **46 FX variants** pairwise within each family: five models in each of the five PRE families and 21 POST models. Additional DSP gates cover all five compressor variants with differing host block sizes, automated drive changes, stereo separation, asymmetric-drive DC removal, and return to exact delayed dry bypass at 44.1/48/96/192 kHz. The FET parallel blend and Variable Mu recovery memory are original algorithms; these tests establish stability and differences, not hardware equivalence.

Drive processing remains fixed at 4x oversampling. Model filter coefficients transition over 35 ms on a sample-based update schedule; tone is smoothed over 20 ms, and an 8 Hz DC blocker removes asymmetric-clipping offset. Optical recovery is evaluated per sample rather than once per host block. Meter tests check each POST stage's measured peak and applied compressor gain reduction. The footer reports measured audio-callback CPU average and peak.

Processor/UI tests cover model and metadata recall, actual IR decoding and collection filtering, and A/B stereo routing. The original raw PRE model indices 0-2 retain their meanings. Old normalized DAW model-selection automation can map differently after expansion from three to five choices; ordinary state-recall tests do not remove that host-automation compatibility limit.

Pass/fail evidence belongs to the matching build's logs. Existing NAM measurement values remain fixed-capture head comparisons in [NAM_REFERENCE_RESULTS.md](NAM_REFERENCE_RESULTS.md); this FX update does not imply those hardware references, new pedals or artwork have passed a new physical-equivalence test.
