#!/usr/bin/env python3
"""Synthetic-material run of the objective half of docs/TRANSPOSE_EVIDENCE_PROTOCOL.md.

Renders deterministic synthetic material through Chimera's production
Transposer (ChimeraTransposeRender) in CLASSIC / LOW-LAT / TIGHT for the
protocol's shifts, then applies Tools/transpose_measure.py unchanged. The
bypass reference is the dry input (transpose off reports no added latency).

Synthetic plucks are NOT isolated DI recordings: this covers protocol §3 on
known signals only. The DI pass (§0–§3 on real recordings) and blind
listening (§5) remain required.

  python Tools/transpose_synthetic_evidence.py --render build/.../ChimeraTransposeRender --out DIR
"""
import argparse, json, math, struct, subprocess, sys, tempfile
from pathlib import Path
import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parent))
import transpose_measure as tm

MODES = [(0, "CLASSIC"), (1, "LOW-LAT"), (2, "TIGHT")]
# Research engine (Tools/TransposeNext, #117 0a54c10): variable delay, no fixed PDC.
RESEARCH_MODES = [(0, "CLASSIC"), (2, "TIGHT"), (10, "SPLIT-UP")]
# Research hybrid: low band time-domain, upper band 10 ms STFT, LR4 split at 100/150/250 Hz.
HYBRID_MODES = [(0, "CLASSIC"), (2, "TIGHT"), (10, "SPLIT-UP"), (11, "HYB100"), (12, "HYB150"), (13, "HYB250")]
SHIFTS = [0, -12, -7, -5, -2, 2, 5, 7, 12]
PRE_S, NOTE_S = 0.5, 2.5

def pluck(f0, rate, seconds=NOTE_S, harmonics=12):
    """Harmonic pluck: 2 ms attack, higher partials decay faster."""
    t = np.arange(int(rate * seconds)) / rate
    x = np.zeros_like(t)
    for k in range(1, harmonics + 1):
        if k * f0 >= 0.45 * rate: break
        x += (1.0 / k) * np.sin(2 * np.pi * k * f0 * t) * np.exp(-t / (1.2 / (1 + 0.35 * k)))
    x *= np.minimum(1.0, t / 0.002)
    return x

def material(rate):
    """name -> (mono signal, fundamental Hz or None). Peak -6 dBFS after 0.5 s silence."""
    items = {
        'bass_B0':        (pluck(30.87, rate), 30.87),
        'bass_E1':        (pluck(41.20, rate), 41.20),
        'guitar_E2':      (pluck(82.41, rate), 82.41),
        'lead_A3':        (pluck(220.0, rate), 220.0),
        'power_chord_E2': (pluck(82.41, rate) + pluck(123.47, rate) + pluck(164.81, rate), 82.41),
    }
    clicks = np.zeros(int(rate * NOTE_S))
    for i in range(int(NOTE_S / 0.25)):
        n = int(i * 0.25 * rate); clicks[n:n + int(0.001 * rate)] = np.hanning(int(0.001 * rate))
    items['clicks'] = (clicks, None)
    out = {}
    for name, (x, f0) in items.items():
        x = np.concatenate([np.zeros(int(PRE_S * rate)), x])
        out[name] = (0.5 * x / np.max(np.abs(x)), f0)
    return out

def write_wav(path, x, rate):
    data = np.asarray(x, dtype='<f4').tobytes()
    Path(path).write_bytes(b'RIFF' + struct.pack('<I', 36 + len(data)) + b'WAVE' +
                           b'fmt ' + struct.pack('<IHHIIHH', 16, 3, 1, rate, rate * 4, 4, 32) +
                           b'data' + struct.pack('<I', len(data)) + data)

def render(probe, rate, block, mode, shift, x, tmp):
    src, dst = Path(tmp) / 'in.f32', Path(tmp) / 'out.f32'
    src.write_bytes(np.asarray(x, dtype='<f4').tobytes())
    latency = int(subprocess.run([str(probe), str(rate), str(block), str(mode), str(shift), str(src), str(dst)],
                                 check=True, capture_output=True, text=True).stdout.strip())
    return np.frombuffer(dst.read_bytes(), dtype='<f4'), latency

def run(probe, out, rates=(48000, 96000), block=128, modes=MODES):
    out = Path(out); rows = []
    with tempfile.TemporaryDirectory(prefix='transpose-evidence-') as tmp:
        for rate in rates:
            for name, (x, f0) in material(rate).items():
                folder = out / f'{rate}' / name; folder.mkdir(parents=True, exist_ok=True)
                write_wav(folder / 'bypass.wav', x, rate)
                for shift in SHIFTS:
                    conditions, reported = [], {}
                    for mode, label in modes:
                        y, latency = render(probe, rate, block, mode, shift, x, tmp)
                        path = folder / f'{label.lower()}_{shift:+d}.wav'; write_wav(path, y, rate)
                        conditions.append({'mode': label, 'path': str(path)}); reported[label] = latency
                    manifest = {'sample_rate_hint': rate, 'shift_semitones': shift, 'reference': str(folder / 'bypass.wav'),
                                'conditions': conditions}
                    if f0: manifest['target_note_hz'] = f0 * 2 ** (shift / 12)
                    mpath = folder / f'manifest_{shift:+d}.json'; mpath.write_text(json.dumps(manifest, indent=1))
                    for row in tm.measure(mpath, folder):
                        latency = reported[row['mode']]
                        row.update({'rate': rate, 'block': block, 'material': name,
                                    'reported_latency_ms': round(1000 * latency / rate, 3) if latency >= 0 else None})
                        rows.append(row)
    (out / 'summary.json').write_text(json.dumps(rows, indent=1) + '\n', encoding='utf-8')
    return rows

def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--render', required=True, help='built ChimeraTransposeRender')
    ap.add_argument('--out', required=True); ap.add_argument('--block', type=int, default=128)
    ap.add_argument('--rates', default='48000,96000', help='comma-separated sample rates')
    ap.add_argument('--research', action='store_true', help='CLASSIC / TIGHT / research SPLIT-UP instead of the product modes')
    ap.add_argument('--hybrid', action='store_true', help='also the low-band/STFT hybrid at 100/150/250 Hz')
    args = ap.parse_args(argv)
    rows = run(args.render, args.out, rates=tuple(int(r) for r in args.rates.split(',')), block=args.block,
               modes=HYBRID_MODES if args.hybrid else RESEARCH_MODES if args.research else MODES)
    print(f'{len(rows)} measurements -> {Path(args.out) / "summary.json"}')
    return 0

if __name__ == '__main__':
    sys.exit(main())
