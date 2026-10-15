#!/usr/bin/env python3
"""Bounded public-document naming regressions; never rewrites research evidence."""
from __future__ import annotations

import json
from pathlib import Path
import shutil
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "Tools"))
import check_public_names as names


class PublicPresentationContracts(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        policy = self.root / names.POLICY_PATH
        policy.parent.mkdir(parents=True)
        shutil.copyfile(ROOT / names.POLICY_PATH, policy)
        self.policy = names.load_policy(self.root)
        for relative in ["README.md", "COPYRIGHT.txt"] + ["docs/" + name for name in self.policy["public_documents"]]:
            self.write(relative, "Chimera product documentation.\n")

    def write(self, relative, text):
        path = self.root / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text, encoding="utf-8")
        return path

    def test_actual_shipped_source_documents_use_product_names(self):
        result = names.validate_source(ROOT)
        self.assertEqual(result["source_documents_checked"], len(self.policy["public_documents"]) + 2)

    def test_original_names_in_captions_tooltips_and_html_data_are_rejected(self):
        for text in ("Original amp: Fender Twin Reverb", "POST: Neve 1073",
                     '<button title="B&#79;SS SD-1">Yellow Asym</button>',
                     'const model = {caption: "Manley Variable Mu"};',
                     "Reference: Mu\u2011Tron III", "  original:  TC  Electronic 2290 "):
            with self.subTest(text=text), self.assertRaisesRegex(RuntimeError, "Original amp/effect name"):
                names.validate_text(text, "injected public presentation", policy=self.policy)

    def test_source_scan_ignores_private_research_catalogs_and_immutable_evidence(self):
        self.write("Source/AmpCatalog.h", 'private_reference = "Fender Twin Reverb";')
        evidence = self.write("docs/evidence/historical/raw.txt", "BOSS SD-1 measured reference\n")
        before = evidence.read_bytes()
        names.validate_source(self.root)
        self.assertEqual(evidence.read_bytes(), before)
        self.write("docs/MANUAL.html", '<script>const catalog=["Fender Twin Reverb"]</script>')
        with self.assertRaisesRegex(RuntimeError, "MANUAL.html"):
            names.validate_source(self.root)

    def test_aliases_descriptors_and_stable_ids_remain_valid(self):
        for text in ("Amber Crown / dense low mids", "Náströnd / Niflheimr", "Tight 515 / Chime 30",
                     "Mid-forward soft clipping", "board0_m29_boss_drive", "amatchless.png",
                     "A DRIVE / B DRIVE / A > B / B > A"):
            names.validate_text(text, "preserved product identity", policy=self.policy)

    def test_only_exact_attribution_fragments_are_exempt(self):
        source = "\n".join(self.policy["required_attribution_exceptions"]["THIRD_PARTY_NOTICES.md"])
        path = self.write("docs/THIRD_PARTY_NOTICES.md", source)
        names.validate_document(path, root=self.root)
        self.write("docs/THIRD_PARTY_NOTICES.md", source + "\nOur new ENGL amplifier.\n")
        with self.assertRaisesRegex(RuntimeError, "Original amp/effect name"):
            names.validate_document(path, root=self.root)
        with self.assertRaisesRegex(RuntimeError, "Original amp/effect name"):
            names.validate_document(self.write("docs/MANUAL.html", source), root=self.root)

    def test_unexpected_extra_packaged_text_is_still_checked(self):
        stage = self.root / "payload"
        stage.mkdir()
        (stage / "MANUAL.html").write_text("Chimera sound controls", encoding="utf-8")
        (stage / "payload-manifest.json").write_text(json.dumps({"id": "board0_m29_boss_drive"}), encoding="utf-8")
        names.validate_stage(stage, self.root)
        for file in ("extra-caption.txt", "Verification.txt", "CTest.log"):
            path = stage / file
            path.write_text("BOSS SD-1 reference", encoding="utf-8")
            with self.subTest(file=file), self.assertRaisesRegex(RuntimeError, "Original amp/effect name"):
                names.validate_stage(stage, self.root)
            path.unlink()

    def test_research_document_is_not_an_allowed_public_attachment(self):
        with self.assertRaisesRegex(RuntimeError, "Research/unreviewed document"):
            names.validate_document_names(["MANUAL.html", "MODELS_AND_REFERENCE.md"], self.root)
        path = self.write("payload/ARTWORK_PROMPTS.json", "{}")
        with self.assertRaisesRegex(RuntimeError, "Research document"):
            names.validate_stage(path.parent, self.root)

    def test_missing_public_document_does_not_silently_pass(self):
        (self.root / "docs/MANUAL.html").unlink()
        with self.assertRaisesRegex(RuntimeError, "missing"):
            names.validate_source(self.root)


if __name__ == "__main__":
    unittest.main()
