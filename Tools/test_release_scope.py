#!/usr/bin/env python3
"""Source channels retain all checks except the two explicit beta deferrals."""
import copy
import json
from pathlib import Path
from types import SimpleNamespace
import evaluate_release_gate as gate
import produce_validation_check as producer
import chimera_version

ROOT = Path(__file__).resolve().parents[1]
POLICY = json.loads((ROOT / 'Validation/release-policy.json').read_text(encoding='utf-8-sig'))
COMMIT = '1' * 40
DEFERRED = {'I2.PITCH_LIVE', 'I2.PITCH_DI'}

def check_ids(profile):
    return {cid for stage in POLICY['profiles'][profile]['required_stages']
            for cid in POLICY['stages'][stage]['required_checks']}

def run():
    # Keep the historical beta inventory contract even while checking an RC or
    # stable source. A new channel must not inherit the beta's two deferrals.
    beta = check_ids('beta_1_3')
    full = check_ids('full_release')
    assert full - beta == DEFERRED and not beta - full
    assert check_ids('transpose_followup') == DEFERRED
    scope = POLICY['release_scope']
    channel = chimera_version.release_channel(ROOT)
    expected_profile = 'full_release' if channel == 'stable' or channel.startswith('rc.') else 'beta_1_3'
    assert scope['profile'] == expected_profile and scope['channel'] == channel
    if channel == 'preview':
        assert scope['version'] == chimera_version.product_version(ROOT)
    else:
        assert scope['version'] == chimera_version.identity(ROOT, COMMIT)['version']
    selected = check_ids(expected_profile)
    deferred = DEFERRED if expected_profile == 'beta_1_3' else set()
    assert full - selected == deferred and not selected - full
    assert set(scope['deferred_checks']) == deferred
    assert {'I1.PITCH', 'I2.UI_AUDIO_TIMING', 'I2.LIVE_REMOVE_DAW', 'I2.GATE_DI',
            'B12.RESULT', 'E2.SAME_DI'} <= selected
    checks = {}
    for cid in full:
        args = SimpleNamespace(check_id=cid, name=cid, commit=COMMIT,
            not_applicable=False, not_executed=False, exit_code=0,
            assertion='pass', depends_on=None,
            artifact=None, fixture_sha256=None, scope_json='{}', waiver_id=None,
            failure_type=None, failure_message=None)
        checks[cid] = producer.build_report(POLICY, args)
    waivers = {'waivers': []}
    assert gate.evaluate_release(POLICY, waivers, checks, expected_profile, COMMIT)['ready']
    failed_transpose = copy.deepcopy(checks)
    for cid in DEFERRED:
        failed_transpose[cid]['assertion']['passed'] = False
    assert gate.evaluate_release(POLICY, waivers, failed_transpose, 'beta_1_3', COMMIT)['ready']
    assert not gate.evaluate_release(POLICY, waivers, failed_transpose, 'full_release', COMMIT)['ready']
    assert gate.evaluate_release(POLICY, waivers, failed_transpose, expected_profile, COMMIT)['ready'] is bool(deferred)
    followup = gate.evaluate_release(POLICY, waivers, failed_transpose, 'transpose_followup', COMMIT)
    assert not followup['ready'] and all(c['computed_status'] == 'BLOCKED' for c in followup['checks'])
    for cid in set(POLICY['hard_gates']) & selected:
        failed = copy.deepcopy(checks)
        failed[cid]['assertion']['passed'] = False
        assert not gate.evaluate_release(POLICY, waivers, failed, expected_profile, COMMIT)['ready'], cid
        del failed[cid]
        assert not gate.evaluate_release(POLICY, waivers, failed, expected_profile, COMMIT)['ready'], cid
    print('PASS: beta defers exactly two transpose goals; RC/stable retain every full-release check; all applicable hard gates fail closed.')

if __name__ == '__main__':
    run()
