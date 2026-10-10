#!/usr/bin/env python3
"""Negative contracts for inventory separation and actual source-bound execution."""
import copy
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

import evaluate_release_gate as gate
import produce_recovered_evidence as producer
import validation_inventory as inventory

ROOT = Path(__file__).resolve().parents[1]


class InventoryTests(unittest.TestCase):
    def setUp(self):
        self.policy = gate.load_json(ROOT / 'Validation/release-policy.json')
        self.mapping = gate.load_json(ROOT / 'Validation/ci-check-map.json')['checks']

    def test_complete_unchanged_inventory(self):
        doc = inventory.inventory(self.policy, self.mapping, 'a' * 40)
        self.assertEqual(len(doc['checks']), 128)
        self.assertEqual(sum(r['hard_gate'] for r in doc['checks']), 63)
        self.assertEqual(doc['counts'], {'definition_missing': 86, 'ci_automatic': 21,
                         'automated_with_external_input': 13, 'manual_acceptance': 7, 'release_acceptance': 1})
        for row in doc['checks']:
            self.assertEqual(row['status'], 'BLOCKED')
            if row['classification'] == 'definition_missing':
                self.assertIsNone(row['assertion'])
                self.assertIsNone(row['producer'])
                self.assertIsNone(row['process'])
        self.assertEqual(doc['deferred_checks'], ['I2.PITCH_LIVE', 'I2.PITCH_DI'])

    def test_undefined_or_external_ownership_cannot_be_added(self):
        for cid in ('A1.01', 'A9.01', 'B8.RESULT', 'I2.LIVE_REMOVE_DAW'):
            with self.subTest(cid=cid):
                mapping = copy.deepcopy(self.mapping)
                mapping[cid] = mapping['A12.01']
                with self.assertRaisesRegex(gate.ValidationError, 'overlap'):
                    inventory.inventory(self.policy, mapping, 'a' * 40)

    def test_missing_or_narrowed_inventory_is_rejected(self):
        del self.mapping['A12.01']
        with self.assertRaisesRegex(gate.ValidationError, 'Unclassified'):
            inventory.inventory(self.policy, self.mapping, 'a' * 40)
        self.policy['stages']['A1']['required_checks'].pop()
        with self.assertRaisesRegex(gate.ValidationError, 'changed'):
            inventory.inventory(self.policy, self.mapping, 'a' * 40)

    def test_stale_or_forged_result_rejected(self):
        original = inventory.inventory(self.policy, self.mapping, 'a' * 40)
        result = {'profile': 'beta_1_3', 'revision': {'commit_sha': 'b' * 40},
                  'checks': [{'id': r['id'], 'computed_status': 'BLOCKED'} for r in original['checks']]}
        with self.assertRaisesRegex(gate.ValidationError, 'mismatch'):
            inventory.inventory(self.policy, self.mapping, 'a' * 40, result=result)
        result['revision']['commit_sha'] = 'a' * 40
        result['checks'][0]['computed_status'] = 'PASS'
        with self.assertRaisesRegex(gate.ValidationError, 'promoted'):
            inventory.inventory(self.policy, self.mapping, 'a' * 40, result=result)

    def test_missing_skipped_duplicate_or_failed_cases_rejected(self):
        doc = {'schema': 'spectralforge.chimera.calculator-contracts', 'schema_version': 1,
               'synthetic_only': True, 'policy_claims': [], 'tests_run': len(producer.contracts.case_names()),
               'failures': 0, 'errors': 0, 'skipped': 0,
               'cases': [{'name': n, 'status': 'PASS'} for n in producer.contracts.case_names()]}
        self.assertTrue(producer.successful_cases(doc))
        changes = [lambda x: x['cases'].pop(), lambda x: x.update(skipped=1),
                   lambda x: x.update(failures=1), lambda x: x.update(errors=1),
                   lambda x: x['cases'].__setitem__(0, x['cases'][1]),
                   lambda x: x['cases'][0].update(status='BLOCKED'),
                   lambda x: x.update(policy_claims=['A9.01']), lambda x: x.update(tests_run=True)]
        for change in changes:
            altered = copy.deepcopy(doc)
            change(altered)
            self.assertFalse(producer.successful_cases(altered))


class ExecutionTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        for name in producer.INPUTS:
            target = self.root / name
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(ROOT / name, target)
        (self.root / '.gitignore').write_text('/build/\n__pycache__/\n*.pyc\n', encoding='utf-8')
        self.command('git', 'init', '-q')
        self.commit()

    def command(self, *args):
        return subprocess.check_output(args, cwd=self.root, text=True, stderr=subprocess.STDOUT).strip()

    def commit(self):
        self.command('git', 'add', '.')
        self.command('git', '-c', 'user.name=Contract Fixture', '-c', 'user.email=fixture@example.invalid',
                     'commit', '-qm', 'Synthetic recovery contract source')
        self.sha = self.command('git', 'rev-parse', 'HEAD')

    def run_producer(self, *args, commit=None):
        return subprocess.run([sys.executable, 'Tools/produce_recovered_evidence.py', '--commit', commit or self.sha,
             '--output', str(self.root / 'build/recovery'), *args], cwd=self.root, text=True,
             stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=30)

    def test_actual_execution_hashes_and_no_policy_claims(self):
        result = self.run_producer()
        self.assertEqual(result.returncode, 0, result.stdout)
        folder = self.root / 'build/recovery'
        doc = gate.load_json(folder / 'recovery-evidence.json')
        self.assertEqual(doc['revision']['commit_sha'], self.sha)
        self.assertEqual(doc['execution_environment'], 'local')
        self.assertEqual(doc['component_status'], 'PASS')
        self.assertEqual(doc['release_verdict'], 'BLOCKED')
        self.assertEqual(doc['policy_claims'], [])
        self.assertTrue(all(r['executed'] and r['exit_code'] == 0 for r in doc['commands']))
        for name, digest in doc['files'].items():
            self.assertEqual(inventory.sha256(folder / name), digest)
        self.assertTrue(producer.successful_cases(gate.load_json(folder / 'cases.json')))
        self.assertEqual(self.run_producer().returncode, 2)  # no stale reuse

    def test_dirty_or_stale_source_rejected(self):
        self.assertEqual(self.run_producer(commit='b' * 40).returncode, 2)
        with (self.root / 'Tools/evaluate_signature_benchmark.py').open('a', encoding='utf-8') as stream:
            stream.write('\n# uncommitted mutation\n')
        result = self.run_producer()
        self.assertEqual(result.returncode, 2)
        self.assertIn('clean checkout', result.stdout)

    def test_false_success_without_cases_rejected(self):
        path = self.root / 'Tools/test_signature_calculator_contracts.py'
        with path.open('a', encoding='utf-8') as stream:
            stream.write('\n')
        # Keep import-time definitions, suppress only execution in the child.
        path.write_text(path.read_text(encoding='utf-8').replace('raise SystemExit(main())', 'raise SystemExit(0)'), encoding='utf-8')
        self.commit()
        result = self.run_producer()
        self.assertEqual(result.returncode, 1, result.stdout)
        doc = gate.load_json(self.root / 'build/recovery/recovery-evidence.json')
        self.assertEqual(doc['component_status'], 'BLOCKED')

    def test_failed_child_is_recorded_and_blocks(self):
        (self.root / 'Tools/test_signature_benchmark.py').write_text('raise SystemExit(23)\n', encoding='utf-8')
        self.commit()
        result = self.run_producer()
        self.assertEqual(result.returncode, 1, result.stdout)
        doc = gate.load_json(self.root / 'build/recovery/recovery-evidence.json')
        self.assertEqual(doc['commands'][0]['exit_code'], 23)
        self.assertEqual(doc['component_status'], 'BLOCKED')

    def test_legacy_exit_zero_without_completion_blocks(self):
        (self.root / 'Tools/test_signature_benchmark.py').write_text('raise SystemExit(0)\n', encoding='utf-8')
        self.commit()
        result = self.run_producer()
        self.assertEqual(result.returncode, 1, result.stdout)
        doc = gate.load_json(self.root / 'build/recovery/recovery-evidence.json')
        self.assertFalse(doc['legacy_completion_assertion'])
        self.assertEqual(doc['component_status'], 'BLOCKED')

    def test_fake_ci_identity_rejected(self):
        result = self.run_producer('--run-id', '123', '--run-attempt', '1')
        self.assertEqual(result.returncode, 2, result.stdout)
        self.assertIn('CI identity', result.stdout)


if __name__ == '__main__':
    unittest.main()
