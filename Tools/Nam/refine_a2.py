#!/usr/bin/env python3
"""Bounded matched refinement from one completed capture/checkpoint artifact.

Only validation chooses the recipe. This command never opens test arrays for
inference, changes the native DSP, fits output levels, or approves a release.
"""
import argparse
import json
from pathlib import Path
import subprocess
import sys

from common import digest, write_json
from recipe_selection import select_recipe
from training_contract import verify_audio_manifest


def refinement_options(steps, threads):
    return ['--steps', str(steps), '--threads', str(threads), '--batch', '2',
            '--frames', '4096', '--lr', '.001', '--tail-fraction', '.1',
            '--validation-selection', 'full', '--center-training-output']


def main():
    p = argparse.ArgumentParser(description=__doc__)
    for name in ('data', 'source', 'trainer', 'out'):
        p.add_argument('--' + name, type=Path, required=True)
    p.add_argument('--steps', type=int, default=1000)
    p.add_argument('--threads', type=int, default=2)
    a = p.parse_args()
    if not 1 <= a.steps <= 5000 or not 1 <= a.threads <= 16:
        raise ValueError('Refinement needs 1..5000 steps and 1..16 threads')
    if a.out.exists() and any(a.out.iterdir()):
        raise RuntimeError('Use a fresh experiment directory')
    verify_audio_manifest(a.data)
    a.out.mkdir(parents=True, exist_ok=True)
    config = json.loads((a.source / 'training-config.json').read_text())
    if config['dataset_sha256'] != digest(a.data / 'manifest.json'):
        raise RuntimeError('Warm-start must use the same dataset for this experiment')
    plan = {'steps_per_arm': a.steps, 'source_checkpoint_sha256': digest(a.source / 'checkpoint.pt'),
            'dataset_sha256': digest(a.data / 'manifest.json'), 'selection_split': 'validation',
            'independent_test_used_for_selection': False, 'release_approved': False,
            'common_options': refinement_options(a.steps, a.threads),
            'adaptation': 'Lower initial LR, doubled crop, 10% terminal-decay sampling, export-consistent centering',
            'recipe_schedules': 'Legacy step half-life and official virtual-epoch schedule retained explicitly'}
    write_json(a.out / 'plan.json', plan)
    arms = {}
    for recipe in ('legacy', 'official-a2'):
        out = a.out / recipe
        cmd = [sys.executable, '-u', str(Path(__file__).with_name('train_a2.py')),
               '--data', str(a.data.resolve()), '--trainer', str(a.trainer.resolve()),
               '--out', str(out.resolve()), '--warm-start', str(a.source.resolve()),
               '--recipe', recipe, *refinement_options(a.steps, a.threads)]
        with (a.out / (recipe + '.log')).open('x') as log:
            log.write(json.dumps(cmd) + '\n')
            log.flush()
            subprocess.run(cmd, stdout=log, stderr=subprocess.STDOUT, check=True)
        arms[recipe] = {'config': json.loads((out / 'training-config.json').read_text()),
                        'validation': json.loads((out / 'validation.json').read_text())}
    if digest(a.source / 'checkpoint.pt') != plan['source_checkpoint_sha256']:
        raise RuntimeError('Source checkpoint changed during experiment')
    verify_audio_manifest(a.data)
    result = select_recipe(arms)
    result['models_sha256'] = {r: {n: v['sha256'] for n, v in arm['validation']['channels'].items()}
                              for r, arm in arms.items()}
    result['plan_sha256'] = digest(a.out / 'plan.json')
    write_json(a.out / 'selection.json', result)
    print(json.dumps(result))


if __name__ == '__main__':
    main()
