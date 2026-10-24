"""The frozen v1 probe. Every offset elsewhere is derived here, never re-typed.

Layout (48 kHz mono float32, 37 s):
  21 tone steps, frequency-major: 1 s each, 750 ms settling + 250 ms measured
  8 s synthetic plucks, then 8 s synthetic chords (first note at 0.5 s)
Changing any value changes probe_sha256 and invalidates comparisons with
existing evidence; the audit refuses to mix reports with different probes.
"""
import numpy as np

from validate_nam import SR, fixture

LEVELS = [-54, -48, -42, -36, -30, -24, -12]
FREQUENCIES = [100, 400, 1200]
STEP_SECONDS = 1
SETTLE_SECONDS = .75
SEGMENT_SECONDS = 8
SEGMENTS = ('pluck', 'chord')
GROWTH_LEVELS = (-48, -24)
THD_LEVEL = -36
HARMONICS = 12

TONE_SECONDS = len(LEVELS) * len(FREQUENCIES) * STEP_SECONDS
TOTAL_SECONDS = TONE_SECONDS + SEGMENT_SECONDS * len(SEGMENTS)
TOTAL_SAMPLES = TOTAL_SECONDS * SR
MEASURE_SAMPLES = int(round((STEP_SECONDS - SETTLE_SECONDS) * SR))
# A tone that completes whole cycles in the window lands exactly on FFT bins.
BIN_HZ = SR / MEASURE_SAMPLES


def tone_window(frequency_index, level_index):
    start = (frequency_index * len(LEVELS) + level_index) * STEP_SECONDS * SR
    return slice(start + STEP_SECONDS * SR - MEASURE_SAMPLES, start + STEP_SECONDS * SR)


def segment(name):
    start = TONE_SECONDS * SR + SEGMENTS.index(name) * SEGMENT_SECONDS * SR
    return slice(start, start + SEGMENT_SECONDS * SR)


def harmonic_bins(hz):
    bins = [hz * k / BIN_HZ for k in range(1, HARMONICS + 1)]
    if any(abs(b - round(b)) > 1e-9 for b in bins) or hz * HARMONICS >= SR / 2:
        raise ValueError(f'{hz} Hz is not coherent with the measurement window')
    return np.array([int(round(b)) for b in bins])


def make_probe():
    t = np.arange(STEP_SECONDS * SR) / SR
    parts = [10 ** (db / 20) * np.sin(2 * np.pi * hz * t) * np.minimum(t / .01, 1)
             for hz in FREQUENCIES for db in LEVELS]
    probe = np.concatenate(parts + [fixture(name == 'chord') for name in SEGMENTS]).astype(np.float32)
    assert len(probe) == TOTAL_SAMPLES
    return probe


def describe():
    """The frozen v1 protocol record. Its text is compared byte-for-byte by audits."""
    return dict(sample_rate=SR, frequencies_hz=FREQUENCIES, input_peak_dbfs=LEVELS,
                settling_seconds=SETTLE_SECONDS, measurement_seconds=STEP_SECONDS - SETTLE_SECONDS,
                nam_slim=1, original_oversampling=4,
                musical_input='Deterministic synthetic plucks/chords, not a recorded guitar DI',
                digital_equal='Identical WAV; no reference or candidate input adjustment',
                calibrated='VH4 only: 11.5 dBu common-source convention, not measured hardware calibration of Nastrond',
                output='No level matching for dynamics; RMS matching only for phase-free spectrum comparisons',
                acceptance='Observations only; no hardware-match or owner listening PASS')
