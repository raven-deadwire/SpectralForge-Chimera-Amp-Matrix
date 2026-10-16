#!/usr/bin/env python3
"""Revalidate PR87's 39 accepted definitions from exact-source raw evidence.

This produces a scoped supplemental audit, not a release-gate override. It never
copies the historical PASS decisions or labels the audit-tool commit a product.
"""
from __future__ import annotations
import argparse
import ast
from collections import Counter
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import xml.etree.ElementTree as ET
import zipfile

import accepted_baseline as baseline
import cab_ir_acceptance as cab
import consolidate_release_gate as consolidator
import release_evidence as evidence

ROOT = Path(__file__).resolve().parents[1]
FOLDER = ROOT / 'docs/evidence/pr87-current-20261016'
PRODUCT = '72ecf35fa90293418da8635225435095dbca1135'
RUN = '38887964838'
REVISION = evidence.identity(PRODUCT, RUN, '1')
ARCHIVE_HASH = '725157890da7b9de1d9d250747ddf8f6d2ce4d70641ff04f4d75d3b5cb0ce7b3'
require = evidence.gate.require
NATIVE_PREFIXES = ('Source/', 'Assets/', 'Preparation/', 'Installer/', 'Tests/', 'cmake/')
UPGRADE_RUN = 38905833729
UPGRADE_TOOL = '584caf48efdc0de9c7ab24f592fc3a556270424e'
UPGRADE_HASH = 'eff61179f6bb00b76b32db8673f807e469c498f50eae780161248532c63cb241'
UPGRADE_IDS = {'A1.01', 'A1.03', 'A1.04', 'A1.05'}


def digest(data):
    return hashlib.sha256(data).hexdigest()


def extract(archive, destination, expected):
    require(digest(archive.read_bytes()) == expected, 'Archive differs from pinned GitHub digest')
    with zipfile.ZipFile(archive) as z:
        names = z.namelist()
        require(len(names) == len(set(names)), 'Duplicate ZIP member')
        for entry in z.infolist():
            p = Path(entry.filename)
            require(not p.is_absolute() and '..' not in p.parts and '\\' not in entry.filename
                    and not ((entry.external_attr >> 16) & 0o170000 == 0o120000), 'Unsafe ZIP member')
        z.extractall(destination)


def source_guard(registry):
    """Retain the old guard and bind every product/test/build input to PR87."""
    paths = subprocess.check_output(['git', 'ls-tree', '-r', '--name-only', PRODUCT], cwd=ROOT, text=True).splitlines()
    native = {p for p in paths if p.startswith(NATIVE_PREFIXES)} | set(registry['native_fingerprint'])
    tracked = subprocess.check_output(['git', 'ls-files', '--cached', '--others', '--exclude-standard'], cwd=ROOT, text=True).splitlines()
    require({p for p in tracked if p.startswith(NATIVE_PREFIXES)} == {p for p in paths if p.startswith(NATIVE_PREFIXES)},
            'Product input inventory changed; new native evidence required')
    fingerprints = {}
    for path in sorted(native):
        original = subprocess.check_output(['git', 'show', PRODUCT + ':' + path], cwd=ROOT)
        current = ROOT / path
        require(current.is_file() and not current.is_symlink() and current.read_bytes() == original,
                'Product input changed: ' + path)
        fingerprints[path] = digest(original)
    changed = baseline.fingerprint_changes(registry)
    require(changed == ['CMakeLists.txt', 'Tests/CabIRAcceptanceTests.cpp'], 'Original baseline guard changed')
    return {'original_baseline_input_changes': changed,
            'pr87_input_count': len(fingerprints),
            'pr87_input_fingerprint': digest(json.dumps(fingerprints, sort_keys=True).encode())}


def completed_output(name, cases, junit, log, kind):
    marker = r'OK \(skipped=[134]\)' if name == 'ChimeraPackagingVersionTests' else r'PASS.*'
    require(evidence.contract_passed({'tests': {name: [marker]}}, 'linux', cases, log),
            'Missing, failed or incomplete full CTest section: ' + name)
    sections = re.split(r'^\d+/\d+ Testing: ', log, flags=re.M)[1:]
    section = next(s for s in sections if s.split('\n', 1)[0] == name)
    output = section.split('Output:\n----------------------------------------------------------\n')[1].split('\n<end of output>\n')[0]
    matches = [c for c in junit.iter('testcase') if c.attrib['name'] == name]
    require(len(matches) == 1, 'Duplicate/missing JUnit case')
    short = matches[0].findtext('system-out', '').strip()
    truncation = '...\n[This part of the test output was removed since it exceeds the threshold of '
    require(bool(short) and (output.startswith(short.split(truncation)[0]) if truncation in short
                            else short == output.strip()), 'JUnit/full-log disagreement: ' + name)
    package_cases = {}
    if name == 'ChimeraPackagingVersionTests':
        tree = ast.parse((ROOT / 'Tests/test_packaging_version.py').read_text())
        expected = {n.name for n in ast.walk(tree) if isinstance(n, ast.FunctionDef) and n.name.startswith('test_')}
        found = re.findall(r"^(test_\w+) \(__main__\.PackagingVersionTests\.\w+\) \.\.\. (ok|skipped .*)$", output, re.M)
        require(len(found) == len(expected) and {n for n, _ in found} == expected, 'Packaging subcase inventory mismatch')
        windows_only = {'test_windows_builder_rejects_mixed_or_missing_payload_version',
                        'test_windows_builder_rejects_relabelled_binary', 'test_windows_juce_and_inno_resource_layouts'}
        linux_only = {'test_debian_upgrade_order_matches_semver'}
        expected_skips = (set() if kind == 'windows' else windows_only) | (set() if kind == 'linux' else linux_only)
        require({n for n, status in found if status != 'ok'} == expected_skips, 'Unexpected skipped packaging assertion')
        package_cases = dict(found)  # Skips are recorded as skips; native-owner execution is checked across platforms.
    return {'test': name, 'status': 'executed_pass', 'package_subcases': package_cases, 'output_sha256': digest(output.encode()),
            'assertions': [line for line in output.splitlines() if line.startswith(('PASS', 'MEASURE signature'))]}


def native_records(root, ledger, policy):
    results, bundles, native_counts = {}, {}, {}
    for kind in evidence.KINDS:
        folder = root / 'evidence' / kind
        doc, paths = evidence.read_bundle(folder, kind, REVISION, policy)
        bundles[kind] = (doc, paths)
        if kind == 'assembly':
            continue
        cases, ok = evidence.test_results(paths['ctest'], paths['inventory'])
        require(doc['outcomes']['ctest'] == 'success' and ok, 'Incomplete native matrix: ' + kind)
        native_counts[kind] = len(cases)
        junit = ET.parse(paths['ctest']).getroot()
        log = paths['dsp_log'].read_text(encoding='utf-8-sig')
        for key, item in ledger['evidence'].items():
            if 'records' not in item or not any(r['platform'] == kind for r in item['records']):
                continue
            record = completed_output(key[2:], cases, junit, log, kind)
            record.update(platform=kind, junit_sha256=evidence.digest(paths['ctest']),
                          full_log_sha256=evidence.digest(paths['dsp_log']))
            results.setdefault(key, []).append(record)
    for key, records in results.items():
        require({r['platform'] for r in records} == {r['platform'] for r in ledger['evidence'][key]['records']},
                'Missing required native platform')
    return results, bundles, native_counts


def install_record(bundles):
    doc, paths = bundles['windows']
    data = evidence.gate.load_json(paths['installer'])
    text = paths['installer_log'].read_text(encoding='utf-8-sig')
    expected = [
        'PASS: Full install: standard VST3 path, app, documentation, shortcuts and uninstall registration; payload hashes match',
        'PASS: Installed VST3 loads through the Windows loader and exports GetPluginFactory',
        'PASS: Independent custom application and VST3 installation folders accept Unicode paths',
        'PASS: VST3-only selection and removal; Unicode custom app path',
        'PASS: Standalone-only selection and removal; no VST3 installed',
    ]
    require(doc['outcomes']['installer'] == 'success' and data['success'] is True
            and data['source_sha'] == PRODUCT and data['run_id'] == RUN
            and data['version'] == '1.3.1-beta.3' and data['installer_version'] == '1.3.1'
            and data['build_id'] == PRODUCT[:10]
            and data['installer_sha256'] == 'ea8c8b90be574a5a0204ecd30afb7cf17ecff4856b97d3227d04cb985e12998e'
            and all(s in data['checks'] and s in text for s in expected)
            and 'FAIL:' not in text, 'Current installer receipt/log mismatch')
    return {'status': 'executed_pass', 'platform': 'windows', 'receipt_sha256': evidence.digest(paths['installer']),
            'log_sha256': evidence.digest(paths['installer_log']), 'checks': data['checks']}


def assembly_record(bundles, policy, mapping):
    doc, paths = bundles['assembly']
    claims = evidence.reports_for_bundle(doc, paths, policy, mapping)
    expected = {cid for cid, spec in mapping.items() if spec['producer'] == 'workflow:assemble-candidate'}
    require(bool(expected) and set(claims) == expected
            and all(c['assertion']['passed'] is True for c in claims.values()), 'Current assembly identity failed')
    return {'status': 'executed_pass', 'checks': sorted(claims),
            'files': {k: evidence.digest(p) for k, p in paths.items()}}


def upgrade_record(folder, metadata):
    """A different tool run may test the same pinned product; never relabel it."""
    run, artifact = metadata['run'], metadata['artifact']
    require(run['id'] == UPGRADE_RUN and run['head_sha'] == UPGRADE_TOOL and run['run_attempt'] == 1
            and artifact['id'] == 12047528479 and artifact['digest'] == 'sha256:' + UPGRADE_HASH
            and artifact['workflow_run']['head_sha'] == UPGRADE_TOOL
            and run['status'] == 'completed' and run['conclusion'] == 'success'
            and artifact['workflow_run']['id'] == run['id'] and not artifact['expired'], 'Upgrade CI incomplete/mismatched')
    data = evidence.gate.load_json(folder / 'result.json')
    tool_sha = run['head_sha']
    executed = subprocess.check_output(['git', 'show', tool_sha + ':Tools/Verify-PR87InstalledUpgrade.ps1'], cwd=ROOT)
    require(executed == (ROOT / 'Tools/Verify-PR87InstalledUpgrade.ps1').read_bytes(), 'Upgrade harness changed since execution')
    require(data['success'] is True and data['product_source_sha'] == PRODUCT
            and data['candidate_build_run'] == RUN and data['product_version'] == '1.3.1-beta.3'
            and data['tool_source_sha'] == tool_sha and data['run_id'] == str(run['id'])
            and data['run_attempt'] == str(run['run_attempt']) and data['previous_release'] == 'v1.3.1-beta.2'
            and data['installer_invocations'] == 4 and data['qualification_only'] is True
            and data['publication_approved'] is False and data['daw_project_acceptance'] is False,
            'Upgrade identity/scope mismatch')
    expected = [
        'PASS: Existing CI Setup/portable bytes, source and qualification-only lifecycle are pinned; no rebuild',
        'PASS: Actual public beta.2 Setup installed in Unicode app and VST3 paths',
        'PASS: Actual beta.2 to beta.3 same-numeric-version upgrade replaces both binaries and metadata while preserving personal sentinels and annotated manual',
        'PASS: Repair restores deliberately damaged standalone to pinned bytes and retains VST3, metadata and user data',
        'PASS: Upgraded uninstall removes owned binaries/registration and retains all personal sentinels and manual snapshot',
    ]
    require(data['checks'] == expected, 'Upgrade assertions missing/failed/duplicated')
    expected_logs = {'01-beta2-install.log', '02-beta3-upgrade.log', '03-beta3-repair.log', '04-beta3-uninstall.log'}
    require(len(data['raw_logs']) == 4 and {r['name'] for r in data['raw_logs']} == expected_logs
            and {p.name for p in folder.iterdir()} == expected_logs | {'result.json'}, 'Incomplete upgrade logs')
    for record in data['raw_logs']:
        path = folder / record['name']
        require(path.is_file() and not path.is_symlink() and path.stat().st_size > 0
                and evidence.digest(path) == record['sha256'], 'Upgrade log changed')
    return {'status': 'executed_pass', 'receipt_sha256': evidence.digest(folder / 'result.json'),
            'product_source_sha': PRODUCT, 'tool_source_sha': tool_sha,
            'run_id': data['run_id'], 'run_attempt': data['run_attempt'], 'checks': data['checks'],
            'raw_logs': data['raw_logs'], 'actual_daw_acceptance': False}


def remaining_category(row):
    cid = row['id']
    if cid in ('A5.06', 'A6.06'):
        return 'REAL_DAW_AND_PRIVATE_ASSET'
    if cid in ('A5.08', 'A6.08'):
        return 'PRIVATE_ASSET_RIGHTS_AND_LISTENING'
    if cid in ('A11.08', 'A11.09'):
        return 'MEASUREMENT_AND_REVIEW_PROVENANCE'
    if cid.startswith('A14.'):
        return 'PACKAGE_OR_LANGUAGE_REVIEW'
    return 'ADDITIONAL_NATIVE_OR_PROTOCOL_CASES'


def audit(archive, snapshot, upgrade=None, upgrade_metadata=None):
    registry, ledger = baseline.load_registry()
    source = source_guard(registry)
    run = snapshot['product_run']
    require(run['id'] == int(RUN) and run['head_sha'] == PRODUCT and run['run_attempt'] == 1
            and run['status'] == 'completed' and run['conclusion'] == 'success', 'Product run identity mismatch')
    artifact = next(a for a in snapshot['product_artifacts']['artifacts'] if a['id'] == 12045707177)
    require(artifact['digest'] == 'sha256:' + ARCHIVE_HASH and artifact['workflow_run']['head_sha'] == PRODUCT
            and artifact['workflow_run']['id'] == int(RUN), 'GitHub archive metadata mismatch')
    policy, mapping = evidence.configuration()
    with tempfile.TemporaryDirectory(prefix='pr87-recovery-') as tmp:
        root = Path(tmp) / 'product'
        extract(archive, root, ARCHIVE_HASH)
        raw_gate = evidence.gate.load_json(root / 'release-gate.json')
        require(raw_gate['revision'] == REVISION and raw_gate['verdict'] == 'BLOCKED'
                and raw_gate['counts']['checks'] == {'total': 128, 'pass': 49, 'blocked': 79, 'not_applicable': 0},
                'Original Product gate differs')
        inputs = Path(tmp) / 'inputs'
        inputs.mkdir()
        for folder in (root / 'evidence').iterdir():
            shutil.copytree(folder, inputs / (evidence.BUNDLE_PREFIX + folder.name))
        regenerated = Path(tmp) / 'regenerated'
        original = consolidator.consolidate(inputs, regenerated, REVISION, evidence.source_profile())
        require(not original['producer']['errors'] and original['counts'] == raw_gate['counts']
                and original['checks'] == raw_gate['checks'], 'Original gate does not reproduce from raw evidence')
        records, bundles, counts = native_records(root, ledger, policy)
        records['E.install'] = install_record(bundles)
        records['E.assembly'] = assembly_record(bundles, policy, mapping)
        # Distribution negatives ran in this same Product run, not in the historical baseline.
        baseline_folder = root / 'evidence/accepted-baseline'
        baseline.read_reports(baseline_folder, REVISION, policy)
        b = evidence.gate.load_json(baseline_folder / 'evidence.json')
        require(b['commands']['distribution']['exit_code'] == 0, 'Current distribution negatives failed')
        require(b['counts'] == {'total': 86, 'pass': 22, 'blocked': 64}, 'Historical evidence silently promoted')
        if upgrade is not None:
            require(upgrade_metadata is not None, 'Upgrade GitHub metadata missing')
            up = Path(tmp) / 'upgrade'
            extract(upgrade, up, UPGRADE_HASH)
            records['E.upgrade'] = upgrade_record(up, upgrade_metadata)
        rows = []
        for row in registry['rows']:
            if row['verification_kind'] not in ('historical', 'distribution'):
                continue
            missing = [key for key in row['evidence_ids'] if key not in records]
            rows.append({'id': row['id'], 'title_ko': row['title_ko'], 'definition': row['definition'],
                         'status': 'VERIFIED_CURRENT_SOURCE' if not missing else 'BLOCKED',
                         'evidence_ids': row['evidence_ids'], 'missing_evidence': missing,
                         'additional_distribution_negatives': row['verification_kind'] == 'distribution',
                         'product_source_sha': PRODUCT, 'actual_daw_acceptance': False})
        require(len(rows) == 39, 'Recovery inventory differs from the accepted 39')
        direct = {cid: all(cab.bundle_claims(*bundles[k]).get(cid) is True for k in evidence.PLATFORMS)
                  for ids in cab.SUITES.values() for cid in ids}
        require(all(direct.values()), 'PR87 direct CAB/IR case is not proven')
        remaining = [dict(id=r['id'], title_ko=r['title_ko'], category=remaining_category(r),
                          status='BLOCKED', remaining_work_ko=r['remaining_work_ko']) for r in registry['rows']
                     if r['verification_kind'] == 'blocked' and r['id'] not in direct]
        external = [c for c in raw_gate['checks'] if c['computed_status'] == 'BLOCKED'
                    and c['id'] not in {r['id'] for r in registry['rows']}]
        reports = evidence.gate.read_checks(regenerated / 'checks')
        for row in rows:
            if row['status'] == 'VERIFIED_CURRENT_SOURCE':
                reports[row['id']] = evidence.make_report(policy, row['id'], PRODUCT, passed=True,
                    scope={'producer': 'pr87-current-source-recovery', 'review_only': True,
                           'product_revision': REVISION, 'definition_sha256': baseline.REGISTRY_SHA256,
                           'evidence_ids': row['evidence_ids'], 'synthetic_only': not row['id'].startswith('A1.'),
                           'actual_daw_acceptance': False, 'publication_authorized': False,
                           'upgrade_run_id': records.get('E.upgrade', {}).get('run_id') if row['id'] in UPGRADE_IDS else None},
                    artifacts=['docs/evidence/pr87-current-20261016/product-gate-38887964838.zip'] +
                        (['docs/evidence/pr87-current-20261016/upgrade-38905833729.zip'] if row['id'] in UPGRADE_IDS else []))
        review_gate = evidence.gate.evaluate_release(policy, evidence.gate.load_json(ROOT / 'Validation/waivers.json'),
                                                     reports, evidence.source_profile(), PRODUCT)
        require(review_gate['verdict'] == 'BLOCKED' and review_gate['ready'] is False,
                'This supplemental audit cannot approve a release')
        review_gate.update(revision=REVISION, review_only=True, publisher_input=False, publication_authorized=False)
        return {'schema': 'spectralforge.chimera.pr87-recovery.v1', 'product_revision': REVISION,
                'audit_tool_sha': subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip(),
                'audit_tool_file_sha256': evidence.digest(Path(__file__)),
                'review_gate': review_gate, 'recovered_check_reports': {r['id']: reports[r['id']] for r in rows},
                'scope': 'Supplemental executed evidence only; no historical verdict transfer or release-gate mutation',
                'source_guard': source, 'definition_sha256': baseline.REGISTRY_SHA256,
                'native_ctest_counts': counts, 'recovery_counts': dict(Counter(r['status'] for r in rows)),
                'rows': rows, 'evidence': records, 'cab_ir_direct': direct,
                'distribution_raw_log_sha256': b['files']['distribution.log'],
                'remaining_accepted_ids': remaining, 'remaining_external_ids': external,
                'original_product_gate': {'verdict': raw_gate['verdict'], 'counts': raw_gate['counts']['checks']},
                'product_gate_overridden': False, 'release_approved': False, 'expiry_approved': False,
                'actual_daw_acceptance': False, 'listening_acceptance': False, 'publication_authorized': False}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--upgrade', type=Path)
    parser.add_argument('--upgrade-metadata', type=Path)
    args = parser.parse_args()
    result = audit(FOLDER / 'product-gate-38887964838.zip', evidence.gate.load_json(FOLDER / 'github-snapshot.json'),
                   args.upgrade, evidence.gate.load_json(args.upgrade_metadata) if args.upgrade_metadata else None)
    with args.output.open('x', encoding='utf-8') as f:
        json.dump(result, f, ensure_ascii=False, indent=2); f.write('\n')
    print(json.dumps({k: result[k] for k in ('product_revision', 'native_ctest_counts', 'recovery_counts', 'original_product_gate', 'publication_authorized')}, indent=2))
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
