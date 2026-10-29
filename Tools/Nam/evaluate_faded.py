#!/usr/bin/env python3
"""Judge A2 models on held-out signals whose final note fades out instead of stopping dead.

common.signal overwrites its last 0.5 s with digital zero, so the final note,
possibly still sounding, stops within one sample. No instrument DI does that,
and the native DSP rings for about 11 ms after the step
(reports/fenrir-quiet-20261028.md). This keeps the validation and test signals
otherwise identical, applies a 5 ms half-cosine fade ending where the silence
begins, renders the native channels with the dataset's own renderer, and
scores the given models with profile v2 metrics and release profile v3.
Training data and the profiles are unchanged.
"""
import argparse, json, subprocess, tempfile
from pathlib import Path
import numpy as np
import torch
from nam.models._from_nam import init_from_nam
from common import RATE, signal, digest, write_json
from capture_channels import VOICES
from quality_profile import fidelity, release_v3_check, RELEASE_V3
from train_a2 import predict

SPLITS = {'validation': 640500+1703, 'test': 640500+2*1703}
SECONDS = 24
FADE_SAMPLES = 240


def faded(x):
    """The same signal with its final note faded over FADE_SAMPLES before the closing silence."""
    y = x.astype(np.float64).copy()
    end = len(y)-RATE//2
    y[end-FADE_SAMPLES:end] *= .5*(1+np.cos(np.pi*np.arange(1, FADE_SAMPLES+1)/FADE_SAMPLES))
    return y.astype('<f4')


def render(renderer, x, channel, folder):
    inp, out, job = folder/'in.f32', folder/'out.f32', folder/'job.json'
    x.tofile(inp); write_json(job, {'channel': channel})
    subprocess.check_output([str(renderer), str(inp), str(out), str(job)])
    y = np.fromfile(out, dtype='<f4')
    if len(y) != len(x) or not np.isfinite(y).all(): raise RuntimeError('Invalid native render')
    return y


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--renderer', type=Path, required=True); ap.add_argument('--data', type=Path, required=True)
    ap.add_argument('--models', type=Path, required=True, help='Folder with Nastrond-<channel>.nam')
    ap.add_argument('--out', type=Path, required=True)
    args = ap.parse_args()
    manifest = json.loads((args.data/'manifest.json').read_text())
    if digest(args.renderer) != manifest['source']['renderer_sha256']: raise RuntimeError('Renderer differs from the dataset\'s')
    torch.set_num_threads(4)
    report = {'purpose': __doc__.split('\n')[0], 'fade_samples': FADE_SAMPLES, 'fade': 'half-cosine ending where the final 0.5 s of silence begins',
              'renderer_sha256': digest(args.renderer), 'profile': RELEASE_V3['id'], 'splits': {}, 'channels': {}}
    with tempfile.TemporaryDirectory() as tmp:
        folder = Path(tmp)
        for split, seed in SPLITS.items():
            original = signal(seed, SECONDS)
            # The fade must start from the same signal the dataset was captured with.
            if not np.array_equal(original, np.load(args.data/split/'input.npy')): raise RuntimeError(f'{split} input differs from the dataset')
            x = faded(original)
            changed = np.flatnonzero(x != original)
            report['splits'][split] = {'seed': seed, 'changed_samples': int(len(changed)),
                                       'final_note_rms_dbfs_before_fade': float(10*np.log10(max(np.mean(original[len(x)-RATE//2-480:len(x)-RATE//2].astype(np.float64)**2), 1e-20)))}
            for channel, (name, _) in enumerate(VOICES):
                ref = render(args.renderer, x, channel, folder).astype(np.float64)
                model = init_from_nam(json.loads((args.models/f'Nastrond-{name}.nam').read_text())).eval()
                f = fidelity(predict(model, x), ref, name, x)
                row = report['channels'].setdefault(name, {'model_sha256': digest(args.models/f'Nastrond-{name}.nam')})
                row[split] = {'metrics': {k: v for k, v in f.items() if k not in ('worst_active_windows', 'quiet_window_details')},
                              'release_v3': release_v3_check(f, name)}
                print(split, name, 'full %.5f quiet %.1f/%.1f' % (f['full_esr'], f['quiet_worst_residual_rms_dbfs'], f['quiet_worst_residual_peak_dbfs']),
                      'v3', row[split]['release_v3']['pass'], flush=True)
    for row in report['channels'].values():
        row['release_v3_numbers_pass'] = all(row[s]['release_v3']['pass'] for s in SPLITS)
    args.out.parent.mkdir(parents=True, exist_ok=True)
    write_json(args.out, report)


if __name__ == '__main__':
    main()
