#!/usr/bin/env python3
"""Execute recovered calculator contracts on clean HEAD; no release-check claims.

The artifact retains actual commands, return codes, per-case results and hashes.
Local execution is explicitly distinct from a GitHub run. Component success
cannot promote any of the 86 undefined or 21 external acceptance checks.
"""
import argparse
from datetime import datetime, timezone
import json
import os
from pathlib import Path
import platform
import re
import subprocess
import sys
import uuid

import evaluate_release_gate as gate
import test_signature_calculator_contracts as contracts
import validation_inventory as inventory

ROOT = Path(__file__).resolve().parents[1]
INPUTS = ('Tools/produce_recovered_evidence.py', 'Tools/validation_inventory.py',
          'Tools/evaluate_release_gate.py', 'Tools/evaluate_signature_benchmark.py',
          'Tools/test_signature_benchmark.py', 'Tools/test_signature_calculator_contracts.py',
          'Validation/signature-benchmark-policy.json', 'Validation/release-policy.json',
          'Validation/ci-check-map.json', 'Validation/legacy-check-recovery.json', 'Validation/waivers.json')


def git(*args):
    return subprocess.check_output(['git', *args], cwd=ROOT, text=True).strip()


def clean_identity(commit):
    gate.require(re.fullmatch('[0-9a-f]{40}', commit) is not None and git('rev-parse', 'HEAD') == commit,
                 'Source SHA must equal checked-out HEAD')
    gate.require(not git('status', '--porcelain', '--untracked-files=all'),
                 'Evidence requires a clean checkout; commit changes before execution')
    for name in INPUTS:
        gate.require(not (ROOT / name).is_symlink(), 'Source input cannot be a symlink')
        gate.require(git('ls-files', '--error-unmatch', name) == name, 'Source input must be tracked')
    return {'commit_sha': commit, 'tree_sha': git('rev-parse', 'HEAD^{tree}')}


def successful_cases(doc):
    expected = set(contracts.case_names())
    if not isinstance(doc, dict) or doc.get('schema') != 'spectralforge.chimera.calculator-contracts':
        return False
    rows = doc.get('cases')
    return (doc.get('schema_version') == 1 and doc.get('synthetic_only') is True and doc.get('policy_claims') == []
            and type(doc.get('tests_run')) is int and doc['tests_run'] == len(expected)
            and all(type(doc.get(k)) is int and doc[k] == 0 for k in ('failures', 'errors', 'skipped'))
            and isinstance(rows, list) and len(rows) == len(expected)
            and all(isinstance(r, dict) and isinstance(r.get('name'), str) and r.get('status') == 'PASS' for r in rows)
            and {r.get('name') for r in rows} == expected)


def execute(output, commit, run_id=None, run_attempt=None):
    revision = clean_identity(commit)
    gate.require((run_id is None) == (run_attempt is None), 'Run id and attempt must be supplied together')
    if run_id is not None:
        gate.require(all(re.fullmatch('[1-9][0-9]*', str(v)) for v in (run_id, run_attempt)), 'Invalid CI identity')
        gate.require(os.environ.get('GITHUB_ACTIONS') == 'true' and
                     os.environ.get('GITHUB_RUN_ID') == run_id and os.environ.get('GITHUB_RUN_ATTEMPT') == run_attempt,
                     'CI identity must match the running Actions environment')
        revision.update(run_id=run_id, run_attempt=run_attempt)
    gate.require(not output.exists(), 'Use a fresh output directory; stale evidence reuse is forbidden')
    # Output must not overwrite source files; build/ is the supported in-repo location.
    output = output.resolve()
    gate.require(not output.is_relative_to(ROOT) or output.is_relative_to(ROOT / 'build'),
                 'In-repository evidence must be under build/')
    output.mkdir(parents=True)
    hashes = {name: inventory.sha256(ROOT / name) for name in INPUTS}
    commands = [
        ('legacy', [sys.executable, 'Tools/test_signature_benchmark.py']),
        ('calculator', [sys.executable, 'Tools/test_signature_calculator_contracts.py', '--report', str(output / 'cases.json')]),
    ]
    receipts = []
    started = datetime.now(timezone.utc).isoformat()
    for name, command in commands:
        log = output / (name + '.log')
        with log.open('wb') as stream:
            try:
                completed = subprocess.run(command, cwd=ROOT, stdout=stream, stderr=subprocess.STDOUT, timeout=120)
                code, reason, executed = completed.returncode, None, True
            except subprocess.TimeoutExpired as exc:
                code, reason, executed = None, str(exc), True
            except OSError as exc:
                code, reason, executed = None, str(exc), False
        receipts.append({'name': name, 'command': command, 'executed': executed,
                         'exit_code': code, 'error': reason, 'log': log.name, 'log_sha256': inventory.sha256(log)})
    case_error = None
    try:
        cases = gate.load_json(output / 'cases.json')
    except gate.ValidationError as exc:
        cases, case_error = None, str(exc)
    legacy_marker = 'PASS: signature benchmark produces PASS/REVISE/INVALID and enforces hard gates independently of score.'
    legacy_complete = (output / 'legacy.log').read_text(encoding='utf-8', errors='replace').splitlines().count(legacy_marker) == 1
    ok = all(r['exit_code'] == 0 for r in receipts) and legacy_complete and successful_cases(cases)
    clean_identity(commit)
    gate.require(hashes == {name: inventory.sha256(ROOT / name) for name in INPUTS}, 'Source changed during execution')
    policy = gate.load_json(ROOT / 'Validation/release-policy.json')
    mapping = gate.load_json(ROOT / 'Validation/ci-check-map.json')['checks']
    inventory.write_inventory(output, inventory.inventory(policy, mapping, commit))
    doc = {'schema': 'spectralforge.chimera.recovered-component-evidence', 'schema_version': 1,
           'receipt_id': str(uuid.uuid4()), 'revision': revision,
           'execution_environment': 'github_actions' if run_id is not None else 'local',
           'platform': platform.platform(), 'python': sys.version, 'started_at': started,
           'finished_at': datetime.now(timezone.utc).isoformat(),
           'source_inputs': hashes, 'commands': receipts, 'case_receipt_error': case_error,
           'legacy_completion_assertion': legacy_complete,
           'component': 'signature-calculator-contracts', 'component_status': 'PASS' if ok else 'BLOCKED',
           'synthetic_only': True, 'policy_claims': [], 'release_verdict': 'BLOCKED',
           'note': 'Calculator correctness only. No numbered A mapping, native CI, reference audio, listening or DAW acceptance.'}
    doc['files'] = {p.name: inventory.sha256(p) for p in output.iterdir() if p.is_file()}
    (output / 'recovery-evidence.json').write_text(json.dumps(doc, indent=2) + '\n', encoding='utf-8')
    return doc


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--commit', required=True)
    parser.add_argument('--output', required=True, type=Path)
    parser.add_argument('--run-id')
    parser.add_argument('--run-attempt')
    args = parser.parse_args()
    try:
        doc = execute(args.output, args.commit, args.run_id, args.run_attempt)
        print(json.dumps({'revision': doc['revision'], 'component_status': doc['component_status'],
                          'policy_claims': [], 'release_verdict': 'BLOCKED'}))
        return 0 if doc['component_status'] == 'PASS' else 1
    except (gate.ValidationError, OSError, ValueError, subprocess.SubprocessError) as exc:
        print('RECOVERY EVIDENCE ERROR: ' + str(exc), file=sys.stderr)
        return 2


if __name__ == '__main__':
    raise SystemExit(main())
