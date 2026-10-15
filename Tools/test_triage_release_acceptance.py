#!/usr/bin/env python3
"""Diagnostic integrity checks against archived evidence, not new product certification."""
import copy
import hashlib
import json
from pathlib import Path
import tempfile
import unittest
import zipfile

import triage_release_acceptance as triage


class AcceptanceTriage(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.folder = triage.ROOT / 'docs/evidence/beta3-acceptance-20261016'
        cls.snapshot = triage.gate.load_json(cls.folder / 'github-snapshot.json')
        source = cls.snapshot['pr62']['head_sha']
        cls.policy, cls.mapping, cls.gaps, cls.waivers = [triage.source_json(source, p) for p in (
            'Validation/release-policy.json', 'Validation/ci-check-map.json',
            'Validation/weekly-publication-policy.json', 'Validation/waivers.json')]

    def setUp(self):
        self.archive = self.folder / 'ci-gate-38386305674.zip'
        self.snapshot = copy.deepcopy(type(self).snapshot)
        self.gaps = copy.deepcopy(type(self).gaps)

    def audit(self):
        return triage.audit(self.archive, self.snapshot, self.policy, self.mapping, self.gaps, self.waivers)

    def mutate_archive(self, target, change):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        output = Path(temporary.name) / 'mutated.zip'
        with zipfile.ZipFile(self.archive) as old, zipfile.ZipFile(output, 'w') as new:
            for entry in old.infolist():
                data = old.read(entry)
                if entry.filename.endswith(target):
                    data = change(data)
                new.writestr(entry, data)
        self.archive = output
        # Deliberately update the outer digest to exercise the embedded checks.
        self.snapshot['gate_artifact']['digest'] = 'sha256:' + hashlib.sha256(output.read_bytes()).hexdigest()

    def test_complete_inventory_is_explained_without_promoting_any_check(self):
        result = self.audit()
        self.assertEqual(result['counts'], dict(total=128, **{'pass': 21, 'blocked': 107, 'not_applicable': 0}))
        self.assertEqual(len(result['rows']), 107)
        self.assertEqual(result['new_strict_passes'], 0)
        self.assertEqual(result['strict_verdict'], 'BLOCKED')
        self.assertFalse(result['publication_authorized'])
        self.assertEqual(sum(result['gap_counts'].values()), 107)

    def test_outer_archive_digest_must_match_github_snapshot(self):
        self.snapshot['gate_artifact']['digest'] = 'sha256:' + '0' * 64
        with self.assertRaisesRegex(triage.gate.ValidationError, 'digest'):
            self.audit()

    def test_other_product_source_is_not_covered(self):
        self.snapshot['pr62']['head_sha'] = 'b' * 40
        with self.assertRaisesRegex(triage.gate.ValidationError, 'source'):
            self.audit()

    def test_workflow_attempt_is_not_interchangeable(self):
        for run in self.snapshot['runs']:
            if run['id'] == self.snapshot['gate_artifact']['workflow_run']['id']:
                run['run_attempt'] += 1
        with self.assertRaisesRegex(triage.gate.ValidationError, 'attempt'):
            self.audit()

    def test_omitted_gap_cannot_disappear(self):
        self.gaps['known_validation_gaps'].pop()
        with self.assertRaisesRegex(triage.gate.ValidationError, 'inventory'):
            self.audit()

    def test_duplicate_gap_is_rejected(self):
        self.gaps['known_validation_gaps'].append(self.gaps['known_validation_gaps'][0])
        with self.assertRaisesRegex(triage.gate.ValidationError, 'inventory'):
            self.audit()

    def test_unknown_gap_needs_explicit_triage(self):
        self.gaps['known_validation_gaps'][0]['gap_type'] = 'ASSUMED_PASS'
        with self.assertRaisesRegex(triage.gate.ValidationError, 'Unknown'):
            self.audit()

    def test_forged_green_summary_cannot_override_raw_checks(self):
        def change(data):
            doc = json.loads(data)
            doc['ready'] = True
            doc['verdict'] = 'PASS'
            return json.dumps(doc).encode()
        self.mutate_archive('release-gate.json', change)
        with self.assertRaisesRegex(triage.gate.ValidationError, 'recomputed'):
            self.audit()

    def test_raw_test_log_tampering_is_rejected(self):
        self.mutate_archive('evidence/windows/raw/LastTest.log', lambda data: data + b'\nPASS forged\n')
        with self.assertRaisesRegex(triage.gate.ValidationError, 'hash mismatch'):
            self.audit()


if __name__ == '__main__':
    unittest.main()
