#!/usr/bin/env python3
"""Configure-time and distribution boundary checks using dummy public digests."""
from __future__ import annotations

import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
SCHEMA = "spectralforge.chimera.tester-allowlist.v1"
DUMMY_DIGEST = hashlib.sha256(b"public build-only fixture, not a tester code").hexdigest()


@unittest.skipUnless(shutil.which("cmake"), "cmake is required for build configuration checks")
class TesterAllowlistBuild(unittest.TestCase):
    def configure(self, value, *, raw=False, missing=False):
        with tempfile.TemporaryDirectory(prefix="chimera-allowlist-config-") as tmp:
            root = Path(tmp)
            manifest = root / "allowlist.json"
            if not missing:
                manifest.write_text(value if raw else json.dumps(value), encoding="utf-8")
            output = root / "generated" / "ChimeraTesterAllowlist.h"
            script = root / "configure.cmake"
            script.write_text(
                "cmake_minimum_required(VERSION 3.22)\n"
                f'include("{(ROOT / "cmake/ChimeraTesterAllowlist.cmake").as_posix()}")\n'
                f'chimera_configure_tester_allowlist("{manifest.as_posix()}" "{output.as_posix()}")\n',
                encoding="utf-8",
            )
            result = subprocess.run(["cmake", "-P", str(script)], text=True, capture_output=True)
            return result, output.read_text(encoding="utf-8") if output.exists() else ""

    def test_empty_manifest_is_disabled_not_a_build_failure(self):
        result, header = self.configure({"schema": SCHEMA, "digests": []})
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("disabled (empty public allowlist)", result.stdout)
        self.assertIn("chimeraTesterAllowlistDigests[]", header)
        self.assertIn("nullptr", header)
        self.assertNotIn(DUMMY_DIGEST, header)

    def test_public_digest_is_the_only_generated_payload(self):
        result, header = self.configure({"schema": SCHEMA, "digests": [DUMMY_DIGEST]})
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn(f'"{DUMMY_DIGEST}"', header)
        self.assertNotIn("CHT1-", header)
        self.assertNotIn("SECRET", header)
        self.assertEqual(header.count(DUMMY_DIGEST), 1)

    def test_invalid_manifest_shapes_fail_closed(self):
        cases = [None, [], {}, {"schema": SCHEMA}, {"digests": []},
                 {"schema": "wrong", "digests": []}, {"schema": 1, "digests": []},
                 {"schema": SCHEMA, "digests": ""}, {"schema": SCHEMA, "digests": {}},
                 {"schema": SCHEMA, "digests": [], "plaintext": "FORBIDDEN"}]
        for value in cases:
            with self.subTest(value=value):
                result, header = self.configure(value)
                self.assertNotEqual(result.returncode, 0)
                self.assertEqual(header, "")
                self.assertNotIn("FORBIDDEN", result.stdout + result.stderr)

    def test_invalid_entries_and_duplicates_fail_closed(self):
        for digests in [[""], ["a" * 63], ["a" * 65], ["A" * 64], ["g" * 64],
                        [True], [4], [None], [{}], ["\"; bad_header(); //"],
                        [DUMMY_DIGEST, DUMMY_DIGEST]]:
            with self.subTest(digests=digests):
                result, header = self.configure({"schema": SCHEMA, "digests": digests})
                self.assertNotEqual(result.returncode, 0)
                self.assertEqual(header, "")

    def test_missing_or_malformed_json_fail_closed(self):
        for raw in ["", "not json", '{"schema":', '{"schema": "' + SCHEMA + '", "digests": [}']:
            result, header = self.configure(raw, raw=True)
            self.assertNotEqual(result.returncode, 0)
            self.assertEqual(header, "")
        result, header = self.configure(None, missing=True)
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(header, "")

    def test_entry_count_is_bounded(self):
        result, header = self.configure({"schema": SCHEMA, "digests": ["0" * 64] * 4097})
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("exceeds 4096", result.stderr)
        self.assertEqual(header, "")

    def test_manifest_file_size_is_bounded(self):
        result, header = self.configure(" " * 524289, raw=True)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("too large", result.stderr)
        self.assertEqual(header, "")


class TesterAllowlistDistributionBoundary(unittest.TestCase):
    def test_committed_production_list_has_no_credentials(self):
        data = json.loads((ROOT / "Validation/tester-code-allowlist.json").read_text())
        self.assertEqual(set(data), {"schema", "digests"})
        self.assertEqual(data["schema"], SCHEMA)
        # This implementation PR must not activate real tester credentials.
        self.assertEqual(data["digests"], [])

    def test_build_does_not_inject_issuance_secret(self):
        cmake = (ROOT / "CMakeLists.txt").read_text()
        workflow = (ROOT / ".github/workflows/build.yml").read_text()
        self.assertNotIn("CHIMERA_TESTER_SECRET", cmake)
        self.assertNotIn("CHIMERA_TESTER_SECRET", workflow)
        self.assertNotIn("secrets: inherit", (ROOT / ".github/workflows/prepare-release.yml").read_text())
        self.assertNotIn("$ENV{CHIMERA_TESTER", cmake)
        self.assertFalse((ROOT / "cmake/ChimeraTesterSecret.h.in").exists())
        self.assertIn("ChimeraTesterAllowlist.cmake", cmake)
        self.assertIn("CMAKE_CONFIGURE_DEPENDS", cmake)

    def test_release_candidates_keep_the_activation_gate(self):
        workflow = (ROOT / ".github/workflows/build.yml").read_text()
        start = workflow.index("      - name: Require approved tester allowlist for release candidates")
        end = workflow.index("      - name: Resolve source version", start)
        step = workflow[start:end]
        self.assertIn("if: github.event_name != 'pull_request'", step)
        self.assertIn("load_allowlist(Path('Validation/tester-code-allowlist.json'))", step)
        self.assertIn("separately approved nonempty tester allowlist", step)
        command = next(line.strip()[len('python -c "'):-1] for line in step.splitlines()
                       if line.strip().startswith('python -c "'))
        result = subprocess.run([__import__("sys").executable, "-c", command],
                                cwd=ROOT, capture_output=True, text=True)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("separately approved nonempty tester allowlist", result.stderr)

    def test_issuer_is_not_a_product_or_packaging_input(self):
        # The owner CLI remains source tooling only. Its test registration is OK.
        cmake = (ROOT / "CMakeLists.txt").read_text()
        self.assertNotIn("Tools/tester_code.py", cmake)
        for path in [ROOT / "Tools/package_release.py", ROOT / "Source/TesterCode.h"]:
            text = path.read_text()
            self.assertNotIn("tester_code.py", text)
            self.assertNotIn("CHIMERA_TESTER_SECRET", text)
        header = (ROOT / "Source/TesterCode.h").read_text()
        self.assertNotIn("hmacSHA256", header)
        self.assertNotIn("productSecret", header)
        self.assertNotIn("detail::check(", header)


if __name__ == "__main__":
    unittest.main()
