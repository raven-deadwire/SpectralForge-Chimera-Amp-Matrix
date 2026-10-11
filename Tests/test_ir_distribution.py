#!/usr/bin/env python3
"""Regression checks for reviewed embedded IRs and public package exclusions."""
from __future__ import annotations
import io
import json
from pathlib import Path
import shutil
import sys
import tempfile
import unittest
import zipfile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "Tools"))
import check_ir_distribution as ir


class IRDistributionTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="chimera-ir-release-")
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        for name in ("CMakeLists.txt", "Source/IRReferenceCatalog.h",
                     "Validation/ir-distribution-policy.json", "docs/THIRD_PARTY_NOTICES.md", *ir.ASSETS):
            target = self.root / name
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(ROOT / name, target)
        self.stage = self.root / "stage"
        self.stage.mkdir()
        (self.stage / "fixture.exe").write_bytes(b"MZ-public-product-fixture")
        (self.stage / "THIRD_PARTY_NOTICES.md").write_text("Attribution fixture", encoding="utf-8")

    def test_reviewed_source_and_product_payload_pass(self):
        result = ir.validate_source(self.root)
        self.assertEqual([row["source_id"] for row in result["allowed_embedded"]], [1, 2])
        self.assertEqual(result["reference_entries"], 0)
        self.assertFalse(result["loose_audio_allowed"])
        ir.validate_stage(self.stage)

    def test_changed_or_additional_embedded_bytes_are_rejected(self):
        target = self.root / ir.ASSETS[0]
        original = target.read_bytes()
        target.write_bytes(original[:-1] + bytes([original[-1] ^ 1]))
        with self.assertRaisesRegex(RuntimeError, "bytes differ"):
            ir.validate_source(self.root)
        target.write_bytes(original)
        (target.parent / "unreviewed.wav").write_bytes(original)
        with self.assertRaisesRegex(RuntimeError, "Unapproved/missing"):
            ir.validate_source(self.root)

    def test_source_id_license_and_attribution_must_remain_recorded(self):
        policy_path = self.root / "Validation/ir-distribution-policy.json"
        baseline = json.loads(policy_path.read_text(encoding="utf-8"))
        for key, value in (("source_id", 3), ("license", "unverified"), ("author", "")):
            policy = json.loads(json.dumps(baseline))
            policy["allowed_embedded"][0][key] = value
            policy_path.write_text(json.dumps(policy), encoding="utf-8")
            with self.subTest(key=key), self.assertRaises(RuntimeError):
                ir.validate_source(self.root)
        policy_path.write_text(json.dumps(baseline), encoding="utf-8")
        (self.root / "docs/THIRD_PARTY_NOTICES.md").write_text("missing original credits", encoding="utf-8")
        with self.assertRaisesRegex(RuntimeError, "Missing packaged attribution"):
            ir.validate_source(self.root)

    def test_cmake_cannot_embed_extra_or_reordered_ir_sources(self):
        path = self.root / "CMakeLists.txt"
        baseline = path.read_text(encoding="utf-8")
        for changed in (baseline.replace(ir.ASSETS[0], "Assets/IRs/private.wav"),
                        baseline.replace("SOURCES Assets/IRs", "SOURCES hidden.wav Assets/IRs", 1),
                        baseline + "\njuce_add_binary_data(Other SOURCES private/capture.wav)\n"):
            path.write_text(changed, encoding="utf-8")
            with self.assertRaises(RuntimeError):
                ir.validate_source(self.root)

    def test_unapproved_catalog_cannot_return(self):
        path = self.root / "Source/IRReferenceCatalog.h"
        baseline = path.read_text(encoding="utf-8")
        path.write_text(baseline.replace('R"IRCAT([])IRCAT"', 'R"IRCAT([{"file":"private.wav"}])IRCAT"', 1), encoding="utf-8")
        with self.assertRaisesRegex(RuntimeError, "Unapproved reference catalog"):
            ir.validate_source(self.root)

    def test_reference_audio_exception_and_capture_paths_are_closed(self):
        for name in ("reference-audio/synthetic.wav", "REFERENCE/ir-catalog.json",
                     "Chimera-Personal-IRs/notes.json", "capture.NAM", "unknown.FLAC",
                     "evidence/private-reference.json", "RAVEN_IR.md", "capture-pack.ZIP"):
            path = self.stage / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(b"fixture")
            with self.subTest(name=name), self.assertRaises(RuntimeError):
                ir.validate_stage(self.stage)
            path.unlink()
            while path.parent != self.stage:
                parent = path.parent
                if not any(parent.iterdir()):
                    parent.rmdir()
                path = parent

    def test_disguised_audio_and_archives_are_rejected_by_content(self):
        path = self.stage / "innocent.dat"
        for data in (b"RIFF\x24\x00\x00\x00WAVEdata", b"FORM\x24\x00\x00\x00AIFFdata",
                     b"fLaC-fixture", b"PK\x03\x04-archive", b"\x1f\x8b-gzip"):
            path.write_bytes(data)
            with self.subTest(data=data), self.assertRaisesRegex(RuntimeError, "disguised"):
                ir.validate_stage(self.stage)

    def test_stage_does_not_follow_external_symlinks(self):
        outside = self.root / "outside"
        outside.write_bytes(b"private")
        link = self.stage / "linked.dat"
        try:
            link.symlink_to(outside)
        except OSError:
            self.skipTest("Symlink creation is unavailable on this runner")
        with self.assertRaisesRegex(RuntimeError, "escapes stage"):
            ir.validate_stage(self.stage)
        self.assertEqual(outside.read_bytes(), b"private")

    def test_portable_archive_repeats_content_and_path_checks(self):
        for name, data in (("reference-audio/private.wav", b"audio"),
                           ("resource.dat", b"RIFF\x24\x00\x00\x00WAVEdata"),
                           ("../capture.bin", b"data")):
            stream = io.BytesIO()
            with zipfile.ZipFile(stream, "w") as archive:
                archive.writestr("Standalone/Chimera.exe", b"MZ-fixture")
                archive.writestr(name, data)
            stream.seek(0)
            with zipfile.ZipFile(stream) as archive, self.subTest(name=name), self.assertRaises(RuntimeError):
                ir.validate_zip_payload(archive)

    def test_duplicate_policy_fields_are_rejected(self):
        path = self.root / "Validation/ir-distribution-policy.json"
        path.write_text('{"schema":"spectralforge.ir-distribution.v1","schema":"override"}', encoding="utf-8")
        with self.assertRaisesRegex(RuntimeError, "Duplicate IR policy key"):
            ir.validate_source(self.root)


if __name__ == "__main__":
    unittest.main()
