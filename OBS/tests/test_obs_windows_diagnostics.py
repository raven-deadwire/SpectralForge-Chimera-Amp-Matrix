#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Exercise the native dump helper against a disposable child on Windows."""
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest


class NativeDumpTests(unittest.TestCase):
    @unittest.skipUnless(sys.platform == "win32", "requires native Windows DbgHelp")
    def test_child_thread_dump_is_valid_and_target_keeps_running(self):
        helper = Path(__file__).with_name("obs_windows_diagnostics.py")
        with tempfile.TemporaryDirectory() as folder:
            dump = Path(folder) / "child.dmp"
            child = subprocess.Popen([sys.executable, "-c", "import time; time.sleep(60)"])
            try:
                result = subprocess.run([sys.executable, str(helper), "--pid", str(child.pid),
                                         "--output", str(dump)], capture_output=True, text=True, timeout=10)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                self.assertGreater(dump.stat().st_size, 32)
                with dump.open("rb") as content:
                    self.assertEqual(content.read(4), b"MDMP")
                self.assertIsNone(child.poll(), "dump capture must not terminate the target")
            finally:
                child.kill()
                child.wait(timeout=10)


if __name__ == "__main__":
    unittest.main()
