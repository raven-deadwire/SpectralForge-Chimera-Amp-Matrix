#!/usr/bin/env python3
"""Apply the owner's existing-behavior decision using explicit, source-bound evidence.

Historical regression evidence retains its original product/run. It does not
certify today's binaries or substitute for the current A12/A13/I1/E1 producers.
"""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys

import audit_acceptance_definitions as historical
import release_evidence as evidence

ROOT = Path(__file__).resolve().parents[1]
REGISTRY = 'Validation/accepted-baseline.json'
REGISTRY_SHA256 = '457260441cd5000c2b82abaa3b4c75d7262c5a5d9187b1615ad6e34a8938dda5'
FOLDER = 'Chimera-release-evidence-accepted-baseline'
PRODUCER = 'accepted-existing-behavior-baseline'
require = evidence.gate.require
INPUTS = (REGISTRY, 'Tools/accepted_baseline.py', 'Tools/evaluate_signature_benchmark.py',
          'Tools/test_signature_calculator_contracts.py', 'Tools/check_ir_distribution.py',
          'Tests/test_ir_distribution.py', 'docs/RELEASE_CHECKLIST.md',
          'Tools/release_evidence.py', 'Tools/audit_acceptance_definitions.py')


def digest(data):
    return hashlib.sha256(data).hexdigest()


def load_registry():
    raw = (ROOT / REGISTRY).read_bytes()
    require(digest(raw) == REGISTRY_SHA256, 'Accepted definition fingerprint changed; review required')
    registry = evidence.gate.load_json(ROOT / REGISTRY)
    require(registry['schema'] == 'spectralforge.chimera.accepted-baseline.v1'
            and registry['owner_instruction'] == '이건 그동안 맞았으니까 처리해'
            and registry['publication_authorized'] is False and registry['expiry_approved'] is False
            and registry['waivers'] == [], 'Existing-behavior approval scope changed')
    proposal = ROOT / registry['proposal_path']
    require(digest(proposal.read_bytes()) == registry['proposal_sha256'], 'Historical proposal changed')
    ledger = historical.load_ledger()
    historical.validate_registry(ledger)
    expected = {r['id'] for r in ledger['rows']}
    rows = registry['rows']
    require(len(rows) == 86 and len({r['id'] for r in rows}) == 86
            and {r['id'] for r in rows} == expected, 'Accepted inventory differs from the 86 legacy IDs')
    policy, automatic = evidence.configuration()
    require(not expected.intersection(automatic), 'Baseline would overwrite an automatic owner')
    for row in rows:
        require(row['strict_hard_gate'] == (row['id'] in policy['hard_gates'])
                and row['original_mapping_recovered'] is False, 'Historical mapping or hard gate changed')
    for path, wanted in registry['fixed_policy_sha256'].items():
        require(digest((ROOT / path).read_bytes()) == wanted, 'Release/expiry policy changed: ' + path)
    return registry, ledger


def fingerprint_changes(registry):
    changed = [path for path, wanted in registry['native_fingerprint'].items()
            if not (ROOT / path).is_file() or (ROOT / path).is_symlink()
            or digest((ROOT / path).read_bytes()) != wanted]
    tracked = subprocess.check_output(['git', 'ls-files', '--cached', '--others', '--exclude-standard'], cwd=ROOT, text=True).splitlines()
    changed.extend(p for p in tracked if p.startswith(('Source/', 'Assets/', 'Preparation/', 'Installer/', 'Tests/', 'cmake/'))
                   and p not in registry['native_fingerprint'])
    return sorted(set(changed))


def current_inputs():
    return {p: digest((ROOT / p).read_bytes()) for p in INPUTS}


def documentation_checks():
    text = (ROOT / 'docs/RELEASE_CHECKLIST.md').read_text(encoding='utf-8')
    version = (ROOT / 'VERSION').read_text().strip()
    channel = (ROOT / 'RELEASE_CHANNEL').read_text().strip()
    candidate = version + '-' + channel
    return {
        'A14.01': ('Candidate package/tag: **' + candidate + ' / v' + candidate + '**' in text
                   and 'published beta.2 remains a historical release' in text
                   and 'target_version=' + candidate in text),
        'A14.05': all(s in text for s in ('86 undefined A checks', '13 input/reference gaps',
            '7 real-environment gaps', 'one final acceptance', 'current counts must come from the generated gate',
            'does not certify missing tests', 'authorize beta.3 publication')),
    }


def run_component_checks(output):
    cases_path = output / 'calculator-cases.json'
    commands = [
        ('calculator', [sys.executable, str(ROOT / 'Tools/test_signature_calculator_contracts.py'),
                        '--report', str(cases_path)]),
        ('distribution', [sys.executable, str(ROOT / 'Tests/test_ir_distribution.py')]),
    ]
    outcomes = {}
    for name, command in commands:
        result = subprocess.run(command, cwd=ROOT, capture_output=True, text=True, timeout=120)
        log = output / (name + '.log')
        log.write_text(result.stdout + result.stderr, encoding='utf-8')
        outcomes[name] = {'command': command, 'exit_code': result.returncode,
                          'log': log.name, 'log_sha256': digest(log.read_bytes())}
    return outcomes


def calculator_cases(folder, commands):
    if commands['calculator']['exit_code'] != 0:
        return set()
    data = evidence.gate.load_json(folder / 'calculator-cases.json')
    import test_signature_calculator_contracts as tests
    expected = set(tests.case_names())
    cases = data['cases']
    require(data['synthetic_only'] is True and data['policy_claims'] == []
            and all(type(data[k]) is int for k in ('tests_run', 'failures', 'errors', 'skipped'))
            and data['tests_run'] == len(cases) == len(expected)
            and len({c['name'] for c in cases}) == len(cases)
            and {c['name'] for c in cases} == expected
            and data['failures'] == data['errors'] == data['skipped'] == 0
            and all(c['status'] == 'PASS' for c in cases), 'Calculator evidence incomplete/failed/skipped')
    return expected


def decisions(registry, native_changes, cases, distribution_ok, doc_checks):
    results = {}
    for row in registry['rows']:
        kind = row['verification_kind']
        status, reason = 'BLOCKED', row['remaining_work_ko']
        if kind == 'historical':
            status = 'PASS' if not native_changes else 'BLOCKED'
            reason = ('Verified historical regression and unchanged implementation; original source/run retained'
                      if status == 'PASS' else 'Historical regression cannot cover changed inputs: ' + ', '.join(native_changes[:8]))
        elif kind == 'calculator':
            status = 'PASS' if row['test_cases'] and set(row['test_cases']).issubset(cases) else 'BLOCKED'
            reason = 'Current calculator contracts' if status == 'PASS' else 'Current calculator cases missing or failed'
        elif kind == 'distribution':
            status = 'PASS' if distribution_ok and not native_changes else 'BLOCKED'
            reason = 'Current distribution negatives plus pinned factory catalog regression' if status == 'PASS' else 'Distribution evidence or source equivalence missing'
        elif kind == 'documentation':
            status = 'PASS' if doc_checks.get(row['id']) is True else 'BLOCKED'
            reason = 'Current candidate/checklist and explicit acceptance scope' if status == 'PASS' else 'Candidate/checklist mismatch'
        results[row['id']] = {'status': status, 'reason': reason, 'kind': kind,
                              'approval': row['approval']}
    return results


def produce(output, revision, *, diagnostic=False):
    require(not output.exists(), 'Fresh baseline output directory required')
    registry, ledger = load_registry()
    # Independently verify archives, source identities, all native platforms and
    # exact test/log sections. The old PR41 failure remains part of the audit.
    historical.verify_reused_evidence(ledger)
    dirty = subprocess.check_output(['git', 'status', '--porcelain', '--untracked-files=no'], cwd=ROOT, text=True)
    actual = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip()
    require(diagnostic or (not dirty and actual == revision['commit_sha']), 'Producer requires clean exact-source checkout')
    output.mkdir(parents=True)
    commands = run_component_checks(output)
    cases = calculator_cases(output, commands)
    changes = fingerprint_changes(registry)
    docs = documentation_checks()
    result = decisions(registry, changes, cases, commands['distribution']['exit_code'] == 0, docs)
    files = {p.name: digest(p.read_bytes()) for p in output.iterdir() if p.is_file()}
    receipt = {'schema': 'spectralforge.chimera.accepted-baseline-evidence.v1',
        'revision': revision, 'registry_sha256': REGISTRY_SHA256, 'diagnostic_only': diagnostic,
        'source_inputs': current_inputs(), 'historical_product_sha': registry['historical_product_sha'],
        'historical_run_id': '38386305674', 'historical_upgrade_run_id': '38777238017',
        'current_binary_acceptance': False, 'publication_authorized': False,
        'commands': commands, 'files': files, 'native_input_changes': changes,
        'documentation_checks': docs, 'checks': result,
        'counts': {'total': len(result), 'pass': sum(r['status'] == 'PASS' for r in result.values()),
                   'blocked': sum(r['status'] == 'BLOCKED' for r in result.values())}}
    (output / 'evidence.json').write_text(json.dumps(receipt, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    return receipt


def read_reports(folder, revision, policy):
    require(folder.is_dir() and not folder.is_symlink(), 'Unsafe baseline evidence folder')
    registry, ledger = load_registry()
    historical.verify_reused_evidence(ledger)
    receipt = evidence.gate.load_json(folder / 'evidence.json')
    require(receipt['schema'] == 'spectralforge.chimera.accepted-baseline-evidence.v1'
            and receipt['revision'] == revision and receipt['registry_sha256'] == REGISTRY_SHA256
            and receipt['diagnostic_only'] is False and receipt['publication_authorized'] is False
            and receipt['current_binary_acceptance'] is False
            and receipt['historical_product_sha'] == registry['historical_product_sha']
            and receipt['historical_run_id'] == '38386305674'
            and receipt['historical_upgrade_run_id'] == '38777238017', 'Baseline evidence identity/scope mismatch')
    require(receipt['source_inputs'] == current_inputs(), 'Stale baseline component source')
    required_files = {'calculator.log', 'calculator-cases.json', 'distribution.log'}
    require(set(receipt['files']) == required_files
            and {p.name for p in folder.iterdir()} == required_files | {'evidence.json'}, 'Unexpected baseline file inventory')
    for name, wanted in receipt['files'].items():
        path = folder / name
        require(path.is_file() and not path.is_symlink() and digest(path.read_bytes()) == wanted,
                'Baseline raw evidence hash mismatch: ' + name)
    require(set(receipt['commands']) == {'calculator', 'distribution'}, 'Baseline command inventory mismatch')
    for name, cmd in receipt['commands'].items():
        require(type(cmd['exit_code']) is int and cmd['log'] == name + '.log'
                and cmd['log_sha256'] == receipt['files'][cmd['log']], 'Baseline execution/log mismatch')
        command = cmd['command']
        script = 'test_signature_calculator_contracts.py' if name == 'calculator' else 'test_ir_distribution.py'
        require(isinstance(command, list) and len(command) == (4 if name == 'calculator' else 2)
                and all(isinstance(a, str) and a for a in command)
                and Path(command[1]).name == script, 'Unexpected baseline command')
        if name == 'calculator':
            require(command[2] == '--report' and Path(command[3]).name == 'calculator-cases.json', 'Calculator output mismatch')
    cases = calculator_cases(folder, receipt['commands'])
    changes = fingerprint_changes(registry)
    docs = documentation_checks()
    expected = decisions(registry, changes, cases, receipt['commands']['distribution']['exit_code'] == 0, docs)
    require(receipt['checks'] == expected and receipt['native_input_changes'] == changes
            and receipt['documentation_checks'] == docs, 'Forged baseline verdict')
    require(receipt['counts'] == {'total': 86, 'pass': sum(v['status'] == 'PASS' for v in expected.values()),
                                 'blocked': sum(v['status'] == 'BLOCKED' for v in expected.values())}, 'Baseline count mismatch')
    reports = {}
    for row in registry['rows']:
        check = expected[row['id']]
        reports[row['id']] = evidence.make_report(policy, row['id'], revision['commit_sha'],
            passed=True if check['status'] == 'PASS' else None,
            reason='' if check['status'] == 'PASS' else check['reason'],
            scope={'producer': PRODUCER, 'definition_sha256': REGISTRY_SHA256,
                'evidence_basis': check['reason'],
                'verification_kind': check['kind'], 'historical_product_sha': registry['historical_product_sha'],
                'historical_regression_only': check['kind'] == 'historical',
                'current_binary_acceptance': False, 'publication_authorized': False,
                'run_id': revision['run_id'], 'run_attempt': revision['run_attempt']},
            artifacts=['evidence/accepted-baseline/evidence.json'])
    return reports


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--commit', required=True)
    parser.add_argument('--run-id', required=True)
    parser.add_argument('--run-attempt', required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--diagnostic', action='store_true')
    args = parser.parse_args()
    result = produce(args.output, evidence.identity(args.commit, args.run_id, args.run_attempt), diagnostic=args.diagnostic)
    print(json.dumps({'baseline_counts': result['counts'], 'diagnostic_only': result['diagnostic_only'],
                      'current_binary_acceptance': False, 'publication_authorized': False}, indent=2))
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
