#!/usr/bin/env python3
"""Choose Náströnd head + cab models on the new guitar-range validation signal.

Follows reports/nastrond-cab-guitar-range-20261104.md: per channel, the lowest
full-signal ESR among candidates passing release v3 on validation; if none
pass, the lowest full-signal ESR. Reads only the validation split.
"""
import argparse, json
from pathlib import Path
import numpy as np, torch
from nam.models._from_nam import init_from_nam
from capture_channels import AMPS
from common import digest, write_json
from quality_profile import fidelity, release_v3_check
from train_a2 import predict

CANDIDATES = ('ndc-full', 'ndc-refine', 'ndc-refine2', 'ndc-refine3', 'ndc-refine4', 'ndc-refine5', 'ndc-refine6', 'ndc-refine6-final')


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--work', type=Path, required=True)
    ap.add_argument('--heldout', type=Path, required=True)
    ap.add_argument('--out', type=Path, required=True)
    a = ap.parse_args()
    torch.set_num_threads(4)
    x = np.load(a.heldout / 'validation/input.npy')
    report = {'rule': 'reports/nastrond-cab-guitar-range-20261104.md', 'split': 'validation',
              'heldout_manifest_sha256': digest(a.heldout / 'manifest.json'), 'channels': {}}
    for name, _ in AMPS['nastrond']['voices']:
        ref = np.load(a.heldout / f'validation/{name}.npy')
        rows = {}
        for run in CANDIDATES:
            path = a.work / run / f'Nastrond-{name}.nam'
            p = predict(init_from_nam(json.loads(path.read_text())).eval(), x)
            f = fidelity(p, ref, name + '-Cab', x)
            rows[run] = {'model': str(path), 'model_sha256': digest(path), 'fidelity': f, 'v3': release_v3_check(f, name + '-Cab')}
            print(name, run, round(f['full_esr'], 4), rows[run]['v3']['pass'], flush=True)
        passing = [r for r in rows if rows[r]['v3']['pass']]
        pool = passing or list(rows)
        chosen = min(pool, key=lambda r: rows[r]['fidelity']['full_esr'])
        report['channels'][name] = {'chosen': chosen, 'chosen_passes_validation': bool(passing), 'candidates': rows}
        print(name, 'chosen', chosen, 'passes' if passing else 'none pass', flush=True)
    write_json(a.out, report)


if __name__ == '__main__':
    main()
