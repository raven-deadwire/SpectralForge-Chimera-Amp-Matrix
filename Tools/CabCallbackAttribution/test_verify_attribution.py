#!/usr/bin/env python3
"""Independent synthetic receipts and adversarial corruption tests.

Fixtures are authored here from the public evidence contract. They do not call
probe/controller summarizers, derive rows from verifier outputs, or claim native
audio execution. The deliberately different wall/CPU values exercise envelope
integrity without imposing CPU <= wall.
"""
from __future__ import annotations

import csv
import json
from pathlib import Path
import tempfile
import unittest

import verify_attribution as verifier


def write_csv(path, rows, fields=None):
    if fields is None:
        fields = list(rows[0])
    with path.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=fields)
        writer.writeheader()
        writer.writerows(rows)


def load_csv(path):
    with path.open(newline="", encoding="utf-8") as handle:
        reader = csv.DictReader(handle)
        return reader.fieldnames, list(reader)


def edit_csv(path, edit):
    fields, rows = load_csv(path)
    edit(rows)
    write_csv(path, rows, fields)


def event(identifier, parent, depth, callback, stage, start, end, cpu_start, cpu_end,
          *, lane=-1, mic=-1, model=0, generation=0, sequence=0, sequence_end=None, logical_bytes=0):
    return {
        "schema_version": 1, "event_id": identifier, "parent_id": parent, "depth": depth,
        "callback_index": callback, "lane": lane, "mic": mic, "stage": stage,
        "role": "none" if model == 0 else "active", "model_key": model, "generation": generation,
        "logical_bytes": logical_bytes, "wall_start_ns": start, "wall_end_ns": end,
        "cpu_start_ns": cpu_start, "cpu_end_ns": cpu_end, "wall_valid": 1, "cpu_valid": 1,
        "wall_ns": end - start, "cpu_ns": cpu_end - cpu_start,
        "wall_start_error": 0, "wall_end_error": 0, "cpu_start_error": 0, "cpu_end_error": 0,
        "complete": 1, "worker_sequence_start": sequence,
        "worker_sequence_end": sequence if sequence_end is None else sequence_end,
        "wall_start_valid": 1, "wall_end_valid": 1, "cpu_start_valid": 1, "cpu_end_valid": 1,
    }


EVENT_HEADER = list(event(1, 0, 0, 0, "callback", 1, 2, 1, 2))
SLOT_HEADER = ("schema_version,callback_index,point,lane,mic,requested_model_key,active_model_key,active_generation,"
               "fading_model_key,fading_generation,pending_present,pending_identity_available,fade_remaining,fade_length,"
               "rejected_models,active_present,fading_present").split(",")


def fixture(directory, *, mode="arithmetic", level="stages", compiled=True, resources="thread_rusage", block_size=64):
    directory.mkdir(parents=True, exist_ok=True)
    prefix = directory / f"{mode}-original-v1-96000-{block_size}-1-{level}"
    enabled = compiled and level != "off"
    cab = mode == "cab"
    resource = level == "resources"
    callbacks, stages, worker, caller, slots = [], [], [], [], []
    for index in range(1600):
        start = 10_000_000 + index * 3_000_000
        wall = 1_000_000 if index == 0 else 100_000
        cpu = 1_200_000 if index == 0 else 50_000
        sequence = (1 if index < 1200 else 20) if cab and enabled else 0
        row = {
            "schema_version": 1, "mode": mode, "engine": "original-v1", "channels": 1,
            "sample_rate": 96000, "block_size": block_size, "callback_index": index,
            "phase": ("worker_and_automation" if cab else "paced_control") if index < 1200
                     else ("forced_six_slot_publication" if cab else "burst_control"),
            "phase_index": index if index < 1200 else index - 1200,
            "automation_requested": int(cab and index < 80), "publication_requested": int(cab and index == 1200),
            "wall_start_ns": start, "wall_end_ns": start + wall, "wall_ns": wall,
            "cpu_start_ns": start, "cpu_end_ns": start + cpu, "cpu_ns": cpu,
            "cpu_valid": 1, "wall_valid": 1, "wall_start_error": 0, "wall_end_error": 0,
            "cpu_start_error": 0, "cpu_end_error": 0, "deadline_ns": block_size * 1e9 / 96000,
            "deadline_miss": int(wall * 96000 > block_size * 1e9), "entry_wall_ns": start - 100, "exit_wall_ns": start + wall + 100,
            "entry_cpu_ns": start - 100, "exit_cpu_ns": start + cpu + 100,
            "envelope_wall_ns": wall + 200, "envelope_cpu_ns": cpu + 200,
            "worker_seq_before": sequence, "worker_seq_after": sequence,
            "worker_state_before": (1 if index < 1200 else 10) if cab and enabled else 0,
            "worker_state_after": (1 if index < 1200 else 10) if cab and enabled else 0,
            "worker_lane_before": -1, "worker_lane_after": -1, "worker_mic_before": -1, "worker_mic_after": -1,
            "trace_event_first": len(stages), "trace_event_count": 0,
            "resource_scope": resources if resource else "disabled", "resource_valid": int(resource),
            "minor_faults_delta": -1, "major_faults_delta": -1, "voluntary_switches_delta": -1,
            "involuntary_switches_delta": -1, "task_faults_delta": -1, "task_pageins_delta": -1,
            "thread_state_valid": int(resource and resources == "process_task_events"),
            "thread_run_state_before": 1 if resource and resources == "process_task_events" else -1,
            "thread_run_state_after": 1 if resource and resources == "process_task_events" else -1,
            "thread_suspend_count_before": 0 if resource and resources == "process_task_events" else -1,
            "thread_suspend_count_after": 0 if resource and resources == "process_task_events" else -1,
            "resource_before_error": 0, "resource_after_error": 0,
            "thread_state_before_error": 0, "thread_state_after_error": 0,
            "resource_before_start_wall_ns": start - 80 if resource else 0,
            "resource_before_end_wall_ns": start - 20 if resource else 0,
            "resource_after_start_wall_ns": start + wall + 20 if resource else 0,
            "resource_after_end_wall_ns": start + wall + 80 if resource else 0,
            "resource_before_query_wall_ns": 60 if resource else -1,
            "resource_after_query_wall_ns": 60 if resource else -1,
            "allocations": 0, "deletions": 0, "checksum": index,
            "wall_start_valid": 1, "wall_end_valid": 1, "cpu_start_valid": 1, "cpu_end_valid": 1,
            "entry_wall_valid": 1, "exit_wall_valid": 1, "entry_cpu_valid": 1, "exit_cpu_valid": 1,
            "entry_wall_error": 0, "exit_wall_error": 0, "entry_cpu_error": 0, "exit_cpu_error": 0,
            "worker_snapshot_before_valid": int(cab and enabled), "worker_snapshot_after_valid": int(cab and enabled),
            "resource_before_valid": int(resource), "resource_after_valid": int(resource),
        }
        for counter in ("minor_faults", "major_faults", "voluntary_switches", "involuntary_switches", "task_faults", "task_pageins"):
            supported = resource and ((resources == "thread_rusage" and not counter.startswith("task_"))
                                      or (resources == "process_task_events" and counter.startswith("task_")))
            row[counter + "_before"] = index * 2 if supported else -1
            row[counter + "_after"] = index * 2 + 1 if supported else -1
            row[counter + "_delta"] = 1 if supported else -1
        for point, cpu_value in (("before_start", start - 80), ("before_end", start - 20),
                                 ("after_start", start + cpu + 20), ("after_end", start + cpu + 80)):
            row["resource_" + point + "_cpu_ns"] = cpu_value if resource else 0
            for clock in ("wall", "cpu"):
                row["resource_" + point + "_" + clock + "_valid"] = int(resource)
                row["resource_" + point + "_" + clock + "_error"] = 0
        if enabled:
            root = len(stages) + 1
            stages.append(event(root, 0, 0, index, "callback", start + 10, start + wall - 10,
                                start + 10, start + cpu - 10, sequence=sequence))
            if cab:
                for lane in range(3):
                    for mic in range(2):
                        begin = start + 100 + (lane * 2 + mic) * 6000
                        identifier = len(stages) + 1
                        stages.append(event(identifier, root, 1, index, "cab_process", begin, begin + 5000,
                                            begin, begin + 4000, lane=lane, mic=mic, sequence=sequence))
                        stages.append(event(identifier + 1, identifier, 2, index, "active_convolution",
                                            begin + 10, begin + 4900, begin + 10, begin + 3900,
                                            lane=lane, mic=mic, model=999, generation=100, sequence=sequence))
                        for point in ("before", "after"):
                            slots.append(dict(zip(SLOT_HEADER, [1, index, point, lane, mic, 999, 999, 100, 0, 0, 0, 0, 0, 4800, 0, 1, 0])))
            else:
                leaf = {"arithmetic": "arithmetic_control", "memory": "memory_control", "clock": "scope_calibration"}[mode]
                count = 64 if mode == "clock" else 1
                for leaf_index in range(count):
                    a = start + 20 + leaf_index * 50
                    stages.append(event(len(stages) + 1, root, 1, index, leaf, a, a + 10, a, a + 10,
                                        logical_bytes=12288 if mode == "memory" else 0))
            row["trace_event_count"] = len(stages) - row["trace_event_first"]
        callbacks.append(row)
    if cab and enabled:
        end = callbacks[1199]["wall_end_ns"] + 200_000
        worker.append(event(1, 0, 0, (1 << 64) - 1, "worker_run", callbacks[0]["wall_start_ns"] - 500,
                            end, 100, 300, sequence=1, sequence_end=20))
        for lane in range(3):
            for mic in range(2):
                begin = end + 10000 + (lane * 2 + mic) * 1000
                row = event(len(caller) + 1, 0, 0, (1 << 64) - 1, "worker_publish",
                            begin, begin + 100, begin, begin + 100, lane=lane, mic=mic, model=999, generation=100)
                row["role"] = "pending"
                caller.append(row)
    write_csv(Path(str(prefix) + ".callbacks.csv"), callbacks)
    write_csv(Path(str(prefix) + ".stages.csv"), stages, EVENT_HEADER)
    write_csv(Path(str(prefix) + ".worker.csv"), worker, EVENT_HEADER)
    write_csv(Path(str(prefix) + ".caller.csv"), caller, EVENT_HEADER)
    write_csv(Path(str(prefix) + ".slots.csv"), slots, SLOT_HEADER)
    capacities = [max(409600, len(stages)), 65536, 16384]
    metadata = {
        "schema_version": 1, "mode": mode, "engine": "original-v1", "channels": 1, "sample_rate": 96000,
        "block_size": block_size, "trace_level": level, "complete": True, "callback_count": 1600,
        "stage_count": len(stages), "worker_event_count": len(worker), "caller_event_count": len(caller),
        "slot_count": len(slots), "dropped": 0, "clock_errors": 0, "nesting_errors": 0,
        "audio_host_validation": "not_executed", "product_acceptance": False, "product_performance_change": False,
        "worker_trace_enabled": cab and enabled, "hooks_compiled": compiled, "source_commit": "a" * 40,
        "compiler": "synthetic-fixture-no-native-execution", "system": "synthetic-fixture",
        "wall_clock": "fixture_monotonic_ns", "thread_cpu_clock": "fixture_thread_cpu_ns",
        "clock_read_order": "wall_then_cpu_at_each_endpoint", "wall_minus_cpu_is_exclusive_scheduling_time": False,
        "resource_scope": resources if resource else "disabled", "resource_capture_requested": resource,
        "resource_capture_valid": resource, "macos_minor_major_faults_supported": False,
        "macos_resource_counters_include_worker": True, "macos_thread_state_is_continuous_scheduler_trace": False,
        "hardware_cache_counters_available": False, "logical_bytes_are_hardware_traffic": False,
        "pending_identity_available_in_snapshot": False, "pending_identity_source": "publication_swap_rejection_events",
        "semantic_valid": True, "worker_scheduling_valid": cab, "worker_start_succeeded": cab,
        "worker_entered_run": cab, "worker_observed_priority": -1 if cab else 0, "worker_cancelled_builds": 0,
        "qos_setter_called": False, "worker_priority_changed": False, "callback_allocations": 0,
        "callback_deletions": 0, "allocation_guard": "thread_local_cpp_new_delete_not_universal_malloc_or_lock_tracing",
        "callbacks_worker_phase": 1200, "callbacks_forced_phase": 400, "automation_callbacks": 80 if cab else 0,
        "paced_sleep_every": 8, "paced_sleep_ms": 1, "event_capacity": capacities[0],
        "worker_event_capacity": capacities[1], "caller_event_capacity": capacities[2], "event_size_bytes": 160,
        "allocated_event_bytes": sum(capacities) * 160, "control_memory_bytes": 1572864,
        "arithmetic_iterations": 256, "memory_touches_per_slot": 256,
        "memory_logical_bytes_per_callback": 12288 if mode == "memory" else 0,
        "calibration_empty_scopes": 64 if mode == "clock" and enabled else 0,
        "kernel_residual": 1e-8, "peak": .1, "max_step": .01, "deadline_misses": sum(r["deadline_miss"] for r in callbacks),
        "deadline_misses_are_product_gate": False, "checksum": 0, "recording_on_reference_oracle": cab and enabled,
        "caller_stream_work": "reference_impulse_and_forced_publication", "parameter_refresh_per_callback": False,
    }
    Path(str(prefix) + ".metadata.json").write_text(json.dumps(metadata), encoding="utf-8")
    return prefix


class EvidenceCorruptionTests(unittest.TestCase):
    def test_registered_block_sizes_keep_own_deadlines(self):
        for block in (32, 64, 256):
            with self.subTest(block=block), tempfile.TemporaryDirectory() as directory:
                fixture(Path(directory), block_size=block)
                result = verifier.verify_directory(directory, expected_block_size=block)
                self.assertEqual(result["exit_code"], 0, result)
                self.assertEqual(result["routes"][0]["callbacks"]["deadline_misses"], int(block != 256))

    def test_wrong_registered_block_is_rejected(self):
        self.assertEqual(self.result(expected_block_size=32)["exit_code"], 2)

    def test_changed_deadline_is_rejected(self):
        self.csv("callbacks", lambda rows: rows[0].update(deadline_ns="2666666.6666666665"))
        self.assert_invalid()

    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)
        self.prefix = fixture(self.root)

    def tearDown(self):
        self.temp.cleanup()

    def result(self, **expected):
        return verifier.verify_directory(self.root, **expected)

    def metadata(self, mutate):
        path = Path(str(self.prefix) + ".metadata.json")
        value = json.loads(path.read_text())
        mutate(value)
        path.write_text(json.dumps(value), encoding="utf-8")

    def csv(self, suffix, mutate):
        edit_csv(Path(str(self.prefix) + "." + suffix + ".csv"), mutate)

    def assert_invalid(self):
        result = self.result()
        self.assertEqual(result["exit_code"], 2, result)
        self.assertFalse(result["complete"])

    def test_complete_raw_cpu_above_wall_is_preserved(self):
        result = self.result(expected_mode="arithmetic", expected_trace_level="stages",
                             expected_engines=["original-v1"], expected_channels=[1])
        self.assertEqual(result["exit_code"], 0, result["errors"])
        callback = result["routes"][0]["callbacks"]
        self.assertEqual(callback["deadline_misses"], 1)
        self.assertEqual(callback["cpu_greater_than_wall_samples"], 1)
        self.assertEqual(callback["miss_records"][0]["cpu_ns"], "1200000")
        self.assertFalse(result["product_fix"])
        self.assertEqual(result["audio_host_validation"], "not_executed")

    def test_missing_callback(self):
        self.csv("callbacks", lambda rows: rows.pop(20))
        self.assert_invalid()

    def test_changed_deadline(self):
        self.csv("callbacks", lambda rows: rows[0].update(deadline_ns="1000000"))
        self.assert_invalid()

    def test_false_zero_miss(self):
        self.csv("callbacks", lambda rows: rows[0].update(deadline_miss="0"))
        self.assert_invalid()

    def test_invalid_clock_cannot_be_marked_valid(self):
        self.csv("callbacks", lambda rows: rows[0].update(cpu_start_error="22"))
        self.assert_invalid()

    def test_recorded_dropped_event_blocks_completion(self):
        self.metadata(lambda value: value.update(dropped=1))
        self.assert_invalid()

    def test_missing_worker_file_is_incomplete(self):
        Path(str(self.prefix) + ".worker.csv").unlink()
        result = self.result()
        self.assertEqual(result["exit_code"], 3, result)
        self.assertFalse(result["complete"])

    def test_bad_parent(self):
        self.csv("stages", lambda rows: rows[1].update(parent_id="0"))
        self.assert_invalid()

    def test_child_outside_callback_envelope(self):
        def mutate(rows):
            rows[1]["wall_end_ns"] = str(int(rows[0]["wall_end_ns"]) + 5000)
            rows[1]["wall_ns"] = str(int(rows[1]["wall_end_ns"]) - int(rows[1]["wall_start_ns"]))
        self.csv("stages", mutate)
        self.assert_invalid()

    def test_incomplete_producer_receipt_stays_incomplete(self):
        self.metadata(lambda value: value.update(complete=False))
        result = self.result()
        self.assertEqual(result["exit_code"], 3, result)
        self.assertFalse(result["complete"])

    def test_missing_stage_is_not_imputed(self):
        self.csv("stages", lambda rows: rows.pop())
        self.assert_invalid()

    def test_unavailable_counter_cannot_be_zero_measurement(self):
        self.csv("callbacks", lambda rows: rows[0].update(minor_faults_before="0", minor_faults_after="0", minor_faults_delta="0"))
        self.assert_invalid()

    def test_expected_grid_detects_missing_route(self):
        result = self.result(expected_engines=["original-v1", "expanded-v2"], expected_channels=[1])
        self.assertEqual(result["exit_code"], 3, result)

    def test_empty_directory_never_passes(self):
        empty = self.root / "empty"
        empty.mkdir()
        self.assertEqual(verifier.verify_directory(empty)["exit_code"], 3)

    def test_compiled_out_control_is_valid_without_attribution(self):
        control = self.root / "control"
        fixture(control, level="off", compiled=False)
        result = verifier.verify_directory(control, expected_mode="arithmetic", expected_trace_level="off")
        self.assertEqual(result["exit_code"], 0, result["errors"])
        self.assertFalse(result["routes"][0]["attribution_available"])
        self.assertEqual(result["routes"][0]["stage_trace"]["event_count"], 0)

    def test_resource_raw_delta_corruption_is_detected(self):
        directory = self.root / "resources"
        prefix = fixture(directory, level="resources")
        good = verifier.verify_directory(directory)
        self.assertEqual(good["exit_code"], 0, good["errors"])
        edit_csv(Path(str(prefix) + ".callbacks.csv"), lambda rows: rows[0].update(minor_faults_delta="999"))
        self.assertEqual(verifier.verify_directory(directory)["exit_code"], 2)

    def test_mac_task_scope_does_not_invent_thread_faults(self):
        directory = self.root / "task"
        fixture(directory, level="resources", resources="process_task_events")
        result = verifier.verify_directory(directory)
        self.assertEqual(result["exit_code"], 0, result["errors"])
        self.assertEqual(result["routes"][0]["callbacks"]["miss_records"][0]["minor_faults_delta"], "-1")

    def test_memory_control_logical_bytes_are_explicit(self):
        directory = self.root / "memory"
        fixture(directory, mode="memory")
        result = verifier.verify_directory(directory)
        self.assertEqual(result["exit_code"], 0, result["errors"])
        memory = next(row for row in result["routes"][0]["stage_trace"]["stages"] if row["stage"] == "memory_control")
        self.assertEqual(memory["logical_bytes"], 1600 * 12288)
        self.assertFalse(result["routes"][0]["stage_trace"]["logical_bytes_are_hardware_traffic"])

    def test_cab_slots_and_separate_caller_worker_streams(self):
        directory = self.root / "cab"
        prefix = fixture(directory, mode="cab")
        result = verifier.verify_directory(directory)
        self.assertEqual(result["exit_code"], 0, result["errors"])
        self.assertEqual(result["routes"][0]["slot_count"], 19200)
        edit_csv(Path(str(prefix) + ".slots.csv"), lambda rows: rows.pop())
        self.assertEqual(verifier.verify_directory(directory)["exit_code"], 2)

    def test_worker_event_cannot_impersonate_callback(self):
        directory = self.root / "cab"
        prefix = fixture(directory, mode="cab")
        edit_csv(Path(str(prefix) + ".worker.csv"), lambda rows: rows[0].update(callback_index="0"))
        self.assertEqual(verifier.verify_directory(directory)["exit_code"], 2)

    def test_pending_identity_is_not_guessed(self):
        directory = self.root / "cab"
        prefix = fixture(directory, mode="cab")
        edit_csv(Path(str(prefix) + ".slots.csv"), lambda rows: rows[0].update(pending_identity_available="1"))
        self.assertEqual(verifier.verify_directory(directory)["exit_code"], 2)


if __name__ == "__main__":
    unittest.main()
