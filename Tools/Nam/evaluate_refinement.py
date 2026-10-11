#!/usr/bin/env python3
"""Evaluate a frozen validation selection on a new independent synthetic seed.

The reference remains the frozen native DSP with its original channel controls.
No gain, alignment or EQ is fitted to the new test. This is not instrument DI.
"""
import argparse
import json
from pathlib import Path
import subprocess
import numpy as np
import torch
from nam.models._from_nam import init_from_nam

from common import SOURCE, digest, signal, write_json
from quality_profile import PROFILE, fidelity
from recipe_selection import NAMES, select_recipe
from train_a2 import predict
from training_contract import verify_audio_manifest


def main():
    p = argparse.ArgumentParser(description=__doc__)
    for name in ('experiment', 'data', 'baseline', 'renderer', 'tool', 'out'):
        p.add_argument('--' + name, type=Path, required=True)
    p.add_argument('--seed', type=int, default=26101122)
    a = p.parse_args()
    torch.set_num_threads(2)
    if a.out.exists() and any(a.out.iterdir()):
        raise RuntimeError('Use a fresh test output directory')
    manifest = verify_audio_manifest(a.data)
    if manifest['source']['source_commit'] != SOURCE:
        raise RuntimeError('Unexpected native source')
    repo = Path(__file__).resolve().parents[2]
    for path, expected in manifest['source']['source_hashes'].items():
        if digest(repo / path) != expected:
            raise RuntimeError('Frozen native DSP changed')
    selection_path = a.experiment / 'selection.json'
    selection = json.loads(selection_path.read_text())
    arms = {r: {'config': json.loads((a.experiment / r / 'training-config.json').read_text()),
                'validation': json.loads((a.experiment / r / 'validation.json').read_text())}
            for r in ('legacy', 'official-a2')}
    if selection['selected_recipe'] != select_recipe(arms)['selected_recipe']:
        raise RuntimeError('Stored selection no longer matches validation')
    models = a.experiment / selection['selected_recipe']
    for name in NAMES:
        if digest(models / ('Nastrond-' + name + '.nam')) != selection['models_sha256'][selection['selected_recipe']][name]:
            raise RuntimeError('Selected model changed')
    a.out.mkdir(parents=True, exist_ok=True)
    # Seal the choice before generating or rendering the new input.
    frozen = {'selection_sha256': digest(selection_path), 'seed': a.seed, 'seconds': 24,
              'models_sha256': selection['models_sha256'][selection['selected_recipe']],
              'baseline_sha256': {n: digest(a.baseline / ('Nastrond-' + n + '.nam')) for n in NAMES},
              'renderer_sha256': digest(a.renderer), 'player_tool_sha256': digest(a.tool),
              'source_commit': SOURCE, 'release_approved': False}
    write_json(a.out / 'test-plan.json', frozen)
    x = signal(a.seed, 24)
    np.save(a.out / 'input.npy', x)
    if digest(a.out / 'input.npy') in {r['input_sha256'] for r in manifest['captures']}:
        raise RuntimeError('New test duplicates an existing split')
    raw = (a.out / 'input.f32').resolve()
    x.tofile(raw)
    rendered = (a.out / 'rendered.f32').resolve()
    job = (a.out / 'player-job.json').resolve()
    write_json(job, {'normalize': False, 'block_size': 64})
    report = {'test_plan_sha256': digest(a.out / 'test-plan.json'), 'quality_profile': PROFILE,
              'input_sha256': digest(a.out / 'input.npy'), 'selected_recipe': selection['selected_recipe'],
              'input_kind': 'new independent synthetic; not instrument DI',
              'selection_frozen_before_test': True, 'release_approved': False,
              'instrument_DI_listening': 'BLOCKED', 'GUI_DAW_acceptance': 'BLOCKED', 'channels': {}}
    for name in NAMES:
        native_job = (a.data / 'test' / (name + '.json')).resolve()
        subprocess.check_output([str(a.renderer.resolve()), str(raw), str(rendered), str(native_job)])
        ref = np.fromfile(rendered, dtype='<f4')
        np.save(a.out / (name + '-native.npy'), ref)
        versions = {}
        for version, folder in [('baseline', a.baseline), ('candidate', models)]:
            model_file = folder / ('Nastrond-' + name + '.nam')
            runtime = json.loads(subprocess.check_output([str(a.tool.resolve()), 'render',
                str(model_file.resolve()), str(raw), str(rendered), str(job)], text=True))
            if not runtime['a2_gate'] or runtime['a2_channels'] != 8:
                raise RuntimeError('Not target-compatible A2-Full')
            y = np.fromfile(rendered, dtype='<f4')
            np.save(a.out / (name + '-' + version + '.npy'), y)
            versions[version] = {'fidelity': fidelity(y, ref, name), 'runtime': runtime,
                                 'model_sha256': digest(model_file)}
            if version == 'candidate':
                model = init_from_nam(json.loads(model_file.read_text())).eval()
                py = predict(model, x)
                residual = (y.astype(np.float64) - py) * PROFILE['source_output_scale'][name]
                parity = float(10 * np.log10(max(np.mean(residual ** 2), 1e-20)))
                versions[version]['engine_parity_residual_dbfs'] = parity
                if parity > PROFILE['engine_parity_residual_max_dbfs']:
                    raise RuntimeError('Python/target engine disagreement')
        report['channels'][name] = versions
        print(json.dumps({'channel': name, 'baseline_esr': versions['baseline']['fidelity']['full_esr'],
                          'candidate_esr': versions['candidate']['fidelity']['full_esr']}), flush=True)
    if digest(selection_path) != frozen['selection_sha256']:
        raise RuntimeError('Selection changed during independent test')
    for n in NAMES:
        if digest(models / ('Nastrond-' + n + '.nam')) != frozen['models_sha256'][n]:
            raise RuntimeError('Model changed during independent test')
    report['accuracy'] = 'PASS' if all(v['candidate']['fidelity']['fidelity_pass'] for v in report['channels'].values()) else 'FAIL'
    report['compatibility'] = 'PASS'
    write_json(a.out / 'comparison.json', report)
    # Preserve a FAIL report without conflating measured fidelity with tool failure.
    print(json.dumps({'accuracy': report['accuracy'], 'release_approved': False}))


if __name__ == '__main__':
    main()
