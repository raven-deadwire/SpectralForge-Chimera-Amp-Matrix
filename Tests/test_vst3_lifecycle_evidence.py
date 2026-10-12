#!/usr/bin/env python3
"""Negative evidence tests. Fixtures are NOT executions of Chimera or any DAW."""
from __future__ import annotations

import copy
import json
from pathlib import Path
import sys
import subprocess
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "Tools"))
import verify_vst3_lifecycle as verifier


class Fixture:
    def __init__(self, scenario, pid):
        self.scenario, self.pid = scenario, pid
        self.events, self.trace = [], []
        self.round, self.op, self.next_id = 0, 0, 0
        self.editor = not scenario.startswith("headless")
        self.opened = set()

    def event(self, phase, edge, instance=0, operation=0):
        seq = len(self.events) + 1
        item = {"schema": 1, "scenario": self.scenario, "pid": self.pid, "tid": 42,
                "seq": seq, "round": self.round, "instance": instance, "operation_id": operation,
                "phase": phase, "edge": edge, "monotonic_us": seq * 100}
        if edge == "return":
            item["elapsed_us"] = 100
        self.events.append(item)

    def phase(self, name, instance=0, inner=lambda: None):
        self.op += 1
        op = self.op
        self.event(name, "begin", instance, op)
        inner()
        self.event(name, "return", instance, op)

    def log(self, event, instance, related=None):
        fields = f" pid={self.pid} tid=42"
        if related is not None:
            fields += f" related={related:x}"
        if event.startswith(("processor.", "editor.")):
            fields += " message_thread=2a on_message_thread=1"
        self.trace.append(f"{len(self.trace) + 1} us thread=2a instance={instance:x} {event}{fields}")

    def scope(self, event, instance, related=None):
        self.log(event + ".begin", instance, related)
        self.log(event + ".end", instance, related)

    def open_editor(self, instance):
        self.phase("editor.create", instance)
        self.phase("editor.attach", instance)
        self.scope("editor.create.body", instance + 1000, instance)
        self.opened.add(instance)

    def close_editor(self, instance):
        if instance not in self.opened:
            return
        for name in ("view.removed", "view.set_frame_null", "view.release", "host_window.destroy"):
            self.phase(name, instance)
        self.scope("editor.destroy.body", instance + 1000, instance)
        self.log("editor.destroy.members.end", instance + 1000, instance)
        self.opened.remove(instance)

    def audio(self, instance):
        self.phase("audio.worker_join", instance, lambda: self.phase("audio.worker", instance, lambda: (
            self.phase("audio.start_processing", instance), self.phase("audio.stop_processing", instance))))

    def create(self):
        self.next_id += 1
        instance = self.next_id
        self.phase("instance.create", instance)
        self.log("processor.create.body.begin", instance)
        for worker, offset in (("ir", 2000), ("tuner", 3000)):
            self.log(worker + ".owner", instance + offset, instance)
        self.log("processor.create.body.end", instance)
        for worker, offset in (("ir", 2000), ("tuner", 3000)):
            self.log(worker + ".worker.begin", instance + offset)
        if self.editor:
            self.open_editor(instance)
        self.audio(instance)
        return instance

    def destroy(self, instance, controller_first=True):
        self.close_editor(instance)
        for name in ("component.deactivate", "component.disconnect", "controller.disconnect",
                     "connections.release", "controller.clear_handler"):
            self.phase(name, instance)
        order = ("controller", "component") if controller_first else ("component", "controller")
        for kind in order:
            self.phase(kind + ".terminate", instance)
            if kind == "component":
                self.phase("audio.release", instance)
            self.phase(kind + ".release", instance)
        self.log("processor.destroy.body.begin", instance)
        for worker, offset in (("ir", 2000), ("tuner", 3000)):
            self.log(worker + ".stop.begin", instance + offset)
            self.log(worker + ".worker.end", instance + offset)
            self.log(worker + ".stop.end", instance + offset)
        self.log("processor.destroy.body.end", instance)
        self.log("processor.destroy.members.end", instance)

    def build(self):
        self.phase("host.com_initialize")
        rounds = 3 if self.scenario == "reopen-project" else 1
        for number in range(1, rounds + 1):
            self.round = number
            for name in ("module.load_library", "module.init", "factory.get"):
                self.phase(name)
            multi = self.scenario == "multiple-instances" or self.scenario.startswith("live-remove")
            instances = [self.create() for _ in range(3 if multi else 1)]
            controller_first = not self.scenario.endswith("component-first")
            if self.scenario.startswith("editor-closed-before-project"):
                self.close_editor(instances[0])
                self.phase("project.idle_after_editor_close", instances[0], lambda: self.audio(instances[0]))
            elif self.scenario.startswith("remove-last"):
                for cycle in range(2):
                    self.close_editor(instances[0])
                    self.phase("plugin.remove", instances[0], lambda: self.destroy(instances[0], controller_first))
                    instances.clear()
                    self.phase("project.empty_idle")
                    if cycle == 0:
                        self.phase("instance.recreate", inner=lambda: instances.append(self.create()))
            elif self.scenario.startswith("live-remove"):
                for _ in range(3):
                    self.phase("plugin.remove", instances[0], lambda: self.destroy(instances[0], controller_first))
                    for peer in instances[1:]:
                        self.audio(peer)
                        self.close_editor(peer)
                        self.open_editor(peer)
                    self.phase("instance.recreate", inner=lambda: instances.__setitem__(0, self.create()))
            self.phase("project.close", inner=lambda: [self.phase("instance.destroy", item,
                lambda item=item: self.destroy(item, controller_first)) for item in instances])
            for name in ("factory.release", "module.exit", "module.free_library", "module.unmapped_check",
                         "host.references_check", "host.release", "host.after_unload_pump"):
                self.phase(name)
            self.event("round.completed", "point")
        self.phase("host.com_uninitialize")
        self.event("host.return_ready", "point")
        return self


class EvidenceTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.fixtures = [Fixture(name, 100 + n).build() for n, name in enumerate(verifier.SCENARIOS)]
        self.rows = [{"scenario": f.scenario, "pid": f.pid, "exit_code": 0, "timed_out": False,
                      "force_terminated": False, "process_exit_confirmed": True, "elapsed_ms": 5000,
                      "return_ready_seen": True, "capture_error": False,
                      "watchdog_ms": 45000, "pending_phases": [], "log_file": f.scenario + ".log",
                      "plugin_trace_file": f.scenario + ".trace.log"} for f in self.fixtures]
        self.summary = {"schema": 1, "parent_pid": 10, "full_suite": True,
                        "total": len(self.rows), "passed": len(self.rows), "scenarios": self.rows}
        self.write()

    def write(self):
        aggregate = []
        for f in self.fixtures:
            (self.root / (f.scenario + ".log")).write_text("\n".join("LIFECYCLE_EVENT " + json.dumps(e) for e in f.events) + "\n")
            (self.root / (f.scenario + ".trace.log")).write_text("\n".join(f.trace) + "\n")
            aggregate.extend(f.events)
            aggregate.append({"schema": 1, "scenario": f.scenario, "pid": 10, "child_pid": f.pid,
                              "phase": "host.process_exit", "edge": "point", "process_exit_confirmed": True,
                              "exit_code": 0, "timed_out": False})
        (self.root / "events.jsonl").write_text("\n".join(json.dumps(e) for e in aggregate) + "\n")
        (self.root / "summary.json").write_text(json.dumps(self.summary))

    def test_complete_synthetic_fixture_exercises_verifier_only(self):
        result = verifier.verify(self.root, 10)
        self.assertEqual(len(result["scenarios"]), 12)
        self.assertEqual(len(result["raw_files"]), 26)
        self.assertEqual(result["scenarios"][4]["rounds"], 3)

    def test_stale_parent_process_is_rejected(self):
        with self.assertRaisesRegex(verifier.EvidenceError, "parent PID"):
            verifier.verify(self.root, 11)

    def test_missing_duplicate_unknown_or_selected_scenario_is_rejected(self):
        original = copy.deepcopy(self.summary)
        for mode in ("missing", "duplicate", "unknown", "selected"):
            self.summary = copy.deepcopy(original)
            if mode == "missing": self.summary["scenarios"].pop()
            if mode == "duplicate": self.summary["scenarios"][-1] = self.summary["scenarios"][0]
            if mode == "unknown": self.summary["scenarios"][-1]["scenario"] = "unknown"
            if mode == "selected": self.summary["full_suite"] = False
            self.write()
            with self.subTest(mode=mode), self.assertRaises(verifier.EvidenceError): verifier.verify(self.root)

    def test_timeout_crash_force_kill_and_unconfirmed_exit_cannot_pass(self):
        for field, value in (("timed_out", True), ("exit_code", 124), ("force_terminated", True),
                             ("capture_error", True), ("return_ready_seen", False),
                             ("process_exit_confirmed", False), ("pending_phases", [{"phase": "module.exit"}])):
            original = self.rows[0][field]
            self.rows[0][field] = value
            self.write()
            with self.subTest(field=field), self.assertRaises(verifier.EvidenceError): verifier.verify(self.root)
            self.rows[0][field] = original

    def test_parent_exit_record_must_exist_and_agree(self):
        path = self.root / "events.jsonl"
        original = path.read_text()
        for mode in ("missing", "wrong_pid", "crash"):
            events = [json.loads(line) for line in original.splitlines()]
            event = next(e for e in events if e.get("child_pid") == 100)
            if mode == "missing": events.remove(event)
            if mode == "wrong_pid": event["child_pid"] = 999
            if mode == "crash": event["exit_code"] = 1
            path.write_text("\n".join(json.dumps(e) for e in events) + "\n")
            with self.subTest(mode=mode), self.assertRaisesRegex(verifier.EvidenceError, "parent process-exit"):
                verifier.verify(self.root)

    def test_pending_or_unmatched_api_calls_are_rejected(self):
        f = self.fixtures[0]
        f.events.remove(next(e for e in f.events if e["phase"] == "module.free_library" and e["edge"] == "return"))
        self.write()
        with self.assertRaisesRegex(verifier.EvidenceError, "pending operations"):
            verifier.verify(self.root)

    def test_missing_unload_boundary_is_rejected_even_with_passing_exit(self):
        self.fixtures[0].events = [e for e in self.fixtures[0].events if e["phase"] != "module.unmapped_check"]
        self.write()
        with self.assertRaisesRegex(verifier.EvidenceError, "module.unmapped_check"):
            verifier.verify(self.root)

    def test_final_removal_must_include_both_remove_cycles(self):
        f = self.fixtures[-1]
        op = [e["operation_id"] for e in f.events if e["phase"] == "plugin.remove" and e["edge"] == "begin"][-1]
        f.events = [e for e in f.events if e["operation_id"] != op]
        self.write()
        with self.assertRaisesRegex(verifier.EvidenceError, "remove/recreate coverage"):
            verifier.verify(self.root)

    def test_named_component_first_order_cannot_be_substituted(self):
        f = self.fixtures[7]
        component = [e for e in f.events if e["phase"] in ("component.terminate", "audio.release", "component.release")]
        controller = [e for e in f.events if e["phase"] in ("controller.terminate", "controller.release")]
        first = f.events.index(component[0])
        f.events = [e for e in f.events if e not in component + controller]
        f.events[first:first] = controller + component
        for n, event in enumerate(f.events):
            event.update(seq=n + 1, monotonic_us=(n + 1) * 100)
        self.write()
        with self.assertRaisesRegex(verifier.EvidenceError, "component.release did not return before controller.terminate"):
            verifier.verify(self.root)

    def test_module_reload_does_not_count_as_recreation_in_same_module(self):
        f = self.fixtures[-1]
        f.phase("module.load_library")
        self.write()
        with self.assertRaisesRegex(verifier.EvidenceError, "module.load_library"):
            verifier.verify(self.root)

    def test_source_mismatch_is_recorded_as_failure_before_host_launch(self):
        metadata = self.root / "build.json"
        metadata.write_text(json.dumps({"source_sha": "a" * 40}))
        result = subprocess.run([sys.executable, str(Path(verifier.__file__)),
                                 "--host", str(self.root / "missing-host.exe"),
                                 "--plugin", str(self.root / "missing-plugin.vst3"),
                                 "--evidence-dir", str(self.root / "run"),
                                 "--build-metadata", str(metadata), "--source-sha", "b" * 40],
                                capture_output=True, text=True, timeout=10)
        self.assertEqual(result.returncode, 1)
        report = json.loads((self.root / "run/verification.json").read_text())
        self.assertIn("source SHA mismatch", report["error"])
        self.assertFalse(report["issue_3_resolved"])
        self.assertEqual(set(report["commercial_daw_acceptance"].values()), {"NOT_RUN"})
        self.assertNotIn("parent_pid", report)

    def test_aggregate_must_match_unmodified_child_log(self):
        path = self.root / self.rows[0]["log_file"]
        path.write_text(path.read_text().replace('"phase": "module.exit"', '"phase": "fabricated"'))
        with self.assertRaisesRegex(verifier.EvidenceError, "raw child log"):
            verifier.verify(self.root)

    def test_empty_wrong_pid_and_unterminated_plugin_traces_are_rejected(self):
        path = self.root / self.rows[0]["plugin_trace_file"]
        original = path.read_text()
        variants = ("", original.replace("pid=100", "pid=999"),
                    "\n".join(line for line in original.splitlines() if "processor.destroy.members.end" not in line) + "\n")
        for value in variants:
            path.write_text(value)
            with self.subTest(value=value[:30]), self.assertRaises(verifier.EvidenceError): verifier.verify(self.root)

    def test_derived_body_end_does_not_substitute_for_member_completion(self):
        f = self.fixtures[1]
        f.trace = [line for line in f.trace if "editor.destroy.members.end" not in line]
        self.write()
        with self.assertRaisesRegex(verifier.EvidenceError, "live editor|unfinished plugin"):
            verifier.verify(self.root)

    def test_worker_stop_return_cannot_precede_worker_exit(self):
        f = self.fixtures[0]
        stop = next(i for i, line in enumerate(f.trace) if "ir.stop.end" in line)
        end = next(i for i, line in enumerate(f.trace) if "ir.worker.end" in line)
        f.trace[stop], f.trace[end] = f.trace[end], f.trace[stop]
        self.write()
        with self.assertRaisesRegex(verifier.EvidenceError, "worker still active"):
            verifier.verify(self.root)

    def test_complete_but_truncated_reopen_trace_cannot_pass(self):
        f = self.fixtures[4]
        end = next(i for i, line in enumerate(f.trace) if "processor.destroy.members.end" in line)
        f.trace = f.trace[:end + 1]
        self.write()
        with self.assertRaisesRegex(verifier.EvidenceError, "instance counts differ"):
            verifier.verify(self.root)

    def test_malformed_duplicate_fields_and_unsafe_files_rejected(self):
        with self.assertRaisesRegex(verifier.EvidenceError, "Duplicate JSON"):
            verifier.parse_json('{"pid":1,"pid":2}')
        for name in ("../outside.log", "C:\\outside.log", "/outside.log", ""):
            with self.subTest(name=name), self.assertRaises(verifier.EvidenceError): verifier.raw_file(self.root, name)


if __name__ == "__main__":
    unittest.main()
