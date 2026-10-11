#!/usr/bin/env python3
"""Small synthetic checks for retained CAB evidence and measured child launches."""

import contextlib
import csv
import hashlib
import io
import os
from pathlib import Path
import subprocess
import tempfile
import types
import unittest
from unittest import mock

import run_cab_realtime_ci as launcher

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


class LauncherFailureControls(unittest.TestCase):
    def call(self, *, measured=True, windows=True, observed=32, query_error=None,
             taskkill_error=None, kill_error=None, wait_error=None):
        with tempfile.TemporaryDirectory(prefix="cab-priority-mock-") as temporary:
            log = Path(temporary) / "child.log"
            owner = self
            class Child:
                pid = 9001
                returncode = None
                kill_calls = 0
                wait_calls = 0
                output = None
                def kill(self):
                    self.kill_calls += 1
                    if kill_error:
                        raise kill_error
                def wait(self, timeout):
                    self.wait_calls += 1
                    self.output.flush()
                    owner.assertNotIn("MEASURED_PROCESS_PRIORITY", log.read_text(),
                                      "parent metadata was written before child completion")
                    if wait_error:
                        raise wait_error
                    self.output.write("CHILD FINISHED\n")
                    self.output.flush()
                    self.returncode = 0
                    return 0
                def poll(self):
                    return self.returncode
            child = Child()
            def popen(command, **kwargs):
                child.output = kwargs["stdout"]
                child.output.write("CHILD STARTED\n")
                child.output.flush()
                return child
            fake_subprocess = types.SimpleNamespace(
                CREATE_NEW_PROCESS_GROUP=0x200, NORMAL_PRIORITY_CLASS=0x20,
                STDOUT=subprocess.STDOUT, TimeoutExpired=subprocess.TimeoutExpired,
                Popen=mock.Mock(side_effect=popen),
                run=mock.Mock(side_effect=taskkill_error,
                              return_value=types.SimpleNamespace(returncode=0)))
            fake_os = types.SimpleNamespace(name="nt" if windows else "posix",
                                            environ={}, SEEK_END=os.SEEK_END)
            query = mock.Mock(side_effect=query_error, return_value=observed)
            with mock.patch.object(launcher, "subprocess", fake_subprocess), \
                 mock.patch.object(launcher, "os", fake_os), \
                 mock.patch.object(launcher, "windows_child_priority_class", query), \
                 contextlib.redirect_stdout(io.StringIO()):
                receipt = launcher.run_logged(["mock-child"], temporary, log,
                                               timeout=600, measured=measured)
            return receipt, child, fake_subprocess, query, log.read_text()

    def test_query_failure_and_taskkill_timeout_keep_latched_failure(self):
        receipt, child, process_api, query, text = self.call(
            query_error=OSError("query denied"),
            taskkill_error=subprocess.TimeoutExpired("taskkill", 30))
        priority = receipt["measured_process_priority"]
        self.assertEqual(receipt["returncode"], 1)
        self.assertEqual(child.returncode, 0)
        self.assertFalse(priority["verified"])
        self.assertIn("query denied", priority["error"])
        self.assertGreater(child.kill_calls, 0)
        self.assertGreater(child.wait_calls, 0)
        self.assertTrue(priority["cleanup"]["direct_child_reaped"])
        self.assertEqual(priority["cleanup"]["descendants"], "unverified")
        self.assertTrue(any("taskkill:" in e for e in priority["cleanup"]["errors"]))
        self.assertLess(text.index("CHILD FINISHED"), text.index("MEASURED_PROCESS_PRIORITY"))

    def test_mismatch_and_all_cleanup_errors_preserve_receipt_without_stdout_race(self):
        receipt, child, process_api, query, text = self.call(
            observed=0x4000, taskkill_error=OSError("taskkill unavailable"),
            kill_error=OSError("kill denied"),
            wait_error=subprocess.TimeoutExpired("direct child", 30))
        priority = receipt["measured_process_priority"]
        self.assertEqual(receipt["returncode"], 1)
        self.assertFalse(priority["verified"])
        self.assertEqual(priority["observed_class"], 0x4000)
        self.assertIn("did not start at NORMAL_PRIORITY_CLASS", priority["error"])
        self.assertGreater(child.kill_calls, 0)
        self.assertGreater(child.wait_calls, 0)
        self.assertFalse(priority["cleanup"]["direct_child_reaped"])
        self.assertEqual(priority["cleanup"]["descendants"], "unverified")
        self.assertEqual(len(priority["cleanup"]["errors"]), 3)
        self.assertEqual(priority["log_metadata_omitted"], "direct_child_not_reaped")
        self.assertNotIn("MEASURED_PROCESS_PRIORITY", text)

    def test_normal_measurement_records_actual_class_only_after_child_finishes(self):
        receipt, child, process_api, query, text = self.call()
        priority = receipt["measured_process_priority"]
        self.assertEqual(receipt["returncode"], 0)
        self.assertEqual(process_api.Popen.call_args.kwargs["creationflags"], 0x220)
        self.assertEqual(priority["requested_class"], 32)
        self.assertEqual(priority["observed_class"], 32)
        self.assertTrue(priority["verified"])
        query.assert_called_once_with(child)
        process_api.run.assert_not_called()
        self.assertLess(text.index("CHILD FINISHED"), text.index("MEASURED_PROCESS_PRIORITY"))

    def test_windows_compile_launch_remains_inherited_and_unqueried(self):
        receipt, child, process_api, query, text = self.call(measured=False)
        self.assertEqual(receipt["returncode"], 0)
        self.assertEqual(process_api.Popen.call_args.kwargs["creationflags"], 0x200)
        self.assertNotIn("measured_process_priority", receipt)
        query.assert_not_called()
        process_api.run.assert_not_called()

    def test_posix_measured_launch_remains_unchanged(self):
        receipt, child, process_api, query, text = self.call(windows=False)
        self.assertEqual(receipt["returncode"], 0)
        self.assertTrue(process_api.Popen.call_args.kwargs["start_new_session"])
        self.assertNotIn("creationflags", process_api.Popen.call_args.kwargs)
        self.assertNotIn("measured_process_priority", receipt)
        query.assert_not_called()
        process_api.run.assert_not_called()


if __name__ == "__main__":
    unittest.main()
