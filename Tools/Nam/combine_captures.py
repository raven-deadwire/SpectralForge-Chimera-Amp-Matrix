#!/usr/bin/env python3
"""Merge captures that differ only in training seed into one training dataset.

The renderer accepts at most 600 s of input, so more training data comes from
several captures. Every part must come from the same native source and
renderer and share identical validation, test and audition inputs. Training
splits are concatenated in the given order. Each excerpt starts and ends in
silence, and the native channels forget their input within 132 ms
(diagnose_history.py), so the joins create no state the parts did not have.
"""
import argparse, copy, json
from pathlib import Path
import numpy as np
from common import digest, write_json
from training_contract import verify_audio_manifest

HELD_OUT = ('validation', 'test', 'audition')


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--out', type=Path, required=True)
    ap.add_argument('parts', type=Path, nargs='+')
    args = ap.parse_args()
    if len(args.parts) < 2: raise ValueError('Give at least two captures')
    if args.out.exists() and any(args.out.iterdir()): raise RuntimeError('Output directory must be empty')
    manifests = [verify_audio_manifest(p) for p in args.parts]
    first = manifests[0]

    def without_seed(source):
        return {k: v for k, v in source.items() if k != 'train_seed'}
    for p, m in zip(args.parts, manifests):
        if without_seed(m['source']) != without_seed(first['source']):
            raise RuntimeError(f'Different native source or renderer: {p}')
        held = {(r['split'], r['name']): (r['input_sha256'], r['output_sha256']) for r in m['captures'] if r['split'] in HELD_OUT}
        if held != {(r['split'], r['name']): (r['input_sha256'], r['output_sha256'])
                    for r in first['captures'] if r['split'] in HELD_OUT}:
            raise RuntimeError(f'Held-out audio differs: {p}')
    seeds = [m['source'].get('train_seed', 640500) for m in manifests]
    if len(set(seeds)) != len(seeds): raise RuntimeError('Training seeds repeat')
    args.out.mkdir(parents=True, exist_ok=True)
    for split in HELD_OUT:
        (args.out/split).mkdir()
        for f in (args.parts[0]/split).glob('*.npy'):
            (args.out/split/f.name).write_bytes(f.read_bytes())
    (args.out/'train').mkdir()
    names = sorted({r['name'] for r in first['captures']})
    for stem in ['input']+names:
        np.save(args.out/'train'/f'{stem}.npy', np.concatenate([np.load(p/'train'/f'{stem}.npy') for p in args.parts]))
    manifest = copy.deepcopy(first)
    manifest['source']['train_seed'] = seeds
    manifest['combined_from'] = [{'path': str(p), 'manifest_sha256': digest(p/'manifest.json'), 'train_seed': s,
                                  'train_samples': int(len(np.load(p/'train'/'input.npy')))} for p, s in zip(args.parts, seeds)]
    for row in manifest['captures']:
        for kind in ('input', 'output'):
            row[kind+'_sha256'] = digest(args.out/row[kind])
        if row['split'] == 'train':
            row['render'] = {'combined': True, 'parts': len(args.parts)}
    write_json(args.out/'manifest.json', manifest)
    if (args.parts[0]/'channel-settings.json').exists():
        (args.out/'channel-settings.json').write_bytes((args.parts[0]/'channel-settings.json').read_bytes())
    verify_audio_manifest(args.out)
    print('combined', len(args.parts), 'captures;', sum(r['train_samples'] for r in manifest['combined_from'])/48000, 's of training')


if __name__ == '__main__':
    main()
