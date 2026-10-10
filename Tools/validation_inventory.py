#!/usr/bin/env python3
"""Describe every required check without inventing missing assertion ownership.

This is a diagnostic sidecar, never an alternative release evaluator.
"""
from collections import Counter
import csv
import hashlib
import json
from pathlib import Path
import re

import evaluate_release_gate as gate

ROOT = Path(__file__).resolve().parents[1]
REGISTRY = ROOT / 'Validation/legacy-check-recovery.json'


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def inventory(policy, mapping, commit, profile='beta_1_3', result=None):
    gate.require(re.fullmatch('[0-9a-f]{40}', commit) is not None, 'Full source SHA required')
    registry = gate.load_json(REGISTRY)
    gate.require(registry['policy_version'] == policy['policy_version'], 'Stale recovery registry')
    gate.require(registry['numbered_definitions_recovered'] == 0, 'Numbered definitions require separate reviewed ownership')
    legacy = registry['legacy_stages']
    external = registry['external_checks']
    required = {cid: stage for stage in policy['profiles'][profile]['required_stages']
                for cid in policy['stages'][stage]['required_checks']}
    legacy_ids = {cid for stage, spec in legacy.items()
                  for cid in policy['stages'][stage]['required_checks']}
    gate.require(all(len(policy['stages'][s]['required_checks']) == spec['count'] for s, spec in legacy.items()),
                 'Legacy required inventory changed; audit must be reviewed')
    gate.require(len(legacy_ids) == 86, 'Legacy inventory must retain all 86 unresolved IDs')
    gate.require(not (legacy_ids & set(mapping) or set(external) & set(mapping) or legacy_ids & set(external)),
                 'Recovery categories overlap; undefined/external check cannot acquire a producer here')
    gate.require(set(required) == legacy_ids | set(external) | set(mapping), 'Unclassified or unexpected required ID')
    observed = {}
    if result is not None:
        gate.require(result['profile'] == profile and result['revision']['commit_sha'] == commit,
                     'Inventory result source/profile mismatch')
        observed = {row['id']: row['computed_status'] for row in result['checks']}
        gate.require(len(observed) == len(result['checks']) and set(observed) == set(required),
                     'Incomplete/duplicate result inventory')
    rows = []
    for cid, stage in required.items():
        row = {'id': cid, 'stage': stage, 'stage_purpose': policy['stages'][stage]['name'],
               'hard_gate': cid in policy['hard_gates'], 'producer': None,
               'status': observed.get(cid, 'BLOCKED')}
        if cid in mapping:
            spec = mapping[cid]
            row.update(classification='ci_automatic', definition_status='reviewed_ci_contract',
                       assertion=spec['name'], producer=spec['producer'], process=spec['platform'],
                       required_input=spec['artifact'], evidence_paths=['Validation/ci-check-map.json'],
                       gap='Current source/run/attempt evidence must pass on every owning platform')
        elif stage in legacy:
            spec = legacy[stage]
            row.update(classification='definition_missing', definition_status='stage_only', assertion=None,
                       process=None, process_candidate=spec['process_candidate'], required_input='Original numbered assertion and ownership record',
                       evidence_paths=spec['evidence_paths'], historical_commits=spec['history'], gap=spec['gap'],
                       number_origin=registry['number_origin'], stage_origin=registry['stage_origin'])
            gate.require(row['status'] == 'BLOCKED', f'Undefined check was promoted: {cid}')
        else:
            category, process, inputs = external[cid]
            row.update(classification=category, definition_status='external_acceptance_required',
                       assertion=policy['stages'][stage]['name'], process=process,
                       required_input=inputs, evidence_paths=['Validation/release-policy.json'],
                       gap='Execution and source-bound external evidence are absent from automatic producers')
            gate.require(row['status'] == 'BLOCKED', f'Unowned external check was promoted: {cid}')
        rows.append(row)
    return {'schema': 'spectralforge.chimera.validation-inventory', 'schema_version': 1,
            'source_sha': commit, 'profile': profile, 'policy_version': policy['policy_version'],
            'policy_sha256': sha256(ROOT / 'Validation/release-policy.json'),
            'ownership_sha256': sha256(ROOT / 'Validation/ci-check-map.json'),
            'registry_sha256': sha256(REGISTRY),
            'result_scope': 'consolidated_product_evidence' if result is not None else 'unexecuted_inventory_only',
            'revision': result['revision'] if result is not None else {'commit_sha': commit},
            'counts': dict(Counter(row['classification'] for row in rows)),
            'deferred_checks': policy['release_scope']['deferred_checks'], 'checks': rows}


def write_inventory(output, doc):
    output.mkdir(parents=True, exist_ok=True)
    (output / 'validation-inventory.json').write_text(json.dumps(doc, indent=2) + '\n', encoding='utf-8')
    fields = ['id', 'stage', 'stage_purpose', 'hard_gate', 'classification', 'definition_status',
              'assertion', 'producer', 'process', 'process_candidate', 'status', 'required_input', 'gap']
    with (output / 'validation-inventory.csv').open('w', encoding='utf-8', newline='') as stream:
        writer = csv.DictWriter(stream, fieldnames=fields, extrasaction='ignore', lineterminator='\n')
        writer.writeheader()
        writer.writerows(doc['checks'])
