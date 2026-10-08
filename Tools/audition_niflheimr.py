#!/usr/bin/env python3
"""DI audition coordinator. Technical completion never implies musical acceptance."""
from __future__ import annotations
import argparse
import csv
import hashlib
import json
import math
from pathlib import Path
import random
import shutil
import subprocess
import sys
import wave

import numpy as np
from measure_niflheimr import digest, read_wav, save_json, measurement_stage

CHANNELS = ['Hrímfaxi', 'Garmr', 'Nidavellir', 'Ymir', 'Hel']
PRESETS = ['Frostline Precision', 'Carrion Barrage', 'Foundry Pulse', 'Jötunn Hammer', 'Mirebound Monolith']


def load_audio(path):
    raw = Path(path).read_bytes()
    try:
        return read_wav(path, raw=raw)
    except ValueError:
        with wave.open(str(path), 'rb') as w:
            channels, width, rate, frames = w.getnchannels(), w.getsampwidth(), w.getframerate(), w.getnframes()
            data = w.readframes(frames)
        if channels not in (1, 2) or width not in (2, 3, 4) or len(data) != frames * channels * width:
            raise ValueError('Use mono/stereo PCM16/24/32 or float32 WAV')
        if width == 3:
            b = np.frombuffer(data, np.uint8).reshape(-1, 3).astype(np.int32)
            x = b[:, 0] | b[:, 1] << 8 | b[:, 2] << 16
            x = (x ^ 0x800000) - 0x800000
        else:
            x = np.frombuffer(data, '<i2' if width == 2 else '<i4')
        return rate, (x.astype(np.float64) / 2 ** (width * 8 - 1)).reshape(-1, channels)


def write_float(path, rate, samples):
    import struct
    x = np.asarray(samples, dtype='<f4')
    if x.ndim != 2 or not np.isfinite(x).all():
        raise ValueError('Invalid audio')
    payload = x.tobytes()
    channels = x.shape[1]
    fmt = struct.pack('<HHIIHH', 3, channels, rate, rate * channels * 4, channels * 4, 32)
    body = b'WAVEfmt ' + struct.pack('<I', 16) + fmt + b'data' + struct.pack('<I', len(payload)) + payload
    Path(path).write_bytes(b'RIFF' + struct.pack('<I', len(body)) + body)


def match_group(arrays, target_db=-24., ceiling_db=-3.):
    rms = np.array([np.sqrt(np.mean(x.astype(np.float64) ** 2)) for x in arrays])
    if not np.isfinite(rms).all() or np.any(rms < 1e-9):
        raise ValueError('Silent/nonfinite output cannot be level matched')
    gains = 10 ** (target_db / 20) / rms
    peak = max(float(np.max(np.abs(x))) * g for x, g in zip(arrays, gains))
    safety = min(1., 10 ** (ceiling_db / 20) / peak)
    gains *= safety  # same extra attenuation for the whole group, no limiter
    return [x * g for x, g in zip(arrays, gains)], gains, target_db + 20 * math.log10(safety)



def descriptive_metrics(audio, rate):
    # Descriptive only: crest/band balance do not score note definition.
    x = audio.astype(np.float64)
    rms = float(np.sqrt(np.mean(x * x)))
    spectrum = np.abs(np.fft.rfft(x, axis=0)) ** 2
    frequencies = np.fft.rfftfreq(len(x), 1 / rate)
    total = float(np.sum(spectrum))
    return {'crest_db': 20 * math.log10(float(np.max(np.abs(x))) / rms),
            'band_power_fractions': {f'{lo}-{hi}Hz': float(np.sum(spectrum[(frequencies >= lo) & (frequencies < hi)])) / total
                                    for lo, hi in ((20, 120), (120, 400), (400, 2000), (2000, 8000))},
            'acceptance': 'DESCRIPTIVE_ONLY'}

def validate(config, base):
    rate = int(config['sample_rate'])
    if rate < 8000 or rate > 192000 or config['oversampling'] not in (1, 2, 4, 8):
        raise ValueError('Unsupported rate/oversampling')
    if not 1 <= config['block_size'] <= 8192 or not -24 <= config['input_gain_db'] <= 24:
        raise ValueError('Invalid input gain/block size')
    for key in ('target_rms_dbfs', 'peak_ceiling_dbfs', 'input_gain_db'):
        if not math.isfinite(config[key]):
            raise ValueError('Nonfinite level setting')
    if not -60 <= config['target_rms_dbfs'] <= -6 or not -24 <= config['peak_ceiling_dbfs'] <= -1:
        raise ValueError('Invalid audition RMS target/peak ceiling')
    segments = config['segments']
    if not segments or len({s['id'] for s in segments}) != len(segments):
        raise ValueError('Define uniquely named audition segments first')
    for s in segments:
        if not s['id'].replace('-', '').replace('_', '').isalnum() or not 0 <= s['start_seconds'] < s['end_seconds']:
            raise ValueError('Invalid segment bounds/id')
    paths = {k: (base / config[k]).resolve() if config.get(k) else None for k in ('di', 'ir')}
    return paths


def execute(config_path, output, head, rig, baseline_head=None, baseline_rig=None, plan=False):
    config_path = Path(config_path).resolve()
    config = json.loads(config_path.read_text(encoding='utf-8'))
    paths = validate(config, config_path.parent)
    pending = [k for k, p in paths.items() if p is None or not p.is_file()]
    if output.exists():
        raise ValueError('Output already exists; select a fresh directory')
    output.parent.mkdir(parents=True, exist_ok=True)
    if plan:
        output.mkdir()
        save_json(output / 'plan.json', {'status': 'READY_FOR_INPUT' if not pending else 'BLOCKED_MISSING_' + '_'.join(pending).upper(),
                  'config': config, 'resolved_paths': {k: str(v) if v else None for k, v in paths.items()},
                  'musical_acceptance': 'PENDING', 'release_approved': False})
        return
    if pending:
        raise ValueError('Missing ' + ', '.join(pending) + '; use --plan to prepare without audio')
    if not head or not rig or bool(baseline_head) != bool(baseline_rig):
        raise ValueError('Both current renderers required; baseline requires both renderers too')
    rate, di = load_audio(paths['di'])
    ir_rate, ir = load_audio(paths['ir'])
    if rate != config['sample_rate'] or ir_rate != rate:
        raise ValueError('DI and common IR must already have the configured sample rate; no hidden resampling')
    if not np.isfinite(di).all() or not np.isfinite(ir).all() or np.max(np.abs(ir)) < 1e-9:
        raise ValueError('Nonfinite audio or silent IR')
    if di.shape[1] != 1:
        raise ValueError('Use mono DI so both production lanes receive identical bass')
    di = di * 10 ** (config['input_gain_db'] / 20)
    if np.max(np.abs(di)) >= 1:
        raise ValueError('Input gain overloads DI; reduce shared input gain')
    original_frames = len(di)
    warmup_frames = round(rate * .25)
    di = np.pad(di, ((warmup_frames, 0), (0, 0)))
    original_hashes = {k: digest(p) for k, p in paths.items()}
    binaries = {'current': [Path(head).resolve(), Path(rig).resolve()]}
    if baseline_head:
        binaries['baseline'] = [Path(baseline_head).resolve(), Path(baseline_rig).resolve()]
    with measurement_stage(output) as stage:
        write_float(stage / 'input.wav', rate, di)
        shutil.copyfile(paths['ir'], stage / 'common-ir.wav')
        save_json(stage / 'controls.json', {'controls': config['controls']})
        groups = {}
        provenance = {}
        for revision, (head_binary, rig_binary) in binaries.items():
            provenance[revision] = {'binaries': {str(p): digest(p) for p in (head_binary, rig_binary)}}
            for mode in ('head-only', 'head-cab', 'preset-cab'):
                dest = stage / (revision + '-' + mode)
                if mode == 'head-only':
                    command = [str(head_binary), str(stage / 'input.wav'), str(dest), '--controls', str(stage / 'controls.json'),
                               '--oversampling', str(config['oversampling']), '--block-size', str(config['block_size']), '--tail-seconds', '2']
                else:
                    command = [str(rig_binary), str(stage / 'input.wav'), str(dest), str(stage / 'common-ir.wav'), mode,
                               str(config['oversampling']), str(config['block_size']), str(stage / 'controls.json')]
                result = subprocess.run(command, capture_output=True, text=True, timeout=1800)
                (stage / (revision + '-' + mode + '.log')).write_text(result.stdout + result.stderr, encoding='utf-8')
                if result.returncode:
                    raise ValueError('Renderer failed: ' + revision + '-' + mode)
                manifest = json.loads((dest / 'manifest.json').read_text(encoding='utf-8'))
                if manifest['input_sha256'] != digest(stage / 'input.wav') or len(manifest['outputs']) != 5:
                    raise ValueError('Renderer input/route identity mismatch')
                if mode != 'head-only' and manifest['ir_sha256'] != original_hashes['ir']:
                    raise ValueError('Renderer cabinet identity mismatch')
                provenance[revision][mode] = manifest
                for row in manifest['outputs']:
                    if digest(dest / row['file']) != row['sha256']:
                        raise ValueError('Renderer WAV hash mismatch')
                    rendered_rate, audio = load_audio(dest / row['file'])
                    latency = int(row['latency_samples'])
                    if rendered_rate != rate or latency < 0 or len(audio) < len(di) + latency:
                        raise ValueError('Invalid output rate/duration/latency')
                    if not np.isfinite(audio).all():
                        raise ValueError('Nonfinite render')
                    groups.setdefault(mode, []).append((revision, row['channel_index'], audio[latency + warmup_frames:latency + len(di)]))
        report = {'schema': 'spectralforge.niflheimr.audition.v1', 'status': 'RENDERED_LISTENING_PENDING',
                  'musical_acceptance': 'PENDING', 'release_approved': False, 'config': config,
                  'input_hashes': original_hashes, 'prepared_di_sha256': digest(stage / 'input.wav'), 'leading_silence_frames': warmup_frames,
                  'provenance': provenance, 'level_matching': 'segment RMS, shared target and common peak attenuation; no limiter; not LUFS',
                  'results': []}
        listening = []
        rng = random.Random(config['blind_seed'])
        for segment in config['segments']:
            start, end = [round(segment[k] * rate) for k in ('start_seconds', 'end_seconds')]
            if end > original_frames:
                raise ValueError('Segment extends beyond DI: ' + segment['id'])
            for mode, items in groups.items():
                # Baseline/current share one matching group, so louder revisions cannot win by level.
                clips = [x[start:end] for _, _, x in items]
                matched, gains, actual_db = match_group(clips, config['target_rms_dbfs'], config['peak_ceiling_dbfs'])
                order = list(range(len(items))); rng.shuffle(order)
                folder = stage / 'listen' / segment['id'] / mode; folder.mkdir(parents=True)
                for blind, index in enumerate(order):
                    revision, channel, _ = items[index]
                    clip = matched[index]; filename = f'sample-{blind + 1:02d}.wav'
                    write_float(folder / filename, rate, clip)
                    report['results'].append({'segment': segment['id'], 'mode': mode, 'file': str((folder / filename).relative_to(stage)),
                        'revision': revision, 'channel': CHANNELS[channel], 'preset': PRESETS[channel] if mode == 'preset-cab' else None,
                        'gain_db': 20 * math.log10(float(gains[index])), 'rms_dbfs': actual_db,
                        'peak_dbfs': 20 * math.log10(float(np.max(np.abs(clip)))), 'descriptive_metrics': descriptive_metrics(clip, rate), 'sha256': digest(folder / filename)})
                    listening.append([segment['id'], mode, filename, 'PENDING', '', '', '', '', '', ''])
        with (stage / 'listening.csv').open('w', newline='', encoding='utf-8-sig') as f:
            writer = csv.writer(f); writer.writerow(['segment','mode','sample','verdict','repeated_note_clarity_1_5','attack_1_5',
                                                    'low_end_separation_1_5','genre_character_1_5','sustain_1_5','notes']); writer.writerows(listening)
        if any(digest(paths[k]) != v for k, v in original_hashes.items()):
            raise ValueError('Original DI/IR changed during render')
        save_json(stage / 'report.json', report)  # separate key; keep hidden during blind listening
        save_json(stage / 'config.json', config)
        stage.rename(output)
        # Verify the delivered inventory after directory publication.
        try:
            for row in report['results']:
                if digest(output / row['file']) != row['sha256']:
                    raise ValueError('Published audition WAV changed')
        except Exception:
            output.rename(stage)
            raise


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--config', required=True, type=Path); p.add_argument('--output', required=True, type=Path)
    p.add_argument('--head-renderer'); p.add_argument('--rig-renderer')
    p.add_argument('--baseline-head-renderer'); p.add_argument('--baseline-rig-renderer'); p.add_argument('--plan', action='store_true')
    a = p.parse_args()
    try:
        execute(a.config, a.output.resolve(), a.head_renderer, a.rig_renderer, a.baseline_head_renderer, a.baseline_rig_renderer, a.plan)
    except Exception as e:
        p.exit(1, str(e) + '\n')

if __name__ == '__main__':
    main()
