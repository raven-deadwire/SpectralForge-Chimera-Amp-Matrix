#!/usr/bin/env python3
"""Audit immutable CI evidence and explain remaining acceptance work.

This diagnostic never issues check receipts, waivers or publication approval.
Its successful exit means the audit completed, not that a release is eligible.
Policies are read from the audited Git source, not from a newer working tree.
"""
from __future__ import annotations
import argparse
from collections import Counter
import csv
import hashlib
import json
from pathlib import Path, PurePosixPath
import subprocess
import zipfile

import evaluate_release_gate as gate
import release_evidence as evidence
import verify_publication_evidence as verification

ROOT = Path(__file__).resolve().parents[1]
NEXT = {
    'ORIGINAL_DEFINITION_MISSING': 'Recover the original per-ID assertion or review an explicit replacement specification; map existing evidence before running only uncovered tests. No automatic PASS or N/A.',
    'INPUT_REFERENCE_EVIDENCE_UNCONNECTED': 'Identify authorized DI/reference inputs and SHA-256, connect a source-bound producer, then execute missing measurement/render/roundtrip assertions.',
    'INDIVIDUAL_RECEIPT_MISSING': 'Run the named listening, actual DAW/device or soak acceptance on the final approved-date binary and record source, package hash, environment, observations and reviewer.',
    'COMPOSITE_REVIEW_GAP': 'Complete all mandatory evidence and obtain separate source/version/package-specific release approval.',
}
SUPPORT = {
    'A1': 'A13.04/.07: Windows install/repair/uninstall and three-platform package/assembly evidence. Beta.1 installed upgrade exists; beta.2 source-state restore is separate from beta.2 installed upgrade.',
    'A2': 'A12.01 and I1.STATE_GAIN_GR: factory/signature regression and full-state integration; original per-ID assertions are unavailable.',
    'A3': 'E1.LEGACY, I1.STATE_GAIN_GR and lifecycle native state/host-ID/A-B/IR tests; actual DAW projects remain separate.',
    'A4': 'A12.01, E1.LEGACY: factory/legacy regression. Reconstructed beta.2 state restoration is not an installed DAW project.',
    'A5': 'A12.04: generic IR import/integrity regression. Historical private Hartke-specific assertions are undefined; do not infer applicability or rights.',
    'A6': 'A12.04: generic IR import/integrity regression. Historical private Marshall-specific assertions are undefined; do not infer applicability or rights.',
    'A7': 'A12.04 and package checks: factory hashes/import metadata; this does not establish private capture provenance or redistribution rights.',
    'A8': 'A12.04: generic integrity/fallback regression. Current factory modeled-CAB behavior differs from historical private-file targets.',
    'A9': 'Tools/evaluate_signature_benchmark.py and its calculator regressions exist; calculator tests do not prove musical benchmark acceptance.',
    'A10': 'Validation/signature-benchmark-policy.json defines score rules; per-ID A10 assertions and source-bound measured results remain absent.',
    'A11': 'Benchmark JSON evaluator/contracts exist; valid schema alone cannot establish real DI/reference measurements.',
    'A14': 'Three-OS document-upgrade fixtures and package document preservation exist; packaging checks do not establish all manual/spec content assertions.',
    'B10': 'I1.STATE_GAIN_GR and lifecycle state/IR recall provide partial automated coverage. Same-DI audio roundtrip acceptance must be explicitly connected.',
    'B11': 'Windows loader, installed app, native VST3 lifecycle CI pass. Actual DAW/device acceptance remains unexecuted.',
    'B12': 'Reconstructed released-source states are covered; actual Studio One 8 project/automation/bounce/reopen acceptance is absent.',
    'B13': 'Bounded CI stress/timing checks exist; they do not prove a 30-minute actual device/host soak.',
    'B14': 'CI is successful but strict gate, expiry approval and final release approval remain incomplete.',
    'E2': 'E1 implementation/state tests pass; verified reference, calibration and same-DI tonal acceptance remain separate.',
    'I2': 'I1 synthetic integration passes; real input, UI/audio timing and live removal in actual DAWs remain separate.',
}


def parse(data):
    return json.loads(data.decode('utf-8-sig'), object_pairs_hook=gate.unique_object,
                      parse_constant=gate.invalid_constant)


def source_json(source, path):
    return parse(subprocess.check_output(['git', 'show', source + ':' + path], cwd=ROOT))


def audit(archive, snapshot, policy, mapping, publication_policy, waivers):
    artifact = snapshot['gate_artifact']
    source = snapshot['pr62']['head_sha']
    gate.require(artifact['digest'] == 'sha256:' + hashlib.sha256(archive.read_bytes()).hexdigest(),
                 'Archive differs from the GitHub artifact digest')
    gate.require(artifact['workflow_run']['head_sha'] == source, 'Artifact source differs from selected PR')
    run = artifact['workflow_run']['id']
    runs = [r for r in snapshot['runs'] if r['id'] == run]
    gate.require(len(runs) == 1 and runs[0]['head_sha'] == source
                 and runs[0]['conclusion'] == 'success' and runs[0]['status'] == 'completed',
                 'No completed successful exact-source artifact workflow')
    revision = evidence.identity(source, run, runs[0]['run_attempt'])
    with zipfile.ZipFile(archive) as bundle:
        names = bundle.namelist()
        gate_names = [n for n in names if PurePosixPath(n).name == 'release-gate.json']
        gate.require(len(gate_names) == 1 and len(names) == len(set(names)), 'Duplicate or missing gate')
        gate.require(sum(i.file_size for i in bundle.infolist()) <= verification.MAX_ARCHIVE_BYTES,
                     'Oversized evidence archive')
        verdict = parse(bundle.read(gate_names[0]))
        gate.require(verdict['revision'] == revision, 'Gate source/run/attempt differs')
        root = PurePosixPath(gate_names[0]).parent
        checks = {}
        for name in names:
            p = PurePosixPath(name)
            if p.parent == root / 'checks' and p.suffix == '.json':
                check = parse(bundle.read(name))
                gate.validate_check(check, name)
                gate.require(check['id'] not in checks and p.stem == check['id'], 'Duplicate/misnamed check')
                checks[check['id']] = check
        verification.verify_embedded_evidence(bundle, verdict, policy, checks, source, run,
            revision['run_attempt'], profile=verdict['profile'], ci_check_map=mapping)
        actual = gate.evaluate_release(policy, waivers, checks, verdict['profile'], source)
        for key in ('checks', 'counts', 'hard_gates', 'verdict', 'ready'):
            gate.require(actual[key] == verdict[key], 'Archived gate differs from recomputed ' + key)
        configured = {}
        for platform in evidence.PLATFORMS:
            inventory = parse(bundle.read(str(root / 'evidence' / platform / 'raw/ctest-inventory.json')))
            configured[platform] = len(inventory['tests'])
    gaps = publication_policy['known_validation_gaps']
    gap_map = {g['id']: g for g in gaps}
    blocked = {c['id'] for c in actual['checks'] if c['computed_status'] == 'BLOCKED'}
    gate.require(len(gaps) == len(gap_map) and set(gap_map) == blocked,
                 'Gap inventory must match the audited blocked checks exactly')
    rows = []
    for c in actual['checks']:
        if c['id'] not in blocked:
            continue
        gap = gap_map[c['id']]
        category = gap['gap_type']
        gate.require(category in NEXT, 'Unknown gap category')
        rows.append({'id': c['id'], 'stage': c['stage'], 'stage_name': gap['stage_name'],
            'strict_hard_gate': c['id'] in policy['hard_gates'], 'strict_status': c['computed_status'],
            'original_reason': c['reason_code'], 'triage_category': category,
            'evidence_support': SUPPORT.get(c['stage'], 'No complete source-bound input/reference acceptance receipt is connected.'),
            'next_action': NEXT[category]})
    return {'schema': 'spectralforge.chimera.acceptance-triage.v1',
        'checked_at_utc': snapshot['checked_at_utc'], 'audited_revision': revision,
        'artifact_id': artifact['id'], 'artifact_digest': artifact['digest'],
        'artifact_workflow_url': runs[0]['html_url'],
        'diagnostic_only': True, 'publication_authorized': False,
        'counts': actual['counts']['checks'], 'strict_verdict': actual['verdict'],
        'reused_ci_check_ids': sorted(mapping['checks']), 'new_strict_passes': 0,
        'configured_ctest_counts': configured,
        'gap_counts': dict(sorted(Counter(r['triage_category'] for r in rows).items())),
        'rows': rows}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--archive', type=Path, required=True)
    parser.add_argument('--snapshot', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    snapshot = gate.load_json(args.snapshot)
    source = snapshot['pr62']['head_sha']
    evidence.identity(source, 1, 1)  # Validate before constructing a Git object expression.
    result = audit(args.archive, snapshot,
        source_json(source, 'Validation/release-policy.json'),
        source_json(source, 'Validation/ci-check-map.json'),
        source_json(source, 'Validation/weekly-publication-policy.json'),
        source_json(source, 'Validation/waivers.json'))
    args.output.mkdir(parents=True, exist_ok=False)
    (args.output / 'triage.json').write_text(json.dumps(result, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    with (args.output / 'blocked-checks.csv').open('w', encoding='utf-8', newline='') as f:
        writer = csv.DictWriter(f, fieldnames=list(result['rows'][0]))
        writer.writeheader()
        writer.writerows(result['rows'])
    print(json.dumps({k: v for k, v in result.items() if k not in ('rows', 'reused_ci_check_ids')}, indent=2))
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
