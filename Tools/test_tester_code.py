#!/usr/bin/env python3
import json
import re
import tempfile
import unittest
from pathlib import Path

import tester_code as tc

ROOT = Path(__file__).resolve().parents[1]


class TesterCodeTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.key = tc.generate_key(1024)
        cls.n, cls.d = int(cls.key["n"], 16), int(cls.key["d"], 16)

    def issue(self, name="Tester", code_id="0001", issued="2026-10-26"):
        return tc.sign(tc.canonical_payload(tc.check_name(name), code_id, issued), self.n, self.d)

    def test_roundtrip_and_fields(self):
        for name in ("Tester", "베타 테스터", "x" * tc.MAX_NAME):
            fields = tc.verify(self.issue(name), self.n)
            self.assertEqual((fields["name"], fields["scope"], fields["v"]), (name, "beta", 1))

    def test_rejects_tampering_and_other_keys(self):
        code = self.issue()
        payload, signature = code[len(tc.PREFIX):].split(".")
        other = self.issue("Other")
        with self.assertRaises(ValueError):
            tc.verify(other.split(".")[0] + "." + signature, self.n)
        with self.assertRaises(ValueError):
            tc.verify(code, int(tc.generate_key(1024)["n"], 16))
        for bad in ("", "CHT1-", "CHT1-a.b.c", "XYZ1-" + code[5:], code + "A", code.replace(".", "..")):
            with self.assertRaises(ValueError):
                tc.verify(bad, self.n)

    def test_one_spelling_per_code(self):
        code = self.issue()
        variants = {code[:-1] + c for c in "ABCDEFGH"} - {code}
        for variant in variants:
            with self.assertRaises(ValueError):
                tc.verify(variant, self.n)

    def test_name_rules(self):
        for bad in ("", " ", "x" * (tc.MAX_NAME + 1), "tab\tname"):
            with self.assertRaises(ValueError):
                tc.check_name(bad)
        self.assertEqual(tc.check_name("  Padded  "), "Padded")

    def test_product_header_is_rsa2048_and_parses(self):
        n = tc.modulus_from_header(ROOT / "Source" / "TesterCodeKey.h")
        self.assertEqual(n.bit_length(), 2048)
        with tempfile.TemporaryDirectory() as tmp:
            header = Path(tmp) / "key.h"
            header.write_text(tc.header_text(self.n), encoding="utf-8")
            self.assertEqual(tc.modulus_from_header(header), self.n)

    def test_fixtures_match_test_key_not_product_key(self):
        text = (ROOT / "Tests" / "TesterCodeFixtures.h").read_text(encoding="utf-8")
        blocks = {name: "".join(re.findall(r'"([^"]+)"', body))
                  for name, body in re.findall(r"(\w+) =\s*((?:\s*\"[^\"]*\")+);", text)}
        test_n = int(blocks["testModulusHex"], 16)
        product_n = tc.modulus_from_header(ROOT / "Source" / "TesterCodeKey.h")
        self.assertNotEqual(test_n, product_n)
        for name in ("asciiCode", "koreanCode"):
            tc.verify(blocks[name], test_n)
            with self.assertRaises(ValueError):
                tc.verify(blocks[name], product_n)

    def test_private_keys_are_not_in_the_repository(self):
        for path in ROOT.rglob("*.json"):
            if ".git" in path.parts or "_deps" in path.parts or "build" in path.parts[len(ROOT.parts):][:1]:
                continue
            try:
                data = json.loads(path.read_text(encoding="utf-8"))
            except (UnicodeDecodeError, json.JSONDecodeError, OSError):
                continue
            self.assertFalse(isinstance(data, dict) and data.get("schema") == "spectralforge.chimera.tester-key.v1",
                             f"private tester key committed: {path}")


if __name__ == "__main__":
    unittest.main()
