#!/usr/bin/env python3
"""Configure-time and distribution boundary checks using dummy public digests."""
from __future__ import annotations

import hashlib
import io
import json
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
from unittest.mock import patch

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

    def test_migration_review_cannot_become_build_input(self):
        path = ROOT / "docs/evidence/tester-migration-20261027/review.json"
        review = json.loads(path.read_text(encoding="utf-8"))
        self.assertEqual(review["status"], "verified-awaiting-activation")
        self.assertEqual(review["expected_count"], 5)
        self.assertEqual(review["verified_count"], len(review["digests"]))
        self.assertEqual(review["verified_count"], review["expected_count"])
        self.assertEqual(len(set(review["digests"])), len(review["digests"]))
        for value in review["digests"]:
            self.assertRegex(value, r"^[0-9a-f]{64}$")
        self.assertNotIn("CHT1-", path.read_text(encoding="utf-8"))
        result, header = self.configure(review)
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(header, "")

    def test_five_digest_candidate_is_valid_but_not_a_product_input(self):
        evidence = ROOT / "docs/evidence/tester-migration-20261027"
        candidate = json.loads((evidence / "allowlist.candidate.json").read_text(encoding="utf-8"))
        review = json.loads((evidence / "review.json").read_text(encoding="utf-8"))
        first, second = [set(batch["digests"]) for batch in review["audit_batches"]]
        self.assertEqual((len(first), len(second)), (2, 4))
        self.assertEqual(len(first & second), review["overlap_count"])
        self.assertEqual(review["overlap_count"], 1)
        self.assertEqual(set(candidate["digests"]), first | second)
        self.assertEqual(candidate["digests"], review["digests"])
        self.assertEqual(len(candidate["digests"]), 5)
        result, header = self.configure(candidate)
        self.assertEqual(result.returncode, 0, result.stderr)
        for digest in candidate["digests"]:
            self.assertEqual(header.count(digest), 1)
        self.assertNotIn("CHT1-", header)
        # Only an isolated configure above uses the candidate. Default builds
        # and release workflows keep the existing, empty production manifest.
        production = ROOT / "Validation/tester-code-allowlist.json"
        self.assertEqual(json.loads(production.read_text(encoding="utf-8"))["digests"], [])
        for path in (ROOT / "CMakeLists.txt", ROOT / ".github/workflows/build.yml"):
            body = path.read_text(encoding="utf-8")
            self.assertIn("Validation/tester-code-allowlist.json", body)
            self.assertNotIn("allowlist.candidate.json", body)

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
        data = json.loads((ROOT / "Validation/tester-code-allowlist.json").read_text(encoding="utf-8"))
        self.assertEqual(set(data), {"schema", "digests"})
        self.assertEqual(data["schema"], SCHEMA)
        # This implementation PR must not activate real tester credentials.
        self.assertEqual(data["digests"], [])

    def test_build_does_not_inject_issuance_secret(self):
        cmake = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
        workflow = (ROOT / ".github/workflows/build.yml").read_text(encoding="utf-8")
        self.assertNotIn("CHIMERA_TESTER_SECRET", cmake)
        self.assertNotIn("CHIMERA_TESTER_SECRET", workflow)
        self.assertNotIn("secrets: inherit", (ROOT / ".github/workflows/prepare-release.yml").read_text(encoding="utf-8"))
        self.assertNotIn("$ENV{CHIMERA_TESTER", cmake)
        self.assertFalse((ROOT / "cmake/ChimeraTesterSecret.h.in").exists())
        self.assertIn("ChimeraTesterAllowlist.cmake", cmake)
        self.assertIn("CMAKE_CONFIGURE_DEPENDS", cmake)

    def test_release_candidates_keep_the_activation_gate(self):
        workflow = (ROOT / ".github/workflows/build.yml").read_text(encoding="utf-8")
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
        cmake = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
        self.assertNotIn("Tools/tester_code.py", cmake)
        for path in [ROOT / "Tools/package_release.py", ROOT / "Source/TesterCode.h"]:
            text = path.read_text(encoding="utf-8")
            self.assertNotIn("tester_code.py", text)
            self.assertNotIn("CHIMERA_TESTER_SECRET", text)
        header = (ROOT / "Source/TesterCode.h").read_text(encoding="utf-8")
        self.assertNotIn("hmacSHA256", header)
        self.assertNotIn("productSecret", header)
        self.assertNotIn("detail::check(", header)


class TesterAllowlistEncodingRegression(unittest.TestCase):
    def test_distribution_checks_read_utf8_under_cp1252_default(self):
        # Model Windows' legacy default while keeping the regression portable.
        # U+201D contains byte 0x9D in UTF-8, which strict cp1252 cannot decode.
        marker = "\n# UTF-8 fixture: \u201d \ud14c\uc2a4\ud2b8\n"
        with self.assertRaises(UnicodeDecodeError):
            marker.encode("utf-8").decode("cp1252")
        original_open = Path.open

        def cp1252_default(path, mode="r", buffering=-1, encoding=None,
                           errors=None, newline=None):
            if "b" not in mode and encoding in (None, "locale"):
                encoding = "cp1252"
            return original_open(path, mode, buffering, encoding, errors, newline)

        with tempfile.TemporaryDirectory(prefix="chimera-encoding-") as tmp:
            fixture = Path(tmp)
            inspected = ["CMakeLists.txt", ".github/workflows/build.yml",
                         ".github/workflows/prepare-release.yml",
                         "Validation/tester-code-allowlist.json",
                         "Tools/package_release.py", "Source/TesterCode.h",
                         "Tools/tester_code.py"]
            for relative in inspected:
                target = fixture / relative
                target.parent.mkdir(parents=True, exist_ok=True)
                target.write_bytes((ROOT / relative).read_bytes())
                if relative != "Validation/tester-code-allowlist.json":
                    with target.open("a", encoding="utf-8") as stream:
                        stream.write(marker)
            with patch(__name__ + ".ROOT", fixture), patch.object(Path, "open", cp1252_default):
                # Prove the simulated default rejects the fixture before running
                # every existing boundary assertion without relaxing any of them.
                with self.assertRaises(UnicodeDecodeError):
                    (fixture / "Tools/package_release.py").read_text()
                suite = unittest.defaultTestLoader.loadTestsFromTestCase(
                    TesterAllowlistDistributionBoundary)
                output = io.StringIO()
                result = unittest.TextTestRunner(stream=output).run(suite)
            self.assertTrue(result.wasSuccessful(), output.getvalue())
            self.assertEqual(result.testsRun, 4)


if __name__ == "__main__":
    unittest.main()
