# Amp and global DSP validation

This build is a development instrument, with eight **voiced algorithmic amp models**. It is not a circuit simulation or a neural capture of the named hardware families. Synthetic measurements establish behavior and regression safety; they do **not** establish perceptual or electrical equivalence to a physical amplifier.

## Changes from the previous test build

The four guitar distortion voices previously used the same two waveshapers. Every voice now has separate input/interstage high-pass filtering, saturation gains/stage counts, asymmetry, output bandwidth, dry blend and envelope-dependent supply sag. A post-distortion DC blocker removes the offset produced by asymmetric saturation. Drive, rig level, global gains and mute/gate gain changes are smoothed.

The full-range EQ remains available in Classic/Dual. Matrix bypasses it and uses the crossover-relative preamp Band Tone. The amplifier creates harmonics beyond its input band by design; the crossover is an input split, not a brick-wall restriction on the distorted output.

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

## Hardware validation still required

A meaningful reproduction test needs a matched dry DI plus reamped hardware outputs, exact gain/EQ/channel/cabinet settings and level calibration. For each intended reference, compare clean-to-distorted sweeps, pickup-volume cleanup, single notes, bass transients, palm mutes, chords/intermodulation, attack/recovery and decay, at matched perceived loudness. No such hardware reference recordings were available for this change.

The previous bass listening report remains a useful first check. Guitar listening, especially low-tuned chords under transpose and high-gain pick attack through IRs, remains a listening acceptance step rather than an automated claim of fidelity.

## Crossover, merge and LOW DI regression gates

`Tests/AlignmentTests.h` compares the static three-band sum against an independent analytic cascade of two bilinear allpasses over 160 frequencies at 44.1/48/96/192 kHz. It requires <0.03 dB magnitude error and <0.15 degree phase error. Flat magnitude does not imply linear phase or a zero-delay dry null.

Automated crossover motion compares parallel bands to a serial time-varying allpass reference, including the same smoothing schedule, and compares 127- vs 511-sample blocks. Gates are <0.00005 FS reference residual, <0.01 dB level difference and <0.000001 FS block-size residual. This catches split/allpass coefficient disagreement and block-boundary timing errors; it does not claim that time-varying filters preserve every instantaneous waveform amplitude.

Identical Dual rigs must null against Classic after the 50:50 merge, with post effects both on and off, at every oversampling choice. Dry impulse positions must equal summed module latency at all four tested rates/factors. Matrix LOW must null when only Pre Drive, amp, cabinet and oversampling settings change, including moving crossovers. COMP 0 must be unity; steady RMS reduction must match its control law within 0.35 dB and preserve linked stereo balance.

## Repeatable A/B workflow

1. Record one dry DI without effects. Keep its exact audio, start time, interface input setting, sample rate, block size and measured peak/RMS fixed; do not replay the part for the B pass.
2. Save reference A using MENU > Save reference. It includes parameter state, both A/B slots and the original user IR bytes. Record the plugin commit and IR source/hash externally alongside the DI.
3. COPY A to B, then change one amp, cabinet or control. Switch A/B to compare. Active slot changes and project/reference recall clear DSP histories/tails; allow at least one second of preroll before measuring a sustained section, and include sufficient silence to measure tails.
4. Match output loudness using LEVEL or OUTPUT. A/B switching does not automatically match loudness. Compare spectra, transient envelope, dynamic cleanup, chord intermodulation and decay on the same region.
5. Re-render A from a fresh instance and null A/A first. Log metrics and accept/reject a change against the fixed reference. Hardware fidelity additionally requires a calibrated hardware reamp of that DI.

`Tests/ReferenceTests.h` creates a versioned deterministic **synthetic** pluck/chord fixture, repeats an amp+factory-IR render (A/A null gate <1e-7 FS), and generates a different-drive B with RMS matching to A (level error gate <0.001 dB). Windows packages contain `reference-audio/DI-synthetic-v1.wav`, `A-tight035-v30.wav`, `B-tight065-v30-RMS-matched.wav`, and `metrics.csv`. These original generated signals are regression/listening fixtures, not real instrument or hardware recordings. The test requires a remaining waveform difference after loudness matching so a gain-only change cannot masquerade as voicing.

Windows state tests also verify A/B amp, COMP and embedded user IR recall after deleting the original IR file, and a byte-exact `.chimera` export/import fixture. UI screenshots cover all routing modes, PRE pedalboard, POST rack and scaling.
