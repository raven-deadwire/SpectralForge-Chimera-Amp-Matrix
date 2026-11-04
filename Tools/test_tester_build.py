#!/usr/bin/env python3
"""Exercise the real CMake secret parser using disposable synthetic values only."""
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
MODULE = ROOT / "cmake/ChimeraTesterSecret.cmake"


class TesterBuildTests(unittest.TestCase):
    def configure(self, root, value, required=False):
        script = root / "configure.cmake"
        script.write_text(
            f'set(CMAKE_BINARY_DIR "{root.as_posix()}")\n'
            f'set(CHIMERA_REQUIRE_TESTER_SECRET {"ON" if required else "OFF"} CACHE BOOL "")\n'
            f'include("{MODULE.as_posix()}")\n', encoding="utf-8")
        env = dict(os.environ, CHIMERA_TESTER_SECRET=value)
        result = subprocess.run(["cmake", "-P", str(script)], env=env,
                                capture_output=True, text=True, timeout=30)
        # Do not use assertNotIn(value, ...) which would echo the input on failure.
        if value.strip():
            self.assertFalse(value.strip() in result.stdout + result.stderr,
                             "CMake printed the synthetic secret")
        return result

    def test_generated_header_and_disabled_reconfigure(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            key = bytes(range(16, 48)).hex()
            self.assertEqual(self.configure(root, " \n" + key.upper() + "\n", True).returncode, 0)
            header = root / "generated/ChimeraTesterSecret.h"
            self.assertTrue(('"' + key + '"') in header.read_text())
            self.assertEqual(self.configure(root, "").returncode, 0)
            self.assertTrue('#define CHIMERA_TESTER_SECRET_HEX ""' in header.read_text())

    def test_missing_required_and_malformed_values_fail(self):
        for value in ("", " ", "ab" * 15, "a" * 33, "g" * 64, "ab cd" * 8, "ab;cd" * 8):
            with self.subTest(kind="missing" if not value.strip() else "malformed"):
                with tempfile.TemporaryDirectory() as tmp:
                    root = Path(tmp)
                    self.assertNotEqual(self.configure(root, value, True).returncode, 0)
                    self.assertFalse((root / "generated/ChimeraTesterSecret.h").exists())

    def test_zero_bytes_and_long_hmac_keys_remain_supported(self):
        for value in ("00" * 16, "fa" * 80):
            with tempfile.TemporaryDirectory() as tmp:
                self.assertEqual(self.configure(Path(tmp), value, True).returncode, 0)

    def test_workflow_secrets_are_guarded_and_step_scoped(self):
        text = (ROOT / ".github/workflows/build.yml").read_text(encoding="utf-8")
        references = [line for line in text.splitlines() if "secrets.CHIMERA_TESTER_SECRET" in line]
        self.assertEqual(len(references), 4)  # preflight plus three platform configure steps
        for line in references:
            self.assertTrue(line.startswith("          CHIMERA_TESTER_SECRET:"))
            self.assertIn("github.event_name != 'pull_request' &&", line)
        configures = [line for line in text.splitlines() if "run: cmake -S . -B build " in line]
        self.assertEqual(len(configures), 3)
        for line in configures:
            self.assertIn("-DCHIMERA_REQUIRE_TESTER_SECRET=${{ github.event_name != 'pull_request' }}", line)


if __name__ == "__main__":
    unittest.main()
