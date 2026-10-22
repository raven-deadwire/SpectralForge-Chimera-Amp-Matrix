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
def read_wav(path):
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
        x = x.reshape(-1, ch).mean(axis=1)  # downmix to mono
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


def cmd_deconvolve(args):
    rate, response = read_wav(args.response)
    f1, f2, dur, sr = _load_sweep_params(args.sweep, args)
    if sr is None:
        sr = rate
    if rate != sr:
        raise SystemExit(f"response is {rate} Hz but sweep is {sr} Hz; "
                         "record at the sweep's sample rate")
    sweep = ess(f1, f2, dur, sr)
    full = deconvolve_spectral(response, sweep, args.reg)
    ir, onset = extract_ir(full, sr, args.ms, args.prefix)
    residual_db = linear_residual_db(response, sweep, full)
    enforce_loader_contract(ir, sr)
    write_wav_float32(args.out, sr, ir)
    print(f"wrote {args.out}: mono, {len(ir)} samples "
          f"({1000*len(ir)/sr:.1f} ms) @ {sr} Hz")
    print(f"linear onset at {onset*1000:.1f} ms; unexplained (nonlinear/noise) "
          f"residual {residual_db:+.1f} dB vs the recording")
    if residual_db > -20:
        print("WARNING: large unexplained residual -- the target is likely "
              "nonlinear (drive/compression on?) or the recording is noisy. A "
              "single IR cannot model distortion; capture the CAB/mic stage clean.")


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

    print(f"self-test against {label} @ {sr} Hz")
    print(f"  time-domain shape correlation : {corr:.4f}  (1.000 = identical)")
    print(f"  magnitude error {args.f1:.0f}-{f2:.0f} Hz : {rms_db:.3f} dB RMS")
    print(f"  linear-model residual         : {residual_db:+.1f} dB")
    print(f"  delivered IR meets loader contract: OK "
          f"({len(ir)} samples, {1000*len(ir)/sr:.0f} ms)")
    ok = corr > 0.999 and rms_db < 0.3 and residual_db < -40
    print(f"  RESULT: {'PASS' if ok else 'CHECK'} "
          f"(expect corr>0.999, err<0.3 dB, residual<-40 dB on a clean target)")
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
