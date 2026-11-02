#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Failure-path regressions for the real OBS harness; no OBS installation needed."""
import contextlib
import io
import json
import shutil
import socket
import subprocess
import tempfile
import threading
import time
import unittest
from pathlib import Path
from types import SimpleNamespace
from unittest import mock

import obs_app_test as app


class Replies:
    def __init__(self, messages):
        self.messages = iter(messages)
        self.sent = []
        self.deadlines = []

    def send(self, message, deadline=None):
        self.sent.append(message)
        self.deadlines.append(deadline)

    def receive(self, deadline=None):
        self.deadlines.append(deadline)
        return next(self.messages)

    def close(self):
        pass


def reply(request_id="1", result=True, code=100, data=None):
    return {"op": 7, "d": {"requestId": request_id,
            "requestStatus": {"result": result, "code": code}, "responseData": data or {}}}


class HarnessDiagnosticsTests(unittest.TestCase):
    def setUp(self):
        self.temp = self.enterContext(tempfile.TemporaryDirectory())
        self.work = Path(self.temp)
        self.enterContext(contextlib.redirect_stdout(io.StringIO()))
        self.enterContext(contextlib.redirect_stderr(io.StringIO()))
        self.enterContext(mock.patch.object(app, "WINDOWS", False))
        self.enterContext(mock.patch.object(app, "MACOS", False))
        self.obs = app.Obs.__new__(app.Obs)
        self.obs.layout = SimpleNamespace(config=self.work / "config")
        self.obs.log = self.work / "obs-run.txt"
        self.obs.trace_path = self.work / "obs-run.events.jsonl"
        self.obs.output = self.obs.log.open("w")
        self.addCleanup(self.obs.output.close)
        self.obs.started = time.monotonic()
        self.obs._phase = "scene-setup"
        self.obs.request_timeout = 0.05
        self.obs.last_request = None
        self.obs.next_id = 0
        self.obs.ws = Replies([])
        self.obs.process = mock.Mock(pid=12345, returncode=None)
        self.obs.process.poll.return_value = None

    def events(self):
        return [json.loads(line) for line in self.obs.trace_path.read_text().splitlines()]

    def test_request_correlation_and_metadata_do_not_expose_state(self):
        self.obs.ws = Replies([{"op": 5, "d": {"eventType": "SceneCreated"}}, reply("older"),
                               reply(data={"private_state": "response-secret"})])
        result = self.obs.request("SetSourceFilterSettings", {"vst3_component_state": "request-secret"})
        self.assertEqual(result, {"private_state": "response-secret"})
        self.assertEqual(len(set(self.obs.ws.deadlines)), 1)
        self.assertEqual([e["event"] for e in self.events()], ["request-begin", "request-response"])
        end = self.events()[-1]
        self.assertEqual((end["request_type"], end["request_id"], end["code"], end["pid"]),
                         ("SetSourceFilterSettings", "1", 100, 12345))
        self.assertEqual(end["phase"], "scene-setup")
        self.assertTrue(end["utc"].endswith("+00:00"))
        self.assertNotIn("secret", self.obs.trace_path.read_text())

    def test_not_ready_response_is_recorded_without_turning_probe_into_failure(self):
        self.obs.ws = Replies([reply(result=False, code=207), reply("2")])
        self.assertFalse(self.obs._ready())
        self.assertTrue(self.obs._ready())
        self.assertEqual([e["code"] for e in self.events() if e["event"] == "request-response"], [207, 100])

    def test_negative_response_keeps_status_and_existing_ok_false_semantics(self):
        self.obs.ws = Replies([reply(result=False, code=600), reply("2", result=False, code=600)])
        self.assertEqual(self.obs.request("GetSourceFilterList", ok=False), {})
        with self.assertRaisesRegex(RuntimeError, "GetSourceFilterList failed"):
            self.obs.request("GetSourceFilterList")
        self.assertFalse(self.events()[-1]["result"])
        self.assertEqual(self.events()[-1]["code"], 600)

    def test_unrelated_frames_cannot_extend_request_deadline(self):
        client, server = socket.socketpair()
        self.addCleanup(client.close)
        self.addCleanup(server.close)
        self.obs.ws = app.WebSocket.__new__(app.WebSocket)
        self.obs.ws.sock, self.obs.ws.buffer, self.obs.ws.timeout = client, b"", 30.0
        stop = threading.Event()
        payload = json.dumps(reply("unrelated")).encode()
        self.assertLess(len(payload), 126)
        frame = bytes([0x81, len(payload)]) + payload

        def traffic():
            server.settimeout(0.1)
            while not stop.is_set():
                try:
                    server.sendall(frame)
                except OSError:
                    break
                stop.wait(0.001)

        thread = threading.Thread(target=traffic, daemon=True)
        thread.start()
        started = time.monotonic()
        try:
            with self.assertRaisesRegex(RuntimeError, "CreateScene id=1") as caught:
                self.obs.request("CreateScene", {"sceneName": "test"})
        finally:
            stop.set()
            thread.join(timeout=1)
        self.assertIsInstance(caught.exception.__cause__, TimeoutError)
        self.assertLess(time.monotonic() - started, 2)
        self.assertEqual(self.events()[-1]["event"], "request-failure")
        self.assertEqual(self.events()[-1]["request_type"], "CreateScene")

    def test_partial_frame_is_bounded_by_same_deadline(self):
        client, server = socket.socketpair()
        self.addCleanup(client.close)
        self.addCleanup(server.close)
        ws = app.WebSocket.__new__(app.WebSocket)
        ws.sock, ws.buffer, ws.timeout = client, b"", 30.0
        server.sendall(b"\x81\x20{")  # Declared payload never finishes.
        with self.assertRaises(TimeoutError):
            ws.receive(deadline=time.monotonic() + 0.02)

    def test_primary_request_error_survives_shutdown_timeout_and_process_is_reaped(self):
        self.obs.process.wait.side_effect = [subprocess.TimeoutExpired("obs", 60), -9]
        original = RuntimeError("CreateScene id=2 timed out")
        with self.assertRaises(RuntimeError) as caught:
            try:
                raise original
            finally:
                self.obs.quit()
        self.assertIs(caught.exception, original)
        self.assertTrue(any("did not shut down within 60 s" in note for note in original.__notes__))
        self.obs.process.kill.assert_called_once()
        self.assertEqual(self.obs.process.wait.call_args_list, [mock.call(timeout=60), mock.call(timeout=10)])
        self.assertTrue((self.work / "obs-run.test-failure.json").exists())
        self.assertTrue((self.work / "obs-run.shutdown-failure.json").exists())
        self.assertTrue(self.obs.output.closed)

    def test_shutdown_timeout_without_primary_is_still_failure(self):
        self.obs.process.wait.side_effect = [subprocess.TimeoutExpired("obs", 60), -9]
        with self.assertRaisesRegex(RuntimeError, "did not shut down within 60 s"):
            self.obs.quit()
        self.assertEqual(self.events()[-1]["event"], "process-reaped")
        self.obs.process.kill.assert_called_once()

    def test_startup_failure_keeps_original_and_reaps_process(self):
        self.obs.process.wait.return_value = -9
        layout = SimpleNamespace(obs="obs", options=[], env={}, cwd=self.work,
                                 config=self.work / "config")
        original = RuntimeError("startup handshake failed")
        with mock.patch.object(app.subprocess, "Popen", return_value=self.obs.process), \
             mock.patch.object(app.Obs, "_connect", side_effect=original), \
             mock.patch.dict(app.os.environ, {"ImageOS": "win25", "ImageVersion": "20261102.1.0",
                                             "GITHUB_SHA": "test-sha", "GITHUB_RUN_ID": "123",
                                             "GITHUB_RUN_ATTEMPT": "2", "GITHUB_TOKEN": "never-log-token"}):
            with self.assertRaises(RuntimeError) as caught:
                app.Obs(layout, 1, self.work / "startup.txt")
        self.assertIs(caught.exception, original)
        self.obs.process.kill.assert_called_once()
        self.obs.process.wait.assert_called_once_with(timeout=10)
        self.assertTrue((self.work / "startup.startup-failure.json").exists())
        trace = (self.work / "startup.events.jsonl").read_text()
        start = json.loads(trace.splitlines()[0])
        self.assertEqual(start["environment"]["ImageVersion"], "20261102.1.0")
        self.assertEqual(start["environment"]["GITHUB_RUN_ATTEMPT"], "2")
        self.assertEqual(start["python"], app.sys.version)
        self.assertTrue(start["platform"])
        self.assertNotIn("never-log-token", trace)

    def test_missing_main_window_still_kills_and_reaps(self):
        self.obs.process.wait.return_value = -9
        with mock.patch.object(app, "WINDOWS", True), \
             mock.patch.object(app, "close_windows", return_value=0), \
             mock.patch.object(self.obs, "capture_evidence"):
            with self.assertRaisesRegex(RuntimeError, "main window found to close"):
                self.obs.quit()
        self.obs.process.kill.assert_called_once()
        self.obs.process.wait.assert_called_once_with(timeout=10)

    def test_unexpected_exit_remains_failure(self):
        self.obs.process.poll.return_value = -11
        self.obs.process.wait.return_value = -11
        with self.assertRaisesRegex(RuntimeError, "exited cleanly \\(code -11\\)"):
            self.obs.quit()
        self.obs.process.kill.assert_not_called()

    def test_graceful_shutdown_preserves_log_before_config_cleanup_without_ui_probe(self):
        logs = self.obs.layout.config / "logs"
        logs.mkdir(parents=True)
        log_bytes = b"OBS version\nChimera registered\nClean shutdown\n"
        (logs / "obs.txt").write_bytes(log_bytes)

        def exited(timeout):
            self.obs.process.poll.return_value = 0
            return 0

        self.obs.process.wait.side_effect = exited
        with mock.patch.object(app, "WINDOWS", True), \
             mock.patch.object(app, "close_windows", return_value=1), \
             mock.patch.object(app, "dialog_text") as dialog, \
             mock.patch.object(app, "screenshot") as screen, \
             mock.patch.object(app.subprocess, "run") as dump:
            self.obs.quit()
        shutil.rmtree(self.obs.layout.config)  # Layout.uninstall(passed) removes this on Windows.
        self.assertEqual((self.work / "obs-logs" / "obs.txt").read_bytes(), log_bytes)
        evidence = json.loads((self.work / "obs-run.shutdown-complete.json").read_text())
        self.assertEqual(evidence["returncode"], 0)
        self.assertFalse(evidence["logs"][0]["tail_only"])
        dialog.assert_not_called()
        screen.assert_not_called()
        dump.assert_not_called()
        self.obs.process.kill.assert_not_called()
        exit_event = next(e for e in self.events() if e["event"] == "process-exit")
        self.assertEqual(exit_event["returncode"], 0)

    def test_failed_evidence_probes_do_not_prevent_kill_and_wait(self):
        self.obs.process.wait.side_effect = [subprocess.TimeoutExpired("obs", 60), -9]
        with mock.patch.object(app, "WINDOWS", True), \
             mock.patch.object(app, "close_windows", return_value=1), \
             mock.patch.object(app, "window_titles", return_value=["OBS"]), \
             mock.patch.object(app.subprocess, "run", side_effect=subprocess.TimeoutExpired("minidump", 10)), \
             mock.patch.object(app, "dialog_text", side_effect=subprocess.TimeoutExpired("powershell", 5)), \
             mock.patch.object(app, "screenshot", side_effect=OSError("no desktop")):
            with self.assertRaisesRegex(RuntimeError, "did not shut down within 60 s"):
                self.obs.quit()
        evidence = json.loads((self.work / "obs-run.shutdown-failure.json").read_text())
        self.assertIn("diagnostic_error", evidence["minidump"])
        self.assertIn("diagnostic_error", evidence["window_text"])
        self.assertIn("diagnostic_error", evidence["screenshot"])
        self.obs.process.kill.assert_called_once()
        self.assertEqual(self.obs.process.wait.call_count, 2)

    def test_screenshot_missing_output_is_reported_despite_zero_command_exit(self):
        with mock.patch.object(app.subprocess, "run") as command:
            with self.assertRaisesRegex(RuntimeError, "did not write a PNG"):
                app.screenshot(self.work / "missing.png")
        self.assertTrue(command.call_args.kwargs["check"])
        self.assertEqual(command.call_args.kwargs["timeout"], 5)

    def test_post_connect_failure_snapshots_log_before_forced_stop(self):
        logs = self.obs.layout.config / "logs"
        logs.mkdir(parents=True)
        (logs / "obs.txt").write_text("module registered\nCreated scene collection\n")
        self.obs.last_request = {"request_type": "CreateScene", "request_id": "2", "state": "failed"}
        self.obs.process.wait.side_effect = [subprocess.TimeoutExpired("obs", 60), -9]
        with self.assertRaises(RuntimeError):
            try:
                raise RuntimeError("request failure")
            finally:
                self.obs.quit()
        self.assertEqual((self.work / "obs-logs" / "obs.txt").read_text(), (logs / "obs.txt").read_text())
        evidence = json.loads((self.work / "obs-run.test-failure.json").read_text())
        self.assertEqual(evidence["last_request"]["request_type"], "CreateScene")
        events = [e["event"] for e in self.events()]
        self.assertLess(events.index("evidence"), events.index("forced-kill"))


if __name__ == "__main__":
    unittest.main()
