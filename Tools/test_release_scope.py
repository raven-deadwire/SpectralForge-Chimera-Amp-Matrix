#!/usr/bin/env python3
"""Only the explicitly deferred transpose goals may leave the current beta gate."""
import copy
import json
from pathlib import Path
from types import SimpleNamespace
import evaluate_release_gate as gate
import produce_validation_check as producer
import chimera_version

ROOT = Path(__file__).resolve().parents[1]
POLICY = json.loads((ROOT / 'Validation/release-policy.json').read_text())
COMMIT = '1' * 40
DEFERRED = {'I2.PITCH_LIVE', 'I2.PITCH_DI'}

def check_ids(profile):
    return {cid for stage in POLICY['profiles'][profile]['required_stages']
            for cid in POLICY['stages'][stage]['required_checks']}

def run():
    beta = check_ids(POLICY['release_scope']['profile'])
    full = check_ids('full_release')
    assert full - beta == DEFERRED and not beta - full
    assert check_ids('transpose_followup') == DEFERRED
    scope=POLICY['release_scope']
    if scope.get('channel') == 'preview':
        assert chimera_version.release_channel(ROOT) == 'preview'
        assert scope['version'] == chimera_version.product_version(ROOT)
    else:
        assert scope['version'] == chimera_version.identity(ROOT, COMMIT)['version']
    assert set(POLICY['release_scope']['deferred_checks']) == DEFERRED
    assert {'I1.PITCH', 'I2.UI_AUDIO_TIMING', 'I2.LIVE_REMOVE_DAW', 'I2.GATE_DI',
            'B12.RESULT', 'E2.SAME_DI'} <= beta
    checks = {}
    for cid in full:
        args = SimpleNamespace(check_id=cid, name=cid, commit=COMMIT,
            not_applicable=False, not_executed=False, exit_code=0,
            assertion='fail' if cid in DEFERRED else 'pass', depends_on=None,
            artifact=None, fixture_sha256=None, scope_json='{}', waiver_id=None,
            failure_type=None, failure_message=None)
        checks[cid] = producer.build_report(POLICY, args)
    waivers = {'waivers': []}
    assert gate.evaluate_release(POLICY, waivers, checks, POLICY['release_scope']['profile'], COMMIT)['ready']
    assert not gate.evaluate_release(POLICY, waivers, checks, 'full_release', COMMIT)['ready']
    followup = gate.evaluate_release(POLICY, waivers, checks, 'transpose_followup', COMMIT)
    assert not followup['ready'] and all(c['computed_status'] == 'BLOCKED' for c in followup['checks'])
    for cid in set(POLICY['hard_gates']) & beta:
        failed = copy.deepcopy(checks)
        failed[cid]['assertion']['passed'] = False
        assert not gate.evaluate_release(POLICY, waivers, failed, POLICY['release_scope']['profile'], COMMIT)['ready'], cid
        del failed[cid]
        assert not gate.evaluate_release(POLICY, waivers, failed, POLICY['release_scope']['profile'], COMMIT)['ready'], cid
    print('PASS: only two authorized transpose goals are deferred; DSP regression, host, timing and all other hard gates fail closed.')

if __name__ == '__main__':
    run()
