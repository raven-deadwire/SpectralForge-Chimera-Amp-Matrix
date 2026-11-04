#!/usr/bin/env python3
"""New guitar-range held-out signals for the Náströnd head + cab qualification.

See reports/nastrond-cab-guitar-range-20261104.md. Renders validation and test
signals with plucked fundamentals of 46 Hz and above through the frozen
renderer, then applies each channel's preset lane cabinet exactly as
add_cabinet.py does for the training capture.
"""
import argparse, concurrent.futures, hashlib, json, subprocess
from pathlib import Path
import numpy as np
from add_cabinet import CABINETS, convolve_file, read_ir
from capture_channels import AMPS
from common import GUITAR_NOTES, digest, signal, write_json

SPLITS = (('validation', 26110401), ('test', 26110402))
SECONDS = 120


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--renderer', type=Path, required=True)
    ap.add_argument('--irs', type=Path, required=True)
    ap.add_argument('--reference', type=Path, required=True, help='head + cab training capture these signals qualify')
    ap.add_argument('--out', type=Path, required=True)
    a = ap.parse_args()
    ref = json.loads((a.reference / 'manifest.json').read_text())
    if digest(a.renderer) != ref['source']['renderer_sha256']:
        raise RuntimeError('Renderer differs from the training capture')
    if a.out.exists() and any(a.out.iterdir()):
        raise RuntimeError('Output directory must be empty')
    voices = AMPS['nastrond']['voices']
    irs = {n: read_ir(a.irs / f'{n}.wav')[0] for n, _ in voices}
    for n, _ in voices:
        sha = hashlib.sha256((a.irs / f'{n}.wav').read_bytes()).hexdigest()
        if sha != ref['source']['cabinet']['channels'][n]['ir_sha256']:
            raise RuntimeError(f'Cabinet IR differs from the training capture: {n}')
    jobs, rows = [], []
    for split, seed in SPLITS:
        d = a.out / split
        (d / 'head').mkdir(parents=True)
        x = signal(seed, SECONDS, GUITAR_NOTES)
        np.save(d / 'input.npy', x)
        x.tofile(d / 'input.f32')
        for ch, (name, _) in enumerate(voices):
            write_json(d / 'head' / f'{name}.json', {'channel': ch})
            jobs.append((split, d, ch, name, len(x)))

    def render(job):
        split, d, ch, name, n = job
        raw = d / 'head' / f'{name}.f32'
        subprocess.check_output([str(a.renderer), str(d / 'input.f32'), str(raw), str(d / 'head' / f'{name}.json')], text=True)
        y = np.fromfile(raw, dtype='<f4')
        if len(y) != n or not np.isfinite(y).all():
            raise RuntimeError('Invalid native capture')
        np.save(d / 'head' / f'{name}.npy', y)
        raw.unlink()
        convolve_file(d / 'head' / f'{name}.npy', d / f'{name}.npy', irs[name])
        return {'split': split, 'name': name, 'channel': ch, 'output': f'{split}/{name}.npy',
                'output_sha256': digest(d / f'{name}.npy'), 'head_output_sha256': digest(d / 'head' / f'{name}.npy')}

    with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:
        for row in pool.map(render, jobs):
            rows.append(row)
            print(row['split'], row['name'], flush=True)
    for split, _ in SPLITS:
        (a.out / split / 'input.f32').unlink()
    write_json(a.out / 'manifest.json', {
        'schema': 1, 'rule': 'reports/nastrond-cab-guitar-range-20261104.md',
        'notes_hz': list(GUITAR_NOTES), 'seconds': SECONDS,
        'splits': {s: {'seed': seed, 'input_sha256': digest(a.out / s / 'input.npy')} for s, seed in SPLITS},
        'reference_capture_manifest_sha256': digest(a.reference / 'manifest.json'),
        'source': ref['source'], 'captures': rows})


if __name__ == '__main__':
    main()
