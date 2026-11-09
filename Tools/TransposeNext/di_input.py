"""Read-only recording QC. No rendering, resampling, gain or label generation."""
import math
import struct

import numpy as np
from scipy.io import wavfile

RATES = (44100, 48000, 96000)
EDGE_S = .2
SILENCE_DB = -40.


def db(value):
    return float(20 * np.log10(value)) if value > 0 else None


def wav_format(path):
    """Read the original fmt chunk (24-bit PCM is int32 after scipy decoding)."""
    with open(path, 'rb') as f:
        header = f.read(12)
        if header[:4] not in (b'RIFF', b'RF64') or header[8:] != b'WAVE':
            raise ValueError('expected little-endian RIFF/RF64 WAV')
        while True:
            chunk = f.read(8)
            if len(chunk) != 8:
                raise ValueError('WAV fmt chunk missing')
            name, size = struct.unpack('<4sI', chunk)
            if name == b'fmt ':
                data = f.read(min(size, 40))
                if len(data) < 16:
                    raise ValueError('truncated WAV fmt chunk')
                tag, channels, rate, _, _, bits = struct.unpack('<HHIIHH', data[:16])
                valid_bits = struct.unpack('<H', data[18:20])[0] if tag == 65534 and len(data) >= 40 else bits
                return dict(format_tag=tag, channels=channels, sample_rate_hz=rate,
                            bits_per_sample=bits, valid_bits_per_sample=valid_bits)
            f.seek(size + size % 2, 1)


def inspect_wav(path, channel=0):
    """Return unmodified mono float64, rate, blocking problems, serializable QC."""
    fmt = wav_format(path)
    rate, data = wavfile.read(path)
    channels = 1 if data.ndim == 1 else data.shape[1]
    if type(channel) is not int or not 0 <= channel < channels:
        raise ValueError(f'channel {channel!r} outside 0..{channels - 1}')
    if data.dtype not in (np.dtype('int16'), np.dtype('int32'), np.dtype('float32')):
        raise ValueError(f'unsupported sample format {data.dtype}')
    if len(data) == 0:
        raise ValueError('empty WAV (zero frames)')
    selected = data if channels == 1 else data[:, channel]
    z = selected.astype(np.float64)
    # Convert before abs: abs(int16(-32768)) otherwise overflows to -32768.
    scale = 32768. if data.dtype == np.int16 else 2.**31 if data.dtype == np.int32 else 1.
    z /= scale
    threshold = 32767. / 32768. if data.dtype == np.int16 else (2.**31 - 256) / 2.**31 if data.dtype == np.int32 else .999
    issues = []
    if rate not in RATES:
        issues.append(f'sample rate {rate} not in {RATES}')
    nonfinite = int(np.count_nonzero(~np.isfinite(z)))
    clipped = int(np.count_nonzero(np.abs(z) >= threshold))
    qc = dict(**fmt, decoded_dtype=str(data.dtype), selected_channel=channel,
              frames=len(z), duration_s=len(z) / rate,
              nonfinite_samples=nonfinite, full_scale_samples=clipped,
              peak_dbfs=None, rms_dbfs=None, quiet_dc=None,
              dc_method='mean of quietest 10% of 50 ms blocks', warnings=[])
    if clipped:
        issues.append('clipped (full-scale samples)')
    if nonfinite:
        issues.append('nonfinite samples')
    else:
        peak = float(np.abs(z).max())
        qc['peak_dbfs'] = db(peak)
        qc['rms_dbfs'] = db(float(np.sqrt(np.mean(z * z))))
        if peak < .01:
            issues.append(f'too quiet (peak {db(max(peak, 1e-12)):.1f} dBFS < -40)')
        if peak > 10**(-1 / 20):
            qc['warnings'].append('peak above recommended -1 dBFS headroom')
        n = max(1, round(.05 * rate))
        blocks = z[:len(z) // n * n].reshape(-1, n) if len(z) >= n else z[None, :]
        quiet = blocks[np.argsort((blocks * blocks).mean(1))[:max(1, len(blocks) // 10)]]
        qc['quiet_dc'] = float(quiet.mean())
        if abs(qc['quiet_dc']) > 1e-3:
            issues.append('DC offset > 1e-3')
    if len(z) > 600 * rate:
        issues.append('longer than 10 minutes')
    return z, rate, issues, qc


def finite_number(x):
    return isinstance(x, (int, float)) and not isinstance(x, bool) and math.isfinite(x)


def segment_structure(segments):
    issues, ids, previous_end = [], set(), 0.
    if not segments:
        return ['no segments']
    for seg in segments:
        sid = seg.get('id', '?')
        if sid in ids:
            issues.append(f'{sid}: duplicate segment id')
        ids.add(sid)
        a, b = seg.get('start_s'), seg.get('end_s')
        if not finite_number(a) or not finite_number(b) or a < 0 or b <= a:
            issues.append(f'{sid}: invalid finite start_s/end_s')
            continue
        if a < previous_end:
            issues.append(f'{sid}: overlapping or unordered segment')
        previous_end = b
        if seg.get('kind') == 'riff' and b - a > 20:
            issues.append(f'{sid}: riff longer than 20 s')
    return issues


def inspect_segment(z, rate, seg, require_mutes=False):
    sid = seg.get('id', '?')
    a_s, b_s = seg.get('start_s'), seg.get('end_s')
    if not finite_number(a_s) or not finite_number(b_s):
        return [f'{sid}: invalid finite start_s/end_s'], {}
    a, b = round(a_s * rate), round(b_s * rate)
    e = round(EDGE_S * rate)
    qc = dict(id=sid, start_sample=a, end_sample_exclusive=b, muted_edges={})
    if a < 0 or b > len(z) or b - a < 4 * e:
        return [f'{sid}: bounds outside file or shorter than {4 * EDGE_S}s'], qc
    if not np.all(np.isfinite(z[a:b])):
        return [f'{sid}: nonfinite segment'], qc
    peak = float(np.abs(z[a:b]).max())
    issues = []
    if peak == 0:
        issues.append(f'{sid}: all-silent segment')

    def edge(name, lo, hi):
        rms = float(np.sqrt(np.mean(z[lo:hi]**2)))
        qc['muted_edges'][name] = dict(start_sample=lo, end_sample_exclusive=hi,
                                     duration_s=(hi-lo)/rate, rms_dbfs=db(rms),
                                     rms_relative_peak_db=db(rms/peak) if peak else None)
        if rms > peak * 10**(SILENCE_DB/20):
            issues.append(f'{sid}: {name} not silent (muted strings required)')

    edge('start_0.2s', a, a+e)
    edge('end_0.2s', b-e, b)
    before, after = seg.get('silence_before_end_s'), seg.get('silence_after_start_s')
    if require_mutes or before is not None or after is not None:
        if not finite_number(before) or not finite_number(after):
            issues.append(f'{sid}: record both observed 0.5 s mute boundaries')
        elif not (a_s <= before <= after <= b_s):
            issues.append(f'{sid}: mute boundaries outside/overlapping segment')
        else:
            left, right = round(before*rate), round(after*rate)
            if left-a < math.ceil(.5*rate) or b-right < math.ceil(.5*rate):
                issues.append(f'{sid}: each recorded mute must be at least 0.5 s')
            if left > a:
                edge('observed_leading', a, left)
            if b > right:
                edge('observed_trailing', right, b)
    if seg.get('kind') == 'steady' and b_s-a_s < 4.5:
        issues.append(f'{sid}: steady segment shorter than 4.5 s')
    return issues, qc
