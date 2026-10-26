#!/usr/bin/env python3
"""Objective measurements for the transpose quality/latency modes (protocol §3).

This does NOT render the plugin — rendering the host/plugin is manual or done by
your DAW's offline bounce. This tool reads the already-rendered WAVs (a bypass
reference plus one file per mode/shift) and reports the objective metrics:
added latency, pitch accuracy, low-register integrity, transient spread, spectral
envelope distance, shift-0 null residual, and loudness (for match logging).

Design notes / honest limits:
  * Latency is estimated from the AMPLITUDE ENVELOPE cross-correlation, which is
    robust across a pitch shift (the envelope survives; the waveform does not).
    Still verify against the host's reported PDC — a mismatch is a bug.
  * ESS/Kirkeby deconvolution (see Tools/capture_cabinet_ir.py) is for LINEAR,
    time-invariant systems. Transpose is neither; do not use it here.
  * The blind listening half (protocol §5) is not automatable and is not here.

Dependencies: numpy (required). matplotlib (optional, only for --plots).

Usage:
    python transpose_measure.py measure manifest.json --out results/ [--plots]
    python transpose_measure.py selftest

Manifest (one per shift amount):
    {
      "sample_rate_hint": 48000,
      "shift_semitones": -12,
      "target_note_hz": 82.41,          # expected OUTPUT fundamental after the shift
      "reference": "renders/bypass_-12.wav",
      "conditions": [
        {"mode": "CLASSIC", "path": "renders/classic_-12.wav"},
        {"mode": "LOW-LAT", "path": "renders/lowlat_-12.wav"},
        {"mode": "TIGHT",   "path": "renders/tight_-12.wav"}
      ]
    }
"""
from __future__ import annotations

import argparse
import csv
import json
import struct
import sys
from pathlib import Path

import numpy as np

LOW_BAND_HZ = 120.0        # "low register" cutoff for the body/energy ratio
CEPSTRUM_LIFTER = 40       # quefrency cutoff for the spectral-envelope estimate


# --------------------------------------------------------------------------- I/O

def read_wav(path):
    """Minimal RIFF/WAVE reader -> (float32 mono signal, sample_rate).

    Handles PCM 16/24/32 and IEEE float32. Stereo is downmixed to mono (mean);
    swap for per-channel analysis if a mode behaves differently L vs R.
    """
    data = Path(path).read_bytes()
    if data[:4] != b"RIFF" or data[8:12] != b"WAVE":
        raise ValueError(f"Not a WAVE file: {path}")
    fmt_code = bits = channels = sr = None
    samples = None
    pos = 12
    while pos + 8 <= len(data):
        cid = data[pos:pos + 4]
        size = struct.unpack_from("<I", data, pos + 4)[0]
        body = data[pos + 8:pos + 8 + size]
        if cid == b"fmt ":
            fmt_code, channels, sr, _, _, bits = struct.unpack_from("<HHIIHH", body, 0)
        elif cid == b"data":
            samples = body
        pos += 8 + size + (size & 1)
    if fmt_code is None or samples is None:
        raise ValueError(f"Missing fmt/data chunk: {path}")
    if fmt_code == 3 and bits == 32:
        arr = np.frombuffer(samples, dtype="<f4").astype(np.float64)
    elif fmt_code == 1 and bits == 16:
        arr = np.frombuffer(samples, dtype="<i2").astype(np.float64) / 32768.0
    elif fmt_code == 1 and bits == 32:
        arr = np.frombuffer(samples, dtype="<i4").astype(np.float64) / 2147483648.0
    elif fmt_code == 1 and bits == 24:
        raw = np.frombuffer(samples, dtype=np.uint8).reshape(-1, 3).astype(np.int32)
        val = (raw[:, 0] | (raw[:, 1] << 8) | (raw[:, 2] << 16))
        val -= (val & 0x800000) << 1  # sign-extend
        arr = val.astype(np.float64) / 8388608.0
    else:
        raise ValueError(f"Unsupported WAV format code={fmt_code} bits={bits}: {path}")
    if channels and channels > 1:
        arr = arr.reshape(-1, channels).mean(axis=1)
    return arr.astype(np.float64), int(sr)


# ----------------------------------------------------------------- primitives

def envelope(signal, sr, smooth_hz=50.0):
    """Rectified, low-passed amplitude envelope (one-pole)."""
    rectified = np.abs(signal)
    a = np.exp(-2.0 * np.pi * smooth_hz / sr)
    out = np.empty_like(rectified)
    acc = 0.0
    for i, x in enumerate(rectified):      # TODO: vectorise with scipy.lfilter if available
        acc = (1.0 - a) * x + a * acc
        out[i] = acc
    return out


def estimate_latency_ms(reference, test, sr):
    """Added latency of `test` vs `reference` from envelope cross-correlation."""
    n = min(len(reference), len(test))
    a = envelope(reference[:n], sr) - np.mean(envelope(reference[:n], sr))
    b = envelope(test[:n], sr) - np.mean(envelope(test[:n], sr))
    corr = np.correlate(b, a, mode="full")
    lag = np.argmax(corr) - (n - 1)
    # parabolic refinement around the peak
    k = int(np.argmax(corr))
    if 0 < k < len(corr) - 1:
        y0, y1, y2 = corr[k - 1], corr[k], corr[k + 1]
        denom = (y0 - 2 * y1 + y2)
        if denom != 0:
            lag += 0.5 * (y0 - y2) / denom
    return 1000.0 * lag / sr


def fundamental_hz(signal, sr, f0_guess=None):
    """Parabolic-interpolated FFT peak near f0_guess (whole spectrum if None)."""
    win = signal * np.hanning(len(signal))
    spec = np.abs(np.fft.rfft(win))
    freqs = np.fft.rfftfreq(len(win), 1.0 / sr)
    if f0_guess:
        lo, hi = 0.5 * f0_guess, 1.5 * f0_guess
        mask = (freqs >= lo) & (freqs <= hi)
        if not mask.any():
            mask = slice(None)
        band = np.where(mask)[0]
        k = band[np.argmax(spec[band])]
    else:
        k = int(np.argmax(spec[1:]) + 1)
    if 0 < k < len(spec) - 1:
        y0, y1, y2 = np.log(spec[k - 1] + 1e-20), np.log(spec[k] + 1e-20), np.log(spec[k + 1] + 1e-20)
        denom = (y0 - 2 * y1 + y2)
        delta = 0.5 * (y0 - y2) / denom if denom else 0.0
    else:
        delta = 0.0
    return (k + delta) * sr / len(win)


def cents(measured, target):
    return 1200.0 * np.log2(measured / target) if measured > 0 and target > 0 else float("nan")


def low_band_ratio(signal, sr, cutoff=LOW_BAND_HZ):
    """Fraction of spectral energy below `cutoff` Hz (low-register 'body')."""
    spec = np.abs(np.fft.rfft(signal * np.hanning(len(signal)))) ** 2
    freqs = np.fft.rfftfreq(len(signal), 1.0 / sr)
    total = spec.sum()
    return float(spec[freqs < cutoff].sum() / total) if total > 0 else float("nan")


def spectral_envelope(signal, sr, lifter=CEPSTRUM_LIFTER):
    """Real-cepstrum liftered log-magnitude envelope (for formant/timbre distance)."""
    win = signal * np.hanning(len(signal))
    log_mag = np.log(np.abs(np.fft.rfft(win)) + 1e-12)
    full = np.concatenate([log_mag, log_mag[-2:0:-1]])
    cep = np.fft.ifft(full).real
    cep[lifter:-lifter] = 0.0
    env = np.fft.fft(cep).real[:len(log_mag)]
    return env


def envelope_distance_db(signal_a, signal_b, sr):
    """RMS difference (dB) between two spectral envelopes — timbre divergence."""
    ea, eb = spectral_envelope(signal_a, sr), spectral_envelope(signal_b, sr)
    n = min(len(ea), len(eb))
    return float(np.sqrt(np.mean((ea[:n] - eb[:n]) ** 2)) * 20.0 / np.log(10))


def transient_spread_ms(signal, sr):
    """Crude attack spread: time for the onset envelope to go 10%->90% of peak,
    plus pre-onset energy as a pre-echo proxy. Expect it to grow as the window
    shrinks (TIGHT should smear less in time, more in frequency)."""
    env = envelope(signal, sr, smooth_hz=200.0)
    peak = env.max()
    if peak <= 0:
        return float("nan"), float("nan")
    onset = int(np.argmax(env >= 0.9 * peak))
    start = int(np.argmax(env >= 0.1 * peak))
    rise_ms = 1000.0 * (onset - start) / sr
    pre = env[:start]
    pre_echo_db = 20.0 * np.log10((pre.max() / peak) + 1e-12) if len(pre) else float("-inf")
    return rise_ms, pre_echo_db


def align_and_null_db(reference, test, sr):
    """Shift-0 transparency: delay-align, level-match, subtract, return residual dB."""
    lag_ms = estimate_latency_ms(reference, test, sr)
    shift = int(round(lag_ms * sr / 1000.0))
    t = test[shift:] if shift > 0 else test
    n = min(len(reference), len(t))
    r, t = reference[:n], t[:n]
    g = np.dot(r, t) / (np.dot(t, t) + 1e-20)   # least-squares gain match
    residual = r - g * t
    rms_r = np.sqrt(np.mean(r ** 2)) + 1e-20
    return float(20.0 * np.log10((np.sqrt(np.mean(residual ** 2)) + 1e-20) / rms_r))


def loudness_dbfs(signal):
    """Plain RMS level in dBFS (kept for reference; prefer lufs_integrated for
    the listening-test level match, protocol §4)."""
    return float(20.0 * np.log10(np.sqrt(np.mean(signal ** 2)) + 1e-20))


# ------------------------------------------------- loudness (ITU-R BS.1770 LUFS)

def _lfilter(b, a, x):
    """Biquad (a normalised so a[0]==1). Uses scipy if present, else a DF2T loop."""
    try:
        from scipy.signal import lfilter
        return lfilter(b, a, x)
    except Exception:
        y = np.empty_like(x)
        b0, b1, b2 = b
        a1, a2 = a[1], a[2]
        x1 = x2 = y1 = y2 = 0.0
        for i in range(len(x)):
            xi = x[i]
            yi = b0 * xi + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2
            x2, x1 = x1, xi
            y2, y1 = y1, yi
            y[i] = yi
        return y


def _biquad_highshelf(f0, gain_db, q, fs):
    A = 10.0 ** (gain_db / 40.0)
    w0 = 2.0 * np.pi * f0 / fs
    cw, sw = np.cos(w0), np.sin(w0)
    alpha = sw / (2.0 * q)
    sqrtA = np.sqrt(A)
    b0 = A * ((A + 1) + (A - 1) * cw + 2 * sqrtA * alpha)
    b1 = -2 * A * ((A - 1) + (A + 1) * cw)
    b2 = A * ((A + 1) + (A - 1) * cw - 2 * sqrtA * alpha)
    a0 = (A + 1) - (A - 1) * cw + 2 * sqrtA * alpha
    a1 = 2 * ((A - 1) - (A + 1) * cw)
    a2 = (A + 1) - (A - 1) * cw - 2 * sqrtA * alpha
    return [b0 / a0, b1 / a0, b2 / a0], [1.0, a1 / a0, a2 / a0]


def _biquad_highpass(f0, q, fs):
    w0 = 2.0 * np.pi * f0 / fs
    cw, sw = np.cos(w0), np.sin(w0)
    alpha = sw / (2.0 * q)
    b0 = (1 + cw) / 2.0
    b1 = -(1 + cw)
    b2 = (1 + cw) / 2.0
    a0 = 1 + alpha
    a1 = -2 * cw
    a2 = 1 - alpha
    return [b0 / a0, b1 / a0, b2 / a0], [1.0, a1 / a0, a2 / a0]


def k_weight(signal, sr):
    """BS.1770 K-weighting: a high-shelf ('head') pre-filter then an RLB high-pass.
    Coefficients are designed for the actual sample rate (so 44.1/48/96 kHz are all
    correct), matching the reference 48 kHz filters."""
    shelf = _biquad_highshelf(1681.9744509555319, 3.99984385397, 0.7071752369554193, sr)
    hpf = _biquad_highpass(38.13547087613982, 0.5003270373253953, sr)
    return _lfilter(hpf[0], hpf[1], _lfilter(shelf[0], shelf[1], signal))


def lufs_integrated(signal, sr):
    """Integrated loudness in LUFS (BS.1770-4), mono (one channel, weight 1.0).

    400 ms blocks, 75% overlap, with the −70 LUFS absolute and −10 LU relative
    gates. For clips shorter than one block it falls back to ungated K-weighted
    loudness. Downmixed mono underreads a true stereo programme slightly, which is
    fine for level-matching a single isolated-DI source; swap in per-channel
    summation (L+R, weight 1.0 each) if you ever match a stereo render."""
    y = k_weight(np.asarray(signal, dtype=np.float64), sr)
    block = int(0.4 * sr)
    hop = int(0.1 * sr)
    if len(y) < block:
        return float(-0.691 + 10.0 * np.log10(np.mean(y ** 2) + 1e-12))
    ms = np.array([np.mean(y[s:s + block] ** 2)
                   for s in range(0, len(y) - block + 1, hop)])
    lj = -0.691 + 10.0 * np.log10(ms + 1e-12)
    keep = lj >= -70.0                                   # absolute gate
    if not keep.any():
        return float("-inf")
    rel = -0.691 + 10.0 * np.log10(np.mean(ms[keep]) + 1e-12) - 10.0
    keep2 = keep & (lj >= rel)                           # relative gate
    if not keep2.any():
        keep2 = keep
    return float(-0.691 + 10.0 * np.log10(np.mean(ms[keep2]) + 1e-12))


# -------------------------------------------------------------------- driver

def measure(manifest_path, out_dir, plots=False):
    manifest = json.loads(Path(manifest_path).read_text())
    shift = manifest.get("shift_semitones")
    target = manifest.get("target_note_hz")
    ref, sr = read_wav(manifest["reference"])
    out_dir = Path(out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)

    loaded = {c["mode"]: read_wav(c["path"]) for c in manifest["conditions"]}
    classic = loaded.get("CLASSIC", (None, sr))[0]

    rows = []
    for mode, (sig, msr) in loaded.items():
        if msr != sr:
            print(f"WARNING: {mode} sample rate {msr} != reference {sr}", file=sys.stderr)
        rise_ms, pre_echo_db = transient_spread_ms(sig, msr)
        row = {
            "shift_semitones": shift,
            "mode": mode,
            "latency_ms_vs_bypass": round(estimate_latency_ms(ref, sig, msr), 3),
            "pitch_hz": round(fundamental_hz(sig, msr, target), 3),
            "pitch_cents_err": round(cents(fundamental_hz(sig, msr, target), target), 2) if target else "",
            "low_band_ratio": round(low_band_ratio(sig, msr), 5),
            "low_band_loss_vs_classic": (
                round(low_band_ratio(classic, sr) - low_band_ratio(sig, msr), 5)
                if classic is not None else ""),
            "env_dist_db_vs_classic": (
                round(envelope_distance_db(classic, sig, msr), 3)
                if classic is not None and mode != "CLASSIC" else ""),
            "transient_rise_ms": round(rise_ms, 3),
            "pre_echo_db": round(pre_echo_db, 2),
            "null_db_vs_bypass": round(align_and_null_db(ref, sig, msr), 2) if shift == 0 else "",
            "loudness_dbfs": round(loudness_dbfs(sig), 2),
            "lufs": round(lufs_integrated(sig, msr), 2),
        }
        rows.append(row)
        print(f"{mode:8s} lat={row['latency_ms_vs_bypass']:>7}ms  "
              f"pitch={row['pitch_cents_err']}c  lowΔ={row['low_band_loss_vs_classic']}  "
              f"envΔ={row['env_dist_db_vs_classic']}dB  preEcho={row['pre_echo_db']}dB")

    csv_path = out_dir / f"transpose_metrics_shift{shift}.csv"
    with csv_path.open("w", newline="") as fh:
        writer = csv.DictWriter(fh, fieldnames=list(rows[0].keys()))
        writer.writeheader()
        writer.writerows(rows)
    print(f"wrote {csv_path}")

    if plots:
        _plots(ref, loaded, sr, shift, out_dir)
    return rows


def matchgains(manifest_path, target_lufs=None):
    """Print the gain (dB) to apply to each file so every condition is loudness-
    matched for the blind listening test (protocol §4). Default target is the
    quietest clip, so every gain is <= 0 dB and nothing is pushed into clipping."""
    manifest = json.loads(Path(manifest_path).read_text())
    files = {"bypass": manifest["reference"]}
    files.update({c["mode"]: c["path"] for c in manifest["conditions"]})
    lufs = {}
    for name, path in files.items():
        sig, sr = read_wav(path)
        lufs[name] = lufs_integrated(sig, sr)
    target = target_lufs if target_lufs is not None else min(lufs.values())
    print(f"target = {target:.2f} LUFS")
    rows = []
    for name, value in lufs.items():
        gain = target - value
        rows.append((name, value, gain))
        print(f"  {name:8s}  measured={value:7.2f} LUFS   apply {gain:+6.2f} dB")
    if target_lufs is None and any(g > 0.01 for _, _, g in rows):
        print("note: a clip needed positive gain; check for clipping or pass --target-lufs")
    return rows


def _plots(ref, loaded, sr, shift, out_dir):
    try:
        import matplotlib
        matplotlib.use("Agg")
        import matplotlib.pyplot as plt
    except ImportError:
        print("matplotlib not installed; skipping plots", file=sys.stderr)
        return
    fig, ax = plt.subplots(figsize=(9, 5))
    freqs = np.fft.rfftfreq(len(ref), 1.0 / sr)
    for mode, (sig, msr) in loaded.items():
        n = min(len(sig), len(ref))
        spec = 20 * np.log10(np.abs(np.fft.rfft(sig[:n] * np.hanning(n))) + 1e-9)
        ax.semilogx(np.fft.rfftfreq(n, 1.0 / msr), spec, label=mode, alpha=0.8)
    ax.axvline(LOW_BAND_HZ, ls="--", c="k", alpha=0.3)
    ax.set(xlabel="Hz", ylabel="dB", title=f"Transpose modes — shift {shift}",
           xlim=(20, sr / 2))
    ax.legend()
    fig.tight_layout()
    path = out_dir / f"transpose_spectra_shift{shift}.png"
    fig.savefig(path, dpi=120)
    print(f"wrote {path}")


# ------------------------------------------------------------------- selftest

def selftest():
    sr = 48000
    n = sr

    # Latency is measured on a transient (protocol §3.1 "feed an impulse"), so the
    # harness is validated on one: a 30 ms raised-cosine gated burst with a sharp
    # onset, present in `ref` at 50 ms and in `test` delayed by a known 20 ms.
    blen = int(0.03 * sr)
    gate = np.sin(np.pi * np.arange(blen) / blen) ** 2
    burst = gate * np.sin(2 * np.pi * 220.0 * np.arange(blen) / sr)
    onset, delay = 2400, 960                              # 50 ms onset, +20 ms delay
    ref = np.zeros(n); ref[onset:onset + blen] = burst
    test = np.zeros(n); test[onset + delay:onset + delay + blen] = burst
    lat = estimate_latency_ms(ref, test, sr)
    assert abs(lat - 20.0) < 1.0, f"latency recovery off: {lat:.3f} ms"

    # Pitch / null are validated on a sustained tone (FFT resolution).
    t = np.arange(n) / sr
    tone = np.sin(2 * np.pi * 220.0 * t)
    f0 = fundamental_hz(tone, sr, 220.0)
    assert abs(cents(f0, 220.0)) < 5.0, f"pitch recovery off: {cents(f0,220.0):.2f} c"
    null = align_and_null_db(tone, tone.copy(), sr)
    assert null < -100.0, f"identical null not deep: {null:.1f} dB"

    lr_low = low_band_ratio(np.sin(2 * np.pi * 50 * t), sr)
    lr_high = low_band_ratio(np.sin(2 * np.pi * 2000 * t), sr)
    assert lr_low > lr_high, "low-band ratio ordering wrong"

    # LUFS sanity: a 0 dBFS 1 kHz tone reads near -3 LUFS (K-weight ~0 dB at 1 kHz);
    # a 10 dB attenuation must drop the reading by ~10 LU.
    full = np.sin(2 * np.pi * 1000.0 * t)
    lu_full = lufs_integrated(full, sr)
    lu_down = lufs_integrated(full * 10 ** (-10 / 20), sr)
    assert -5.0 < lu_full < -1.0, f"LUFS of 0 dBFS 1 kHz off: {lu_full:.2f}"
    assert abs((lu_full - lu_down) - 10.0) < 0.3, f"LUFS scaling off: {lu_full-lu_down:.2f}"
    print(f"selftest OK  (latency={lat:.3f}ms pitch={cents(f0,220.0):.2f}c "
          f"null={null:.1f}dB lufs_1k={lu_full:.2f})")


def main(argv=None):
    p = argparse.ArgumentParser(description=__doc__,
                                formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = p.add_subparsers(dest="cmd", required=True)
    m = sub.add_parser("measure", help="analyse rendered WAVs from a manifest")
    m.add_argument("manifest")
    m.add_argument("--out", default="transpose-results")
    m.add_argument("--plots", action="store_true")
    g = sub.add_parser("matchgains", help="per-file LUFS and the gain to level-match them")
    g.add_argument("manifest")
    g.add_argument("--target-lufs", type=float, default=None)
    sub.add_parser("selftest", help="validate the metric harness on synthetic signals")
    args = p.parse_args(argv)
    if args.cmd == "selftest":
        selftest()
    elif args.cmd == "matchgains":
        matchgains(args.manifest, args.target_lufs)
    else:
        measure(args.manifest, args.out, args.plots)


if __name__ == "__main__":
    main()
