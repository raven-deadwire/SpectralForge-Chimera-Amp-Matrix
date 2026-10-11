#!/usr/bin/env python3
"""Evaluate a frozen validation selection on a new independent synthetic seed.

The reference remains the frozen native DSP with its original channel controls.
No gain, alignment or EQ is fitted to the new test. This is not instrument DI.
"""
import argparse
import json
from pathlib import Path
import subprocess
import uuid
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
    # A local binary hash alone does not establish which DSP was compiled.
    # Require an exact replay of the already-known validation reference first.
    probe_in = (a.out / 'identity-input.f32').resolve()
    probe_out = (a.out / 'identity-output.f32').resolve()
    np.load(a.data / 'validation/input.npy').astype('<f4').tofile(probe_in)
    for name in NAMES:
        subprocess.check_output([str(a.renderer.resolve()), str(probe_in), str(probe_out),
            str((a.data / 'validation' / (name + '.json')).resolve())])
        if not np.array_equal(np.fromfile(probe_out, dtype='<f4'),
                              np.load(a.data / 'validation' / (name + '.npy'))):
            raise RuntimeError('Renderer does not reproduce frozen validation: ' + name)
    probe_in.unlink()
    probe_out.unlink()
    x = signal(a.seed, 24)
    np.save(a.out / 'input.npy', x)
    if digest(a.out / 'input.npy') in {r['input_sha256'] for r in manifest['captures']}:
        raise RuntimeError('New test duplicates an existing split')
    raw = (a.out / 'input.f32').resolve()
    x.tofile(raw)
    rendered = (a.out / 'rendered.f32').resolve()
    job = (a.out / 'player-job.json').resolve()
    write_json(job, {'normalize': False, 'block_size': 64})
    zero = (a.out / 'silence.f32').resolve()
    np.zeros(48000, dtype='<f4').tofile(zero)
    report = {'test_plan_sha256': digest(a.out / 'test-plan.json'), 'quality_profile': PROFILE,
              'native_validation_replay': 'bit-identical for all five channels',
              'input_sha256': digest(a.out / 'input.npy'), 'selected_recipe': selection['selected_recipe'],
              'input_kind': 'new independent synthetic; not instrument DI',
              'selection_frozen_before_test': True, 'release_approved': False,
              'instrument_DI_listening': 'BLOCKED', 'GUI_DAW_acceptance': 'BLOCKED', 'channels': {}}
    for name in NAMES:
        capture = next(r for r in manifest['captures'] if r['split'] == 'train' and r['name'] == name)
        native_job = (a.out / (name + '-native-settings.json')).resolve()
        write_json(native_job, {'channel': capture['channel'], 'controls': capture['controls']})
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
                if not np.isfinite(parity) or parity > PROFILE['engine_parity_residual_max_dbfs']:
                    raise RuntimeError('Python/target engine disagreement')
                preset = (a.out / ('Nastrond-' + name + '.t3kpreset')).resolve()
                settings = (a.out / (name + '-preset-settings.json')).resolve()
                write_json(settings, {'name': 'Nastrond ' + name + ' Research',
                    'id': str(uuid.uuid5(uuid.NAMESPACE_URL, 'nastrond/refinement/' + name)),
                    'normalize': False, 'block_size': 64})
                subprocess.check_output([str(a.tool.resolve()), 'preset', str(model_file.resolve()),
                                         str(preset), str(settings)])
                subprocess.check_output([str(a.tool.resolve()), 'render-preset', str(preset), str(raw), str(rendered)])
                delta = float(np.max(np.abs(np.fromfile(rendered, dtype='<f4') - y)))
                if not np.isfinite(delta) or delta > 1e-6:
                    raise RuntimeError('Restored preset changed raw output')
                versions[version]['preset_restore_max_abs'] = delta
                versions[version]['preset_sha256'] = digest(preset)
                subprocess.check_output([str(a.tool.resolve()), 'render', str(model_file.resolve()),
                                         str(zero), str(rendered), str(job)])
                quiet = np.fromfile(rendered, dtype='<f4')[PROFILE['skip_samples']:] * PROFILE['source_output_scale'][name]
                silence = float(10*np.log10(max(np.mean(quiet.astype(np.float64)**2), 1e-20)))
                versions[version]['steady_silence_dbfs'] = silence
                versions[version]['steady_silence_pass'] = bool(np.isfinite(silence) and silence <= PROFILE['steady_silence_rms_max_dbfs'])
        report['channels'][name] = versions
        print(json.dumps({'channel': name, 'baseline_esr': versions['baseline']['fidelity']['full_esr'],
                          'candidate_esr': versions['candidate']['fidelity']['full_esr']}), flush=True)
    if digest(selection_path) != frozen['selection_sha256']:
        raise RuntimeError('Selection changed during independent test')
    for n in NAMES:
        if digest(models / ('Nastrond-' + n + '.nam')) != frozen['models_sha256'][n]:
            raise RuntimeError('Model changed during independent test')
        if digest(a.baseline / ('Nastrond-' + n + '.nam')) != frozen['baseline_sha256'][n]:
            raise RuntimeError('Baseline changed during independent test')
    if digest(a.renderer) != frozen['renderer_sha256'] or digest(a.tool) != frozen['player_tool_sha256']:
        raise RuntimeError('Offline tool changed during independent test')
    report['accuracy'] = 'PASS' if all(v['candidate']['fidelity']['fidelity_pass'] and
        v['candidate']['steady_silence_pass'] for v in report['channels'].values()) else 'FAIL'
    report['compatibility'] = 'PASS'
    write_json(a.out / 'comparison.json', report)
    # Preserve a FAIL report without conflating measured fidelity with tool failure.
    print(json.dumps({'accuracy': report['accuracy'], 'release_approved': False}))


if __name__ == '__main__':
    main()
