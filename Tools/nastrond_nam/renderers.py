"""Renderer invocation with a content-addressed cache.

A cached output is reused only when the stamp records the same key (renderer
binary, input and state/model hashes) and the output still has the recorded
hash. File names are unchanged from v1 so existing caches stay valid.
"""
import hashlib
import json
import subprocess
from pathlib import Path

import numpy as np
from scipy.io import wavfile

from validate_nam import SR


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def run(command):
    result = subprocess.run([str(x) for x in command], capture_output=True, text=True)
    if result.returncode:
        raise RuntimeError(f'Render failed ({result.returncode}): {command[0]}\n{result.stderr}')
    return result.stdout.strip()


def parse_info(text):
    """`rate=48000 oversampling=4 latency=6 ...` -> numbers; free text is kept in the report as-is."""
    info = {}
    for token in text.split():
        key, sep, value = token.partition('=')
        if sep:
            try:
                info[key] = float(value) if '.' in value else int(value)
            except ValueError:
                info[key] = value
    return info


def read_wav(path):
    rate, data = wavfile.read(path)
    if rate != SR or data.ndim != 1 or data.dtype.kind != 'f':
        raise ValueError(f'Expected mono float {SR} Hz: {path}')
    return data.astype(np.float32)


def _stamp(path):
    try:
        text = Path(path).read_text()
        return json.loads(text) if text.startswith('{') else {}
    except FileNotFoundError:
        return {}


def original(renderer, renderer_sha, probe_path, probe_sha, out, name, state):
    config = out / (name + '.json')
    config.write_text(json.dumps(state))
    dest, stamp = out / (name + '.f32'), out / (name + '.stamp.json')
    key = renderer_sha + probe_sha + digest(config)
    cached = _stamp(stamp)
    if not dest.exists() or cached.get('key') != key or cached.get('output_sha256') != digest(dest):
        info = run([renderer, probe_path, dest, config])
        stamp.write_text(json.dumps(dict(key=key, renderer=info, output_sha256=digest(dest))))
    else:
        info = cached['renderer']
    return info, np.fromfile(dest, dtype=np.float32), digest(dest)


def nam(renderer, renderer_sha, model, model_sha, probe, base):
    inp, dest, stamp = base.with_suffix('.input.wav'), base.with_suffix('.wav'), base.with_suffix('.stamp')
    wavfile.write(inp, SR, probe)
    key = model_sha + digest(inp) + renderer_sha + ':slim=1'
    cached = _stamp(stamp)
    if not dest.exists() or cached.get('key') != key or cached.get('output_sha256') != digest(dest):
        run([renderer, '--slim', '1', model, inp, dest])
        stamp.write_text(json.dumps(dict(key=key, output_sha256=digest(dest))))
    return read_wav(dest), digest(dest)
