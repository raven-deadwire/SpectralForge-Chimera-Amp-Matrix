#!/usr/bin/env python3
"""Exercise approved baseline source/evidence boundaries; fixtures are not CI receipts."""
import copy
import json
from pathlib import Path
import shutil
import tempfile
import unittest
from unittest.mock import patch

import accepted_baseline as baseline


class AcceptedBaseline(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.suite_temp = tempfile.TemporaryDirectory()
        cls.template = Path(cls.suite_temp.name) / 'component'
        cls.revision = baseline.evidence.identity('a' * 40, '123', '2')
        cls.original = baseline.produce(cls.template, cls.revision, diagnostic=True)
        # A controlled fixture emulates a CI receipt solely for negative tests.
        cls.fixture = copy.deepcopy(cls.original)
        cls.fixture['diagnostic_only'] = False

    @classmethod
    def tearDownClass(cls):
        cls.suite_temp.cleanup()

    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.folder = Path(self.temp.name) / 'evidence'
        shutil.copytree(self.template, self.folder)
        self.receipt = copy.deepcopy(self.fixture)
        self.policy, self.mapping = baseline.evidence.configuration()

    def write(self):
        (self.folder / 'evidence.json').write_text(json.dumps(self.receipt), encoding='utf-8')

    def read(self):
        self.write()
        return baseline.read_reports(self.folder, self.revision, self.policy)

    def rejected(self):
        with self.assertRaises(baseline.evidence.gate.ValidationError):
            self.read()

    def test_61_evidence_supported_and_25_specific_gaps(self):
        reports = self.read()
        self.assertEqual(len(reports), 86)
        self.assertEqual(sum(r['assertion'] is not None and r['assertion']['passed'] for r in reports.values()), 61)
        self.assertTrue(set(reports).isdisjoint(self.mapping))
        self.assertNotIn('B14.RESULT', reports)
        for cid, report in reports.items():
            self.assertEqual(report['policy']['hard_gate'], cid in self.policy['hard_gates'])
            self.assertFalse(report['scope']['current_binary_acceptance'])
            self.assertFalse(report['scope']['publication_authorized'])
            if report['assertion']:
                self.assertIsNone(report['failure'])
        self.assertIsNone(reports['A11.08']['assertion'])

    def test_diagnostic_cannot_be_consumed_as_ci(self):
        self.receipt['diagnostic_only'] = True
        self.rejected()

    def test_revision_source_run_and_attempt_are_bound(self):
        for field, value in (('commit_sha', 'b'*40), ('run_id', '999'), ('run_attempt', '3')):
            with self.subTest(field=field):
                self.receipt = copy.deepcopy(self.fixture)
                self.receipt['revision'][field] = value
                self.rejected()

    def test_definition_hash_is_bound(self):
        self.receipt['registry_sha256'] = '0'*64
        self.rejected()

    def test_current_component_source_is_bound(self):
        self.receipt['source_inputs']['Tools/evaluate_signature_benchmark.py'] = '0'*64
        self.rejected()

    def test_historical_source_cannot_be_relabelled(self):
        self.receipt['historical_product_sha'] = 'b'*40
        self.rejected()

    def test_publication_and_binary_acceptance_cannot_be_added(self):
        for field in ('publication_authorized', 'current_binary_acceptance'):
            with self.subTest(field=field):
                self.receipt = copy.deepcopy(self.fixture)
                self.receipt[field] = True
                self.rejected()

    def test_missing_evidence_cannot_be_promoted(self):
        self.receipt['checks']['A11.08']['status'] = 'PASS'
        self.receipt['counts']['pass'] += 1
        self.receipt['counts']['blocked'] -= 1
        self.rejected()

    def test_native_change_reopens_all_historical_claims(self):
        registry, _ = baseline.load_registry()
        import test_signature_calculator_contracts as tests
        result = baseline.decisions(registry, ['Source/PluginProcessor.cpp'],
                                    set(tests.case_names()), True, baseline.documentation_checks())
        self.assertEqual(sum(r['status'] == 'PASS' for r in result.values()), 22)
        self.assertTrue(all(r['status'] == 'BLOCKED' for r in result.values() if r['kind'] == 'historical'))
        with patch.object(baseline, 'fingerprint_changes', return_value=['Source/PluginProcessor.cpp']):
            self.rejected()

    def test_failed_component_cannot_keep_pass(self):
        for name in ('calculator', 'distribution'):
            with self.subTest(name=name):
                self.receipt = copy.deepcopy(self.fixture)
                self.receipt['commands'][name]['exit_code'] = 1
                self.rejected()

    def test_boolean_exit_code_rejected(self):
        self.receipt['commands']['calculator']['exit_code'] = False
        self.rejected()

    def test_raw_log_tampering(self):
        (self.folder / 'calculator.log').write_text('PASS invented', encoding='utf-8')
        self.rejected()

    def test_case_omission_duplicate_skip_and_false_count(self):
        original = json.loads((self.template / 'calculator-cases.json').read_text())
        for mutation in ('missing', 'duplicate', 'skipped', 'boolean'):
            with self.subTest(mutation=mutation):
                self.receipt = copy.deepcopy(self.fixture)
                cases = copy.deepcopy(original)
                if mutation == 'missing': cases['cases'].pop()
                elif mutation == 'duplicate': cases['cases'][-1] = cases['cases'][0]
                elif mutation == 'skipped': cases['skipped'] = 1
                else: cases['failures'] = False
                path = self.folder / 'calculator-cases.json'
                path.write_text(json.dumps(cases), encoding='utf-8')
                self.receipt['files'][path.name] = baseline.digest(path.read_bytes())
                self.rejected()

    def test_extra_file_is_rejected(self):
        (self.folder / 'unexpected.json').write_text('{}', encoding='utf-8')
        self.rejected()

    def test_count_tampering(self):
        self.receipt['counts']['pass'] = 86
        self.rejected()

    def test_consolidator_uses_baseline_but_keeps_external_gates_blocked(self):
        from test_release_consolidator import Consolidation
        fixture = Consolidation()
        fixture.setUp()
        self.addCleanup(fixture.doCleanups)
        self.write()
        shutil.copytree(self.folder, fixture.inputs / baseline.FOLDER)
        result, output = fixture.run_gate()
        self.assertEqual(result['producer']['errors'], [])
        self.assertEqual(result['counts']['checks'], {'total':128, 'pass':82, 'blocked':46, 'not_applicable':0})
        self.assertFalse(result['ready'])
        for cid in ('B1.RESULT','B8.RESULT','B12.RESULT','B14.RESULT','E2.SAME_DI','I2.LIVE_REMOVE_DAW'):
            self.assertEqual(fixture.status(result, cid), 'BLOCKED')
        self.assertTrue((output / 'evidence/accepted-baseline/evidence.json').is_file())

    def test_consolidator_rejects_forged_baseline_artifact(self):
        from test_release_consolidator import Consolidation
        fixture = Consolidation()
        fixture.setUp()
        self.addCleanup(fixture.doCleanups)
        self.receipt['checks']['A11.08']['status'] = 'PASS'
        self.write()
        shutil.copytree(self.folder, fixture.inputs / baseline.FOLDER)
        result, _ = fixture.run_gate()
        self.assertTrue(result['producer']['errors'])
        self.assertEqual(fixture.status(result, 'A13.07'), 'BLOCKED')
        self.assertEqual(fixture.status(result, 'A11.08'), 'BLOCKED')


if __name__ == '__main__':
    unittest.main(verbosity=2)
