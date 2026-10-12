"""Numerical checks against independent waveform calculations, no training."""

import copy
import json
import math
from pathlib import Path
import unittest

from audit_gain_feasibility import audit_report, scalar_diagnostic


def waveform_metrics(prediction, reference):
    reference_power = sum(x * x for x in reference)
    prediction_power = sum(x * x for x in prediction)
    return {
        "full_esr": sum((p - r) ** 2 for p, r in zip(prediction, reference)) / reference_power,
        "rms_error_db": 10 * math.log10(prediction_power / reference_power),
        "peak_error_db": 20 * math.log10(max(map(abs, prediction)) / max(map(abs, reference))),
    }


class GainFeasibilityTests(unittest.TestCase):
    def test_pure_gain_is_exactly_recoverable(self):
        reference = [-2.0, -1.0, 0.0, 1.0, 2.0]
        prediction = [2 * r for r in reference]
        row = scalar_diagnostic(waveform_metrics(prediction, reference))
        self.assertAlmostEqual(row["oracle_scalar_linear"], 0.5, places=14)
        self.assertAlmostEqual(row["oracle_minimum_full_esr"], 0.0, places=14)
        self.assertTrue(row["simultaneous_level_feasible"])

    def test_oracle_bound_matches_direct_dot_product_and_residual(self):
        reference = [0.3, -0.4, 0.8, -1.0, 0.2, 0.5, -0.1]
        prediction = [0.2, -0.7, 1.2, -0.8, 0.3, 0.1, -0.2]
        gain = sum(p * r for p, r in zip(prediction, reference)) / sum(p * p for p in prediction)
        direct_error = sum((gain * p - r) ** 2 for p, r in zip(prediction, reference)) / sum(r * r for r in reference)
        row = scalar_diagnostic(waveform_metrics(prediction, reference))
        self.assertAlmostEqual(row["oracle_scalar_linear"], gain, places=14)
        self.assertAlmostEqual(row["oracle_minimum_full_esr"], direct_error, places=14)
        for offset in [-0.3, -0.01, 0.01, 0.3]:
            error = sum(((gain + offset) * p - r) ** 2 for p, r in zip(prediction, reference)) / sum(r * r for r in reference)
            self.assertGreater(error, direct_error)

    def test_orthogonal_signal_has_no_gain_solution(self):
        row = scalar_diagnostic(waveform_metrics([0, 1, 0, -1], [1, 0, -1, 0]))
        self.assertEqual(row["oracle_scalar_linear"], 0)
        self.assertEqual(row["oracle_minimum_full_esr"], 1)
        self.assertIsNone(row["oracle_scalar_db_if_positive"])

    def test_scalar_oracle_does_not_silently_remove_dc(self):
        reference = [-1, 0, 1, 0]
        prediction = [r + 0.25 for r in reference]
        row = scalar_diagnostic(waveform_metrics(prediction, reference))
        self.assertGreater(row["oracle_minimum_full_esr"], 0.1)

    def test_rms_peak_crest_conflict_cannot_be_padded_away(self):
        row = scalar_diagnostic({"full_esr": 0.12, "rms_error_db": -0.65, "peak_error_db": 2.36})
        self.assertFalse(row["simultaneous_level_feasible"])
        self.assertAlmostEqual(row["level_interval_signed_overlap_db"], -1.51)

    def test_level_intervals_include_exact_boundary(self):
        row = scalar_diagnostic({"full_esr": 0.1, "rms_error_db": 0, "peak_error_db": 1.5})
        self.assertTrue(row["simultaneous_level_feasible"])
        self.assertEqual(row["simultaneous_allowed_gain_db"], [-0.5, -0.5])

    def test_inconsistent_or_nonfinite_metrics_rejected(self):
        for metrics in [
            {"full_esr": 0, "rms_error_db": 6, "peak_error_db": 6},
            {"full_esr": -0.1, "rms_error_db": 0, "peak_error_db": 0},
            {"full_esr": math.nan, "rms_error_db": 0, "peak_error_db": 0},
            {"full_esr": 0.1, "rms_error_db": math.inf, "peak_error_db": 0},
            {"full_esr": 0.1, "rms_error_db": 0, "peak_error_db": True},
        ]:
            with self.assertRaises(ValueError):
                scalar_diagnostic(metrics)

    def test_archived_report_is_not_mutated_and_policy_cannot_drift(self):
        report_path = Path(__file__).resolve().parents[2] / "reports/refinement-20261011.json"
        report = json.loads(report_path.read_text())
        before = copy.deepcopy(report)
        output = audit_report(report)
        self.assertEqual(report, before)
        self.assertFalse(output["release_approved"])
        self.assertEqual(
            [name for name, row in output["channels"].items()
             if not row["previously_exposed_test"]["simultaneous_level_feasible"]],
            ["Fenrir", "Ragnarok"],
        )
        self.assertTrue(all(
            row["previously_exposed_test"]["scalar_full_esr_pass_impossible"]
            for row in output["channels"].values()
        ))
        report["quality_profile"]["quiet_residual_rms_max_dbfs"] = -65
        with self.assertRaises(ValueError):
            audit_report(report)


if __name__ == "__main__":
    unittest.main()
