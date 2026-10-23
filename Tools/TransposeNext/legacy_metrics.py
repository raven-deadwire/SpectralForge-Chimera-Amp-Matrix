#!/usr/bin/env python3
"""Deterministic synthetic comparisons. No instrument-DI/host acceptance claims.

Native renderer does processing; Python only generates inputs and measures outputs.
Raw floats are interleaved stereo (R=-0.65 L). All algorithms receive the same file.
"""
import argparse
import hashlib
import json
import platform
import subprocess
from pathlib import Path

import numpy as np
from scipy import signal, optimize
from scipy.io import wavfile

ALGORITHMS = ["baseline", "relative", "signalsmith-matched", "signalsmith-default", "dual-delay-8ms"]


def db(x):
    return float(20 * np.log10(max(float(x), 1e-15)))


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def fixture(name, rate):
    meta = {"name": name, "synthetic": True, "rate": rate}
    duration = 3.5 if name == "impulses" else 3.0
    t = np.arange(round(duration * rate)) / rate
    f = 27.5 if name == "sine27.5" else 30.8677063285
    meta["fundamental_hz"] = f
    if name.startswith("sine"):
        x = .2 * np.sin(2 * np.pi * f * t)
    elif name == "harmonics":
        x = sum(.12 * a * np.sin(2 * np.pi * f * h * t + .19 * h)
                for h, a in enumerate([1, .8, .5, .3, .2, .16, .12, .1], 1))
    elif name == "impulses":
        size = 256
        while size < rate * .032:
            size *= 2
        hop = size // 8
        # One impulse per event; varied phase relative to the STFT hop.
        onsets = [int(.4 * rate) + k * int(.35 * rate) + k * (hop // 8)
                  for k in range(8)]
        x = np.zeros_like(t)
        x[onsets] = .2
        meta["onsets"] = onsets
    elif name.startswith("riff"):
        spacing = .075 if name == "riff75" else .125
        onsets = [.4 + k * spacing for k in range(16)]
        x = np.zeros_like(t)
        rng = np.random.default_rng(8)
        for start in onsets:
            local = t - start
            mask = (local >= 0) & (local < spacing * .82)
            u = local[mask]
            body = sum(.16 * a * np.sin(2 * np.pi * f * h * u + .13 * h)
                       for h, a in enumerate([1, .6, .3, .15, .1], 1))
            x[mask] += (1 - np.exp(-u / .001)) * np.exp(-u / .025) * body
            x[mask] += .07 * np.exp(-u / .002) * rng.uniform(-1, 1, len(u))
        meta["onsets"] = [round(s * rate) for s in onsets]
        meta["spacing_ms"] = spacing * 1000
    elif name == "decay":
        local = np.maximum(0, t - .3)
        x = (t >= .3) * (1 - np.exp(-local / .003)) * np.exp(-local / .28) * (
            .16 * np.sin(2 * np.pi * f * local) + .06 * np.sin(4 * np.pi * f * local))
    else:
        raise ValueError(name)
    return np.column_stack((x, -.65 * x)).astype("<f4"), meta


def tone_metrics(y, rate, f, source_amplitude):
    # Ignore startup and filter edges. 2 seconds supports the low fundamentals.
    z = y[round(.75 * rate):round(2.75 * rate), 0].astype(float)
    t = np.arange(len(z)) / rate
    w = signal.windows.hann(len(z), sym=False)
    def amplitude(hz):
        return 2 * abs(np.sum(w * z * np.exp(-2j * np.pi * hz * t))) / w.sum()
    # Fit dominant component near the intended tone; this does not assume it is correct.
    grid = np.linspace(f * .65, f * 1.4, 121)
    best = int(np.argmax([amplitude(hz) for hz in grid]))
    lo, hi = grid[max(0, best - 1)], grid[min(len(grid) - 1, best + 1)]
    fit = optimize.minimize_scalar(lambda hz: -amplitude(hz), bounds=(lo, hi), method="bounded")
    dominant = float(fit.x)
    coherent = amplitude(f)
    spectrum = np.fft.rfft(w*z)
    bins = np.fft.rfftfreq(len(z),1/rate)
    # Local-band power includes detuning/modulation, unlike exact-tone projection.
    band = (bins>=f*.8)&(bins<=f*1.2)
    band_rms = np.sqrt(2*np.sum(np.abs(spectrum[band])**2)/(len(z)*np.sum(w*w)))
    # Baseband <=6 Hz modulation, measured only in the central second.
    baseband = signal.sosfiltfilt(signal.butter(3, 6, fs=rate, output="sos"),
                                  2 * z * np.exp(-2j * np.pi * f * t))
    b = baseband[round(.4 * rate):-round(.4 * rate)]
    amp = np.abs(b)
    inst = f + np.gradient(np.unwrap(np.angle(b))) * rate / (2 * np.pi)
    cents = 1200 * np.log2(np.maximum(inst, .01) / f)
    return {"target_gain_db": db(coherent / source_amplitude),
            "local_fundamental_band_gain_db": db(band_rms/(source_amplitude/np.sqrt(2))),
            "dominant_component_gain_db": db(amplitude(dominant)/source_amplitude),
            "dominant_hz": dominant, "dominant_error_cents": float(1200 * np.log2(dominant / f)),
            "amplitude_modulation_p95_p5_db": db(np.percentile(amp, 95) / max(np.percentile(amp, 5), 1e-15)),
            "warble_p95_p5_cents": float(np.percentile(cents, 95) - np.percentile(cents, 5)),
            "rms": float(np.sqrt(np.mean(z*z)))}


def impulse_metrics(y, meta):
    rate = meta["rate"]
    events = []
    for onset in meta["onsets"]:
        start = max(0, onset - round(.08 * rate))
        end = min(len(y), onset + round(.18 * rate))
        energy = y[start:end, 0].astype(float) ** 2
        if energy.sum() <= 1e-20:
            events.append({"missing": True})
            continue
        energy /= energy.sum()
        cumulative = np.cumsum(energy)
        lo, hi = [int(np.searchsorted(cumulative, p)) for p in (.05, .95)]
        centre = np.sum(energy * np.arange(len(energy))) + start - onset
        events.append({"centroid_ms": float(centre / rate * 1000),
                       "energy5_ms": float((lo + start - onset) / rate * 1000),
                       "energy95_ms": float((hi + start - onset) / rate * 1000),
                       "smear_ms": float((hi - lo) / rate * 1000)})
    valid = [e for e in events if not e.get("missing")]
    result = {"events": events, "missing_count": len(events) - len(valid)}
    if valid:
        for key in ("centroid_ms", "smear_ms"):
            result[key + "_median"] = float(np.median([e[key] for e in valid]))
            result[key + "_range"] = [min(e[key] for e in valid), max(e[key] for e in valid)]
    return result


def riff_metrics(y, meta, delay):
    rate = meta["rate"]
    x = y[:, 0].astype(float)
    # 5 ms moving power envelope. Analyse per intended event, not phase correlation.
    envelope = np.sqrt(np.convolve(x*x, np.ones(round(.005*rate))/round(.005*rate), "same"))
    spacing = round(meta["spacing_ms"] * rate / 1000)
    peaks, valleys, errors = [], [], []
    for onset in meta["onsets"]:
        expected = onset + round(delay * rate / 1000)
        left, right = max(0, expected-round(.015*rate)), min(len(x), expected+round(.055*rate))
        pos = left + int(np.argmax(envelope[left:right]))
        peaks.append(envelope[pos])
        errors.append((pos-expected)/rate*1000)
        a, b = expected+int(spacing*.7), expected+int(spacing*.95)
        valleys.append(float(np.mean(envelope[a:b])))
    # Ratio lower is better separation, but can't prove perceived riff clarity.
    body = signal.sosfiltfilt(signal.butter(3, [20, 120], btype="bandpass", fs=rate, output="sos"), x)
    attack = signal.sosfiltfilt(signal.butter(3, [1000, 8000], btype="bandpass", fs=rate, output="sos"), x)
    return {"valley_to_peak_db": db(np.mean(valleys)/np.mean(peaks)),
            "envelope_peak_offset_ms_median": float(np.median(errors)),
            "body20_120_to_attack1k_8k_db": db(np.linalg.norm(body)/max(np.linalg.norm(attack), 1e-15))}
