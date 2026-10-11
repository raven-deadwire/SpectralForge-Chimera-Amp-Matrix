#!/usr/bin/env python3
"""Exercise release workflow command sequencing with real child-process failures.

Read the checked-in preflight blocks rather than maintaining a duplicate command
list. Substitute an offline Python probe for each command and run the same Bash
flags used by Actions. Exit 23 at any command must prevent all later commands.
"""
from __future__ import annotations

import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile
import textwrap
import unittest


ROOT = Path(__file__).resolve().parents[1]
PREFLIGHTS = (
    ("build.yml", "Verify catalog and validation producer contracts"),
    ("build.yml", "Check release packaging and validation syntax"),
    ("update-candidate.yml", "Check candidate packaging and validation contracts"),
)
HEAD = "a" * 40
DONE = ["__preflight_done__"]
PROBE = '''\
import json, os, sys
from pathlib import Path
command = sys.argv[1:]
with Path(os.environ["CHIMERA_TEST_CALLS"]).open("a", encoding="utf-8") as stream:
    stream.write(json.dumps(command) + "\\n")
if command == json.loads(os.environ["CHIMERA_TEST_FAIL"]):
    raise SystemExit(23)
if command[:1] == ["__git__"]:
    print("a" * 40)
'''
WRAPPERS = '''\
python() { "$CHIMERA_TEST_PYTHON" -S "$CHIMERA_TEST_PROBE" "$@"; }
git() { "$CHIMERA_TEST_PYTHON" -S "$CHIMERA_TEST_PROBE" __git__ "$@"; }
'''


def workflow_script(workflow: str, name: str) -> str:
    """Extract a named literal run block without a third-party YAML dependency."""
    lines = (ROOT / ".github/workflows" / workflow).read_text(encoding="utf-8").splitlines()
    starts = [i for i, line in enumerate(lines) if line.strip() == "- name: " + name]
    if len(starts) != 1:
        raise AssertionError(f"Expected exactly one {name!r} step in {workflow}")
    start = starts[0]
    indent = len(lines[start]) - len(lines[start].lstrip())
    end = start + 1
    while end < len(lines):
        line = lines[end]
        if line.strip() and len(line) - len(line.lstrip()) <= indent:
            break
        end += 1
    step = lines[start + 1:end]
    if " " * (indent + 2) + "shell: bash" not in step:
        raise AssertionError(f"{workflow}: {name} must use the fail-fast Bash shell on Windows too")
    run = " " * (indent + 2) + "run: |"
    if step.count(run) != 1:
        raise AssertionError(f"{workflow}: {name} must have one literal run block")
    script = textwrap.dedent("\n".join(step[step.index(run) + 1:]))
    # Actions resolves the source SHA before invoking the shell.
    return re.sub(r"\$\{\{.*?\}\}", HEAD, script)


class PreflightFailures(unittest.TestCase):
    def setUp(self):
        # Git Bash is available on the Windows Actions images used by these jobs.
        # Missing Bash is a failure, never skipped release-gate coverage.
        self.bash = shutil.which("bash")
        self.assertIsNotNone(self.bash, "The release workflows require Bash")
        self.tmp = tempfile.TemporaryDirectory(prefix="chimera-preflight-")
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        self.probe = self.root / "probe.py"
        self.probe.write_text(PROBE, encoding="utf-8")

    def run_script(self, script, failed_command=None, *, errexit=True):
        calls = self.root / "calls.jsonl"
        calls.unlink(missing_ok=True)
        env = {**os.environ,
               "CHIMERA_TEST_PYTHON": Path(sys.executable).as_posix(),
               "CHIMERA_TEST_PROBE": self.probe.as_posix(),
               "CHIMERA_TEST_CALLS": str(calls),
               "CHIMERA_TEST_FAIL": json.dumps(failed_command)}
        command = [self.bash, "--noprofile", "--norc"]
        if errexit:
            command.append("-e")
        command += ["-o", "pipefail", "-c", WRAPPERS + script + "\npython __preflight_done__\n"]
        result = subprocess.run(command, env=env, cwd=self.root, capture_output=True,
                                text=True, encoding="utf-8", timeout=30)
        trace = [json.loads(line) for line in calls.read_text(encoding="utf-8").splitlines()] if calls.exists() else []
        return result, trace

    def test_every_failed_command_stops_the_actual_preflight(self):
        for workflow, name in PREFLIGHTS:
            with self.subTest(workflow=workflow, step=name):
                script = workflow_script(workflow, name)
                complete, expected = self.run_script(script)
                self.assertEqual(complete.returncode, 0, complete.stderr)
                self.assertGreater(len(expected), 2)
                self.assertEqual(expected[-1], DONE)
                for position, command in enumerate(expected[:-1]):
                    with self.subTest(command=command):
                        result, observed = self.run_script(script, command)
                        self.assertEqual(result.returncode, 23, result.stderr)
                        self.assertEqual(observed, expected[:position + 1])

    def test_probe_exposes_failure_masking_without_errexit(self):
        # Prove this fixture detects the original last-command-wins failure mode.
        script = workflow_script(*PREFLIGHTS[1])
        _, commands = self.run_script(script)
        result, observed = self.run_script(script, commands[0], errexit=False)
        self.assertEqual(result.returncode, 0)
        self.assertEqual(observed, commands)
        self.assertEqual(observed[-1], DONE)


if __name__ == "__main__":
    unittest.main()
