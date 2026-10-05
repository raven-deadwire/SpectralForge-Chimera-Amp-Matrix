#!/usr/bin/env python3
"""Audit like-for-like NAM macro measurements; never certify hardware/DI tone.

Inputs are the full reports from compare_original_nam.py, including all 26
captures. Target groups are fixed to the baseline's high-gain head examples.
No weights, audio, level fitting or spectral correction is applied here.
"""
import argparse
import hashlib
import json
import math
from pathlib import Path
from statistics import mean


GROUPS = {
    'crush': ('vh4', [569473, 569474, 569477]),
    'impact': ('twinjet', [652436, 652437]),
    'rot': ('granophyre', [403977, 421075, 421074]),
    'bloom': ('matamp', [614727, 561845, 543854]),
}


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def require(condition, message):
    if not condition:
        raise ValueError(message)


def reference_identity(report):
    refs = report['references']
    require(len(refs) == 26 and len({r['model_id'] for r in refs}) == 26,
            'Expected exactly 26 distinct baseline captures')
    return {r['model_id']: (r['family'], r['sha256'], r['gain_group'], r['capture_scope'],
                           {v: (data['input_gain_db'], data['audio_sha256'])
                            for v, data in r['variants'].items()}) for r in refs}


def audit(reports):
    baseline = reports['baseline']
    identity = reference_identity(baseline)
    for label, report in reports.items():
        for key in ['manifest_sha256', 'probe_sha256', 'nam_renderer_sha256', 'protocol']:
            require(report[key] == baseline[key], f'{label}: incompatible {key}')
        require(reference_identity(report) == identity, f'{label}: changed NAM identity/render')
        require(set(report['original']) == set(baseline['original']), f'{label}: changed states')
        require(len(report['original']) == 17, f'{label}: expected 17 Original states')
        for state, item in report['original'].items():
            require(item['state'] == baseline['original'][state]['state'],
                    f'{label}: changed state {state}')

    groups, failed = {}, []
    for macro, (family, ids) in GROUPS.items():
        group = dict(family=family, model_ids=ids, measurements={})
        for label, report in reports.items():
            refs = {r['model_id']: r for r in report['references']}
            for model_id in ids:
                require(refs[model_id]['family'] == family, 'Wrong target family')
            observations = {}
            for segment in ['pluck', 'chord']:
                per_reference = []
                for model_id in ids:
                    comparisons = refs[model_id]['variants']['digital_equal']['spectral_comparisons']
                    scores = {state: comparisons[state][segment]['log_spectrum_rms_db']
                              for state in ['default', macro+'_0', macro+'_1']}
                    require(all(math.isfinite(v) for v in scores.values()), 'Nonfinite score')
                    improvement = scores[macro+'_0'] - scores[macro+'_1']
                    per_reference.append(dict(model_id=model_id, scores_db=scores,
                                              endpoint_improvement_db=improvement))
                    if label == 'candidate' and improvement < .1:
                        failed.append(f'{macro}/{model_id}/{segment}: {improvement:.6f} dB')
                observations[segment] = dict(
                    mean_scores_db={state: mean(r['scores_db'][state] for r in per_reference)
                                    for state in ['default', macro+'_0', macro+'_1']},
                    per_reference=per_reference)
            group['measurements'][label] = observations
        groups[macro] = group

    # Neutral gain and macro settings must not be used to inflate improvements.
    drift = {}
    for segment in ['pluck', 'chord']:
        a = reports['incoming']['original']['default']['measurements']['synthetic_notes'][segment]
        b = reports['candidate']['original']['default']['measurements']['synthetic_notes'][segment]
        drift[segment] = b['rms_dbfs'] - a['rms_dbfs']
        require(abs(drift[segment]) < .001, f'Default {segment} RMS drift')
    result = dict(
        schema=1, reference_count=26, original_states=17,
        protocol='Unchanged digital input and NAM outputs; RMS-matched Welch 65-8000 Hz; '
                 'each target capture must improve by >=0.1 dB from macro 0 to 1 '
                 'on both synthetic pluck and chord. No hardware-match threshold.',
        default_rms_drift_from_incoming_db=drift,
        groups=groups,
        status=dict(synthetic_endpoint_direction='PASS' if not failed else 'FAIL',
                    clank_amp_only_reference='BLOCKED: Meshuggah captures include cabinet',
                    instrument_di_listening='NOT_RUN', hardware_matching='BLOCKED',
                    final_voicing='BLOCKED: owner instrument-DI listening required'),
        failures=failed)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ['baseline', 'incoming', 'candidate', 'out']:
        parser.add_argument('--'+name, required=True, type=Path)
    args = parser.parse_args()
    paths = {name: getattr(args, name) for name in ['baseline', 'incoming', 'candidate']}
    result = audit({name: json.loads(path.read_text()) for name, path in paths.items()})
    result['comparison_sha256'] = {name: digest(path) for name, path in paths.items()}
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(json.dumps(result, indent=2)+'\n')
    print(result['status']['synthetic_endpoint_direction'], 'synthetic macro direction only')
    for macro, group in result['groups'].items():
        for segment in ['pluck', 'chord']:
            v = group['measurements']['candidate'][segment]['mean_scores_db']
            print(f"{macro}/{segment}: {v[macro+'_0']:.3f} -> {v[macro+'_1']:.3f} dB")
    if result['failures']:
        raise SystemExit('; '.join(result['failures']))


if __name__ == '__main__':
    main()
