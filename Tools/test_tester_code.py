#!/usr/bin/env python3
import json
import os
import re
import stat
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

import tester_code as tc

ROOT = Path(__file__).resolve().parents[1]
TOOL = Path(__file__).resolve().parent / "tester_code.py"


def fixtures() -> dict:
    text = (ROOT / "Tests" / "TesterCodeFixtures.h").read_text(encoding="utf-8")
    return dict(re.findall(r'(\w+) = "([^"]*)"', text))


class TesterCodeTests(unittest.TestCase):
    def setUp(self):
        self.secret = bytes(range(32))

    def test_roundtrip_and_name_rules(self):
        for raw, name in (("Ziel", "ZIEL"), ("Seo Mary", "SEOMARY"), ("K.loop", "KLOOP"), ("bard", "BARD")):
            code = tc.issue(self.secret, raw)
            self.assertTrue(code.startswith(f"CHT1-{name}-"))
            self.assertEqual(tc.verify(code, self.secret), name)
        for bad in ("", "A", "X" * 17, "지엘", "Ziel!"):
            with self.assertRaises(ValueError):
                tc.check_name(bad)

    def test_check_is_not_guessable_from_another_code(self):
        ziel = tc.issue(self.secret, "Ziel")
        with self.assertRaises(ValueError):
            tc.verify("CHT1-BARD-" + ziel.split("-")[2], self.secret)
        with self.assertRaises(ValueError):
            tc.verify(ziel, bytes(range(1, 33)))
        checks = {tc.check(self.secret, f"T{i:03d}") for i in range(300)}
        self.assertGreater(len(checks), 295)

    def test_typing_aids(self):
        code = tc.issue(self.secret, "Ziel")
        self.assertEqual(tc.verify(" " + code.lower() + "\n", self.secret), "ZIEL")
        for bad in ("CHT1ZIEL", "CHT1-ZIEL", code + "-X", code[:-1]):
            with self.assertRaises(ValueError):
                tc.verify(bad, self.secret)

    def test_fixtures_match_this_implementation(self):
        f = fixtures()
        secret = bytes.fromhex(f["testSecretHex"])
        self.assertEqual(secret, bytes(range(0x10, 0x30)))
        for key, raw in (("zielCode", "Ziel"), ("seoMaryCode", "Seo Mary"), ("kloopCode", "K.loop"),
                         ("shortestCode", "AB"), ("longestCode", "X" * 16)):
            self.assertEqual(f[key], tc.issue(secret, raw))

    def test_cli_secret_issue_verify(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "s.private.json"
            run = lambda *a: subprocess.run([sys.executable, str(TOOL), *a], capture_output=True, text=True)
            made = run("secret", "--out", str(path))
            self.assertEqual(made.returncode, 0, made.stderr)
            self.assertNotIn(json.loads(path.read_text())["secret"], made.stdout)
            self.assertNotEqual(run("secret", "--out", str(path)).returncode, 0)  # never overwrites
            issued = run("issue", "--secret", str(path), "--name", "Ziel", "--name", "Seo Mary")
            codes = issued.stdout.split()
            self.assertEqual(len(codes), 2, issued.stderr)
            self.assertEqual(run("verify", "--secret", str(path), "--code", codes[1]).stdout.strip(), "SEOMARY")

    def test_invalid_paste_and_secret_fail_closed(self):
        code = tc.issue(self.secret, "Ziel")
        for bad in (" " * tc.MAX_PASTED + code, code + "\u00a0", "CHT1-ZIEL-가나다라마바",
                    "CHT1-ZIEL-!!!!!!", "CHT1-ZIEL-SSSSSß", "A" * 5000):
            with self.subTest(bad=bad[:40]), self.assertRaises(ValueError):
                tc.verify(bad, self.secret)
        for weak in (b"", bytes(15)):
            with self.assertRaises(ValueError):
                tc.issue(weak, "TESTER")
            with self.assertRaises(ValueError):
                tc.verify(code, weak)
        with tempfile.TemporaryDirectory() as tmp:
            p = Path(tmp) / "malformed.json"
            for value in (None, 123, "a" * 33, "g" * 64, "00" * 15):
                p.write_text(json.dumps({"schema": tc.SCHEMA, "secret": value}))
                with self.assertRaises(ValueError):
                    tc.load_secret(p)

    def test_secret_creation_is_private_exclusive_and_outside_git(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            target = root / "test.private.json"
            value = self.secret.hex()
            old_mask = os.umask(0)
            try:
                tc.write_secret(target, value)
            finally:
                os.umask(old_mask)
            if os.name == "posix":
                self.assertEqual(stat.S_IMODE(target.stat().st_mode), 0o600)
            original = target.read_bytes()
            with self.assertRaises(FileExistsError):
                tc.write_secret(target, value)
            self.assertEqual(target.read_bytes(), original)
            repo = root / "checkout"
            (repo / ".git").mkdir(parents=True)
            with self.assertRaises(ValueError):
                tc.write_secret(repo / "ordinary-name.json", value)
            if os.name == "posix":
                link = root / "link.private.json"
                link.symlink_to(target)
                with self.assertRaises(FileExistsError):
                    tc.write_secret(link, value)
            shown = root / "explicit.private.json"
            result = subprocess.run([sys.executable,str(TOOL),"secret","--out",str(shown),"--show-secret"], capture_output=True,text=True)
            self.assertEqual(result.returncode,0,result.stderr)
            self.assertIn(json.loads(shown.read_text())["secret"],result.stdout)

    @unittest.skipUnless(os.environ.get("CHIMERA_TESTER_VERIFIER"), "native verifier supplied by CTest")
    def test_native_verifier_matches_input_boundaries(self):
        code = tc.issue(self.secret, "Ziel")
        cases = [code, code.lower(), " " + code + "\n", " " * tc.MAX_PASTED + code,
                 code + "\u00a0", "CHT1-ZIEL-가나다라마바", "CHT1-ZIEL-!!!!!!", "CHT1-X-000000",
                 tc.issue(self.secret, "SS").replace("-SS-", "-ß-"),
                 tc.issue(self.secret, "KELVIN").replace("-KELVIN-", "-KELVIN-")]
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "code.txt"
            for text in cases:
                try:
                    tc.verify(text,self.secret)
                    expected = 0
                except ValueError:
                    expected = 1
                path.write_text(text,encoding="utf-8")
                result = subprocess.run([os.environ["CHIMERA_TESTER_VERIFIER"],"--verify-test-code",self.secret.hex(),str(path)],timeout=5)
                self.assertEqual(result.returncode,expected)

    def test_no_secret_in_repository(self):
        for path in ROOT.rglob("*.json"):
            if ".git" in path.parts or "_deps" in path.parts or "build" in path.parts[len(ROOT.parts):][:1]:
                continue
            try:
                data = json.loads(path.read_text(encoding="utf-8"))
            except (UnicodeDecodeError, json.JSONDecodeError, OSError):
                continue
            self.assertFalse(isinstance(data, dict) and data.get("schema") == tc.SCHEMA,
                             f"tester secret committed: {path}")
        self.assertFalse((ROOT / "Source" / "TesterCodeKey.h").exists())


if __name__ == "__main__":
    unittest.main()
