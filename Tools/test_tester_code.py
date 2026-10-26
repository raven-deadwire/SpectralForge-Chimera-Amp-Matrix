#!/usr/bin/env python3
import json
import re
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
            self.assertIn(json.loads(path.read_text())["secret"], made.stdout)
            self.assertNotEqual(run("secret", "--out", str(path)).returncode, 0)  # never overwrites
            issued = run("issue", "--secret", str(path), "--name", "Ziel", "--name", "Seo Mary")
            codes = issued.stdout.split()
            self.assertEqual(len(codes), 2, issued.stderr)
            self.assertEqual(run("verify", "--secret", str(path), "--code", codes[1]).stdout.strip(), "SEOMARY")

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
