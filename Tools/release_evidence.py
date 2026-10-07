#!/usr/bin/env python3
"""Exact-source CI receipts. Only policy-owned automatic checks may be claimed.

The consolidator regenerates check assertions from these hashed raw inputs; it
never merges seeded reports or trusts a platform's self-declared PASS verdict.
"""
from __future__ import annotations
import hashlib
import json
from pathlib import Path, PurePosixPath
import re
from types import SimpleNamespace

import evaluate_release_gate as gate
import produce_validation_check as producer
from produce_ctest_validation import test_results

ROOT = Path(__file__).resolve().parents[1]
PLATFORMS = ('windows', 'linux', 'macos')
KINDS = (*PLATFORMS, 'assembly')
PROFILE = 'beta_1_2'
ARTIFACT = 'Chimera-A-stage-release-gate'
BUNDLE_PREFIX = 'Chimera-release-evidence-'
OUTCOMES = {'success', 'failure', 'cancelled', 'skipped', ''}
RAW_FILES = {
    'ctest': 'build/ctest-results.xml',
    'inventory': 'build/ctest-inventory.json',
    'dsp_log': 'build/Testing/Temporary/LastTest.log',
    'installer': 'build/installer-verification/InstallerVerification.json',
    'installer_log': 'build/installer-verification/InstallerVerification.txt',
    'msix': 'build/msix-verification/MSIXVerification.txt',
    'scan': 'build/windows-security/WindowsSecurity.txt',
    'scan_inventory': 'build/windows-security/FileInventory.json',
    'scan_status': 'build/windows-security/DefenderStatus.json',
    'macos_package': 'dist/macOS-package-verification.txt',
    'linux_package': 'dist/Linux-package-verification.txt',
    'candidate': 'candidate/candidate-source.json',
    'sums': 'candidate/SHA256SUMS.txt',
    'manifest': 'candidate/update-beta.json',
}


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def configuration():
    policy = gate.load_json(ROOT / 'Validation/release-policy.json')
    mapping = gate.load_json(ROOT / 'Validation/ci-check-map.json')
    gate.require(mapping['policy_version'] == policy['policy_version'], 'Stale CI ownership map')
    return policy, mapping['checks']


def identity(commit, run_id, attempt):
    gate.require(isinstance(commit, str) and re.fullmatch('[0-9a-f]{40}', commit), 'Full source SHA required')
    gate.require(str(run_id).isdigit() and int(run_id) > 0, 'Positive run id required')
    gate.require(str(attempt).isdigit() and int(attempt) > 0, 'Positive run attempt required')
    return {'commit_sha': commit, 'run_id': str(run_id), 'run_attempt': str(attempt)}


def make_report(policy, cid, commit, *, passed=None, reason='', scope=None, artifacts=None):
    return producer.build_report(policy, SimpleNamespace(
        check_id=cid, name=cid, commit=commit, not_applicable=False,
        not_executed=passed is None, exit_code=0 if passed else 1,
        assertion=None if passed is None else ('pass' if passed else 'fail'),
        depends_on=None, artifact=artifacts, fixture_sha256=None,
        scope_json=json.dumps(scope or {}), waiver_id=None,
        failure_type=None if passed else 'ARTIFACT_MISSING' if passed is None else 'TEST_FAILURE',
        failure_message=reason or None))


def collect_bundle(root, output, kind, revision, outcomes):
    """Called after the relevant steps, using their outcomes (not conclusions)."""
    policy, _ = configuration()
    gate.require(kind in KINDS and all(v in OUTCOMES for v in outcomes.values()), 'Invalid receipt outcome')
    output.mkdir(parents=True, exist_ok=False)
    files = {}
    keys = (['candidate', 'sums', 'manifest'] if kind == 'assembly' else
            ['ctest', 'inventory', 'dsp_log'] +
            (['installer', 'installer_log', 'msix', 'scan', 'scan_inventory', 'scan_status']
             if kind == 'windows' else [kind + '_package']))
    for key in keys:
        source = root / RAW_FILES[key]
        if not source.is_file():
            continue
        destination = output / 'raw' / source.name
        destination.parent.mkdir(exist_ok=True)
        destination.write_bytes(source.read_bytes())
        files[key] = {'path': destination.relative_to(output).as_posix(), 'sha256': digest(destination)}
    doc = {'schema': 'spectralforge.chimera.release-evidence', 'schema_version': 1,
           'policy_version': policy['policy_version'], 'profile': PROFILE,
           'revision': revision, 'kind': kind, 'outcomes': outcomes, 'files': files}
    (output / 'evidence.json').write_text(json.dumps(doc, indent=2) + '\n', encoding='utf-8')
    return doc


def read_bundle(folder, kind, revision, policy):
    doc = gate.load_json(folder / 'evidence.json')
    gate.require(isinstance(doc, dict) and doc.get('schema') == 'spectralforge.chimera.release-evidence'
                 and type(doc.get('schema_version')) is int and doc['schema_version'] == 1,
                 'Malformed evidence schema')
    gate.require(doc.get('kind') == kind and doc.get('profile') == PROFILE
                 and doc.get('revision') == revision
                 and doc.get('policy_version') == policy['policy_version'], 'Stale/mismatched evidence identity')
    outcomes = doc.get('outcomes')
    gate.require(isinstance(outcomes, dict) and all(isinstance(v, str) and v in OUTCOMES for v in outcomes.values()),
                 'Malformed execution outcomes')
    files = doc.get('files')
    gate.require(isinstance(files, dict) and not (set(files) - set(RAW_FILES)), 'Malformed file inventory')
    paths = {}
    used_paths = set()
    for key, entry in files.items():
        gate.require(isinstance(entry, dict) and isinstance(entry.get('path'), str), 'Malformed evidence path')
        gate.require(entry['path'] not in used_paths, 'Duplicate raw evidence path')
        used_paths.add(entry['path'])
        relative = PurePosixPath(entry['path'])
        gate.require(not relative.is_absolute() and '..' not in relative.parts and '\\' not in entry['path'],
                     'Unsafe evidence path')
        path = folder / relative
        gate.require(path.resolve().is_relative_to(folder.resolve()) and path.is_file()
                     and not path.is_symlink(), 'Missing/unsafe raw evidence')
        gate.require(isinstance(entry.get('sha256'), str) and digest(path) == entry['sha256'],
                     f'Raw evidence hash mismatch: {key}')
        gate.require(path.stat().st_size > 0, 'Empty raw evidence file')
        paths[key] = path
    actual_files = {p.relative_to(folder).as_posix() for p in folder.rglob('*') if p.is_file()}
    gate.require(actual_files == used_paths | {'evidence.json'}, 'Unexpected raw evidence files')
    return doc, paths


def contract_tests(spec, kind):
    """Portable requirements plus the Windows runtime's additional assertions."""
    tests = dict(spec['tests'])
    if kind == 'windows':
        for name, patterns in spec.get('windows_tests', {}).items():
            tests[name] = tests.get(name, []) + patterns
    return tests


def contract_passed(spec, kind, cases, log):
    """Bind markers to one completed CTest section, never a global PASS search.

    CTest's JUnit system-out is truncated on the audited run. Its status and
    configured inventory are checked by the caller; the separately hashed full
    LastTest.log supplies sub-suite coverage. A missing/duplicate/malformed
    section or disagreement between the two receipts cannot certify a check.
    """
    sections = re.split(r'^\d+/\d+ Testing: ', log, flags=re.MULTILINE)[1:]
    tests = contract_tests(spec, kind)
    if not tests or any(not patterns for patterns in tests.values()):
        return False
    for name, patterns in tests.items():
        matches = [s for s in sections if s.split('\n', 1)[0] == name]
        if not cases.get(name, False) or len(matches) != 1:
            return False
        section = matches[0]
        if not re.search(r'^\d+/\d+ Test: ' + re.escape(name) + r'$', section, re.MULTILINE):
            return False
        parts = section.split('Output:\n----------------------------------------------------------\n')
        if len(parts) != 2 or parts[1].count('\n<end of output>\n') != 1:
            return False
        output, trailer = parts[1].split('\n<end of output>\n')
        if (trailer.splitlines().count('Test Passed.') != 1
                or 'Test Failed.' in trailer
                or not re.search(r'^"' + re.escape(name) + r'" end time: .+$', trailer, re.MULTILINE)
                or re.search(r'^(?:FAIL|BLOCKED|UNRESPONSIVE)\b', output, re.MULTILINE)):
            return False
        lines = output.splitlines()
        if not all(any(re.fullmatch(pattern, line) for line in lines) for pattern in patterns):
            return False
    return True


def reports_for_bundle(doc, paths, policy, mapping):
    """Return sparse automatic claims only; no B/E2/I2/manual promotion exists."""
    kind = doc['kind']
    outcomes = doc['outcomes']
    commit = doc['revision']['commit_sha']
    scope = {'platform': kind, 'producer': 'exact-source-ci', 'synthetic_only': True,
             'run_id': doc['revision']['run_id'], 'run_attempt': doc['revision']['run_attempt']}
    reports = {}

    def record(cid, ok, reason):
        reports[cid] = make_report(policy, cid, commit, passed=bool(ok), reason=reason,
                                  scope=scope, artifacts=[entry['path'] for entry in doc['files'].values()])

    def text_has(key, marker):
        if key not in paths:
            return False
        text = paths[key].read_text(encoding='utf-8-sig')
        return marker in text and 'FAIL:' not in text and 'BLOCKED:' not in text

    if kind == 'assembly':
        ok = all(outcomes.get(key) == 'success' for key in ('assemble', 'source', 'upload'))
        ok = ok and all(key in paths for key in ('candidate', 'sums', 'manifest'))
        if ok:
            source = gate.load_json(paths['candidate'])
            manifest = gate.load_json(paths['manifest'])
            ok = (source.get('revision') == commit and source.get('runId') == doc['revision']['run_id']
                  and source.get('published') is False and source.get('version') == policy['release_scope']['version']
                  and source.get('tag') == 'v' + source.get('version', '')
                  and manifest.get('version') == source.get('version') and manifest.get('schema') == 1)
            lines = paths['sums'].read_text().splitlines()
            ok = ok and len(lines) == 5 and all(re.fullmatch(r'[0-9a-f]{64}  \S+', line) for line in lines)
            if ok:
                sums = {line.split()[1]: line.split()[0] for line in lines}
                prefix = 'SpectralForge-Chimera-' + source['version'] + '-'
                suffixes = ('win64-Setup.exe', 'win64.zip', 'macos-universal.pkg', 'linux-x86_64.deb', 'linux-x86_64.tar.gz')
                ok = set(sums) == {prefix + suffix for suffix in suffixes}
                expected_assets = {'windows': 'win64-Setup.exe', 'macos': 'macos-universal.pkg', 'linux': 'linux-x86_64.deb'}
                assets = manifest.get('assets')
                ok = ok and isinstance(assets, list) and len(assets) == 3
                if ok:
                    ok = {a.get('platform') for a in assets} == set(expected_assets) and all(
                        a.get('name') == prefix + expected_assets.get(a.get('platform'), '')
                        and a.get('sha256') == sums.get(a.get('name'))
                        and type(a.get('size')) is int and a['size'] > 0 for a in assets)
        record('A13.07', ok, 'Candidate assembly/upload or exact-source candidate receipt missing/failed')
        return reports

    cases, matrix_ok = {}, False
    if 'ctest' in paths and 'inventory' in paths:
        cases, matrix_ok = test_results(paths['ctest'], paths['inventory'])
    ctest_ok = outcomes.get('ctest') == 'success'
    for cid, spec in mapping.items():
        if spec['platform'] not in ('all', kind):
            continue
        owner = spec['producer']
        if owner.startswith('ctest:'):
            record(cid, ctest_ok and cases.get(owner.partition(':')[2], False),
                   'CTest did not run/pass, or its required case is missing/failed/skipped')
        elif owner.startswith('ctest-contract:'):
            record(cid, ctest_ok and matrix_ok and 'dsp_log' in paths and
                   contract_passed(spec, kind, cases, paths['dsp_log'].read_text(encoding='utf-8-sig')),
                   'Missing/failed CTest inventory, case or scoped full-log contract assertion')
        elif owner == 'workflow:build/' + kind:
            package_ok = outcomes.get('package') == 'success'
            if kind != 'windows':
                package_ok = package_ok and text_has(kind + '_package', 'PASS:')
            record(cid, ctest_ok and matrix_ok and 'dsp_log' in paths and package_ok,
                   'Incomplete/failed CTest matrix, DSP log or platform package verification')
    if kind == 'windows':
        installed = gate.load_json(paths['installer']) if 'installer' in paths else {}
        ok = (outcomes.get('installer') == 'success' and installed.get('success') is True
              and installed.get('source_sha') == commit
              and installed.get('run_id') == doc['revision']['run_id']
              and installed.get('version') == policy['release_scope']['version']
              and isinstance(installed.get('checks'), list) and bool(installed['checks'])
              and all(isinstance(c, str) and c.startswith('PASS:') for c in installed['checks'])
              and text_has('installer_log', 'PASS:'))
        record('A13.04', ok, 'Installer install/repair/uninstall evidence missing, failed or stale')
        record('A13.05', outcomes.get('msix') == 'success' and
               text_has('msix', 'PASS: Uninstall removes only the CI package registration'),
               'MSIX execution/completion evidence missing or failed')
        inventory = gate.load_json(paths['scan_inventory']) if 'scan_inventory' in paths else None
        status = gate.load_json(paths['scan_status']) if 'scan_status' in paths else None
        scan_metadata_ok = (isinstance(inventory, list) and bool(inventory) and
            all(isinstance(row, dict) and isinstance(row.get('Path'), str) and bool(row['Path'])
                and type(row.get('Bytes')) is int and row['Bytes'] >= 0
                and isinstance(row.get('SHA256'), str) and re.fullmatch('[0-9a-f]{64}', row['SHA256'])
                for row in inventory) and isinstance(status, dict) and status.get('AMServiceEnabled') is True
            and isinstance(status.get('AMEngineVersion'), str) and bool(status['AMEngineVersion'])
            and isinstance(status.get('AntivirusSignatureVersion'), str) and bool(status['AntivirusSignatureVersion']))
        record('A13.06', scan_metadata_ok and outcomes.get('scan') == 'success' and
               text_has('scan', 'PASS: completed local Defender inspection.') and
               text_has('scan', 'Source revision: ' + commit) and
               all(k in paths for k in ('scan_inventory', 'scan_status')),
               'Defender execution/completion evidence missing, stale or failed')
    return reports
