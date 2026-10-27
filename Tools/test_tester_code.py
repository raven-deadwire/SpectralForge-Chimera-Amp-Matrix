#!/usr/bin/env python3
"""Throwaway fixtures only; never import production tester data."""
import contextlib
import io
import hashlib
import hmac
import json
import os
import stat
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch
from types import SimpleNamespace

import tester_code as tc

ROOT = Path(__file__).resolve().parents[1]
TOOL = Path(__file__).resolve().parent / "tester_code.py"
TEMP_ROOT = str(Path("/tmp").resolve()) if os.name == "posix" else None
CODE = "CHT1-TESTER-0123AB"
OTHER = "CHT1-OTHER-0123AB"


class TesterCodeTests(unittest.TestCase):
    def setUp(self):
        # Some sandbox executors place a synthetic .git marker at the writable
        # /tmp root. Simulate a normal external temp directory for unit tests
        # only; production code retains its strict all-ancestor checkout guard.
        original_exists = Path.exists
        artificial_root_marker = Path(TEMP_ROOT) / ".git" if TEMP_ROOT else None
        self.marker_patch = patch.object(Path, "exists", lambda path: False
            if artificial_root_marker is not None and path == artificial_root_marker
            else original_exists(path))
        self.marker_patch.start()
        self.addCleanup(self.marker_patch.stop)

    def cli(self, *arguments):
        stdout, stderr = io.StringIO(), io.StringIO()
        with patch.object(sys, "argv", [str(TOOL), *arguments]), \
                contextlib.redirect_stdout(stdout), contextlib.redirect_stderr(stderr):
            try:
                result = tc.main()
            except (ValueError, OSError):
                result = 1
                print("error: tester-code operation failed", file=sys.stderr)
            except SystemExit as error:
                result = error.code
        return SimpleNamespace(returncode=result, stdout=stdout.getvalue(), stderr=stderr.getvalue())

    def test_issue_uses_random_choices_without_a_key(self):
        with patch.object(tc.secrets, "choice", side_effect=list("0123AB")) as random_choice:
            self.assertEqual(tc.issue("Test.er"), CODE)
            self.assertEqual(random_choice.call_count, 6)
            random_choice.assert_called_with(tc.ALPHABET)
        self.assertFalse(hasattr(tc, "load_secret"))
        self.assertFalse(hasattr(tc, "check"))

    def test_name_rules(self):
        for raw, name in (("Tester", "TESTER"), ("Test One", "TESTONE"), ("Test.two", "TESTTWO"),
                          ("test-three", "TESTTHREE"), ("TEST_04", "TEST04")):
            self.assertEqual(tc.check_name(raw), name)
        for bad in ("", "A", "X" * 17, "지엘", "TEST!", "ßß", "KK"):
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                tc.check_name(bad)

    def test_digest_and_full_name_binding(self):
        expected = hashlib.sha256(b"chimera-tester-allowlist-v1|CHT1-TESTER-0123AB").hexdigest()
        self.assertEqual(tc.digest(CODE), expected)
        self.assertNotEqual(tc.digest(OTHER), expected)
        self.assertEqual(tc.verify(CODE, [expected]), "TESTER")
        for bad in (OTHER, CODE[:-1] + "C", "CHT1-TESTER-0123AC"):
            with self.assertRaises(ValueError):
                tc.verify(bad, [expected])

    def test_typing_aids_canonicalize_before_hashing(self):
        allow = [tc.digest(CODE)]
        for typed in (CODE.lower(), " " + CODE + "\n", "CHT1-TESTER-OI23AB", "cht1-tester-ol23ab"):
            self.assertEqual(tc.canonical(typed), CODE)
            self.assertEqual(tc.verify(typed, allow), "TESTER")

    def test_invalid_inputs_fail_closed(self):
        for bad in ("CHT1TESTER", "CHT1-TESTER", CODE + "-X", CODE[:-1], CODE + "\u00a0",
                    "CHT1-TESTER-가나다라마바", "CHT1-TESTER-!!!!!!", "CHT1-TESTER-SSSSSß",
                    "CHT1-X-0123AB", "CHT1-TESTER-0123AU", " " * tc.MAX_PASTED + CODE,
                    "A" * 5000, CODE + "\x00"):
            with self.subTest(bad=bad[:40]), self.assertRaises(ValueError):
                tc.verify(bad, [tc.digest(CODE)])
        with self.assertRaises(ValueError):
            tc.verify(CODE, [])
        with self.assertRaises(ValueError):
            tc.verify(CODE, [tc.digest(OTHER)])

    def test_legacy_hmac_is_not_an_issuance_authority(self):
        # Public, throwaway test key: legacy formatting is not enough for acceptance.
        mac = hmac.new(bytes(range(32)), b"chimera-tester-v1|UNISSUED", hashlib.sha256).digest()
        bits = int.from_bytes(mac[:4], "big")
        tail = "".join(tc.ALPHABET[(bits >> (27 - 5 * i)) & 31] for i in range(6))
        with self.assertRaises(ValueError):
            tc.verify("CHT1-UNISSUED-" + tail, [tc.digest(CODE)])

    def test_manifest_validation(self):
        with tempfile.TemporaryDirectory(dir=TEMP_ROOT) as tmp:
            p = Path(tmp) / "allowlist.json"
            for bad in (None, [], {}, {"schema": tc.SCHEMA, "digests": "abc"},
                        {"schema": tc.SCHEMA, "digests": ["a" * 63]},
                        {"schema": tc.SCHEMA, "digests": ["A" * 64]},
                        {"schema": tc.SCHEMA, "digests": [tc.digest(CODE)] * 2},
                        {"schema": tc.SCHEMA, "digests": ["g" * 64]},
                        {"schema": tc.SCHEMA, "digests": [], "codes": [CODE]}):
                p.write_text(json.dumps(bad))
                with self.assertRaises(ValueError):
                    tc.load_allowlist(p)
            p.write_text(json.dumps({"schema": tc.SCHEMA, "digests": []}))
            self.assertEqual(tc.load_allowlist(p), [])
            p.write_text(json.dumps({"schema": tc.SCHEMA, "digests": [tc.digest(CODE)]}))
            self.assertEqual(tc.load_allowlist(p), [tc.digest(CODE)])

    def test_private_creation_exclusive_permissions_and_git_rejection(self):
        with tempfile.TemporaryDirectory(dir=TEMP_ROOT) as tmp:
            root = Path(tmp)
            target = root / "test.private.json"
            old_mask = os.umask(0)
            try:
                tc.write_private(target, [CODE])
            finally:
                os.umask(old_mask)
            self.assertEqual(tc.load_private(target), [CODE])
            if os.name == "posix":
                self.assertEqual(stat.S_IMODE(target.stat().st_mode), 0o600)
            original = target.read_bytes()
            with self.assertRaises(FileExistsError):
                tc.write_private(target, [OTHER])
            self.assertEqual(target.read_bytes(), original)
            repo = root / "checkout"
            (repo / ".git").mkdir(parents=True)
            with self.assertRaises(ValueError):
                tc.write_private(repo / "ordinary.json", [CODE])
            worktree = root / "worktree"
            worktree.mkdir()
            (worktree / ".git").write_text("gitdir: elsewhere")
            with self.assertRaises(ValueError):
                tc.write_private(worktree / "ordinary.json", [CODE])
            if os.name == "posix":
                link = root / "link.private.json"
                link.symlink_to(target)
                with self.assertRaises(ValueError):
                    tc.write_private(link, [CODE])
                parentlink = root / "linked-dir"
                parentlink.symlink_to(root, target_is_directory=True)
                with self.assertRaises(ValueError):
                    tc.write_private(parentlink / "new.json", [CODE])

    def test_export_is_public_digest_only_and_merges_reserves(self):
        with tempfile.TemporaryDirectory(dir=TEMP_ROOT) as tmp:
            root = Path(tmp)
            first, second, out = root / "first.json", root / "second.json", root / "allowlist.json"
            tc.write_private(first, [CODE])
            tc.write_private(second, [OTHER])
            tc.export_allowlist([first, second], out)
            body = out.read_text(encoding="utf-8")
            self.assertNotIn(CODE, body)
            self.assertNotIn("TESTER", body)
            self.assertNotIn("OTHER", body)
            self.assertEqual(set(tc.load_allowlist(out)), {tc.digest(CODE), tc.digest(OTHER)})
            with self.assertRaises(FileExistsError):
                tc.export_allowlist([first], out)

    def test_cli_uses_files_and_never_prints_plaintext(self):
        with tempfile.TemporaryDirectory(dir=TEMP_ROOT) as tmp:
            root = Path(tmp)
            private, public, pasted = root / "private.json", root / "public.json", root / "pasted.txt"
            # Do not generate actual credentials in tests: use fixed throwaway fixtures.
            tc.write_private(private, [CODE])
            run = self.cli
            exported = run("export", "--private-file", str(private), "--out", str(public))
            self.assertEqual(exported.returncode, 0, exported.stderr)
            pasted.write_text(CODE)
            checked = run("verify", "--allowlist", str(public), "--code-file", str(pasted))
            self.assertEqual(checked.returncode, 0, checked.stderr)
            self.assertNotIn(CODE, checked.stdout + checked.stderr + exported.stdout + exported.stderr)
            invalid = run("issue", "--name", "TESTER", "--private-out", str(private))
            self.assertNotEqual(invalid.returncode, 0)
            self.assertNotIn(CODE, invalid.stdout + invalid.stderr)
            self.assertNotIn(str(private), invalid.stderr)
            self.assertNotEqual(run("secret", "--out", str(root / "old.json")).returncode, 0)
            rejected = run("verify", "--code", CODE)
            self.assertNotEqual(rejected.returncode, 0)
            self.assertNotIn(CODE, rejected.stdout + rejected.stderr)

    def test_issue_cli_mocked_randomness(self):
        with tempfile.TemporaryDirectory(dir=TEMP_ROOT) as tmp:
            path = Path(tmp) / "private.json"
            with patch.object(sys, "argv", [str(TOOL), "issue", "--name", "TESTER", "--private-out", str(path)]), \
                    patch.object(tc.secrets, "choice", side_effect=list("0123AB")), \
                    patch("builtins.print") as output:
                self.assertEqual(tc.main(), 0)
            self.assertEqual(tc.load_private(path), [CODE])
            self.assertNotIn(CODE, str(output.call_args_list))

    def test_nonregular_and_oversize_inputs_rejected(self):
        with tempfile.TemporaryDirectory(dir=TEMP_ROOT) as tmp:
            root = Path(tmp)
            huge = root / "huge.txt"
            huge.write_text("X" * (tc.MAX_PASTED + 1))
            with self.assertRaises(ValueError):
                tc.read_regular(huge, tc.MAX_PASTED)
            if os.name == "posix":
                fifo = root / "fifo"
                os.mkfifo(fifo)
                with self.assertRaises(ValueError):
                    tc.read_regular(fifo)

    @unittest.skipUnless(os.environ.get("CHIMERA_TESTER_VERIFIER"), "native verifier supplied by CTest")
    def test_native_verifier_matches_input_boundaries(self):
        cases = [CODE, CODE.lower(), " " + CODE + "\n", "CHT1-TESTER-OI23AB", OTHER,
                 " " * tc.MAX_PASTED + CODE, CODE + "\u00a0", "CHT1-TESTER-SSSSSß",
                 "CHT1-TESTER-가나다라마바", "CHT1-TESTER-!!!!!!", "CHT1-X-000000"]
        expected_digest = tc.digest(CODE)
        with tempfile.TemporaryDirectory(dir=TEMP_ROOT) as tmp:
            path = Path(tmp) / "code.txt"
            for text in cases:
                try:
                    tc.verify(text, [expected_digest])
                    expected = 0
                except ValueError:
                    expected = 1
                path.write_text(text, encoding="utf-8")
                result = subprocess.run([os.environ["CHIMERA_TESTER_VERIFIER"], "--verify-test-code",
                                         expected_digest, str(path)], capture_output=True, timeout=5)
                self.assertEqual(result.returncode, expected)

    @staticmethod
    def legacy_migration_fixtures():
        # Old HMAC issuance with a PUBLIC test key, never an owner's secret.
        result = []
        for name in ("TEST01", "TEST02", "TEST03", "TEST04", "TEST05"):
            mac = hmac.new(bytes(range(32)), f"chimera-tester-v1|{name}".encode("ascii"), hashlib.sha256).digest()
            bits = int.from_bytes(mac[:4], "big")
            check = "".join(tc.ALPHABET[(bits >> (27 - 5 * i)) & 31] for i in range(6))
            result.append(f"CHT1-{name}-{check}")
        return result

    def test_migration_stages_only_digests_and_retains_existing_grants(self):
        with tempfile.TemporaryDirectory(dir=TEMP_ROOT) as tmp:
            root = Path(tmp)
            private, out = root / "selected.private.json", root / "review.json"
            codes = self.legacy_migration_fixtures()
            tc.write_private(private, codes)
            before = private.read_bytes()
            retained = tc.digest(OTHER)

            def audit(arguments, **kwargs):
                self.assertEqual(arguments[1], "--audit-migration-code")
                self.assertTrue(all(code not in str(arguments) for code in codes))
                path = Path(arguments[3])
                self.assertIn(path.read_text(encoding="ascii"), codes)
                if os.name == "posix":
                    self.assertEqual(stat.S_IMODE(path.stat().st_mode), 0o600)
                    self.assertEqual(stat.S_IMODE(path.parent.stat().st_mode), 0o700)
                return SimpleNamespace(returncode=0, stdout=b"PASS: private migration lifecycle\n")

            with patch.object(tc, "load_allowlist", return_value=[retained]), \
                    patch.object(tc.subprocess, "run", side_effect=audit) as native, \
                    patch.object(tc, "issue", side_effect=AssertionError("migration must not issue")):
                result = self.cli("migrate", "--private-file", str(private), "--expected-count", "5",
                                  "--native-verifier", sys.executable, "--out", str(out))
            self.assertEqual(result.returncode, 0)
            self.assertEqual(native.call_count, 5)
            self.assertEqual(set(tc.load_allowlist(out)), {retained, *map(tc.digest, codes)})
            self.assertEqual(private.read_bytes(), before)
            public = out.read_text(encoding="utf-8") + result.stdout + result.stderr
            for code in codes:
                self.assertNotIn(code, public)
                self.assertNotIn(code.split("-")[1], public)

    def test_incomplete_migration_never_creates_partial_allowlist(self):
        with tempfile.TemporaryDirectory(dir=TEMP_ROOT) as tmp:
            root = Path(tmp)
            private, out = root / "two.private.json", root / "review.json"
            tc.write_private(private, self.legacy_migration_fixtures()[:2])
            with patch.object(tc.subprocess, "run") as native:
                for count in (0, 5, tc.MAX_ENTRIES + 1):
                    with self.assertRaises(ValueError):
                        tc.migrate(private, count, Path(sys.executable), out)
                native.assert_not_called()
            self.assertFalse(out.exists())

    def test_migration_rejects_canonical_duplicates_and_repeated_tester(self):
        with tempfile.TemporaryDirectory(dir=TEMP_ROOT) as tmp:
            root = Path(tmp)
            private, out = root / "input.private.json", root / "review.json"
            for codes in ([CODE, CODE.lower()], [CODE, CODE[:-1] + "C"], [CODE, "[withheld]"]):
                private.write_text(json.dumps({"schema": tc.PRIVATE_SCHEMA, "codes": codes}), encoding="utf-8")
                with self.assertRaises(ValueError):
                    tc.migrate(private, 2, Path(sys.executable), out)
                self.assertFalse(out.exists())

    def test_migration_cannot_write_production_or_overwrite_inputs(self):
        production = ROOT / "Validation/tester-code-allowlist.json"
        original = production.read_bytes()
        with tempfile.TemporaryDirectory(dir=TEMP_ROOT) as tmp:
            root = Path(tmp)
            private, existing = root / "input.private.json", root / "review.json"
            tc.write_private(private, [CODE])
            existing.write_text("keep", encoding="utf-8")
            for target in (production, private, existing):
                with self.assertRaises(ValueError):
                    tc.migrate(private, 1, Path(sys.executable), target)
            self.assertEqual(tc.load_private(private), [CODE])
            self.assertEqual(existing.read_text(encoding="utf-8"), "keep")
        self.assertEqual(production.read_bytes(), original)

    def test_migration_review_is_not_an_allowlist(self):
        with self.assertRaises(ValueError):
            tc.load_allowlist(ROOT / "docs/evidence/tester-migration-20261027/review.json")

    def test_reviewed_candidate_has_only_five_digests_and_rejects_public_fixtures(self):
        evidence = ROOT / "docs/evidence/tester-migration-20261027"
        candidate = tc.load_allowlist(evidence / "allowlist.candidate.json")
        review = json.loads((evidence / "review.json").read_text(encoding="utf-8"))
        self.assertEqual(len(candidate), 5)
        self.assertEqual(candidate, review["digests"])
        for code in [CODE, OTHER, *self.legacy_migration_fixtures()]:
            with self.assertRaises(ValueError):
                tc.verify(code, candidate)
        self.assertEqual(tc.load_allowlist(ROOT / "Validation/tester-code-allowlist.json"), [])

    def test_failed_native_migration_is_all_or_nothing_and_redacted(self):
        with tempfile.TemporaryDirectory(dir=TEMP_ROOT) as tmp:
            root = Path(tmp)
            private, out = root / "input.private.json", root / "review.json"
            tc.write_private(private, [CODE, OTHER])
            for failure in (SimpleNamespace(returncode=1, stdout=CODE.encode()),
                            SimpleNamespace(returncode=0, stdout=b"wrong audit"),
                            subprocess.TimeoutExpired([CODE], 30)):
                good = SimpleNamespace(returncode=0, stdout=b"PASS: private migration lifecycle\n")
                with patch.object(tc.subprocess, "run", side_effect=[good, failure]):
                    result = self.cli("migrate", "--private-file", str(private), "--expected-count", "2",
                                      "--native-verifier", sys.executable, "--out", str(out))
                self.assertNotEqual(result.returncode, 0)
                self.assertNotIn(CODE, result.stdout + result.stderr)
                self.assertFalse(out.exists())
            self.assertEqual(tc.load_private(private), [CODE, OTHER])

    @unittest.skipUnless(os.environ.get("CHIMERA_TESTER_VERIFIER"), "native verifier supplied by CTest")
    def test_five_legacy_fixtures_native_migration_lifecycle(self):
        with tempfile.TemporaryDirectory(dir=TEMP_ROOT) as tmp:
            root = Path(tmp)
            private, out = root / "input.private.json", root / "review.json"
            codes = self.legacy_migration_fixtures()
            tc.write_private(private, codes)
            tc.migrate(private, 5, Path(os.environ["CHIMERA_TESTER_VERIFIER"]), out)
            allowed = tc.load_allowlist(out)
            for code in codes:
                self.assertEqual(tc.verify(code, allowed), code.split("-")[1])
            with self.assertRaises(ValueError):
                tc.verify(CODE, allowed)


if __name__ == "__main__":
    unittest.main()
