#!/usr/bin/env python3
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
TOOLS = ROOT / "Tools"
sys.path.insert(0, str(TOOLS))
from aax_native import BUNDLE, verify_bundle
from aax_windows_release import BINARY, SCHEMA
from chimera_version import identity


class AaxWindowsReleaseTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name).resolve()   # macOS: /var is a link to /private/var
        self.package = self.root / "input"
        self.bundle = self.package / BUNDLE
        self.binary = self.bundle / BINARY
        self.binary.parent.mkdir(parents=True)
        data = bytearray(100)
        data[:2] = b"MZ"
        struct.pack_into("<I", data, 60, 64)
        data[64:68] = b"PE\0\0"
        for pos, value in ((68, 0x8664), (86, 0x2000), (88, 0x20b)):
            struct.pack_into("<H", data, pos, value)
        self.binary.write_bytes(data)
        for name in ("Plugin.ico", "desktop.ini"):
            (self.bundle / name).write_bytes(name.encode())
        self.receipt = {**identity(), "schema": SCHEMA, "platform": "windows",
                        "pace_verification": "PASS", **verify_bundle(self.bundle, "windows")}

    def pin(self):
        raw = json.dumps(self.receipt).encode()
        (self.package / "aax-release-receipt.json").write_bytes(raw)
        return hashlib.sha256(raw).hexdigest()

    def run_tool(self, pin, *args, package=None):
        return subprocess.run([sys.executable, TOOLS / "aax_windows_release.py",
                               package or self.package, pin, *args],
                              text=True, capture_output=True)

    def test_prepares_and_rechecks_frozen_bundle(self):
        pin = self.pin()
        target = self.root / "stage"
        result = self.run_tool(pin, "--prepare", target)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.binary.write_bytes(b"changed original")
        self.assertEqual(self.run_tool(pin, package=target).returncode, 0)
        (target / BUNDLE / "desktop.ini").write_bytes(b"changed frozen payload")
        self.assertNotEqual(self.run_tool(pin, package=target).returncode, 0)

    def test_rejects_consistent_bundle_from_wrong_source(self):
        self.receipt["source_sha"] = "f" * 40
        result = self.run_tool(self.pin())
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("source_sha", result.stderr)

    def test_rejects_unsigned_and_wrong_receipt_pin(self):
        self.receipt["pace_verification"] = "NOT_RUN"
        self.assertNotEqual(self.run_tool(self.pin()).returncode, 0)
        self.assertNotEqual(self.run_tool("a" * 64).returncode, 0)

    def test_rejects_extra_file_even_if_listed_and_pinned(self):
        (self.bundle / "extra.dll").write_bytes(b"not expected")
        self.receipt.update(verify_bundle(self.bundle, "windows"))
        self.assertNotEqual(self.run_tool(self.pin()).returncode, 0)

    def test_rejects_linked_resource(self):
        pin = self.pin()
        path = self.bundle / "Plugin.ico"
        alternate = self.root / "same.ico"
        path.rename(alternate)
        try:
            path.symlink_to(alternate)
        except OSError:
            self.skipTest("symlink creation is unavailable")
        self.assertNotEqual(self.run_tool(pin).returncode, 0)


if __name__ == "__main__":
    unittest.main()
