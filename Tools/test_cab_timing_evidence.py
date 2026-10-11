#!/usr/bin/env python3
"""Small, synthetic corruption checks for the retained CAB callback evidence."""

import csv
import hashlib
from pathlib import Path
import tempfile
import unittest

from run_cab_realtime_ci import preserve_timing_samples, validate_timing_samples


FIELDS = (
    "schema_version,engine,suite,sample_rate,block_size,channels,callback_index,phase,phase_index,"
    "automation_requested,wall_us,budget_us,deadline_miss,thread_cpu_us,thread_cpu_valid,"
    "thread_cycles,thread_cycles_valid,clock_start_error,clock_end_error,"
    "thread_cpu_start_us,thread_cpu_end_us,thread_cycles_start,thread_cycles_end"
).split(",")


def fixture(clock="cpu", suite="integration"):
    budget = 1e6 * 64 / 96000
    rows = []
    for index in range(1600):
        cpu_first, cpu_delta = index * 10000 + 1, 1600 - index
        cycle_first, cycle_delta = index * 100000 + 10000, 10000 - index
        rows.append(dict(zip(FIELDS, map(str, (
            1, "expanded-v2", suite, 96000, 64, 1, index,
            "worker_and_automation" if index < 1200 else "forced_six_slot_publication",
            index if index < 1200 else index - 1200, int(index < 80), index + 1, budget,
            int(index + 1 > budget), cpu_delta if clock == "cpu" else -1, int(clock == "cpu"),
            cycle_delta if clock == "cycles" else 0, int(clock == "cycles"), 0, 0,
            cpu_first if clock == "cpu" else -1, cpu_first + cpu_delta if clock == "cpu" else -1,
            cycle_first if clock == "cycles" else 0, cycle_first + cycle_delta if clock == "cycles" else 0,
        )))))
    # A complete failed deadline result must still have valid, preservable raw
    # evidence. The evidence validator must not replace the executable gate.
    summary = {"test": "ChimeraCabExpansionIntegrationTests", "engine": "expanded-v2",
               "sr": "96000", "block": "64", "channels": "1",
               "p50_us": "801", "p99_us": "1585", "max_us": "1600", "misses": "934/1600"}
    return rows, summary


def write_rows(root, rows, suite="integration"):
    path = root / "cab-timing-diagnostics" / suite / "expanded-v2" / "96000-64-1.csv"
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, fieldnames=FIELDS)
        writer.writeheader()
        writer.writerows(rows)
    return path


class CabTimingEvidenceTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="chimera-callback-evidence-")
        self.addCleanup(self.temporary.cleanup)
        self.output = Path(self.temporary.name) / "output"
        self.rows, self.summary = fixture()

    def validate(self):
        write_rows(self.output, self.rows)
        return validate_timing_samples(self.output, [self.summary])

    def test_accepts_complete_failed_gate_evidence_for_both_clock_backends(self):
        for clock in ("cpu", "cycles"):
            with self.subTest(clock=clock):
                self.rows, self.summary = fixture(clock)
                result = self.validate()
                self.assertEqual(result[0]["samples"], 1600)
                self.assertEqual(result[0]["thread_cpu_valid_samples"], 1600 if clock == "cpu" else 0)
                self.assertEqual(result[0]["thread_cycles_valid_samples"], 1600 if clock == "cycles" else 0)

    def test_rejects_missing_file(self):
        with self.assertRaises(FileNotFoundError):
            validate_timing_samples(self.output, [self.summary])

    def test_rejects_dropped_callback(self):
        del self.rows[500]
        with self.assertRaisesRegex(RuntimeError, "Incomplete raw callback"):
            self.validate()

    def test_rejects_duplicate_callback_without_changing_population(self):
        self.rows[500] = self.rows[499].copy()
        with self.assertRaisesRegex(RuntimeError, "identity/phase mismatch"):
            self.validate()

    def test_rejects_wrong_phase_at_publication_boundary(self):
        self.rows[1200]["phase"] = "worker_and_automation"
        with self.assertRaisesRegex(RuntimeError, "identity/phase mismatch"):
            self.validate()

    def test_rejects_duplicate_route_reference(self):
        write_rows(self.output, self.rows)
        with self.assertRaisesRegex(RuntimeError, "Duplicate raw callback route"):
            validate_timing_samples(self.output, [self.summary, self.summary.copy()])

    def test_rejects_changed_p99_wall_time(self):
        self.rows[1584]["wall_us"] = "1585.25"
        with self.assertRaisesRegex(RuntimeError, "disagree with executable p99_us"):
            self.validate()

    def test_rejects_changed_deadline(self):
        self.rows[50]["budget_us"] = "2000"
        with self.assertRaisesRegex(RuntimeError, "changed deadline"):
            self.validate()

    def test_rejects_cpu_delta_taken_from_another_callback(self):
        self.rows[1584]["thread_cpu_us"] = self.rows[50]["thread_cpu_us"]
        with self.assertRaisesRegex(RuntimeError, "Unpaired thread CPU"):
            self.validate()

    def test_rejects_swapped_complete_cpu_pairs(self):
        # Per-row end-start still matches. Only callback ordering exposes this
        # mix-up, so arithmetic-only validation is insufficient.
        for key in ("thread_cpu_start_us", "thread_cpu_end_us", "thread_cpu_us"):
            self.rows[10][key], self.rows[1000][key] = self.rows[1000][key], self.rows[10][key]
        with self.assertRaises(RuntimeError):
            self.validate()

    def test_rejects_cycle_delta_taken_from_another_callback(self):
        self.rows, self.summary = fixture("cycles")
        self.rows[1584]["thread_cycles"] = self.rows[50]["thread_cycles"]
        with self.assertRaisesRegex(RuntimeError, "Unpaired thread cycle"):
            self.validate()

    def test_rejects_swapped_complete_cycle_pairs(self):
        self.rows, self.summary = fixture("cycles")
        for key in ("thread_cycles_start", "thread_cycles_end", "thread_cycles"):
            self.rows[10][key], self.rows[1000][key] = self.rows[1000][key], self.rows[10][key]
        with self.assertRaises(RuntimeError):
            self.validate()

    def test_rejects_infinite_cpu_time_even_when_delta_matches(self):
        self.rows[1584]["thread_cpu_end_us"] = "inf"
        self.rows[1584]["thread_cpu_us"] = "inf"
        with self.assertRaises(RuntimeError):
            self.validate()

    def test_rejects_invented_zero_for_unavailable_cpu_clock(self):
        self.rows, self.summary = fixture("cycles")
        self.rows[100]["thread_cpu_us"] = "0"
        with self.assertRaisesRegex(RuntimeError, "Unavailable thread CPU"):
            self.validate()

    def test_preserves_partial_files_and_distinct_suites_with_exact_hashes(self):
        build = Path(self.temporary.name) / "build"
        integration = write_rows(build, self.rows[:80])
        extended_rows, _ = fixture(suite="extended")
        extended = write_rows(build, extended_rows[:400], suite="extended")
        profile_rows, _ = fixture(suite="profile")
        write_rows(build, profile_rows[:20], suite="profile")
        result = preserve_timing_samples(build, self.output, ("integration", "extended"))
        self.assertEqual(len(result), 2)
        source_files = {path.relative_to(build).as_posix(): path for path in (integration, extended)}
        self.assertEqual({item["path"] for item in result}, set(source_files))
        for item in result:
            original = source_files[item["path"]].read_bytes()
            self.assertEqual((self.output / item["path"]).read_bytes(), original)
            self.assertEqual(item["bytes"], len(original))
            self.assertEqual(item["sha256"], hashlib.sha256(original).hexdigest())
        self.assertFalse((self.output / "cab-timing-diagnostics" / "profile").exists())


if __name__ == "__main__":
    unittest.main()
