"""Fail closed on split leakage, changed audio, and changed resume settings."""
import hashlib
import json
from pathlib import Path


def verify_audio_manifest(data):
    data = Path(data).resolve()
    manifest = json.loads((data / 'manifest.json').read_text())
    split_inputs, seen = {}, set()
    for row in manifest['captures']:
        key = (row['split'], row['name'])
        if key in seen:
            raise RuntimeError('Duplicate capture')
        seen.add(key)
        for kind in ('input', 'output'):
            path = (data / row[kind]).resolve()
            if not path.is_relative_to(data):
                raise RuntimeError('Audio path escapes dataset')
            actual = hashlib.sha256(path.read_bytes()).hexdigest()
            if actual != row[kind + '_sha256']:
                raise RuntimeError('Audio hash mismatch: ' + row[kind])
        previous = split_inputs.setdefault(row['split'], row['input_sha256'])
        if previous != row['input_sha256']:
            raise RuntimeError('Channels do not share split input')
    names = {'Fenrir', 'Surtr', 'Nidhoggr', 'Fimbulvetr', 'Ragnarok'}
    splits = {'train', 'validation', 'test', 'audition'}
    if seen != {(s, n) for s in splits for n in names}:
        raise RuntimeError('Missing or unexpected capture')
    if len(set(split_inputs.values())) != len(splits):
        raise RuntimeError('Input reused across training/validation/test splits')
    return manifest


def training_identity(config):
    # Runtime versions and all optimization/sampling choices are part of resume
    # identity. Paths and warm-start provenance are handled separately on resume.
    identity = {k: v for k, v in config.items() if k not in ('identity', 'warm_start')}
    return hashlib.sha256(json.dumps(identity, sort_keys=True, allow_nan=False).encode()).hexdigest()
