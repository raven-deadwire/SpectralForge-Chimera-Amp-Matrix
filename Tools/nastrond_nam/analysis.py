"""Measurements of one rendered probe and spectral distances between two segments.

`log_spectrum_rms_db` is the frozen v1 metric. It averages linear Welch bins,
so ~76% of its 1354 bins lie between 2 and 8 kHz and only 31 below 250 Hz:
it mostly reports treble. `band_spectrum_rms_db` uses the 42 logarithmic
bands that the voicing fit already used, giving each octave equal weight.
Both are reported; audits choose explicitly.
"""
import numpy as np
from scipy import signal

from validate_nam import SR, metrics as legacy_metrics
from . import protocol

BAND_EDGES = np.geomspace(65, 8000, 43)


def db(x):
    return float(20 * np.log10(np.float64(x) + 1e-30))


def band_masks(frequencies):
    masks = [(frequencies >= a) & (frequencies < b) for a, b in zip(BAND_EDGES[:-1], BAND_EDGES[1:])]
    return [m for m in masks if m.any()]


def band_power(x, masks=None):
    f, p = signal.welch(x, SR, nperseg=8192)
    masks = band_masks(f) if masks is None else masks
    return f, masks, np.array([p[m].mean() for m in masks])


def compare_segments(reference, candidate):
    """Frozen v1 metrics plus polarity and octave-weighted spectral distance."""
    result = legacy_metrics(reference, candidate)
    # The v1 correlation keeps its sign: a negative value is an inverted
    # polarity at the best lag, not a dissimilar waveform.
    result['polarity'] = -1 if result['correlation'] < 0 else 1
    gain = np.sqrt(np.sum(reference ** 2) / max(np.sum(candidate ** 2), 1e-20))
    f, masks, pa = band_power(reference)
    _, _, pb = band_power(candidate * gain, masks)
    keep = pa > pa.max() * 1e-5
    result['band_spectrum_rms_db'] = float(np.sqrt(np.mean((10 * np.log10((pb[keep] + 1e-20) / (pa[keep] + 1e-20))) ** 2)))
    return result


def measure(data):
    if len(data) != protocol.TOTAL_SAMPLES or not np.isfinite(data).all():
        raise ValueError('Invalid render length or non-finite samples')
    response = []
    for fi, hz in enumerate(protocol.FREQUENCIES):
        bins = protocol.harmonic_bins(hz)
        readings = []
        for li, level in enumerate(protocol.LEVELS):
            v = np.asarray(data[protocol.tone_window(fi, li)], dtype=np.float64)
            v = v - np.mean(v)
            harmonics = abs(np.fft.rfft(v))[bins]
            readings.append(dict(input_peak_dbfs=level,
                                 rms_dbfs=db(np.sqrt(np.mean(v * v))),
                                 fundamental_peak_dbfs=db(2 * harmonics[0] / len(v)),
                                 thd_2_to_12=float(np.linalg.norm(harmonics[1:]) / max(harmonics[0], 1e-30))))
        weak, strong = (readings[protocol.LEVELS.index(x)] for x in protocol.GROWTH_LEVELS)
        response.append(dict(hz=hz, readings=readings,
                             growth_24db_input_db=strong['rms_dbfs'] - weak['rms_dbfs'],
                             fundamental_growth_24db_input_db=strong['fundamental_peak_dbfs'] - weak['fundamental_peak_dbfs']))
    musical = {}
    for name in protocol.SEGMENTS:
        v = np.asarray(data[protocol.segment(name)], dtype=np.float64)
        # The first note is at 0.5 s. Sustain/attack uses a fixed, phase-agnostic RMS window.
        attack = np.sqrt(np.mean(v[int(.51 * SR):int(.56 * SR)] ** 2))
        sustain = np.sqrt(np.mean(v[int(.8 * SR):int(1.0 * SR)] ** 2))
        musical[name] = dict(rms_dbfs=db(np.sqrt(np.mean(v * v))), peak_dbfs=db(np.max(abs(v))),
                             first_note_sustain_attack_db=float(20 * np.log10((sustain + 1e-30) / (attack + 1e-30))))
    return dict(response=response, synthetic_notes=musical, peak_dbfs=db(np.max(abs(data))))
