#!/usr/bin/env python3
import unittest

from analyze_callback_pairs import analyze


def report(revision, control_ratio=1.0, ui_ratio=1.0, pairs=5):
    value = {
        "source_revision": revision,
        "os": "Test OS",
        "cpu": "Test CPU",
        "juce": "8.0.8",
        "sample_rate": 48000,
        "block_size": 128,
        "fixture": "fixed",
        "scope": "test",
        "rows": [],
    }
    for pair in range(1, pairs + 1):
        for scene, ratio in (("closed_audio", control_ratio), ("meter_rigs", ui_ratio)):
            base = 1000.0 + pair
            value["rows"].append({
                "pair": pair,
                "scene": scene,
                "scale": 1,
                "instances": 1,
                "callback_p99_us": base * (ratio if revision == "candidate" else 1.0),
                "callback_mean_us": 500.0 * (ratio if revision == "candidate" else 1.0),
                "callback_deadline_misses": 0,
                "callback_count": 1500,
                "audio_output_fnv64": "same",
            })
    return value


class CallbackTimingProtocolTests(unittest.TestCase):
    def test_detects_ui_specific_regression(self):
        result = analyze(report("baseline"), report("candidate", ui_ratio=1.25))
        self.assertEqual(result["probe_diagnosis"], "LIKELY_REGRESSION")
        self.assertEqual(result["release_gate_status"], "BLOCKED")

    def test_control_normalization_removes_machine_wide_drift(self):
        result = analyze(
            report("baseline"),
            report("candidate", control_ratio=1.30, ui_ratio=1.30),
        )
        self.assertEqual(result["probe_diagnosis"], "LIKELY_NONREGRESSION")
        ui = next(row for row in result["scenes"] if row["scene"] == "meter_rigs")
        self.assertAlmostEqual(ui["median_control_normalized_p99_ratio"], 1.0)

    def test_detects_control_adjusted_deadline_misses(self):
        baseline = report("baseline")
        candidate = report("candidate")
        for row in candidate["rows"]:
            if row["scene"] == "meter_rigs":
                row["callback_deadline_misses"] = 2
        result = analyze(baseline, candidate)
        self.assertEqual(result["probe_diagnosis"], "LIKELY_REGRESSION")

    def test_rejects_mismatched_conditions(self):
        candidate = report("candidate")
        candidate["cpu"] = "Different CPU"
        with self.assertRaisesRegex(ValueError, "cpu"):
            analyze(report("baseline"), candidate)

    def test_rejects_audio_change(self):
        candidate = report("candidate")
        candidate["rows"][0]["audio_output_fnv64"] = "changed"
        with self.assertRaisesRegex(ValueError, "not identical"):
            analyze(report("baseline"), candidate)

    def test_requires_five_pairs(self):
        with self.assertRaisesRegex(ValueError, "at least 5"):
            analyze(report("baseline", pairs=4), report("candidate", pairs=4))


if __name__ == "__main__":
    unittest.main()
