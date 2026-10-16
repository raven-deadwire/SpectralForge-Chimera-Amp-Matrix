#!/usr/bin/env python3
"""Current-source CAB/IR assertions; synthetic component scope, never release approval."""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
BASELINE_SHA256 = '457260441cd5000c2b82abaa3b4c75d7262c5a5d9187b1615ad6e34a8938dda5'
SUITES = {
    'factory': ('A2.07', 'A5.01', 'A6.01'),
    'boundaries': ('A5.05', 'A6.05'),
    'preservation': ('A8.02',),
}
BOUNDARIES = {
    'mono_min_rate_min_frames': True, 'stereo_max_rate_one_second': True,
    'raw_exact_4MiB': True, 'positive_sample_limit': True, 'negative_sample_limit': True,
    'raw_over_4MiB': False, 'raw_under_header': False, 'corrupt': False, 'silent': False,
    'rate_below': False, 'rate_above': False, 'zero_channels': False, 'three_channels': False,
    'short_frames': False, 'over_one_second': False, 'nan_late_right': False,
    'positive_inf_late_right': False, 'negative_inf_late_right': False,
    'sample_over_positive': False, 'sample_over_negative': False,
}
MARKERS = {
    'factory': 'PASS CAB_IR A2.07 A5.01 A6.01 factory_absent_valid_corrupt',
    'boundaries': 'PASS CAB_IR A5.05 A6.05 boundaries_20',
    'preservation': 'PASS CAB_IR A8.02 rejected_load_preserves_six_slots_18',
}


def validate_definitions():
    raw = (ROOT / 'Validation/accepted-baseline.json').read_bytes()
    if hashlib.sha256(raw).hexdigest() != BASELINE_SHA256:
        raise ValueError('CAB/IR assertions require review against the changed accepted definitions')
    rows = {r['id']: r for r in json.loads(raw)['rows']}
    for ids in SUITES.values():
        for cid in ids:
            if rows[cid]['approval'] != 'APPROVED_EXISTING_BEHAVIOR':
                raise ValueError('Unapproved definition: ' + cid)


def complete_output(suite, text):
    lines = text.splitlines()
    if lines.count(MARKERS[suite]) != 1 or 'FAIL CAB_IR' in text:
        return False
    if suite == 'factory':
        rows = re.findall(r'^CASE factory preset=(\d+) phase=(\d+) files=(\d+) PASS$', text, re.M)
        return (len(rows) == 9 and {(int(p), int(s)) for p, s, _ in rows}
                == {(p, s) for p in (31, 32, 33) for s in range(3)}
                and all(int(n) == 0 if int(s) == 0 else int(n) > 0 for _, s, n in rows))
    if suite == 'boundaries':
        rows = re.findall(r'^CASE boundary (\w+) accepted=([01]) bytes=(\d+) PASS$', text, re.M)
        return len(rows) == len(BOUNDARIES) and {name: bool(int(ok)) for name, ok, _ in rows} == BOUNDARIES
    rows = re.findall(r'^CASE preserve lane=([012]) mic=([01]) rejected=(missing|corrupt|silent)\.wav PASS$', text, re.M)
    return len(rows) == 18 and set(rows) == {(str(l), str(m), f) for l in range(3) for m in range(2) for f in ('missing', 'corrupt', 'silent')}


def claims(junit, inventory):
    """Require executed JUnit cases AND all individual assertion markers, no skip/omission."""
    validate_definitions()
    result = {cid: False for ids in SUITES.values() for cid in ids}
    if not junit.is_file() or not inventory.is_file():
        return result
    names = [test['name'] for test in json.loads(inventory.read_text(encoding='utf-8-sig'))['tests']]
    cases = list(ET.parse(junit).getroot().iter('testcase'))
    case_names = [case.attrib['name'] for case in cases]
    if len(names) != len(set(names)) or len(case_names) != len(set(case_names)):
        raise ValueError('Duplicate CAB/IR CTest evidence')
    by_name = dict(zip(case_names, cases))
    for suite, ids in SUITES.items():
        name = 'ChimeraCabIRAcceptance_' + suite
        case = by_name.get(name)
        ok = (name in names and case is not None and case.attrib.get('status') == 'run'
              and all(case.find(tag) is None for tag in ('failure', 'error', 'skipped'))
              and complete_output(suite, case.findtext('system-out', '')))
        for cid in ids:
            result[cid] = ok
    return result


def bundle_claims(doc, paths):
    if doc['kind'] not in ('windows', 'linux', 'macos') or doc['outcomes'].get('ctest') != 'success':
        return {}
    if not {'ctest', 'inventory'}.issubset(paths):
        return {}
    return claims(paths['ctest'], paths['inventory'])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--commit', required=True)
    parser.add_argument('--run-id', required=True)
    parser.add_argument('--run-attempt', required=True)
    parser.add_argument('--platform', choices=('windows', 'linux', 'macos'), required=True)
    args = parser.parse_args()
    actual = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip()
    if actual != args.commit or not re.fullmatch('[0-9a-f]{40}', actual):
        raise ValueError('Evidence source differs from checkout')
    if args.output.exists():
        raise ValueError('Refusing stale output')
    junit, inventory = args.build / 'cab-ir-results.xml', args.build / 'cab-ir-inventory.json'
    verdicts = claims(junit, inventory)
    args.output.mkdir(parents=True)
    files = {}
    for path in (junit, inventory):
        if path.is_file():
            data = path.read_bytes();(args.output / path.name).write_bytes(data)
            files[path.name] = hashlib.sha256(data).hexdigest()
    result = {'schema': 'spectralforge.chimera.cab-ir-component.v1',
        'revision': {'commit_sha': actual, 'run_id': args.run_id, 'run_attempt': args.run_attempt},
        'platform': args.platform, 'definition_sha256': BASELINE_SHA256,
        'synthetic_only': True, 'publication_authorized': False, 'actual_daw_acceptance': False,
        'product_gate_result': False, 'files': files,
        'checks': {cid: 'PASS' if ok else 'BLOCKED' for cid, ok in verdicts.items()}}
    (args.output / 'component.json').write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps(result, indent=2))
    return 0 if all(verdicts.values()) else 1


if __name__ == '__main__':
    raise SystemExit(main())
