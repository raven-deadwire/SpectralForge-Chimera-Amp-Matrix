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
# 150 Hz hybrid with low-band onset re-anchor, low-band delay bound 40 / 20 / 12 ms.
ANCHOR_MODES = [(0, "CLASSIC"), (2, "TIGHT"), (12, "HYB150"), (14, "HYB150-A40"), (15, "HYB150-A20"), (16, "HYB150-A12")]
SHIFTS = [0, -12, -7, -5, -2, 2, 5, 7, 12]
PRE_S, NOTE_S = 0.5, 2.5
REPICK_S = 0.6          # repick material: a new pluck every 0.6 s, the previous one cut off
ONSET_SPLIT_HZ = 150.0  # low-band onset measured below the hybrid crossover

def pluck(f0, rate, seconds=NOTE_S, harmonics=12):
    """Harmonic pluck: 2 ms attack, higher partials decay faster."""
    t = np.arange(int(rate * seconds)) / rate
    x = np.zeros_like(t)
    for k in range(1, harmonics + 1):
        if k * f0 >= 0.45 * rate: break
        x += (1.0 / k) * np.sin(2 * np.pi * k * f0 * t) * np.exp(-t / (1.2 / (1 + 0.35 * k)))
    x *= np.minimum(1.0, t / 0.002)
    return x

def repick(f0, rate):
    x = np.zeros(int(rate * NOTE_S))
    for k in range(int(NOTE_S / REPICK_S)):
        start = int(k * REPICK_S * rate)
        note = pluck(f0, rate, REPICK_S)
        x[start:start + len(note)] = note[:len(x) - start]
    return x

def events(name):
    """Input onset times (s) of each material, after the 0.5 s lead-in."""
    if name == 'clicks': return [PRE_S + 0.25 * i for i in range(int(NOTE_S / 0.25))]
    if name.endswith('_repick'): return [PRE_S + REPICK_S * i for i in range(int(NOTE_S / REPICK_S))]
    return [PRE_S]

def material(rate):
    """name -> (mono signal, fundamental Hz or None). Peak -6 dBFS after 0.5 s silence."""
    items = {
        'bass_B0':        (pluck(30.87, rate), 30.87),
        'bass_E1':        (pluck(41.20, rate), 41.20),
        'guitar_E2':      (pluck(82.41, rate), 82.41),
        'lead_A3':        (pluck(220.0, rate), 220.0),
        'power_chord_E2': (pluck(82.41, rate) + pluck(123.47, rate) + pluck(164.81, rate), 82.41),
    }
    items['bass_E1_repick'] = (repick(41.20, rate), 41.20)
    items['guitar_E2_repick'] = (repick(82.41, rate), 82.41)
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

def lowpass_lr4(x, rate, hz=ONSET_SPLIT_HZ):
    w = 2 * np.pi * hz / rate; c, alpha = np.cos(w), np.sin(w) / np.sqrt(2)
    b = [(1 - c) / 2 / (1 + alpha), (1 - c) / (1 + alpha), (1 - c) / 2 / (1 + alpha)]
    a = [1.0, -2 * c / (1 + alpha), (1 - alpha) / (1 + alpha)]
    return tm._lfilter(b, a, tm._lfilter(b, a, np.asarray(x, dtype=float)))

def onset_latency_ms(reference, test, rate, times, max_ms=45.0):
    """Median per-event attack delay: the first sample after each input onset
    whose magnitude exceeds both a quarter of that event's peak (first 80 ms
    after the crossing window) and twice the preceding 30 ms (the previous
    note's tail), in reference and test, subtracted. Separate from the
    whole-file figure, which a long sustain dominates. An envelope or its
    derivative follows the waveform of a 31 Hz note and is not usable here."""
    def crossing(x, t, span):
        start, end = int(t * rate), int((t + span) * rate)
        base = np.max(np.abs(x[max(0, start - int(0.03 * rate)):start]), initial=0.0)
        peak = np.max(np.abs(x[start:end + int(0.08 * rate)]), initial=0.0)
        above = np.nonzero(np.abs(x[start:end]) > max(0.25 * peak, 2.0 * base))[0]
        return (start + above[0]) / rate if len(above) else None
    lags = []
    for t in times:
        r = crossing(reference, t, 0.02)
        if r is None: continue
        y = crossing(test, t, max_ms / 1000)
        if y is not None: lags.append(1000.0 * (y - r))
    return round(float(np.median(lags)), 2) if lags else None

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
                    onsets = {}
                    low_ref = lowpass_lr4(x, rate)
                    for c in conditions:
                        y = tm.read_wav(c['path'])[0]
                        onsets[c['mode']] = (onset_latency_ms(x, y, rate, events(name)),
                                             onset_latency_ms(low_ref, lowpass_lr4(y, rate), rate, events(name)))
                    for row in tm.measure(mpath, folder):
                        latency = reported[row['mode']]
                        row['onset_ms'], row['low_onset_ms'] = onsets[row['mode']]
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
    ap.add_argument('--anchor', action='store_true', help='150 Hz hybrid with low-band onset re-anchor (40/20/12 ms bounds)')
    args = ap.parse_args(argv)
    rows = run(args.render, args.out, rates=tuple(int(r) for r in args.rates.split(',')), block=args.block,
               modes=ANCHOR_MODES if args.anchor else HYBRID_MODES if args.hybrid else RESEARCH_MODES if args.research else MODES)
    print(f'{len(rows)} measurements -> {Path(args.out) / "summary.json"}')
    return 0

if __name__ == '__main__':
    sys.exit(main())
