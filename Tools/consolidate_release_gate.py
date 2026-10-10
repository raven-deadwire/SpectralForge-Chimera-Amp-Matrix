#!/usr/bin/env python3
"""Consolidate one same-source/run/attempt CI bundle per required producer.

BLOCKED is a valid, publish-ineligible result, not a reason to hide the artifact.
Invalid bundles also produce a BLOCKED verdict and an infrastructure failure.
"""
from __future__ import annotations
import argparse
import json
from pathlib import Path
import shutil
import subprocess
import xml.etree.ElementTree as ET

import release_evidence as evidence
import validation_inventory

gate = evidence.gate


def consolidate(inputs, output, revision, profile):
    policy, mapping = evidence.configuration()
    gate.require(profile == evidence.PROFILE and policy['release_scope']['profile'] == profile,
                 'Consolidator must use the intended beta_1_3 release profile')
    gate.require(not output.exists(), 'Use a fresh output directory; refusing stale gate reuse')
    output.mkdir(parents=True)
    checks_dir = output / 'checks'
    checks_dir.mkdir()
    reports = {}
    for stage in policy['profiles'][profile]['required_stages']:
        for cid in policy['stages'][stage]['required_checks']:
            reports[cid] = evidence.make_report(policy, cid, revision['commit_sha'],
                reason='No concrete evidence for this source; manual/unowned validation remains unexecuted',
                scope={'producer': 'unclaimed-baseline', 'profile': profile})
    received = {}
    errors = []
    provenance = []
    for kind in evidence.KINDS:
        folder = inputs / (evidence.BUNDLE_PREFIX + kind)
        try:
            doc, paths = evidence.read_bundle(folder, kind, revision, policy)
            received[kind] = evidence.reports_for_bundle(doc, paths, policy, mapping)
            # Keep the original hashed bundle, not only the derived verdict.
            shutil.copytree(folder, output / 'evidence' / kind)
            provenance.append({'kind': kind, 'manifest_sha256': evidence.digest(folder / 'evidence.json'),
                               'revision': doc['revision']})
        except (gate.ValidationError, OSError, ValueError, TypeError, KeyError, AttributeError, ET.ParseError) as exc:
            errors.append({'kind': kind, 'reason': str(exc)})
            received[kind] = {}
    expected_folders = {evidence.BUNDLE_PREFIX + k for k in evidence.KINDS}
    if inputs.exists() and {p.name for p in inputs.iterdir()} - expected_folders:
        errors.append({'kind': 'unexpected', 'reason': 'Unexpected/duplicate evidence bundle input'})
    # Ownership determines the required platform set. Never last-writer-wins.
    for cid, spec in mapping.items():
        if cid not in reports:
            continue
        owners = (['assembly'] if spec['producer'] == 'workflow:assemble-candidate' else
                  list(evidence.PLATFORMS) if spec['platform'] == 'all' else [spec['platform']])
        claims = [received[k].get(cid) for k in owners]
        ok = all(c and c['execution']['exit_code'] == 0 and c['assertion']['passed'] is True for c in claims)
        failed = [k for k, c in zip(owners, claims) if not c or c['assertion']['passed'] is not True]
        reports[cid] = evidence.make_report(policy, cid, revision['commit_sha'], passed=ok,
            reason='' if ok else 'Missing/failed evidence from: ' + ', '.join(failed),
            scope={'producer': spec['producer'], 'platforms': owners, 'synthetic_only': True,
                   'run_id': revision['run_id'], 'run_attempt': revision['run_attempt']},
            artifacts=['evidence/' + k + '/evidence.json' for k in owners if k in {p['kind'] for p in provenance}])
    if errors:
        # Even an unexpected bundle must prevent a future otherwise-complete PASS.
        reports['A13.07'] = evidence.make_report(policy, 'A13.07', revision['commit_sha'], passed=False,
            reason='Evidence infrastructure error: ' + '; '.join(e['kind'] + ': ' + e['reason'] for e in errors))
    for cid, report in reports.items():
        (checks_dir / (cid + '.json')).write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    result = gate.evaluate_release(policy, gate.load_json(evidence.ROOT / 'Validation/waivers.json'),
                                   gate.read_checks(checks_dir), profile, revision['commit_sha'])
    result['revision'] = revision
    result['producer'] = {'name': 'consolidate_release_gate', 'inputs': provenance, 'errors': errors}
    (output / 'release-gate.json').write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
    (output / 'release-gate.md').write_text(gate.render_summary(result), encoding='utf-8')
    validation_inventory.write_inventory(output,
        validation_inventory.inventory(policy, mapping, revision['commit_sha'], profile, result))
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--commit', required=True)
    parser.add_argument('--run-id', required=True)
    parser.add_argument('--run-attempt', required=True)
    parser.add_argument('--profile', required=True)
    parser.add_argument('--inputs', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    actual = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=evidence.ROOT, text=True).strip()
    gate.require(actual == args.commit, 'Consolidator source must be checked-out HEAD')
    result = consolidate(args.inputs, args.output,
        evidence.identity(args.commit, args.run_id, args.run_attempt), args.profile)
    print(gate.render_summary(result))
    return 2 if result['producer']['errors'] else 0


if __name__ == '__main__':
    raise SystemExit(main())

