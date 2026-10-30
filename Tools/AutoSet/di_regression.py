#!/usr/bin/env python3
"""Replay the nine Transpose DI takes through production Auto Set. No recordings supplied."""
import argparse
import hashlib
import json
import subprocess
import sys
from pathlib import Path

import numpy as np
from scipy.io import wavfile
from scipy.signal import butter, sosfilt

RATES = (44100, 48000, 96000)
INSTRUMENTS = ('guitar_dropA_A1', 'bass_B0', 'bass_dropA_A0')


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def audio(path):
    rate, data = wavfile.read(path)
    if data.ndim != 1 or rate not in RATES or not len(data):
        raise ValueError(f'{path}: require nonempty mono WAV at native 44.1/48/96 kHz')
    if data.dtype == np.int16:
        data = data.astype(np.float64) / 32768
    elif data.dtype == np.int32:
        data = data.astype(np.float64) / 2147483648
    elif data.dtype in (np.float32, np.float64):
        data = data.astype(np.float64)
    else:
        raise ValueError(f'{path}: unsupported WAV encoding')
    if not np.isfinite(data).all() or np.max(np.abs(data)) >= .999:
        raise ValueError(f'{path}: nonfinite or clipped audio')
    if len(data) > 120 * rate:
        raise ValueError(f'{path}: longer than renderer limit (120 seconds)')
    return rate, data


def resolve(base, value):
    return (base / value).resolve() if value else None


def inspect(takes_path, spec_path):
    takes = json.loads(takes_path.read_text())
    spec = json.loads(spec_path.read_text())
    errors, missing, ready = [], [], []
    rows = takes['takes']
    if len(rows) != 9 or len({t['id'] for t in rows}) != 9 or {(t['instrument'], t['rate']) for t in rows} != {(i, r) for i in INSTRUMENTS for r in RATES}:
        errors.append('Expected the nine unique native-rate/instrument takes from PR #198')
    for take in rows:
        tid = take['id']
        wav = resolve(takes_path.parent, take.get('wav'))
        if not wav or not wav.is_file():
            missing.append(f'{tid}: no recording')
            continue
        if not take.get('labels_reviewed') or not take.get('take_note', {}).get('date'):
            missing.append(f'{tid}: reviewed labels/session date missing')
            continue
        labels = resolve(takes_path.parent, take.get('labels'))
        if not labels or not labels.is_file():
            missing.append(f'{tid}: reviewed labels file missing')
            continue
        try:
            rate, data = audio(wav)
            if rate != take['rate']:
                raise ValueError('declared/native sample rate mismatch; do not resample')
            windows = spec['windows'].get(tid)
            if not windows:
                missing.append(f'{tid}: reviewed six-second analysis windows missing')
                continue
            kinds = {w['segment'] for w in windows}
            if not {'dynamics', 'steady-open', 'steady-fret5', 'riff'}.issubset(kinds):
                raise ValueError('need dynamics, steady-open, steady-fret5 and riff windows')
            for window in windows:
                start = window.get('start_s')
                if not isinstance(start, (float, int)) or not np.isfinite(start):
                    raise ValueError('analysis start must be a reviewed finite time')
                segment = next(s for s in take['segments'] if s['id'] == window['segment'])
                if start < segment['start_s'] or start + 6 > segment['end_s'] or round((start+6)*rate) > len(data):
                    raise ValueError('six-second window outside source segment/WAV')
            ready.append((take, wav, labels, windows))
        except (ValueError, KeyError, StopIteration) as e:
            errors.append(f'{tid}: {e}')
    profiles = []
    for entry in spec['profiles']:
        path = resolve(spec_path.parent, entry['path'])
        if not path or not path.is_file():
            missing.append(f"profile {entry['id']}: absent")
            continue
        profile = json.loads(path.read_text())
        if entry.get('requires_real_ir') and (not profile.get('ir') or not profile.get('ir_provenance')):
            missing.append(f"profile {entry['id']}: real cabinet IR and provenance required")
            continue
        ir = resolve(path.parent, profile.get('ir'))
        if ir and not ir.is_file():
            missing.append(f"profile {entry['id']}: IR file absent")
            continue
        profiles.append((entry, path, ir))
    status = 'INVALID_INPUT' if errors else 'MISSING_INPUT' if missing else 'READY_TO_MEASURE'
    return {'status': status, 'errors': errors, 'missing': missing,
            'recording_test_completed': False, 'listening_completed': False}, ready, profiles


def band_delta(before, after, rate, frequency):
    # Paired renders share the same EQ-input samples; no cross-take alignment or gain matching.
    sos = butter(3, [frequency * .94, frequency * 1.06], fs=rate, btype='bandpass', output='sos')
    a, b = sosfilt(sos, before), sosfilt(sos, after)
    a, b = a[round(.25*rate):], b[round(.25*rate):]
    return float(10*np.log10(max(np.dot(b, b), 1e-30)/max(np.dot(a, a), 1e-30)))


def run(args, ready, profiles):
    out = args.out.resolve()
    out.mkdir(parents=True, exist_ok=False)
    executable = args.renderer.resolve()
    report = {'status': 'MEASURED_REQUIRES_REVIEW', 'listening_completed': False,
              'renderer_sha256': sha(executable), 'script_sha256': sha(__file__),
              'takes_manifest_sha256': sha(args.takes), 'spec_sha256': sha(args.spec), 'cases': []}
    for take, wav, labels, windows in ready:
        instrument = 'guitar' if take['instrument'].startswith('guitar') else 'bass'
        for entry, profile, ir in profiles:
            if entry['instrument'] != instrument:
                continue
            dest = out / (take['id'] + '--' + entry['id'])
            subprocess.run([str(executable), '--dynamic-eq-capture', str(wav), str(profile), str(dest)], check=True)
            rate, captured = audio(dest / 'pre-tone.wav')
            capture_meta = json.loads((dest/'capture.json').read_text())
            latency = capture_meta['tone_tap_latency_samples']
            for window in windows:
                # Original times are DI-relative. Exclude the later POST FX latency.
                start = round(window['start_s'] * rate) + latency
                clip = captured[start:start+6*rate]
                if len(clip) != 6*rate:
                    raise ValueError('latency-adjusted analysis window exceeds capture')
                input_path = dest / (window['segment'] + '.wav')
                wavfile.write(input_path, rate, clip.astype(np.float32))
                segment = next(s for s in take['segments'] if s['id'] == window['segment'])
                fundamental = segment.get('f0_hint_hz', take['lowest_f0_hz'])
                for strength in (.25, .5, .75, 1.):
                    result_dir = dest / (window['segment'] + '-' + str(strength))
                    subprocess.run([str(executable), '--dynamic-eq-wav', str(input_path), instrument,
                                    entry['style'], str(strength), str(result_dir)], check=True)
                    result = json.loads((result_dir/'analysis.json').read_text())
                    _, before = audio(result_dir/'before.wav')
                    _, after = audio(result_dir/'after.wav')
                    deltas = {str(h): band_delta(before, after, rate, fundamental*h) for h in (1, 2, 3)}
                    # Safety guard only. No automatic claim that a real resonance is musically undesirable.
                    protected = window['segment'].startswith('steady') or window['segment'] == 'dynamics'
                    failed = instrument == 'bass' and protected and deltas['1'] < -.5
                    report['cases'].append({'take': take['id'], 'profile': entry['id'], 'segment': window['segment'],
                        'strength': strength, 'di_sha256': sha(wav), 'labels_sha256': sha(labels),
                        'profile_sha256': sha(profile), 'ir_sha256': sha(ir) if ir else None,
                        'capture_sha256': sha(dest/'pre-tone.wav'), 'analysis_input_sha256': sha(input_path),
                        'analysis_report_sha256': sha(result_dir/'analysis.json'),
                        'nodes': result['nodes'], 'maximum_reduction_db': result['maximum_reduction_db'],
                        'reference_f0_hz': fundamental, 'harmonic_delta_db': deltas, 'low_fundamental_guard': 'FAIL' if failed else 'PASS' if instrument == 'bass' and protected else 'NOT_APPLICABLE',
                        'resonance_origin': 'REQUIRES_PAIRED_CAPTURE_AND_LISTENING'})
    if any(c['low_fundamental_guard'] == 'FAIL' for c in report['cases']):
        report['status'] = 'FAIL_LOW_FUNDAMENTAL_GUARD'
    (out/'report.json').write_text(json.dumps(report, indent=2)+'\n')
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('command', choices=('plan', 'run'))
    parser.add_argument('--takes', type=Path, required=True)
    parser.add_argument('--spec', type=Path, required=True)
    parser.add_argument('--renderer', type=Path)
    parser.add_argument('--out', type=Path)
    args = parser.parse_args()
    report, ready, profiles = inspect(args.takes.resolve(), args.spec.resolve())
    if args.command == 'run' and report['status'] == 'READY_TO_MEASURE':
        if not args.renderer or not args.out:
            parser.error('run needs --renderer and --out')
        report = run(args, ready, profiles)
    print(json.dumps(report, indent=2))
    return 0 if report['status'] in ('READY_TO_MEASURE', 'MEASURED_REQUIRES_REVIEW') else 2


if __name__ == '__main__':
    try:
        sys.exit(main())
    except (ValueError, OSError, subprocess.CalledProcessError) as error:
        print(json.dumps({'status': 'ERROR', 'error': str(error), 'listening_completed': False}))
        sys.exit(1)
