#!/usr/bin/env python3
"""Synthetic calculator tests from 156fcf2/ec6a13b; no numbered A/B claims.

These supplement the original three tests in test_signature_benchmark.py.
"""
import argparse
import copy
import json
import math
from pathlib import Path
import unittest

import evaluate_signature_benchmark as benchmark

ROOT = Path(__file__).resolve().parents[1]


class CalculatorContracts(unittest.TestCase):
    def setUp(self):
        self.policy = json.loads((ROOT / 'Validation/signature-benchmark-policy.json').read_text(encoding='utf-8'))
        self.renders = {'schema': 'spectralforge.chimera.signature-renders',
                        'schema_version': 1, 'dsp_valid': True,
                        'renders': copy.deepcopy(self.policy['reference'])}

    def evaluate(self):
        return benchmark.evaluate(self.policy, self.renders)

    def test_reference_shape(self):
        result = self.evaluate()
        self.assertEqual(result['verdict'], 'PASS')
        self.assertAlmostEqual(result['overall_score'], 100)
        self.assertTrue(all(result['hard_gates'].values()))

    def test_hard_failure_overrides_high_score(self):
        self.renders['renders']['wild']['crest_db'] = 9.93
        result = self.evaluate()
        self.assertGreater(result['overall_score'], 85)
        self.assertEqual(result['verdict'], 'REVISE')
        self.assertIn('H4 hard gate failed', result['reasons'])

    def test_invalid_dsp(self):
        self.renders.update(dsp_valid=False, renders={})
        result = self.evaluate()
        self.assertEqual(result['verdict'], 'INVALID')
        self.assertNotIn('overall_score', result)

    def test_raw_bias_centering_rmse_and_weights(self):
        metric = 'rms_1500_4000_db'
        for song, delta in zip(benchmark.SONGS, (1., 2., 3.)):
            self.renders['renders'][song][metric] += delta
        result = self.evaluate()
        measured = result['metrics'][metric]
        for song, raw, centered in zip(benchmark.SONGS, (1, 2, 3), (-1, 0, 1)):
            self.assertAlmostEqual(measured['raw_delta'][song], raw)
            self.assertAlmostEqual(measured['centered_delta'][song], centered)
        self.assertAlmostEqual(measured['common_bias'], 2)
        self.assertAlmostEqual(measured['shape_rmse'], math.sqrt(2 / 3))
        self.assertAlmostEqual(measured['shape_score'], 1 - math.sqrt(2 / 3) / 1.2)
        pairs = measured['pairs']
        self.assertAlmostEqual(measured['gap_mae'], sum(abs(p['gap_error']) for p in pairs) / len(pairs))
        self.assertAlmostEqual(measured['score'], .5 * measured['shape_score'] +
                               .3 * measured['order_score'] + .2 * measured['gap_score'])
        self.assertAlmostEqual(result['overall_score'], 100 * sum(
            self.policy['metrics'][name]['weight'] * item['score'] for name, item in result['metrics'].items()))

    def test_common_bias_invariance(self):
        for values in self.renders['renders'].values():
            for metric in values:
                values[metric] += 3
        result = self.evaluate()
        self.assertEqual(result['verdict'], 'PASS')
        self.assertAlmostEqual(result['overall_score'], 100)
        for metric in result['metrics'].values():
            self.assertAlmostEqual(metric['common_bias'], 3)
            self.assertAlmostEqual(metric['shape_rmse'], 0)

    def test_ties_and_minimum_directional_gap(self):
        result = self.evaluate()
        for metric in ('crest_db', 'rms_1500_4000_db'):
            self.assertEqual({(p['a'], p['b']) for p in result['metrics'][metric]['pairs']},
                             {('crom', 'wild'), ('wild', 'azhi')})
        self.renders['renders']['wild']['rms_1500_4000_db'] = -17.29
        pair = next(p for p in self.evaluate()['metrics']['rms_1500_4000_db']['pairs'] if p['a'] == 'crom')
        self.assertLess(pair['render_gap'], 0)
        self.assertFalse(pair['order_pass'])

    def test_original_hard_gate_boundaries(self):
        eps = 1e-7
        for gate, metric, values, fail_song, fail_value in (
            ('H1', 'rms_80_200_db', (.4 + eps, 0, 1.1 + 2 * eps), 'azhi', 1.1),
            ('H2', 'side_80_200_pct', (6, 7, 3), 'azhi', 3 + eps),
            ('H3', 'side_4000_8000_pct', (1, 2.5, 0), 'wild', 2.5 - eps),
            ('H4', 'crest_db', (9, 9.25, 8), 'wild', 9.25 - eps),
        ):
            with self.subTest(gate=gate):
                for song, value in zip(benchmark.SONGS, values):
                    self.renders['renders'][song][metric] = value
                self.assertTrue(self.evaluate()['hard_gates'][gate])
                self.renders['renders'][fail_song][metric] = fail_value
                self.assertFalse(self.evaluate()['hard_gates'][gate])

    def test_secondary_three_of_four(self):
        self.assertEqual(self.policy['secondary_required'], 3)
        self.renders['renders']['wild']['rms_1500_4000_db'] = -17.40
        self.assertEqual(self.evaluate()['secondary_passed'], 3)
        self.renders['renders']['azhi']['side_1500_4000_pct'] = 38.0
        result = self.evaluate()
        self.assertEqual(result['secondary_passed'], 2)
        self.assertIn('Secondary gates 2/4; need 3', result['reasons'])
        self.assertEqual(result['verdict'], 'REVISE')

    def test_score_and_critical_boundaries(self):
        self.assertEqual(self.policy['score']['overall_pass'], 85)
        self.assertEqual(self.policy['score']['critical_metric_min'], .70)
        # Test-only policy copies probe inclusive comparisons. No file changes.
        boundary = self.evaluate()['overall_score']
        self.policy['score']['overall_pass'] = boundary
        self.assertEqual(self.evaluate()['verdict'], 'PASS')
        self.policy['score']['overall_pass'] = boundary + .0001
        self.assertEqual(self.evaluate()['verdict'], 'REVISE')
        self.renders['renders']['wild']['rms_1500_4000_db'] = -17.4
        value = self.evaluate()['metrics']['rms_1500_4000_db']['score']
        self.policy['score']['critical_metric_min'] = value
        self.assertNotIn('rms_1500_4000_db', self.evaluate()['critical_below_threshold'])
        self.policy['score']['critical_metric_min'] = value + 1e-7
        self.assertIn('rms_1500_4000_db', self.evaluate()['critical_below_threshold'])

    def test_missing_measurement(self):
        del self.renders['renders']['crom']['crest_db']
        with self.assertRaisesRegex(ValueError, 'Missing/non-finite'):
            self.evaluate()

    def test_numeric_measurements_exclude_bool_and_nonfinite(self):
        for value in (None, '9.69', float('nan'), float('inf'), -float('inf'), True, False):
            with self.subTest(value=value):
                self.renders['renders']['crom']['crest_db'] = value
                with self.assertRaisesRegex(ValueError, 'Missing/non-finite'):
                    self.evaluate()

    def test_schema_and_missing_song(self):
        self.renders['schema_version'] = 999
        with self.assertRaisesRegex(ValueError, 'Unsupported'):
            self.evaluate()
        self.renders['schema_version'] = 1
        del self.renders['renders']['azhi']
        with self.assertRaisesRegex(ValueError, 'Missing render'):
            self.evaluate()

    def test_json_roundtrip(self):
        result = self.evaluate()
        self.assertEqual(result, json.loads(json.dumps(result, allow_nan=False)))
        self.assertEqual(set(result['metrics']), set(self.policy['metrics']))
        self.assertEqual(set(result['hard_gates']), {'H1', 'H2', 'H3', 'H4'})
        self.assertEqual(set(result['secondary_gates']), {'S1', 'S2', 'S3', 'S4'})


def case_names():
    return unittest.defaultTestLoader.getTestCaseNames(CalculatorContracts)


class ReceiptResult(unittest.TextTestResult):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.cases = []

    def startTest(self, test):
        self.cases.append({'name': test._testMethodName, 'status': 'BLOCKED'})
        super().startTest(test)

    def addSuccess(self, test):
        self.cases[-1]['status'] = 'PASS'
        super().addSuccess(test)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--report', type=Path)
    args = parser.parse_args()
    result = unittest.TextTestRunner(verbosity=2, resultclass=ReceiptResult).run(
        unittest.defaultTestLoader.loadTestsFromTestCase(CalculatorContracts))
    if args.report:
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text(json.dumps({'schema': 'spectralforge.chimera.calculator-contracts',
            'schema_version': 1, 'synthetic_only': True, 'policy_claims': [],
            'tests_run': result.testsRun, 'failures': len(result.failures),
            'errors': len(result.errors), 'skipped': len(result.skipped), 'cases': result.cases}, indent=2) + '\n', encoding='utf-8')
    return 0 if result.wasSuccessful() and not result.skipped else 1


if __name__ == '__main__':
    raise SystemExit(main())
