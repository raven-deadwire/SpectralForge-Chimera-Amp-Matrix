#!/usr/bin/env python3
"""Adversarial checks against the real PR87 raw artifact, never release evidence."""
import copy
import json
from pathlib import Path
import shutil
import tempfile
import unittest
from unittest import mock
import xml.etree.ElementTree as ET

import recover_pr87_evidence as r

class RecoveryTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory()
        cls.root = Path(cls.tmp.name)
        cls.product = cls.root / 'product'
        r.extract(r.FOLDER / 'product-gate-38887964838.zip', cls.product, r.ARCHIVE_HASH)
        cls.upgrade = cls.root / 'upgrade'
        r.extract(r.FOLDER / 'upgrade-38905833729.zip', cls.upgrade, r.UPGRADE_HASH)
        cls.metadata = r.evidence.gate.load_json(r.FOLDER / 'upgrade-metadata.json')
        cls.snapshot = r.evidence.gate.load_json(r.FOLDER / 'github-snapshot.json')
        p = cls.product / 'evidence/windows/raw'
        cls.log = (p / 'LastTest.log').read_text(encoding='utf-8-sig')
        cls.junit = ET.parse(p / 'ctest-results.xml').getroot()
        cls.cases, _ = r.evidence.test_results(p / 'ctest-results.xml', p / 'ctest-inventory.json')

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def audit(self, upgrade=True):
        return r.audit(r.FOLDER / 'product-gate-38887964838.zip', self.snapshot,
            r.FOLDER / 'upgrade-38905833729.zip' if upgrade else None, self.metadata if upgrade else None)

    def test_exact_product_39_and_overall_blocked(self):
        result = self.audit()
        self.assertEqual(result['recovery_counts'], {'VERIFIED_CURRENT_SOURCE': 39})
        self.assertEqual(result['review_gate']['counts']['checks'],
                         {'total': 128, 'pass': 88, 'blocked': 40, 'not_applicable': 0})
        self.assertEqual(result['review_gate']['verdict'], 'BLOCKED')
        self.assertFalse(result['review_gate']['publisher_input'])
        self.assertFalse(result['publication_authorized'])
        self.assertEqual(result['original_product_gate']['counts']['pass'], 49)
        self.assertEqual(len(result['remaining_accepted_ids']), 19)
        self.assertEqual(len(result['remaining_external_ids']), 21)

    def test_missing_upgrade_leaves_four_blocked(self):
        result = self.audit(False)
        self.assertEqual(result['recovery_counts'], {'BLOCKED': 4, 'VERIFIED_CURRENT_SOURCE': 35})
        self.assertEqual({x['id'] for x in result['rows'] if x['status'] == 'BLOCKED'}, r.UPGRADE_IDS)

    def test_archive_tampering_is_rejected(self):
        p = self.root / 'tampered.zip'
        p.write_bytes((r.FOLDER / 'product-gate-38887964838.zip').read_bytes() + b'x')
        with self.assertRaises(r.evidence.gate.ValidationError):
            r.extract(p, self.root / 'unused', r.ARCHIVE_HASH)

    def test_product_run_source_and_attempt_are_not_relabelled(self):
        for key, value in [('head_sha', '0' * 40), ('run_attempt', 2), ('conclusion', 'failure')]:
            with self.subTest(key=key):
                snapshot = copy.deepcopy(self.snapshot); snapshot['product_run'][key] = value
                with mock.patch.object(r, 'source_guard', return_value={}):
                    with self.assertRaises(r.evidence.gate.ValidationError):
                        r.audit(r.FOLDER / 'product-gate-38887964838.zip', snapshot)

    def test_changed_native_input_is_rejected(self):
        registry, _ = r.baseline.load_registry()
        original = Path.read_bytes
        def changed(path):
            data = original(path)
            return data + b'changed' if str(path).endswith('/Source/PluginProcessor.cpp') else data
        with mock.patch.object(Path, 'read_bytes', changed):
            with self.assertRaises(r.evidence.gate.ValidationError):
                r.source_guard(registry)

    def test_skipped_native_case_is_rejected(self):
        cases = dict(self.cases); cases['ChimeraTests'] = False
        with self.assertRaises(r.evidence.gate.ValidationError):
            r.completed_output('ChimeraTests', cases, self.junit, self.log, 'windows')

    def test_duplicate_or_unterminated_section_is_rejected(self):
        for log in (self.log + self.log, self.log.replace('Test Passed.', 'Test Failed.')):
            with self.subTest(duplicate=len(log) > len(self.log)):
                with self.assertRaises(r.evidence.gate.ValidationError):
                    r.completed_output('ChimeraTests', self.cases, self.junit, log, 'windows')

    def test_junit_full_log_disagreement_is_rejected(self):
        junit = copy.deepcopy(self.junit)
        case = next(c for c in junit.iter('testcase') if c.attrib['name'] == 'ChimeraTests')
        case.find('system-out').text = 'Unrelated success'
        with self.assertRaises(r.evidence.gate.ValidationError):
            r.completed_output('ChimeraTests', self.cases, junit, self.log, 'windows')

    def test_platform_skip_cannot_be_claimed_on_wrong_owner(self):
        with self.assertRaises(r.evidence.gate.ValidationError):
            r.completed_output('ChimeraPackagingVersionTests', self.cases, self.junit, self.log, 'linux')

    def test_upgrade_receipt_source_attempt_and_false_are_rejected(self):
        for key, value in [('product_source_sha', '0' * 40), ('run_attempt', '2'), ('success', False),
                           ('installer_invocations', 3), ('publication_approved', True)]:
            with self.subTest(key=key), tempfile.TemporaryDirectory() as tmp:
                folder = Path(tmp) / 'u'; shutil.copytree(self.upgrade, folder)
                result = r.evidence.gate.load_json(folder / 'result.json'); result[key] = value
                (folder / 'result.json').write_text(json.dumps(result))
                with self.assertRaises(r.evidence.gate.ValidationError):
                    r.upgrade_record(folder, self.metadata)

    def test_upgrade_raw_log_tampering_is_rejected(self):
        with tempfile.TemporaryDirectory() as tmp:
            folder = Path(tmp) / 'u'; shutil.copytree(self.upgrade, folder)
            (folder / '02-beta3-upgrade.log').write_text('forged')
            with self.assertRaises(r.evidence.gate.ValidationError):
                r.upgrade_record(folder, self.metadata)

    def test_unrelated_successful_upgrade_run_is_rejected(self):
        metadata = copy.deepcopy(self.metadata); metadata['run']['id'] += 1
        with self.assertRaises(r.evidence.gate.ValidationError):
            r.upgrade_record(self.upgrade, metadata)

if __name__ == '__main__':
    unittest.main(verbosity=2)
