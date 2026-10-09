#!/usr/bin/env python3
"""Summarize completed matched A2 recipe arms without selecting a release recipe."""

import argparse
from contextlib import ExitStack
import fcntl
import hashlib
import json
import math
from pathlib import Path
import re

from quality_profile import PROFILE


NAMES = ('Fenrir', 'Surtr', 'Nidhoggr', 'Fimbulvetr', 'Ragnarok')
COMPLETE = {'PAUSED_AFTER_STAGE', 'STEP_BUDGET_REACHED', 'READY_FOR_INDEPENDENT_TESTS'}
MATCH_CONFIG = ('dataset_sha256', 'config', 'source', 'sample_rate', 'trainer_commit',
                'target_player_commit', 'batch', 'frames', 'tail_fraction', 'seed',
                'threads', 'validation_selection', 'quality_profile')


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def number(value, label, minimum=None):
    require(type(value) in (int, float) and math.isfinite(value), 'Nonfinite/invalid '+label)
    require(minimum is None or value >= minimum, 'Out-of-range '+label)
    return value


def percent(value, reference):
    return None if reference == 0 else 100.0 * (value / reference - 1.0)


def summarize(root, candidate='official-a2'):
    require(re.fullmatch(r'official-a2(?:-[A-Za-z0-9._-]+)?', candidate) is not None,
            'Candidate must be an official-a2 variant folder name')
    arm_order = ('legacy', candidate)
    evidence = {}

    def read(path, decode=True):
        raw = path.read_bytes()
        evidence[str(path.relative_to(root))] = hashlib.sha256(raw).hexdigest()
        return json.loads(raw) if decode else evidence[str(path.relative_to(root))]

    def check_fidelity(value, esr, label):
        require(value['profile'] == PROFILE['id'], 'Quality profile mismatch: '+label)
        require(abs(number(value['full_esr'], label+' fidelity ESR', 0)-esr) <= 1e-8,
                'ESR/fidelity disagreement: '+label)
        for key in ('fidelity_pass', 'active_pass', 'quiet_pass', 'level_pass', 'full_esr_pass'):
            require(type(value[key]) is bool, 'Invalid fidelity flag: '+label+'/'+key)
        return value

    with ExitStack() as stack:
        # A shared lock holds both completed arms stable while reading their evidence.
        for arm in arm_order:
            lock = stack.enter_context((root/arm/'coordinator.lock').open('r'))
            try:
                fcntl.flock(lock, fcntl.LOCK_SH | fcntl.LOCK_NB)
            except BlockingIOError as exc:
                raise RuntimeError('Arm is still running: '+arm) from exc
        source = read(root/'source/training-config.json')
        source_hash = read(root/'source/checkpoint.pt', decode=False)
        snapshot = read(root/'source/snapshot.json')
        require(snapshot['checkpoint_sha256'] == source_hash, 'Frozen source checkpoint hash mismatch')
        arms = {}
        for arm in arm_order:
            folder = root/arm
            expected_recipe = 'legacy' if arm == 'legacy' else 'official-a2'
            progress = read(folder/'progress.json')
            require(progress['status'] in COMPLETE, 'Incomplete arm: '+arm)
            cfg = read(folder/'training-config.json')
            init = read(folder/'initialization.json')
            best = read(folder/'validation.json')
            last = progress['last_result']
            end = progress['last_completed_step']
            require(type(end) is int and end > 0, 'Invalid completed step: '+arm)
            require(progress['last_saved_step'] == progress['stage_end'] == last['step'] == end,
                    'Progress checkpoint step mismatch: '+arm)
            require(progress['stages'] and progress['stages'][-1] == last,
                    'Missing verified final stage: '+arm)
            require(read(folder/'checkpoint.pt', decode=False) == last['checkpoint_sha256'],
                    'Checkpoint evidence hash mismatch: '+arm)
            require(evidence[str((folder/'validation.json').relative_to(root))] == last['validation_sha256'],
                    'Final validation evidence hash mismatch: '+arm)
            require(cfg['identity'] == last['identity'] == best['identity'], 'Run identity mismatch: '+arm)
            require(cfg['recipe'] == best['recipe'] == expected_recipe, 'Recipe mismatch: '+arm)
            require(best['checkpoint_step'] == end, 'Final report checkpoint mismatch: '+arm)
            require(cfg['quality_profile'] == best['quality_profile'] == progress['quality_profile'] == PROFILE,
                    'Changed quality policy: '+arm)
            require(cfg['validation_selection'] == 'full', 'Full validation is required: '+arm)
            for key in ('dataset_sha256', 'config', 'source', 'sample_rate', 'trainer_commit', 'target_player_commit'):
                require(cfg[key] == source[key], 'Source config mismatch: '+arm+'/'+key)
            require(init['source_checkpoint_sha256'] == cfg['warm_start']['checkpoint_sha256'] == source_hash,
                    'Warm-start checkpoint mismatch: '+arm)
            require(init['seed'] == cfg['seed'], 'Initialization seed mismatch: '+arm)
            for key in ('initial_validation_esr', 'raw_prediction_max_abs_delta'):
                require(set(init[key]) == set(NAMES), 'Incomplete initialization channels: '+arm+'/'+key)
            for name in NAMES:
                number(init['initial_validation_esr'][name], arm+'/'+name+' initial ESR', 0)
                require(number(init['raw_prediction_max_abs_delta'][name], arm+'/'+name+' raw delta', 0) <= 1e-5,
                        'Warm start changed raw output: '+arm+'/'+name)
            schedules = []
            for stage in progress['stages']:
                step = stage['step']
                current = read(folder/f'validation-step-{step}.json')
                require(current['identity'] == cfg['identity'] and current['recipe'] == expected_recipe,
                        'Current validation identity mismatch: '+arm)
                require(current['checkpoint_step'] == step, 'Current validation step mismatch: '+arm)
                start = current['crop_schedule_start_step']
                require(start == (schedules[-1]['end'] if schedules else 0) and step > start,
                        'Crop history is incomplete: '+arm)
                crop_hash = current['crop_schedule_sha256']
                require(isinstance(crop_hash, str) and len(crop_hash) == 64
                        and all(c in '0123456789abcdef' for c in crop_hash), 'Invalid crop hash: '+arm)
                schedules.append({'start': start, 'end': step, 'sha256': crop_hash})
            require(schedules[-1]['end'] == end, 'Missing terminal crop schedule: '+arm)
            require(best['crop_schedule_start_step'] == schedules[-1]['start']
                    and best['crop_schedule_sha256'] == schedules[-1]['sha256'],
                    'Export/current crop schedule mismatch: '+arm)
            require(set(current['esr_by_channel']) == set(best['channels']) == set(NAMES),
                    'Missing result channels: '+arm)
            rows = {}
            for i, name in enumerate(NAMES):
                selected = best['channels'][name]
                require(selected['selected_step'] == current['best_steps'][i]
                        and 0 <= selected['selected_step'] <= end, 'Selection step mismatch: '+arm+'/'+name)
                require(read(folder/f'Nastrond-{name}.nam', decode=False)
                        == selected['sha256'] == last['export_sha256'][name], 'Export hash mismatch: '+arm+'/'+name)
                initial = init['initial_validation_esr'][name]
                actual = number(current['esr_by_channel'][name], arm+'/'+name+' current ESR', 0)
                retained = number(selected['esr'], arm+'/'+name+' best ESR', 0)
                require(retained <= min(initial, actual)+1e-6, 'Best export is worse than an available candidate: '+arm+'/'+name)
                rows[name] = {
                    'initial_esr': initial, 'initial_fidelity': None,
                    'initial_fidelity_note': 'Not recorded; no inferred or historical mixed-state fidelity substituted',
                    'current_esr': actual,
                    'current_fidelity': check_fidelity(current['tone3000_fidelity'][name], actual, arm+'/'+name+' current'),
                    'best_esr': retained,
                    'best_fidelity': check_fidelity(selected['tone3000_fidelity'], retained, arm+'/'+name+' best'),
                    'best_selected_step': selected['selected_step'],
                    'current_change_vs_initial_pct': percent(actual, initial),
                    'best_change_vs_initial_pct': percent(retained, initial),
                }
            aggregate = {key: sum(row[key] for row in rows.values())/len(NAMES)
                         for key in ('initial_esr', 'current_esr', 'best_esr')}
            for key in ('current', 'best'):
                aggregate[key+'_change_vs_initial_pct'] = percent(aggregate[key+'_esr'], aggregate['initial_esr'])
                aggregate[key+'_fidelity_pass_channels'] = sum(row[key+'_fidelity']['fidelity_pass'] for row in rows.values())
            arms[arm] = {'status': progress['status'], 'checkpoint_step': end, 'config': cfg,
                         'initialization': init, 'crop_schedules': schedules, 'channels': rows, 'aggregate': aggregate}
        left, right = (arms[name] for name in arm_order)
        for key in MATCH_CONFIG:
            require(left['config'][key] == right['config'][key], 'Unmatched comparison config: '+key)
        for key in ('source_checkpoint_step', 'source_selected_steps', 'optimizer', 'seed'):
            require(left['initialization'][key] == right['initialization'][key], 'Unmatched initialization: '+key)
        require(left['checkpoint_step'] == right['checkpoint_step'], 'Unmatched training budgets')
        require(left['crop_schedules'] == right['crop_schedules'], 'Unmatched crop histories')
        for name in NAMES:
            require(abs(left['channels'][name]['initial_esr']-right['channels'][name]['initial_esr']) <= 1e-6,
                    'Initial physical models differ: '+name)
            for key in ('current', 'best'):
                right['channels'][name][key+'_change_vs_legacy_pct'] = percent(
                    right['channels'][name][key+'_esr'], left['channels'][name][key+'_esr'])
        for key in ('current', 'best'):
            right['aggregate'][key+'_change_vs_legacy_pct'] = percent(right['aggregate'][key+'_esr'], left['aggregate'][key+'_esr'])
        return {'schema': 1, 'comparison_evidence_verified': True, 'release_approved': False,
                'automatic_adoption': False, 'source_checkpoint_sha256': source_hash,
                'source_checkpoint_step': left['initialization']['source_checkpoint_step'],
                'quality_profile': PROFILE, 'arm_order': list(arm_order), 'arms': arms, 'evidence_sha256': evidence,
                'scope': 'Matched synthetic validation experiment; no independent test, engine parity, or listening approval',
                'percentage_definition': '100 * (candidate/reference - 1); negative ESR change means lower error',
                'qualification': 'Upstream-derived objective and normalization with matched random crops; not entire TONE3000 cloud replication'}


def markdown(result):
    baseline, candidate = result['arm_order']
    left, right = (result['arms'][name] for name in result['arm_order'])
    lines = ['# Náströnd A2 recipe comparison', '',
             f"Same frozen source checkpoint at step {result['source_checkpoint_step']}; each arm completed {left['checkpoint_step']} additional optimizer steps.", '',
             f"Candidate folder: {candidate}. Initial learning rates: {baseline} {left['config']['learning_rate']['initial']:g}; {candidate} {right['config']['learning_rate']['initial']:g}. The candidate retains the official-a2 loss/normalization recipe; the complete schedules are recorded in comparison.json or the candidate-specific JSON report.", '',
             f'| Channel | Initial ESR | Legacy current | {candidate} current | Candidate change vs legacy | Legacy best | Candidate best |',
             '|---|---:|---:|---:|---:|---:|---:|']
    for name in NAMES:
        a, b = left['channels'][name], right['channels'][name]
        change = b['current_change_vs_legacy_pct']
        text = 'undefined (zero reference)' if change is None else f'{change:+.2f}%'
        lines.append(f"| {name} | {a['initial_esr']:.6f} | {a['current_esr']:.6f} | {b['current_esr']:.6f} | {text} | {a['best_esr']:.6f} | {b['best_esr']:.6f} |")
    lines += ['', 'Current refers to the final trained checkpoint. Best may retain the unchanged source model for an individual channel.', '',
              'Negative percentage changes mean lower ESR. Full per-channel fidelity metrics, initial-relative changes, aggregate results, configuration, and evidence hashes are in comparison.json.', '',
              f"Current validation fidelity passes: legacy {left['aggregate']['current_fidelity_pass_channels']}/5; {candidate} {right['aggregate']['current_fidelity_pass_channels']}/5. Best retained passes: legacy {left['aggregate']['best_fidelity_pass_channels']}/5; {candidate} {right['aggregate']['best_fidelity_pass_channels']}/5.", '',
              'Initial fidelity was not recorded; only measured initial ESR and raw-output reparameterization checks are available.', '',
              'This experiment does not approve a release or automatically select a recipe. Independent tests, actual engine parity, and GUI/DAW listening are not established by this report. The objective is upstream-derived; matched random sampling differs from the complete upstream training workflow.', '']
    return '\n'.join(lines)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--comparison', type=Path, required=True)
    parser.add_argument('--candidate', default='official-a2',
                        help='Candidate subfolder; custom variants keep separate summary files')
    args = parser.parse_args()
    result = summarize(args.comparison, args.candidate)
    suffix = '' if args.candidate == 'official-a2' else '-'+args.candidate
    report_name = 'comparison'+suffix+'.json'
    readme = markdown(result).replace('comparison.json or the candidate-specific JSON report', report_name)
    readme = readme.replace('are in comparison.json.', 'are in '+report_name+'.')
    outputs = {report_name: json.dumps(result, ensure_ascii=False, indent=2, allow_nan=False)+'\n',
               'README'+suffix+'.md': readme}
    for name, contents in outputs.items():
        path = args.comparison/name
        temporary = path.with_suffix(path.suffix+'.tmp')
        temporary.write_text(contents)
        temporary.replace(path)
    print(json.dumps({'comparison': str(args.comparison), 'candidate': args.candidate, 'report': report_name, 'evidence_verified': True,
                      'release_approved': False, 'aggregate': {k: v['aggregate'] for k, v in result['arms'].items()}}))


if __name__ == '__main__':
    main()
