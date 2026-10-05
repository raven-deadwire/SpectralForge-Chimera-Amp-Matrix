# Local 1.2 Preview integration evidence

The production source hashes identify the code used for the full 43-preset and
17-gain-path regression. All passed. The dispatch comparison to the measured
Original core has zero residual at 44.1/48/192/384 kHz across all five voices.
All six contexts, five Original preset audio roundtrips and A/B gain recall passed.
Four guitar signatures restored all3,981 parameters with zero audio difference.

New preset RMS spans −26.2433 to −25.8944 dBFS. Worst nominal peak is −12.0181 dBFS;
worst +6 dB input peak is −11.9383 dBFS. These are deterministic harmonic plucks,
not a real-DI, LUFS or listening certification. See `preset-levels.csv`.

The subsequent test-only fix narrows the pre-1.2 fixture removal and native-state
parameter classifier to `nativeAmp_..._m24_`, avoiding unrelated pedal model24.
It does not alter product code or the recorded audio. Exact-commit Windows CI
reruns the corrected tests and supplies installer, lifecycle and UI evidence.

Other local checks: native amp100 oversampling paths, core DSP, gate processor,
95 Preparation unit tests, reproducible catalog generation, catalog/release-scope
contracts and packaging-version tests. Final platform evidence belongs to the
candidate's exact-commit GitHub Actions artifacts, not this local log.
