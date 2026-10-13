"""Exercise deletion authority and exact Windows installation path mapping."""
from __future__ import annotations

import copy
import hashlib
import json
import os
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "Tools"))
import windows_document_rules as rules


def inventory(path="AMP_VALIDATION.md", data=b"released\r\n", *, retire=True, keep=False):
    return {"schema": rules.SCHEMA, "files": [{
        "path": path, "platforms": ["windows"], "retire": retire, "preserve_always": keep,
        "variants": [{"size": len(data), "sha256": hashlib.sha256(data).hexdigest(),
                      "platforms": ["windows"], "releases": ["fixture"],
                      "evidence": [{"kind": "test-fixture"}]}],
    }]}


class WindowsDocumentRulesTest(unittest.TestCase):
    def test_historical_and_current_stage_mapping_and_raw_hashes(self):
        old = b"released\r\n"
        current = b"new manual\n"
        with tempfile.TemporaryDirectory() as folder:
            stage = Path(folder)
            (stage / "README.txt").write_bytes(current)
            (stage / "MANUAL.html").write_bytes(current)
            actual = rules.document_rules(inventory(data=old), stage)
        self.assertIn(("Documentation\\AMP_VALIDATION.md", len(old), hashlib.sha256(old).hexdigest(), True, False), actual)
        self.assertIn(("README.txt", len(current), hashlib.sha256(current).hexdigest(), False, False), actual)
        self.assertIn(("Documentation\\MANUAL.html", len(current), hashlib.sha256(current).hexdigest(), False, False), actual)
        self.assertNotIn(hashlib.sha256(old.replace(b"\r\n", b"\n")).hexdigest(), str(actual))

    def test_nested_reference_mapping_is_finite_and_preservable(self):
        self.assertEqual(rules.install_path("reference/ir-tags/fixture.wav.json"),
                         "Documentation\\reference\\ir-tags\\fixture.wav.json")
        with tempfile.TemporaryDirectory() as folder:
            actual = rules.document_rules(inventory("reference/ir-tags/fixture.wav.json", retire=False, keep=True), Path(folder))
        self.assertTrue(actual[0][-1])
        self.assertFalse(actual[0][-2])

    def test_no_personal_audio_presets_projects_or_arbitrary_json_can_be_authority(self):
        for path in ("IRs/personal.wav", "reference/personal.wav", "personal.chimera", "existing.song",
                     "private.json", "Documentation/AMP_VALIDATION.md", "PreservedDocuments/readme.txt"):
            with self.subTest(path=path), self.assertRaises(ValueError):
                rules.install_path(path)

    def test_ambiguous_windows_paths_rejected(self):
        for path in ("../README.txt", "/README.txt", "C:/README.txt", "reference/../README.txt",
                     "reference//README.txt", "reference\\README.txt", "README.txt:stream", "README*.txt",
                     "README.txt ", "README.txt.", "read'me.txt", "{app}/README.txt", "README\x00.txt"):
            with self.subTest(path=path), self.assertRaises(ValueError):
                rules.install_path(path)

    def test_license_never_becomes_retirement_rule(self):
        for name in ("THIRD_PARTY_NOTICES.md", "COPYRIGHT.txt", "LICENSE.txt"):
            with self.subTest(name=name), tempfile.TemporaryDirectory() as folder, self.assertRaises(ValueError):
                rules.document_rules(inventory(name), Path(folder))

    def test_stage_links_are_not_trusted(self):
        for link in ("symlink", "hardlink"):
            with self.subTest(link=link), tempfile.TemporaryDirectory() as folder:
                stage = Path(folder) / "stage"
                stage.mkdir()
                target = Path(folder) / "target"
                target.write_bytes(b"manual")
                if link == "symlink":
                    (stage / "MANUAL.html").symlink_to(target)
                else:
                    os.link(target, stage / "MANUAL.html")
                with self.assertRaises(ValueError):
                    rules.document_rules(inventory(), stage)

    def test_retired_document_cannot_reenter_current_payload(self):
        with tempfile.TemporaryDirectory() as folder:
            stage = Path(folder)
            (stage / "AMP_VALIDATION.md").write_bytes(b"new unreviewed research")
            with self.assertRaises(ValueError):
                rules.document_rules(inventory(), stage)

    def test_unproven_or_malformed_hashes_are_rejected(self):
        changes = ({"sha256": "f" * 63}, {"sha256": "F" * 64}, {"size": -1},
                   {"size": True}, {"size": rules.MAX_DOCUMENT_BYTES + 1}, {"evidence": []}, {"releases": []})
        for change in changes:
            value = inventory()
            value["files"][0]["variants"][0].update(change)
            with self.subTest(change=change), tempfile.TemporaryDirectory() as folder, self.assertRaises(ValueError):
                rules.document_rules(value, Path(folder))

    def test_nonwindows_variants_never_become_windows_delete_authority(self):
        value = inventory()
        value["files"][0]["variants"][0]["platforms"] = ["linux"]
        with tempfile.TemporaryDirectory() as folder:
            self.assertEqual(rules.document_rules(value, Path(folder)), [])

    def test_case_colliding_paths_rejected(self):
        value = inventory()
        other = copy.deepcopy(value["files"][0])
        other["path"] = "amp_validation.md"
        value["files"].append(other)
        with tempfile.TemporaryDirectory() as folder, self.assertRaises(ValueError):
            rules.document_rules(value, Path(folder))

    def test_reviewed_production_inventory_can_compile_without_normalization(self):
        value = json.loads((ROOT / "Validation/legacy-document-inventory.json").read_text())
        with tempfile.TemporaryDirectory() as folder:
            text = rules.render_rules(value, Path(folder))
        self.assertIn("procedure LoadDocumentRules;", text)
        self.assertIn("Documentation\\ARTWORK_PROMPTS.json", text)
        self.assertIn("False, True", text)
        self.assertNotIn("LoadStringFromFile", text)


if __name__ == "__main__":
    unittest.main()
