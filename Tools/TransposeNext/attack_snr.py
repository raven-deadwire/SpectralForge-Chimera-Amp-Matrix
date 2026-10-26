"""Per-event attack SNR for the 1-8 kHz attack metric (T1, fixed-PDC study).

The attack metric looks for the 1-8 kHz 2 ms RMS peak within 70 ms of each
event. When an event barely rises above the band's floor (for example a faint
marker over a sustained low note, whose harmonics leak into 1-8 kHz), that
peak lands anywhere and the measured delay is meaningless. SNR here is the
peak within 10 ms after the event over the largest envelope 20-3 ms before
it; the 3 ms gap keeps the centred window and zero-phase filter, which spread
the attack backwards, out of the floor. See
docs/TRANSPOSE_ATTACK_METRIC_FAINT_MARKERS_20261025.md.
"""
import numpy as np
from scipy import signal

def hf_envelope(y, rate):
    hi = signal.sosfiltfilt(signal.butter(3, [1000, 8000], fs=rate, btype='bandpass', output='sos'), y[:, 0].astype(float))
    w = round(.002*rate)
    return np.sqrt(signal.convolve(hi*hi, np.ones(w)/w, mode='same'))

def event_snr_db(env, onset, rate):
    peak = env[onset:onset+round(.010*rate)].max()
    floor = env[max(0, onset-round(.020*rate)):max(1, onset-round(.003*rate))].max()
    return float(20*np.log10(peak/max(floor, 1e-12)))

def event_snrs(x, onsets, rate):
    e = hf_envelope(x, rate)
    return [event_snr_db(e, o, rate) for o in onsets]
