#!/usr/bin/env python3
"""Compare complete normal JSON outputs with PR89's frozen evaluator."""
import argparse
import ast
import copy
import hashlib
import json
from pathlib import Path
import random
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / 'Tools'))
import evaluate_signature_benchmark as current

BASE = 'd27e0f549e1115d4ae7d918af0e7ea26caa1584d'
PATH = 'Tools/evaluate_signature_benchmark.py'


def run():
    original = subprocess.check_output(['git', 'show', f'{BASE}:{PATH}'], cwd=ROOT)
    baseline = {'__name__': 'frozen_signature_evaluator'}
    exec(compile(original, f'{BASE}:{PATH}', 'exec'), baseline)
    # The scoring function itself and the frozen policy must be unchanged.
    def scoring(source):
        return ast.dump(next(n for n in ast.parse(source).body
                             if isinstance(n, ast.FunctionDef) and n.name == 'evaluate'))
    assert scoring(original) == scoring((ROOT / PATH).read_bytes())
    policy_bytes = (ROOT / 'Validation/signature-benchmark-policy.json').read_bytes()
    assert policy_bytes == subprocess.check_output(
        ['git', 'show', f'{BASE}:Validation/signature-benchmark-policy.json'], cwd=ROOT)
    policy = json.loads(policy_bytes)
    reference = {'schema': 'spectralforge.chimera.signature-renders', 'schema_version': 1,
                 'dsp_valid': True, 'renders': copy.deepcopy(policy['reference'])}
    revised = copy.deepcopy(reference)
    revised['renders']['wild']['crest_db'] = 9.0
    fixtures = [reference, revised, dict(reference, dsp_valid=False, renders={})]
    for song in current.SONGS:
        for metric in policy['metrics']:
            for number in (0, 1, -7, 0.0, 1.0, -3.25, 12.5):
                data = copy.deepcopy(reference)
                data['renders'][song][metric] = number
                fixtures.append(data)
    for bias in (-10, -3, 0, 3, 10):
        data = copy.deepcopy(reference)
        for values in data['renders'].values():
            for metric in values:
                values[metric] += bias
        fixtures.append(data)
    rng = random.Random(20261016)
    for _ in range(256):
        data = copy.deepcopy(reference)
        for values in data['renders'].values():
            for metric in values:
                values[metric] += rng.uniform(-5, 5)
        fixtures.append(data)
    counts = {}
    hashes = []
    for data in fixtures:
        before = json.dumps(baseline['evaluate'](policy, data), indent=2, allow_nan=False) + '\n'
        result = current.evaluate(policy, data)
        after = json.dumps(result, indent=2, allow_nan=False) + '\n'
        assert before == after
        counts[result['verdict']] = counts.get(result['verdict'], 0) + 1
        hashes.append(hashlib.sha256(after.encode()).hexdigest())
    return {'baseline_commit': BASE, 'source_sha256': hashlib.sha256((ROOT / PATH).read_bytes()).hexdigest(),
            'policy_sha256': hashlib.sha256(policy_bytes).hexdigest(),
            'evaluate_ast_identical': True, 'policy_byte_identical': True,
            'complete_json_byte_identical': len(fixtures), 'verdict_counts': counts,
            'result_sha256': hashes, 'synthetic_only': True, 'publisher_input': False,
            'policy_claims': [], 'release_approved': False}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    result = run()
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
    print(f"PASS: {result['complete_json_byte_identical']} complete JSON results byte-identical; score function and policy unchanged")
