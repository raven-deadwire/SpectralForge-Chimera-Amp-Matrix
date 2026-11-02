#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Probe safety and evidence tests; no native OBS process is launched."""
import argparse
import contextlib
import io
import json
import os
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

import obs_startup_probe as probe


class FakeProcess:
    pid = 123
    returncode = None

    def poll(self):
        return self.returncode


class FakeObs:
    instances = []
    failed_request_number = None
    failed_start_number = None
    force_registered = None
    log_extra = ""

    def __init__(self, layout, _port, log):
        self.layout = layout
        self.process = FakeProcess()
        self.requests = []
        self.number = len(type(self).instances) + 1
        type(self).instances.append(self)
        module = layout.plugin.exists()
        logs = layout.config / "logs"
        logs.mkdir()
        text = (f"[chimera-obs] registered {probe.app.FILTER_KIND}\n"
                "[chimera-obs] module test\n") if module else "OBS startup complete\n"
        (logs / "run.txt").write_text(text + self.log_extra, encoding="utf-8")
        log.write_text("OBS stdout\n", encoding="utf-8")
        if self.number == self.failed_start_number:
            raise RuntimeError("startup failed and process could not be reaped")

    def phase(self, _name):
        pass

    def request(self, kind, data=None):
        self.requests.append(kind)
        if self.number == self.failed_request_number and kind == "CreateScene":
            raise TimeoutError("CreateScene test timeout")
        if kind == "GetVersion":
            return {"obsVersion": "32.2.2", "obsWebSocketVersion": "5.7.4"}
        if kind == "GetSourceFilterKindList":
            registered = self.layout.plugin.exists() if self.force_registered is None else self.force_registered
            return {"sourceFilterKinds": [probe.app.FILTER_KIND] if registered else []}
        if kind == "GetSceneList":
            return {"scenes": [{"sceneName": "Scene"}]}
        if kind == "GetInputList":
            return {"inputs": [{"inputName": probe.app.SOURCE}] if "CreateInput" in self.requests else []}
        if kind == "GetSourceFilterList":
            return {"filters": []}
        return {}

    def quit(self):
        self.process.returncode = 0


class StartupProbeTests(unittest.TestCase):
    def setUp(self):
        temp = tempfile.TemporaryDirectory()
        self.addCleanup(temp.cleanup)
        self.root = Path(temp.name)
        self.module = self.root / "staged-module"
        (self.module / "bin" / "64bit").mkdir(parents=True)
        (self.module / "bin" / "64bit" / "chimera-obs.dll").write_bytes(b"module")
        (self.module / "data").mkdir()
        self.obs = self.root / "obs64.exe"
        self.obs.write_bytes(b"obs")
        self.args = argparse.Namespace(obs=str(self.obs), module=str(self.module), vst3=None,
                                       work=str(self.root / "evidence"), repeats=3, stop_after="input")
        self.config = self.root / "appdata" / "obs-studio"
        self.plugin = self.root / "programdata" / "obs-studio" / "plugins" / "chimera-obs"
        FakeObs.instances = []
        FakeObs.failed_request_number = None
        FakeObs.failed_start_number = None
        FakeObs.force_registered = None
        FakeObs.log_extra = ""
        self.stack = contextlib.ExitStack()
        self.addCleanup(self.stack.close)
        self.stack.enter_context(patch.object(probe.app, "WINDOWS", True))
        self.stack.enter_context(patch.object(probe.app, "MACOS", False))
        self.stack.enter_context(patch.object(probe.app, "Obs", FakeObs))
        self.stack.enter_context(patch.object(probe.app, "free_port", return_value=4455))
        self.stack.enter_context(patch.object(probe.app, "write_di", lambda path: path.write_bytes(b"DI")))
        self.stack.enter_context(patch.object(probe, "environment", return_value={"source_sha": "test-sha"}))
        self.stack.enter_context(patch.dict(os.environ, {"APPDATA": str(self.root / "appdata"),
                                                       "ProgramData": str(self.root / "programdata")}))
        self.stack.enter_context(contextlib.redirect_stderr(io.StringIO()))

    def test_failure_is_retained_while_safe_alternating_cases_continue(self):
        FakeObs.failed_request_number = 1
        summary = probe.run(self.args)
        self.assertFalse(summary["passed"])
        self.assertFalse(summary["aborted"])
        self.assertEqual([case["mode"] for case in summary["cases"]],
                         ["absent", "module", "module", "absent", "absent", "module"])
        self.assertEqual([case["status"] for case in summary["cases"]], ["failed"] + ["passed"] * 5)
        self.assertIn("CreateScene test timeout", summary["cases"][0]["errors"][0]["traceback"])
        self.assertFalse(summary["cases"][0]["log_control"]["module_registered_in_log"])
        self.assertEqual(summary["cases"][0]["log_control"]["instance_log_entries"], 0)
        saved = json.loads((Path(self.args.work) / "summary.json").read_text())
        self.assertFalse(saved["passed"])
        self.assertEqual(len(saved["cases"]), 6)
        self.assertFalse(self.config.exists())
        self.assertFalse(self.plugin.exists())
        for index, case in enumerate(summary["cases"], 1):
            folder = Path(self.args.work) / f"{index:02d}-{case['mode']}"
            self.assertTrue((folder / "obs-config" / "logs" / "run.txt").is_file())
            self.assertTrue((folder / "result.json").is_file())
            self.assertIn("sha256", next(iter(case["environment"]["binaries"]["obs_executable"]["files"].values())))

    def test_absent_control_cannot_pass_when_another_copy_registers_chimera(self):
        FakeObs.force_registered = True
        self.args.repeats = 1
        summary = probe.run(self.args)
        self.assertFalse(summary["passed"])
        self.assertIn("invalid absent control", summary["cases"][0]["errors"][0]["message"])
        self.assertEqual(summary["cases"][1]["status"], "passed")

    def test_instance_activity_invalidates_a_registered_only_control(self):
        FakeObs.log_extra = "[chimera-obs] 'unexpected': loaded Chimera\n"
        self.args.repeats = 1
        summary = probe.run(self.args)
        self.assertFalse(summary["passed"])
        self.assertTrue(all(case["status"] == "failed" for case in summary["cases"]))
        self.assertIn("instance activity", summary["cases"][1]["errors"][0]["message"])

    def test_existing_user_paths_are_refused_without_cleanup(self):
        for path in (self.config, self.plugin, Path(self.args.work)):
            with self.subTest(path=path):
                path.mkdir(parents=True)
                marker = path / "user-file.txt"
                marker.write_text("keep", encoding="utf-8")
                with self.assertRaisesRegex(RuntimeError, "refusing existing path"):
                    probe.run(self.args)
                self.assertEqual(marker.read_text(), "keep")
                self.assertEqual(FakeObs.instances, [])
                marker.unlink()
                path.rmdir()

    def test_unreaped_startup_failure_stops_and_keeps_owned_config_and_plugin(self):
        FakeObs.failed_start_number = 2
        summary = probe.run(self.args)
        self.assertTrue(summary["aborted"])
        self.assertFalse(summary["passed"])
        self.assertEqual(len(summary["cases"]), 2)
        self.assertEqual(len(summary["not_run"]), 4)
        self.assertTrue((self.config / probe.OWNER).exists())
        self.assertTrue((self.plugin / probe.OWNER).exists())
        self.assertTrue((Path(self.args.work) / "02-module" / "obs-config" / "logs" / "run.txt").exists())

    def test_initial_sequence_precedes_control_requests_and_never_creates_filter(self):
        self.args.repeats = 1
        summary = probe.run(self.args)
        self.assertTrue(summary["passed"])
        for obs in FakeObs.instances:
            self.assertEqual(obs.requests[:5], ["GetVersion", "CreateScene", "SetCurrentProgramScene",
                                               "CreateInput", "GetSourceFilterKindList"])
            self.assertNotIn("CreateSourceFilter", obs.requests)
            self.assertNotIn("StartRecord", obs.requests)
        self.assertFalse(Path(self.args.vst3).exists())

    def test_stop_after_version_does_not_enter_scene_or_input_paths(self):
        self.args.repeats = 1
        self.args.stop_after = "version"
        summary = probe.run(self.args)
        self.assertTrue(summary["passed"])
        for obs in FakeObs.instances:
            self.assertEqual(obs.requests[:2], ["GetVersion", "GetSourceFilterKindList"])
            self.assertNotIn("CreateScene", obs.requests)
            self.assertNotIn("CreateInput", obs.requests)
            self.assertNotIn("GetSceneList", obs.requests)
            self.assertNotIn("GetInputList", obs.requests)
            self.assertNotIn("GetSourceFilterList", obs.requests)


if __name__ == "__main__":
    unittest.main()
