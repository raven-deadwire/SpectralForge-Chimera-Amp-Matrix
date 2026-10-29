#!/usr/bin/env python3
"""Append captures' training splits to a combined dataset in place.

combine_captures.py writes a new dataset, so the old and new training arrays
must fit on disk together. This instead grows the existing .npy files: it
rewrites each header with the new length (same header size) and appends the
part's samples, so a part costs only its own training arrays. Each part must
share the dataset's native source, renderer and held-out audio, with a new
training seed. With --consume, each part's directory is deleted once appended.
"""
import argparse, json, shutil
from pathlib import Path
import numpy as np
from common import digest, write_json
from training_contract import verify_audio_manifest

HELD_OUT = ('validation', 'test', 'audition')


def header_bytes(n, size):
    """A version 1.0 .npy header for a float32 vector of n samples, padded to size bytes."""
    text = "{'descr': '<f4', 'fortran_order': False, 'shape': (%d,), }" % n
    body = 10+len(text)+1
    if body > size: raise RuntimeError('New .npy header does not fit in place')
    text += ' '*(size-body)+'\n'
    return b'\x93NUMPY\x01\x00'+len(text).to_bytes(2, 'little')+text.encode('latin1')


def append(target, part):
    with open(target, 'r+b') as fh:
        if np.lib.format.read_magic(fh) != (1, 0): raise RuntimeError(f'Unexpected .npy version: {target}')
        shape, fortran, dtype = np.lib.format.read_array_header_1_0(fh)
        if fortran or dtype != np.dtype('<f4') or len(shape) != 1: raise RuntimeError(f'Unexpected array: {target}')
        data_offset = fh.tell()
        add = np.load(part, mmap_mode='r')
        if add.dtype != np.dtype('<f4') or add.ndim != 1: raise RuntimeError(f'Unexpected array: {part}')
        fh.seek(0, 2)
        if fh.tell() != data_offset+4*shape[0]: raise RuntimeError(f'Truncated or padded data: {target}')
        for i in range(0, len(add), 1 << 24):
            fh.write(np.ascontiguousarray(add[i:i+(1 << 24)]).tobytes())
        fh.seek(0)
        fh.write(header_bytes(shape[0]+len(add), data_offset))
    return len(add)


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--dataset', type=Path, required=True)
    ap.add_argument('--consume', action='store_true', help='Delete each part directory once appended')
    ap.add_argument('parts', type=Path, nargs='+')
    args = ap.parse_args()
    manifest = json.loads((args.dataset/'manifest.json').read_text())
    native = lambda src: {k: v for k, v in src.items() if k != 'train_seed'}
    held = {(r['split'], r['name']): (r['input_sha256'], r['output_sha256'])
            for r in manifest['captures'] if r['split'] in HELD_OUT}
    seeds = list(manifest['source'].get('train_seed', []))
    stems = ['input']+sorted({r['name'] for r in manifest['captures']})
    for part in args.parts:
        m = verify_audio_manifest(part)
        if native(m['source']) != native(manifest['source']): raise RuntimeError(f'Different native source or renderer: {part}')
        if held != {(r['split'], r['name']): (r['input_sha256'], r['output_sha256'])
                    for r in m['captures'] if r['split'] in HELD_OUT}:
            raise RuntimeError(f'Held-out audio differs: {part}')
        seed = m['source'].get('train_seed')
        if seed is None or seed in seeds: raise RuntimeError(f'Missing or repeated training seed: {part}')
        lengths = {append(args.dataset/'train'/f'{stem}.npy', part/'train'/f'{stem}.npy') for stem in stems}
        if len(lengths) != 1: raise RuntimeError('Channel lengths differ within a part')
        seeds.append(seed)
        manifest.setdefault('combined_from', []).append({'path': str(part), 'manifest_sha256': digest(part/'manifest.json'),
                                                        'train_seed': seed, 'train_samples': int(lengths.pop())})
        manifest['source']['train_seed'] = seeds
        # Hashing multi-GB arrays after every part would reread them each time. Until
        # the final hash the manifest says the append is unfinished, and the trainer's
        # hash check refuses the dataset.
        manifest['append_in_progress'] = True
        write_json(args.dataset/'manifest.json', manifest)
        if args.consume: shutil.rmtree(part)
        print('appended', part.name, 'total', sum(r['train_samples'] for r in manifest['combined_from'])/48000, 's', flush=True)
    for row in manifest['captures']:
        if row['split'] == 'train':
            for kind in ('input', 'output'): row[kind+'_sha256'] = digest(args.dataset/row[kind])
            row['render'] = {'combined': True, 'parts': len(manifest['combined_from'])}
    manifest.pop('append_in_progress', None)
    write_json(args.dataset/'manifest.json', manifest)
    verify_audio_manifest(args.dataset)


if __name__ == '__main__':
    main()
