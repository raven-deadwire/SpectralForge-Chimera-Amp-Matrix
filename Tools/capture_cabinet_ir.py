#!/usr/bin/env python3
"""Black-box cabinet/IR capture via exponential sine-sweep (ESS) deconvolution.

This is the standard, legitimate way to capture the *linear* response of a
cabinet + microphone chain (a real miked cab, a reamp loop, or any processor
you are licensed to run): play a log sweep through the target, record the
output, and deconvolve to recover the impulse response. Nothing here inspects,
decompiles, or redistributes another product's code or assets -- it only
measures how a signal is transformed, exactly as a hardware IR vendor would.

Scope and honesty:
  * Captures a LINEAR time-invariant response only. A full amp's nonlinear
    distortion CANNOT be represented by one IR; capture the CAB/mic stage with
    drive neutral. The Farina method pushes harmonic-distortion products to
    negative time, so this tool keeps only the causal linear IR and reports the
    residual distortion energy it discarded.
  * You are responsible for the target's licence. Measuring the response of a
    tool you own for your own interoperability/research is generally fine;
    redistributing IRs that capture someone else's copyrighted cabinet voicing
    is a separate question -- keep captures private unless you have the rights.

Output matches the Chimera IR LOADER contract (see Source/IRLibrary.cpp):
  mono WAV, 8-384 kHz, >= 8 samples and <= 1 second, finite, |sample| <= 32.

Requires numpy only. No downloads, no network, no third-party binaries.

Usage
-----
  # 1. Generate the test sweep to play through the target (and a param sidecar):
  python3 capture_cabinet_ir.py sweep --out sweep.wav --sr 48000

  # 2. Record the target's output of that sweep to response.wav, then:
  python3 capture_cabinet_ir.py deconvolve --sweep sweep.wav \
          --response response.wav --out captured_ir.wav --ms 120

  # Self-test the whole pipeline against a known reference IR (no hardware):
  python3 capture_cabinet_ir.py selftest --reference ../Assets/IRs/guitar_v30_sm57.wav
"""
import argparse
import json
import struct
import sys
from pathlib import Path

import numpy as np


# --------------------------------------------------------------------------- #
# Minimal dependency-free WAV I/O (PCM 16/24/32 and IEEE float 32/64).
# --------------------------------------------------------------------------- #
def read_wav(path, mono=True):
    data = Path(path).read_bytes()
    if data[:4] != b"RIFF" or data[8:12] != b"WAVE":
        raise ValueError(f"{path}: not a RIFF/WAVE file")
    fmt = None
    samples = None
    pos = 12
    while pos + 8 <= len(data):
        cid = data[pos:pos + 4]
        size = struct.unpack_from("<I", data, pos + 4)[0]
        body = data[pos + 8:pos + 8 + size]
        if cid == b"fmt ":
            tag, ch, sr, _, _, bits = struct.unpack_from("<HHIIHH", body, 0)
            fmt = dict(tag=tag, ch=ch, sr=sr, bits=bits)
        elif cid == b"data":
            samples = body
        pos += 8 + size + (size & 1)
    if fmt is None or samples is None:
        raise ValueError(f"{path}: missing fmt/data chunk")
    ch, bits, tag = fmt["ch"], fmt["bits"], fmt["tag"]
    if tag == 3 and bits == 32:
        x = np.frombuffer(samples, dtype="<f4").astype(np.float64)
    elif tag == 3 and bits == 64:
        x = np.frombuffer(samples, dtype="<f8").astype(np.float64)
    elif tag in (1, 0xFFFE) and bits == 16:
        x = np.frombuffer(samples, dtype="<i2").astype(np.float64) / 32768.0
    elif tag in (1, 0xFFFE) and bits == 24:
        raw = np.frombuffer(samples, dtype=np.uint8).reshape(-1, 3).astype(np.int32)
        v = (raw[:, 0] | (raw[:, 1] << 8) | (raw[:, 2] << 16))
        v = np.where(v & 0x800000, v - 0x1000000, v)
        x = v.astype(np.float64) / 8388608.0
    elif tag in (1, 0xFFFE) and bits == 32:
        x = np.frombuffer(samples, dtype="<i4").astype(np.float64) / 2147483648.0
    else:
        raise ValueError(f"{path}: unsupported format tag={tag} bits={bits}")
    if ch > 1:
        x = x.reshape(-1, ch)
        x = x.mean(axis=1) if mono else x   # (n,) mono, else (n, ch)
    return fmt["sr"], x


def write_wav_float32(path, sr, x):
    x = np.ascontiguousarray(x, dtype="<f4")
    body = x.tobytes()
    fmt = struct.pack("<HHIIHH", 3, 1, sr, sr * 4, 4, 32)
    chunks = b"fmt " + struct.pack("<I", len(fmt)) + fmt
    chunks += b"data" + struct.pack("<I", len(body)) + body
    riff = b"RIFF" + struct.pack("<I", 4 + len(chunks)) + b"WAVE" + chunks
    Path(path).write_bytes(riff)


# --------------------------------------------------------------------------- #
# Exponential sine sweep (Farina) and its inverse filter.
# --------------------------------------------------------------------------- #
def ess(f1, f2, dur, sr):
    """Return an exponential (log) sine sweep from f1 to f2 Hz over dur seconds.

    Recovery is done by regularised spectral deconvolution against this exact
    sweep (see deconvolve_spectral), which is numerically robust where an
    analytic time-reversed inverse rings and colours the band edges.
    """
    n = int(round(dur * sr))
    t = np.arange(n) / sr
    w1, w2 = 2 * np.pi * f1, 2 * np.pi * f2
    k = np.log(w2 / w1)
    sweep = np.sin(w1 * dur / k * (np.exp(t / dur * k) - 1.0))
    # Fade the raw sweep edges so it is safe to emit and play.
    fade = max(1, int(0.005 * sr))
    env = np.ones(n)
    env[:fade] = np.linspace(0, 1, fade)
    env[-fade:] = np.linspace(1, 0, fade)
    return (sweep * env).astype(np.float64)


def fft_convolve(a, b):
    n = 1
    while n < len(a) + len(b) - 1:
        n <<= 1
    return np.fft.irfft(np.fft.rfft(a, n) * np.fft.rfft(b, n), n)[:len(a) + len(b) - 1]


def deconvolve_spectral(response, sweep, reg_db=-60.0):
    """Recover the linear impulse response by regularised spectral division.

    response = sweep (convolved with) IR, so IR = response / sweep in the
    frequency domain. A Tikhonov floor tied to the sweep's peak magnitude keeps
    the division stable outside the excited band, where |sweep| -> 0.
    """
    n = 1
    while n < len(response) + len(sweep):
        n <<= 1
    S = np.fft.rfft(sweep, n)
    R = np.fft.rfft(response, n)
    reg = (10.0 ** (reg_db / 20.0)) ** 2 * np.max(np.abs(S) ** 2)
    return np.fft.irfft(R * np.conj(S) / (np.abs(S) ** 2 + reg), n)


def fft_resample(x, sr_in, sr_out):
    """Band-limited Fourier resampling (as scipy.signal.resample), numpy-only.
    Suitable for decaying IRs; edge effects are negligible for a windowed IR."""
    if sr_in == sr_out:
        return np.asarray(x, dtype=np.float64)
    n_in = len(x)
    n_out = int(round(n_in * sr_out / sr_in))
    X = np.fft.rfft(x)
    Y = np.zeros(n_out // 2 + 1, dtype=complex)
    k = min(len(X), len(Y))
    Y[:k] = X[:k]
    if n_out < n_in and k > 0:                       # downsample: halve Nyquist bin
        Y[k - 1] *= 0.5
    return np.fft.irfft(Y, n_out) * (n_out / n_in)


def to_minimum_phase(ir, oversample=8):
    """Return the minimum-phase IR with the same magnitude response (homomorphic
    / real-cepstrum method). Energy is front-loaded, so a zero-latency cab stage
    keeps the voicing without the linear IR's bulk delay."""
    n = len(ir)
    nfft = 1
    while nfft < oversample * n:
        nfft <<= 1
    mag = np.abs(np.fft.fft(ir, nfft))
    logmag = np.log(np.maximum(mag, 1e-12))
    cep = np.fft.ifft(logmag).real                   # real cepstrum
    w = np.zeros(nfft)
    w[0] = 1.0
    w[1:nfft // 2] = 2.0
    w[nfft // 2] = 1.0                               # causal (min-phase) window
    mp = np.fft.ifft(np.exp(np.fft.fft(cep * w))).real
    return mp[:n]


# --------------------------------------------------------------------------- #
# IR extraction / conditioning to the IR LOADER contract.
# --------------------------------------------------------------------------- #
def extract_ir(full, sr, length_ms, prefix_ms=1.0):
    """Isolate the causal linear IR from a deconvolved response, window it,
    and normalise it. `full` comes from deconvolve_spectral.
    Returns (ir, onset_seconds)."""
    peak = int(np.argmax(np.abs(full)))
    length = max(8, int(round(length_ms * 1e-3 * sr)))
    prefix = min(peak, int(round(prefix_ms * 1e-3 * sr)))
    start = peak - prefix
    ir = full[start:start + length].copy()
    if len(ir) < length:
        ir = np.pad(ir, (0, length - len(ir)))
    # Short fades only: ramp through the pre-onset prefix, and a brief
    # raised-cosine tail purely to kill the truncation click. Keep them short
    # so the captured magnitude response is preserved, not reshaped.
    fin = max(1, min(prefix, int(0.0005 * sr)))
    fout = min(len(ir) // 8, max(8, int(0.004 * sr)))
    if fin > 0:
        ir[:fin] *= np.linspace(0, 1, fin)
    ir[-fout:] *= 0.5 * (1 + np.cos(np.linspace(0, np.pi, fout)))
    peakabs = float(np.max(np.abs(ir)))
    if peakabs > 1e-20:
        ir = ir / peakabs * 0.98  # headroom; loader rejects |x|>32, this is <1
    return ir, peak / sr


def linear_residual_db(response, sweep, full):
    """Fraction of the recorded response the recovered linear IR cannot explain.

    Reconvolve the recovered IR with the sweep and compare to the recording.
    On a clean linear target only the regularisation error remains (very low);
    a nonlinear target (drive/compression) leaves its harmonics unexplained, so
    this rises. This is the honest nonlinearity/noise indicator -- not a claim
    that distortion separates cleanly in time."""
    recon = fft_convolve(sweep, full)
    m = min(len(response), len(recon))
    r = response[:m] - recon[:m]
    return 10.0 * np.log10((np.sum(r ** 2) + 1e-30) / (np.sum(response[:m] ** 2) + 1e-30))


def enforce_loader_contract(ir, sr):
    if not np.all(np.isfinite(ir)):
        raise ValueError("IR contains non-finite samples")
    if len(ir) < 8:
        raise ValueError("IR shorter than loader minimum (8 samples)")
    if len(ir) > sr:
        raise ValueError(f"IR longer than 1 second ({len(ir)} > {sr})")
    if np.max(np.abs(ir)) > 32.0:
        raise ValueError("IR exceeds loader amplitude limit (|x|>32)")


# --------------------------------------------------------------------------- #
# Subcommands.
# --------------------------------------------------------------------------- #
def cmd_sweep(args):
    sweep = ess(args.f1, args.f2, args.dur, args.sr)
    # Pad trailing silence so the target's tail is fully captured in the recording.
    tail = np.zeros(int(round(args.tail * args.sr)))
    out = np.concatenate([sweep, tail]) * args.level
    write_wav_float32(args.out, args.sr, out)
    sidecar = Path(args.out).with_suffix(".sweep.json")
    sidecar.write_text(json.dumps(
        dict(f1=args.f1, f2=args.f2, dur=args.dur, sr=args.sr,
             level=args.level, tail=args.tail), indent=2))
    print(f"wrote {args.out} ({len(out)} samples @ {args.sr} Hz)")
    print(f"wrote {sidecar} (replay these exact params when deconvolving)")
    print("Play this sweep through the target and record its output at the "
          "same sample rate.")


def _load_sweep_params(sweep_path, args):
    sidecar = Path(sweep_path).with_suffix(".sweep.json")
    if sidecar.exists():
        p = json.loads(sidecar.read_text())
        return p["f1"], p["f2"], p["dur"], p["sr"]
    return args.f1, args.f2, args.dur, None


def finish_ir(full, sr, args):
    """Condition one deconvolved response into a deliverable IR: window, then
    optional minimum-phase, optional resample, normalise, enforce the contract.
    Returns (ir, out_sr, onset_seconds)."""
    ir, onset = extract_ir(full, sr, args.ms, args.prefix)
    if args.min_phase:
        ir = to_minimum_phase(ir)
    out_sr = sr
    if args.target_sr and args.target_sr != sr:
        ir = fft_resample(ir, sr, args.target_sr)
        out_sr = args.target_sr
    peak = float(np.max(np.abs(ir)))
    if peak > 1e-20:
        ir = ir / peak * 0.98
    enforce_loader_contract(ir, out_sr)
    return ir, out_sr, onset


def cmd_deconvolve(args):
    rate, resp = read_wav(args.response, mono=False)
    f1, f2, dur, sr = _load_sweep_params(args.sweep, args)
    if sr is None:
        sr = rate
    if rate != sr:
        raise SystemExit(f"response is {rate} Hz but sweep is {sr} Hz; "
                         "record at the sweep's sample rate")
    sweep = ess(f1, f2, dur, sr)
    if resp.ndim == 1:
        channels = [("", resp)]                      # single mono response
    else:
        labels = ["micA", "micB", "mic3", "mic4"][:resp.shape[1]]
        channels = [(labels[i], resp[:, i]) for i in range(resp.shape[1])]
        print(f"stereo/multi response: capturing {len(channels)} mic channels "
              f"({', '.join(l for l, _ in channels)})")
    out = Path(args.out)
    for label, x in channels:
        full = deconvolve_spectral(x, sweep, args.reg)
        ir, out_sr, onset = finish_ir(full, sr, args)
        residual_db = linear_residual_db(x, sweep, full)
        path = out if not label else out.with_name(f"{out.stem}_{label}{out.suffix}")
        write_wav_float32(str(path), out_sr, ir)
        tag = f"[{label}] " if label else ""
        extra = (" [min-phase]" if args.min_phase else "")
        extra += (f" [resampled {sr}->{out_sr} Hz]" if out_sr != sr else "")
        print(f"{tag}wrote {path}: mono, {len(ir)} samples "
              f"({1000*len(ir)/out_sr:.1f} ms) @ {out_sr} Hz{extra}")
        print(f"{tag}linear onset at {onset*1000:.1f} ms; unexplained "
              f"(nonlinear/noise) residual {residual_db:+.1f} dB vs the recording")
        if residual_db > -20:
            print(f"{tag}WARNING: large unexplained residual -- the target is "
                  "likely nonlinear (drive/compression on?) or the recording is "
                  "noisy. A single IR cannot model distortion; capture the "
                  "CAB/mic stage clean.")


def cmd_selftest(args):
    """Convolve a sweep through a known reference IR, deconvolve, and report
    how faithfully the pipeline recovers it. Proves the method without any
    hardware or third-party target."""
    sr = args.sr
    if args.reference and Path(args.reference).exists():
        rrate, ref = read_wav(args.reference)
        if rrate != sr:
            print(f"(reference is {rrate} Hz; running self-test at {rrate} Hz)")
            sr = rrate
        ref = ref[:sr]  # clamp to <=1s
        label = args.reference
    else:
        # Synthetic band-limited decaying IR as a stand-in cabinet response.
        n = int(0.08 * sr)
        t = np.arange(n) / sr
        ref = (np.sin(2 * np.pi * 95 * t) * np.exp(-t * 70)
               + 0.4 * np.sin(2 * np.pi * 2200 * t) * np.exp(-t * 300))
        ref /= np.max(np.abs(ref))
        label = "synthetic reference IR"

    f2 = min(args.f2, 0.45 * sr)  # stay clear of Nyquist for a fair sweep
    sweep = ess(args.f1, f2, args.dur, sr)
    response = fft_convolve(sweep, ref)              # ideal linear target
    full = deconvolve_spectral(response, sweep)

    # Method validation: align the recovered IR to the reference on their peaks
    # using an identical window (same pre-peak length and span) so the score
    # reflects the deconvolution, not a sub-sample offset.
    pr, pf = int(np.argmax(np.abs(ref))), int(np.argmax(np.abs(full)))
    pre = min(pr, pf, int(0.002 * sr))
    span = min(len(ref) - pr, len(full) - pf, int(0.5 * sr))
    a = ref[pr - pre:pr + span].astype(np.float64)
    b = full[pf - pre:pf + span].astype(np.float64)
    an = a / (np.max(np.abs(a)) + 1e-30)
    bn = b / (np.max(np.abs(b)) + 1e-30)
    corr = float(np.dot(an, bn) / (np.linalg.norm(an) * np.linalg.norm(bn) + 1e-30))
    # Magnitude response (gain-normalised; an IR's absolute level is arbitrary).
    A = np.abs(np.fft.rfft(a, 2 * sr)) + 1e-9
    B = np.abs(np.fft.rfft(b, 2 * sr)) + 1e-9
    freqs = np.fft.rfftfreq(2 * sr, 1 / sr)
    band = (freqs >= args.f1) & (freqs <= f2)
    mag_db_err = 20 * np.log10(B[band] / A[band])
    mag_db_err -= np.mean(mag_db_err)
    rms_db = float(np.sqrt(np.mean(mag_db_err ** 2)))
    residual_db = linear_residual_db(response, sweep, full)

    # Contract validation: the delivered, conditioned IR meets the IR LOADER spec.
    ir, _ = extract_ir(full, sr, 1000.0 * len(ref) / sr + 5, prefix_ms=0.5)
    enforce_loader_contract(ir, sr)

    # Minimum-phase conversion must preserve magnitude and front-load energy.
    mp = to_minimum_phase(a)
    Amp = np.abs(np.fft.rfft(a, 2 * sr)) + 1e-9
    Mmp = np.abs(np.fft.rfft(mp, 2 * sr)) + 1e-9
    mp_err = 20 * np.log10(Mmp[band] / Amp[band])
    mp_err -= np.mean(mp_err)
    mp_rms = float(np.sqrt(np.mean(mp_err ** 2)))

    def centroid(x):
        e = x ** 2
        return float(np.sum(np.arange(len(x)) * e) / (np.sum(e) + 1e-30))
    front = centroid(mp) < centroid(a)

    # Resampling must preserve the magnitude response up to the new Nyquist.
    rs_sr = 48000 if sr != 48000 else 44100
    rs = fft_resample(a, sr, rs_sr)
    fr2 = min(f2, 0.45 * rs_sr)
    fa, fb = np.fft.rfftfreq(2 * sr, 1 / sr), np.fft.rfftfreq(2 * rs_sr, 1 / rs_sr)
    cf = np.linspace(args.f1, fr2, 300)
    ia = np.interp(cf, fa, np.abs(np.fft.rfft(a, 2 * sr)))
    ib = np.interp(cf, fb, np.abs(np.fft.rfft(rs, 2 * rs_sr)))
    rs_err = 20 * np.log10((ib + 1e-9) / (ia + 1e-9))
    rs_err -= np.mean(rs_err)
    rs_rms = float(np.sqrt(np.mean(rs_err ** 2)))

    print(f"self-test against {label} @ {sr} Hz")
    print(f"  time-domain shape correlation : {corr:.4f}  (1.000 = identical)")
    print(f"  magnitude error {args.f1:.0f}-{f2:.0f} Hz : {rms_db:.3f} dB RMS")
    print(f"  linear-model residual         : {residual_db:+.1f} dB")
    print(f"  delivered IR meets loader contract: OK "
          f"({len(ir)} samples, {1000*len(ir)/sr:.0f} ms)")
    print(f"  min-phase magnitude error     : {mp_rms:.3f} dB RMS, "
          f"energy front-loaded: {'yes' if front else 'NO'}")
    print(f"  resample {sr}->{rs_sr} Hz magnitude error : {rs_rms:.3f} dB RMS")
    ok = (corr > 0.999 and rms_db < 0.3 and residual_db < -40
          and mp_rms < 0.5 and front and rs_rms < 0.3)
    print(f"  RESULT: {'PASS' if ok else 'CHECK'} "
          f"(expect corr>0.999, err<0.3 dB, residual<-40 dB, min-phase/resample "
          f"mag preserved)")
    return 0 if ok else 1


def main(argv=None):
    p = argparse.ArgumentParser(description=__doc__,
                                formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = p.add_subparsers(dest="cmd", required=True)

    s = sub.add_parser("sweep", help="generate an ESS test sweep + param sidecar")
    s.add_argument("--out", default="sweep.wav")
    s.add_argument("--f1", type=float, default=20.0)
    s.add_argument("--f2", type=float, default=20000.0)
    s.add_argument("--dur", type=float, default=5.0, help="sweep seconds")
    s.add_argument("--tail", type=float, default=1.0, help="trailing silence s")
    s.add_argument("--sr", type=int, default=48000)
    s.add_argument("--level", type=float, default=0.5, help="sweep peak level")
    s.set_defaults(func=cmd_sweep)

    d = sub.add_parser("deconvolve", help="recover IR from a recorded response")
    d.add_argument("--sweep", required=True, help="the sweep.wav used (reads its sidecar)")
    d.add_argument("--response", required=True, help="recorded target output WAV")
    d.add_argument("--out", default="captured_ir.wav")
    d.add_argument("--ms", type=float, default=120.0, help="IR length in ms (<=1000)")
    d.add_argument("--prefix", type=float, default=1.0, help="ms kept before onset")
    d.add_argument("--reg", type=float, default=-60.0,
                   help="deconvolution regularisation floor in dB (lower = sharper)")
    d.add_argument("--target-sr", type=int, default=0,
                   help="resample the IR to this sample rate (0 = keep source)")
    d.add_argument("--min-phase", action="store_true",
                   help="convert to minimum phase (front-loaded, zero-latency cab)")
    d.add_argument("--f1", type=float, default=20.0)
    d.add_argument("--f2", type=float, default=20000.0)
    d.add_argument("--dur", type=float, default=5.0)
    d.set_defaults(func=cmd_deconvolve)

    t = sub.add_parser("selftest", help="validate the pipeline against a known IR")
    t.add_argument("--reference", default="", help="reference IR WAV (optional)")
    t.add_argument("--f1", type=float, default=20.0)
    t.add_argument("--f2", type=float, default=20000.0)
    t.add_argument("--dur", type=float, default=5.0)
    t.add_argument("--sr", type=int, default=48000)
    t.set_defaults(func=cmd_selftest)

    args = p.parse_args(argv)
    return args.func(args) or 0


if __name__ == "__main__":
    sys.exit(main())
