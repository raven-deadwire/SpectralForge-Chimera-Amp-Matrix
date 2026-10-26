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
            body = out.read_text()
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


if __name__ == "__main__":
    unittest.main()
