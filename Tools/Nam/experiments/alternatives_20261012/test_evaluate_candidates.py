"""Failure-injection checks for evaluation provenance and audio completeness.

These tests do not generate the reserved test signal or run model evaluation.
"""
from pathlib import Path
import tempfile
import unittest

import numpy as np

import evaluate_candidates as evaluator


class EvaluationGuardTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='nam-evaluation-guard-')
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.manifest = self.root / 'manifest.json'
        evaluator.write_json(self.manifest, {'fixture': 'provenance only, no audio'})
        self.plan = {
            'future_test_seed': 2610122201, 'future_test_seconds': 24,
            'data_manifest_sha256': evaluator.digest(self.manifest),
            'policy_sha256': evaluator.digest(evaluator.NAM_DIR / 'quality_profile.py'),
            'quality_profile': evaluator.PROFILE,
            'eq_implementation_sha256': evaluator.digest(Path(evaluator.__file__).with_name('memory_factorization.py'))}
        evaluator.write_json(self.root / 'plan.json', self.plan)
        self.validation = {'plan_sha256': evaluator.digest(self.root / 'plan.json'),
                           'channels': {name: {} for name in evaluator.NAMES}}
        evaluator.write_json(self.root / 'validation.json', self.validation)
        self.selection = {
            'plan_sha256': evaluator.digest(self.root / 'plan.json'),
            'validation_sha256': evaluator.digest(self.root / 'validation.json'),
            'release_approved': False,
            'baseline': {n: {'model_sha256': 'baseline', 'job_sha256': 'raw'} for n in evaluator.NAMES},
            'selected': {n: {'model_sha256': 'fitted', 'job_sha256': 'eq'} for n in evaluator.NAMES},
            'best_esr': {'raw': {n: {'model_sha256': 'raw-fit', 'job_sha256': 'raw'} for n in evaluator.NAMES},
                         'eq-chain': {n: {'model_sha256': 'fitted', 'job_sha256': 'eq'} for n in evaluator.NAMES}}}
        evaluator.write_json(self.root / 'selection.json', self.selection)

    def test_stale_validation_is_rejected_even_when_selection_exists(self):
        evaluator.frozen_selection(self.root, 2610122201, self.manifest)
        with (self.root / 'validation.json').open('a') as f:
            f.write(' ')
        with self.assertRaisesRegex(RuntimeError, 'changed validation'):
            evaluator.frozen_selection(self.root, 2610122201, self.manifest)

    def test_incomplete_channel_set_cannot_be_sealed_as_complete(self):
        del self.validation['channels']['Ragnarok']
        evaluator.write_json(self.root / 'validation.json', self.validation)
        self.selection['validation_sha256'] = evaluator.digest(self.root / 'validation.json')
        evaluator.write_json(self.root / 'selection.json', self.selection)
        with self.assertRaisesRegex(RuntimeError, 'Incomplete'):
            evaluator.frozen_selection(self.root, 2610122201, self.manifest)

    def test_new_seed_is_rejected_after_fit(self):
        with self.assertRaisesRegex(RuntimeError, 'seed/duration'):
            evaluator.frozen_selection(self.root, 2610122202, self.manifest)

    def test_model_and_eq_cannot_be_recombined_after_selection(self):
        self.assertEqual(evaluator.frozen_roles(self.selection, 'Fenrir', 'fitted', 'eq'),
                         ['selected', 'best-esr-eq-chain'])
        with self.assertRaisesRegex(RuntimeError, 'pair is absent'):
            evaluator.frozen_roles(self.selection, 'Fenrir', 'baseline', 'eq')

    def test_release_approval_cannot_enter_research_selection(self):
        self.selection['release_approved'] = True
        evaluator.write_json(self.root / 'selection.json', self.selection)
        with self.assertRaisesRegex(RuntimeError, 'must not approve'):
            evaluator.frozen_selection(self.root, 2610122201, self.manifest)

    def test_truncated_partial_or_nonfinite_float_output_is_rejected(self):
        file = self.root / 'audio.f32'
        for values in (np.zeros(3, np.float32), np.array([0, 0, np.nan, 0], np.float32)):
            values.tofile(file)
            with self.assertRaisesRegex(RuntimeError, 'Incomplete|nonfinite'):
                evaluator.read_audio(file, 4)
        np.zeros(4, np.float32).tofile(file)
        with file.open('ab') as f:
            f.write(b'\x00')
        with self.assertRaisesRegex(RuntimeError, 'Incomplete'):
            evaluator.read_audio(file, 4)

    def test_engine_metadata_cannot_hide_gain_or_partial_render(self):
        valid = {'samples': 4096, 'sample_rate': 48000, 'a2_gate': True,
                 'a2_channels': 8, 'normalize': False, 'normalization_gain': 1.0}
        evaluator.check_runtime(valid, 4096)
        for key, value in [('samples', 4095), ('sample_rate', 44100), ('a2_channels', 3),
                           ('a2_gate', False), ('normalize', True), ('normalization_gain', 1.01)]:
            with self.subTest(key=key), self.assertRaises(RuntimeError):
                evaluator.check_runtime({**valid, key: value}, 4096)


if __name__ == '__main__':
    unittest.main()
