#!/usr/bin/env python3
"""FabFilter black-box measurement kit (#97 P0-2).

Measures how a reference plug-in transforms known test signals and compares
it with Chimera's production DSP. It only analyses audio a user rendered in a
DAW: it never loads, inspects, decompiles or redistributes FabFilter code or
assets. numpy only (matplotlib optional for plots).

  generate --out KIT [--rate 48000]              test WAVs, manifest.json, RENDER_GUIDE.md
  chimera  --kit KIT --probe PROBE --out DIR     same signals through Chimera (ChimeraKitProbe)
  analyze  --kit KIT --renders DIR --out JSON    per-case response / static curve / timing
  compare  --kit KIT --reference DIR --candidate DIR --out JSON [--plots DIR]

Render files are named <case id>.wav at the kit sample rate. See
docs/FABFILTER_BLACKBOX_KIT.md.
"""
import argparse, hashlib, json, math, struct, subprocess, sys, tempfile
from pathlib import Path
import numpy as np

KIT_VERSION = 1
IMPULSE_LENGTH = 1 << 17
IMPULSE_OFFSET = 8192
IMPULSE_AMPLITUDE = 0.25
TONE_HZ = 1000.0
STATIC_LEVELS = list(range(-40, 1, 2))      # sine peak dBFS
STATIC_STEP_S = 0.6
STEP_LEVELS = (-40.0, -4.0, -40.0)          # quiet, loud, quiet
STEP_SECONDS = (1.0, 1.5, 1.5)

# Chimera Tone EQ type indices (Source/GraphicalEQ.h eqFilterNames()).
EQ_CASES = [
    ("eq_bell_1k_p6_q1",   "Bell",              1000, +6.0, 1.0,   0),
    ("eq_bell_10k_p6_q1",  "Bell",             10000, +6.0, 1.0,   0),
    ("eq_bell_100_m6_q4",  "Bell",               100, -6.0, 4.0,   0),
    ("eq_lowshelf_100_p6", "Low Shelf",          100, +6.0, 0.707, 1),
    ("eq_highshelf_8k_p6", "High Shelf",        8000, +6.0, 0.707, 2),
    ("eq_lowcut12_100",    "Low Cut 12 dB/oct",  100,  0.0, 0.707, 3),
    ("eq_highcut12_10k",   "High Cut 12 dB/oct", 10000, 0.0, 0.707, 4),
    ("eq_notch_1k_q4",     "Notch",             1000,  0.0, 4.0,   5),
]
COMP_CASES = [
    ("comp_static_k0",  "static", -20.0, 4.0, 10.0, 100.0, 0.0),
    ("comp_static_k12", "static", -20.0, 4.0, 10.0, 100.0, 12.0),
    ("comp_step_10_100", "step",  -20.0, 4.0, 10.0, 100.0, 0.0),
    ("comp_step_1_50",   "step",  -20.0, 4.0, 1.0,   50.0, 0.0),
]

# ---------------------------------------------------------------- WAV I/O
def write_wav(path, audio, rate):
    """Stereo IEEE float32 WAV."""
    audio = np.asarray(audio, dtype=np.float32)
    if audio.ndim == 1:
        audio = np.stack([audio, audio], axis=1)
    data = audio.astype('<f4').tobytes()
    header = b'RIFF' + struct.pack('<I', 36 + len(data)) + b'WAVE'
    header += b'fmt ' + struct.pack('<IHHIIHH', 16, 3, audio.shape[1], int(rate), int(rate) * 4 * audio.shape[1], 4 * audio.shape[1], 32)
    Path(path).write_bytes(header + b'data' + struct.pack('<I', len(data)) + data)

def read_wav(path):
    """Returns (float64 frames x channels, rate). PCM 16/24/32 and float 32/64."""
    raw = Path(path).read_bytes()
    if raw[:4] != b'RIFF' or raw[8:12] != b'WAVE':
        raise ValueError(f'{path}: not a RIFF/WAVE file')
    fmt = data = None; pos = 12
    while pos + 8 <= len(raw):
        tag, size = raw[pos:pos + 4], struct.unpack('<I', raw[pos + 4:pos + 8])[0]
        body = raw[pos + 8:pos + 8 + size]
        if tag == b'fmt ': fmt = body
        elif tag == b'data': data = body
        pos += 8 + size + (size & 1)
    if fmt is None or data is None:
        raise ValueError(f'{path}: missing fmt or data chunk')
    code, channels, rate, _, align, bits = struct.unpack('<HHIIHH', fmt[:16])
    if code == 0xFFFE and len(fmt) >= 26:
        code = struct.unpack('<H', fmt[24:26])[0]
    frames = len(data) // align
    if code == 3 and bits in (32, 64):
        x = np.frombuffer(data[:frames * align], dtype='<f4' if bits == 32 else '<f8').astype(np.float64)
    elif code == 1 and bits == 16:
        x = np.frombuffer(data[:frames * align], dtype='<i2') / 32768.0
    elif code == 1 and bits == 24:
        b = np.frombuffer(data[:frames * align], dtype=np.uint8).reshape(-1, 3).astype(np.int32)
        v = b[:, 0] | (b[:, 1] << 8) | (b[:, 2] << 16)
        x = np.where(v >= 1 << 23, v - (1 << 24), v) / float(1 << 23)
    elif code == 1 and bits == 32:
        x = np.frombuffer(data[:frames * align], dtype='<i4') / 2147483648.0
    else:
        raise ValueError(f'{path}: unsupported WAV format {code}/{bits}-bit')
    return x.reshape(-1, channels), rate

# ---------------------------------------------------------------- signals
def sine_segments(levels_db, seconds, rate):
    """Phase-continuous 1 kHz sine whose peak level steps per segment."""
    counts = [int(round(s * rate)) for s in seconds]
    amplitude = np.concatenate([np.full(c, 10 ** (l / 20)) for l, c in zip(levels_db, counts)])
    n = np.arange(len(amplitude))
    return amplitude * np.sin(2 * np.pi * TONE_HZ * n / rate), counts

def signal_for(case, rate):
    if case['kind'] == 'eq':
        x = np.zeros(IMPULSE_LENGTH); x[IMPULSE_OFFSET] = IMPULSE_AMPLITUDE
        return x
    if case['signal'] == 'static':
        return sine_segments(STATIC_LEVELS, [STATIC_STEP_S] * len(STATIC_LEVELS), rate)[0]
    return sine_segments(STEP_LEVELS, STEP_SECONDS, rate)[0]

def build_manifest(rate):
    cases = []
    for cid, shape, freq, gain, q, chimera_type in EQ_CASES:
        cases.append({'id': cid, 'kind': 'eq', 'signal': 'impulse',
                      'reference': {'plugin': 'FabFilter Pro-Q 4', 'processing_mode': 'Zero Latency', 'band_shape': shape,
                                    'frequency_hz': freq, 'gain_db': gain, 'q': q, 'output_db': 0, 'auto_gain': 'off', 'other_bands': 'none'},
                      'chimera': {'type': chimera_type, 'frequency': freq, 'gain': gain, 'q': q}})
    for cid, signal, thr, ratio, attack, release, knee in COMP_CASES:
        cases.append({'id': cid, 'kind': 'comp', 'signal': signal,
                      'reference': {'plugin': 'FabFilter Pro-C 3', 'style': 'Clean', 'threshold_db': thr, 'ratio': ratio,
                                    'attack_ms': attack, 'release_ms': release, 'knee_db': knee, 'range_db': 'maximum',
                                    'lookahead_ms': 0, 'hold_ms': 0, 'auto_release': 'off', 'auto_gain': 'off', 'output_db': 0,
                                    'mix': '100%', 'stereo_link': '100%', 'sidechain_filters': 'off', 'oversampling': 'off'},
                      'chimera': {'threshold': thr, 'ratio': ratio, 'attack': attack, 'release': release, 'knee': knee, 'detector': 'rms'}})
    return {'kit_version': KIT_VERSION, 'sample_rate': rate, 'tone_hz': TONE_HZ,
            'impulse': {'length': IMPULSE_LENGTH, 'offset': IMPULSE_OFFSET, 'amplitude': IMPULSE_AMPLITUDE},
            'static_levels_dbfs_peak': STATIC_LEVELS, 'static_step_s': STATIC_STEP_S,
            'step_levels_dbfs_peak': list(STEP_LEVELS), 'step_seconds': list(STEP_SECONDS), 'cases': cases}

GUIDE = """# Chimera FabFilter black-box kit — render guide

Sample rate: {rate} Hz. Each `signals/<id>.wav` is a 32-bit float stereo file.

1. Create a {rate} Hz session. Import one signal per track, start at the session start.
2. Insert only the listed plug-in with the settings below (all other bands/features off).
   Disable dither, normalisation, master processing and track fades.
3. Export (bounce) each track **post-insert, 32-bit float, {rate} Hz, stereo, full length**
   and name it exactly `<id>.wav`. Keep plug-in delay compensation ON.
4. Put the exports in one folder and run
   `python Tools/fabfilter_kit.py analyze --kit <kit> --renders <folder> --out reference.json`.
5. Record the exact plug-in version, OS and DAW in `renders/INFO.txt`.

| id | plug-in | settings |
|---|---|---|
{rows}
"""

def generate(out, rate):
    out = Path(out); (out / 'signals').mkdir(parents=True, exist_ok=True)
    manifest = build_manifest(rate)
    for case in manifest['cases']:
        path = out / 'signals' / f"{case['id']}.wav"
        write_wav(path, signal_for(case, rate), rate)
        case['signal_sha256'] = hashlib.sha256(path.read_bytes()).hexdigest()
    (out / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n', encoding='utf-8')
    rows = '\n'.join(f"| `{c['id']}` | {c['reference']['plugin']} | " +
                     ', '.join(f'{k}={v}' for k, v in c['reference'].items() if k != 'plugin') + ' |'
                     for c in manifest['cases'])
    (out / 'RENDER_GUIDE.md').write_text(GUIDE.format(rate=rate, rows=rows), encoding='utf-8')
    return manifest

def load_kit(kit):
    return json.loads((Path(kit) / 'manifest.json').read_text(encoding='utf-8'))

# ---------------------------------------------------------------- Chimera
def chimera(kit, probe, out):
    manifest = load_kit(kit); rate = manifest['sample_rate']
    out = Path(out); out.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='chimera-kit-') as tmp:
        for case in manifest['cases']:
            x, _ = read_wav(Path(kit) / 'signals' / f"{case['id']}.wav")
            src, dst = Path(tmp) / 'in.f32', Path(tmp) / 'out.f32'
            src.write_bytes(x.astype('<f4').tobytes())
            c = case['chimera']
            if case['kind'] == 'eq':
                args = ['eq', rate, c['type'], c['frequency'], c['gain'], c['q']]
            else:
                args = ['comp', rate, c['threshold'], c['ratio'], c['attack'], c['release'], c['knee'], c['detector']]
            subprocess.run([str(probe), *map(str, args), str(src), str(dst)], check=True)
            y = np.frombuffer(dst.read_bytes(), dtype='<f4').reshape(-1, 2)
            write_wav(out / f"{case['id']}.wav", y, rate)
    (out / 'INFO.txt').write_text('Chimera production DSP via ChimeraKitProbe (offline, 256-sample blocks)\n', encoding='utf-8')

# ---------------------------------------------------------------- analysis
def octave_grid(rate):
    top = min(20000.0, 0.45 * rate)
    return 20.0 * 2 ** (np.arange(0, int(np.floor(24 * np.log2(top / 20.0))) + 1) / 24.0)

def analyse_eq(x, y, rate):
    n = 1 << int(np.ceil(np.log2(max(len(x), len(y)) + IMPULSE_OFFSET)))
    X, Y = np.fft.rfft(x, n), np.fft.rfft(y, n)
    H = Y / np.where(np.abs(X) > 1e-12, X, 1e-12)
    h = np.fft.irfft(H, n)
    latency = int(np.argmax(np.abs(h)));  latency = latency - n if latency > n // 2 else latency
    f = np.fft.rfftfreq(n, 1 / rate)
    grid = octave_grid(rate)
    mag = 20 * np.log10(np.maximum(np.interp(grid, f, np.abs(H)), 1e-12))
    phase = np.unwrap(np.angle(H * np.exp(2j * np.pi * f * latency / rate)))
    phase_deg = np.degrees(np.interp(grid, f, phase))
    gd_1k = float(np.interp(1000.0, f, -np.gradient(np.unwrap(np.angle(H)), 2 * np.pi * f) * 1000))
    result = {'latency_samples': latency, 'group_delay_1k_ms': gd_1k,
              'grid_hz': grid.tolist(), 'magnitude_db': mag.tolist(), 'phase_deg': phase_deg.tolist()}
    result.update(shape_metrics(grid, mag))
    return result

def crossing(grid, mag, level, start, step):
    i = start
    while 0 <= i + step < len(grid):
        a, b = mag[i] - level, mag[i + step] - level
        if a == 0: return grid[i]
        if a * b < 0:
            t = a / (a - b); return float(np.exp(np.log(grid[i]) + t * (np.log(grid[i + step]) - np.log(grid[i]))))
        i += step
    return None

def shape_metrics(grid, mag):
    """Shape-independent features; each case reads the ones that apply."""
    peak, dip = int(np.argmax(mag)), int(np.argmin(mag))
    out = {'max_db': float(mag[peak]), 'max_hz': float(grid[peak]), 'min_db': float(mag[dip]), 'min_hz': float(grid[dip]),
           'low_end_db': float(mag[0]), 'high_end_db': float(mag[-1])}
    extreme = peak if abs(mag[peak]) >= abs(mag[dip]) else dip
    g = mag[extreme]
    if abs(g) > 0.5 and 0 < extreme < len(grid) - 1:
        lo, hi = crossing(grid, mag, g / 2, extreme, -1), crossing(grid, mag, g / 2, extreme, 1)
        if lo and hi:
            out['half_gain_q'] = float(grid[extreme] / (hi - lo)); out['half_gain_bandwidth_oct'] = float(np.log2(hi / lo))
    for name, level in (('minus3db_rising_hz', -3.0),):
        idx = crossing(grid, mag, level, 0, 1)
        if idx: out[name] = idx
    idx = crossing(grid, mag, -3.0, len(grid) - 1, -1)
    if idx: out['minus3db_falling_hz'] = idx
    span = mag[-1] - mag[0]
    if abs(span) > 1.0:
        out['shelf_midpoint_hz'] = crossing(grid, mag, mag[0] + span / 2, 0, 1)
    return out

def envelope(x):
    """Instantaneous amplitude via the analytic signal."""
    n = len(x); X = np.fft.fft(x); h = np.zeros(n); h[0] = 1
    if n % 2 == 0: h[n // 2] = 1; h[1:n // 2] = 2
    else: h[1:(n + 1) // 2] = 2
    return np.abs(np.fft.ifft(X * h))

def align(x, y, rate):
    """Integer lag of y versus x from the first quiet (linear) second."""
    m = min(len(x), len(y), int(rate))
    c = np.fft.irfft(np.fft.rfft(y[:m], 2 * m) * np.conj(np.fft.rfft(x[:m], 2 * m)), 2 * m)
    lag = int(np.argmax(c[:m // 2].tolist() + c[-m // 2:].tolist()))
    return lag if lag < m // 2 else lag - m

def gain_reduction(x, y, rate):
    lag = align(x, y, rate)
    y = np.roll(np.pad(y, (0, max(0, len(x) - len(y))))[:len(x)], -lag)
    ex, ey = envelope(x), envelope(y)
    k = max(1, int(rate / TONE_HZ))                           # one tone cycle
    smooth = lambda v: np.convolve(v, np.ones(k) / k, mode='same')
    gr = 20 * np.log10(np.maximum(smooth(ex), 1e-9) / np.maximum(smooth(ey), 1e-9))
    return gr, lag

def static_fit(levels, out_levels):
    """Grid fit of threshold / ratio / knee to the hard/soft-knee compressor law."""
    levels, gr = np.asarray(levels, float), np.asarray(levels, float) - np.asarray(out_levels, float)
    best = None
    for knee in np.arange(0, 24.5, 0.5):
        for thr in np.arange(-40, 0.25, 0.25):
            over = levels - thr
            soft = np.where(np.abs(over) < knee / 2, (over + knee / 2) ** 2 / (2 * knee) if knee > 0 else 0, np.maximum(over, 0))
            denom = float(np.dot(soft, soft))
            if denom < 1e-9: continue
            slope = float(np.clip(np.dot(soft, gr) / denom, 0, 1))
            err = float(np.sum((soft * slope - gr) ** 2))
            if best is None or err < best[0]: best = (err, thr, slope, knee)
    err, thr, slope, knee = best
    return {'threshold_db': float(thr), 'ratio': float(1 / (1 - slope)) if slope < 0.999 else float('inf'),
            'knee_db': float(knee), 'rms_error_db': math.sqrt(err / len(levels))}

def analyse_comp(case, manifest, x, y, rate):
    gr, lag = gain_reduction(x, y, rate)
    if case['signal'] == 'static':
        step = int(round(manifest['static_step_s'] * rate)); rows = []
        for i, level in enumerate(manifest['static_levels_dbfs_peak']):
            a, b = i * step + int(0.35 * rate), (i + 1) * step - int(0.05 * rate)
            rows.append({'input_dbfs_peak': level, 'gain_reduction_db': float(np.median(gr[a:b]))})
        fit = static_fit([r['input_dbfs_peak'] for r in rows], [r['input_dbfs_peak'] - r['gain_reduction_db'] for r in rows])
        return {'latency_samples': lag, 'static_curve': rows, 'fit': fit}
    c = [int(round(s * rate)) for s in manifest['step_seconds']]
    up, down = c[0], c[0] + c[1]
    steady = float(np.median(gr[down - int(0.3 * rate):down - int(0.02 * rate)]))
    def first(condition):
        hit = np.nonzero(condition)[0]
        return float(hit[0] / rate * 1000) if len(hit) else None
    att = gr[up:down]; rel = gr[down:]
    start_rel = float(np.median(gr[down - int(0.005 * rate):down]))
    return {'latency_samples': lag, 'steady_gain_reduction_db': steady,
            'attack_10_ms': first(att >= 0.1 * steady), 'attack_63_ms': first(att >= (1 - math.exp(-1)) * steady),
            'attack_90_ms': first(att >= 0.9 * steady),
            'release_63_ms': first(rel <= math.exp(-1) * start_rel), 'release_90_ms': first(rel <= 0.1 * start_rel),
            'gr_trace_ms': (np.arange(0, len(gr), int(rate / 1000)) / rate * 1000).tolist(),
            'gr_trace_db': gr[::int(rate / 1000)].tolist()}

def analyse(kit, renders):
    manifest = load_kit(kit); rate = manifest['sample_rate']; results = {}; missing = []
    for case in manifest['cases']:
        path = Path(renders) / f"{case['id']}.wav"
        if not path.exists(): missing.append(case['id']); continue
        y, r = read_wav(path)
        if r != rate: raise ValueError(f'{path}: rendered at {r} Hz; kit is {rate} Hz')
        x, _ = read_wav(Path(kit) / 'signals' / f"{case['id']}.wav")
        x, y = x[:, 0], y[:, 0]
        if not np.all(np.isfinite(y)): raise ValueError(f'{path}: non-finite samples')
        results[case['id']] = analyse_eq(x, y, rate) if case['kind'] == 'eq' else analyse_comp(case, manifest, x, y, rate)
        results[case['id']]['render_sha256'] = hashlib.sha256(path.read_bytes()).hexdigest()
    info = Path(renders) / 'INFO.txt'
    return {'kit_version': manifest['kit_version'], 'sample_rate': rate, 'renders': str(renders),
            'render_info': info.read_text(encoding='utf-8').strip() if info.exists() else None,
            'missing': missing, 'cases': results}

COMPARE_SCALARS = ('latency_samples', 'group_delay_1k_ms', 'max_db', 'max_hz', 'min_db', 'min_hz', 'half_gain_q', 'low_end_db', 'high_end_db',
                   'minus3db_rising_hz', 'minus3db_falling_hz', 'shelf_midpoint_hz', 'steady_gain_reduction_db',
                   'attack_10_ms', 'attack_63_ms', 'attack_90_ms', 'release_63_ms', 'release_90_ms')

def compare(reference, candidate):
    rows = {}
    for cid, ref in reference['cases'].items():
        cand = candidate['cases'].get(cid)
        if cand is None: continue
        row = {'reference': {}, 'candidate': {}, 'delta': {}}
        for key in COMPARE_SCALARS:
            if ref.get(key) is not None and cand.get(key) is not None:
                row['reference'][key], row['candidate'][key] = ref[key], cand[key]
                row['delta'][key] = cand[key] - ref[key]
        if 'magnitude_db' in ref:
            d = np.asarray(cand['magnitude_db']) - np.asarray(ref['magnitude_db'])
            grid = np.asarray(ref['grid_hz'])
            row['magnitude_max_abs_delta_db'] = float(np.max(np.abs(d)))
            row['magnitude_max_abs_delta_hz'] = float(grid[int(np.argmax(np.abs(d)))])
            row['magnitude_rms_delta_db'] = float(np.sqrt(np.mean(d ** 2)))
            dp = np.asarray(cand['phase_deg']) - np.asarray(ref['phase_deg'])
            dp -= 360 * np.round(np.median(dp) / 360)
            row['phase_max_abs_delta_deg'] = float(np.max(np.abs(dp)))
        if 'fit' in ref and 'fit' in cand:
            row['fit'] = {'reference': ref['fit'], 'candidate': cand['fit']}
            row['static_curve_max_abs_delta_db'] = float(max(abs(a['gain_reduction_db'] - b['gain_reduction_db'])
                                                            for a, b in zip(ref['static_curve'], cand['static_curve'])))
        rows[cid] = row
    return {'reference': reference.get('render_info'), 'candidate': candidate.get('render_info'), 'cases': rows,
            'note': 'Measured deltas only. Not an equivalence claim, listening result or acceptance verdict.'}

def plots(kit, reference, candidate, out):
    try:
        import matplotlib; matplotlib.use('Agg'); import matplotlib.pyplot as plt
    except ImportError:
        return False
    out = Path(out); out.mkdir(parents=True, exist_ok=True)
    for cid, ref in reference['cases'].items():
        cand = candidate['cases'].get(cid, {})
        fig, ax = plt.subplots(figsize=(8, 4))
        if 'magnitude_db' in ref:
            ax.semilogx(ref['grid_hz'], ref['magnitude_db'], label='reference')
            if cand: ax.semilogx(cand['grid_hz'], cand['magnitude_db'], '--', label='Chimera')
            ax.set_xlabel('Hz'); ax.set_ylabel('dB')
        elif 'static_curve' in ref:
            ax.plot([r['input_dbfs_peak'] for r in ref['static_curve']], [r['gain_reduction_db'] for r in ref['static_curve']], 'o-', label='reference')
            if cand: ax.plot([r['input_dbfs_peak'] for r in cand['static_curve']], [r['gain_reduction_db'] for r in cand['static_curve']], 's--', label='Chimera')
            ax.set_xlabel('input dBFS (sine peak)'); ax.set_ylabel('gain reduction dB')
        else:
            ax.plot(ref['gr_trace_ms'], ref['gr_trace_db'], label='reference')
            if cand: ax.plot(cand['gr_trace_ms'], cand['gr_trace_db'], '--', label='Chimera')
            ax.set_xlabel('ms'); ax.set_ylabel('gain reduction dB')
        ax.set_title(cid); ax.grid(True, alpha=.3); ax.legend(); fig.tight_layout(); fig.savefig(out / f'{cid}.png', dpi=110); plt.close(fig)
    return True

def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest='command', required=True)
    g = sub.add_parser('generate'); g.add_argument('--out', required=True); g.add_argument('--rate', type=int, default=48000)
    c = sub.add_parser('chimera'); c.add_argument('--kit', required=True); c.add_argument('--probe', required=True); c.add_argument('--out', required=True)
    a = sub.add_parser('analyze'); a.add_argument('--kit', required=True); a.add_argument('--renders', required=True); a.add_argument('--out', required=True)
    m = sub.add_parser('compare'); m.add_argument('--kit', required=True); m.add_argument('--reference', required=True)
    m.add_argument('--candidate', required=True); m.add_argument('--out', required=True); m.add_argument('--plots')
    args = ap.parse_args(argv)
    if args.command == 'generate':
        manifest = generate(args.out, args.rate); print(f"wrote {len(manifest['cases'])} signals to {args.out}")
    elif args.command == 'chimera':
        chimera(args.kit, args.probe, args.out); print(f'wrote Chimera renders to {args.out}')
    elif args.command == 'analyze':
        result = analyse(args.kit, args.renders); Path(args.out).write_text(json.dumps(result, indent=1) + '\n', encoding='utf-8')
        print(f"analysed {len(result['cases'])} cases; missing: {', '.join(result['missing']) or 'none'}")
    else:
        reference, candidate = analyse(args.kit, args.reference), analyse(args.kit, args.candidate)
        result = compare(reference, candidate); Path(args.out).write_text(json.dumps(result, indent=1) + '\n', encoding='utf-8')
        if args.plots: plots(args.kit, reference, candidate, args.plots)
        for cid, row in result['cases'].items():
            summary = row.get('magnitude_max_abs_delta_db', row.get('static_curve_max_abs_delta_db'))
            print(f"{cid}: {'max |delta| %.2f dB' % summary if summary is not None else ''} {json.dumps({k: round(v, 3) for k, v in row['delta'].items()})}")
    return 0

if __name__ == '__main__':
    sys.exit(main())
