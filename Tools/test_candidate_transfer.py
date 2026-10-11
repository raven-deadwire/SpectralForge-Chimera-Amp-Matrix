#!/usr/bin/env python3
"""Exercise the production Setup/ZIP transport paths without native binaries."""
from __future__ import annotations

import contextlib
import hashlib
import io
import json
import os
from pathlib import Path
import re
import tempfile
from types import SimpleNamespace
import unittest
from unittest import mock

import chimera_version
import package_installer_candidate as installer
import package_update_candidate as portable

ROOT = Path(__file__).resolve().parents[1]
SHA = "1234567890abcdef1234567890abcdef12345678"
CHUNK = 128


@contextlib.contextmanager
def fixture(kind: str, size: int):
    with tempfile.TemporaryDirectory(prefix="chimera-transfer-") as temporary:
        root = Path(temporary)
        (root / "VERSION").write_text("1.2.0\n", encoding="utf-8")
        (root / "RELEASE_CHANNEL").write_text("preview\n", encoding="utf-8")
        identity = chimera_version.identity(root, SHA)
        data = bytes(((i * 37) ^ (i >> 3) ^ 0x5A) & 255 for i in range(size))
        digest = hashlib.sha256(data).hexdigest()
        copied = []
        if kind == "installer":
            artifact = root / "installer-candidate" / f"SpectralForge-Chimera-update-{SHA[:10]}-win64-Setup.exe"
            output = root / "installer-transfer"
            artifact.parent.mkdir()
            checksum = artifact.with_name(artifact.name + ".sha256.txt")
            receipt = {**identity, "success": True, "installer_sha256": digest}
            evidence = root / "build/candidate-installer-verification"
            evidence.mkdir(parents=True)
            receipt_path = evidence / "InstallerVerification.json"
            receipt_path.write_text(json.dumps(receipt), encoding="utf-8-sig")
            copied.append(receipt_path)
            for relative in ("build/candidate-installer-verification/InstallerVerification.txt",
                             "installer-candidate/payload/payload-manifest.json",
                             "installer-candidate/payload/WINDOWS_INSTALL.txt",
                             "build/candidate-installer-security/WindowsSecurity.txt"):
                path = root / relative
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text("Synthetic transport fixture; not a security attestation.\n", encoding="utf-8")
                copied.append(path)
            invoke = installer.transfer
            manifest_name = "installer-parts-manifest.json"
            original_manifest = receipt
        else:
            artifact = root / "candidate" / f"Chimera-update-{SHA[:10]}-win64.zip"
            output = root / "candidate-transfer"
            artifact.parent.mkdir()
            checksum = artifact.with_name(artifact.name + ".sha256")
            original_manifest = {**identity, "run_id": "12345", "run_attempt": "1",
                                 "published_release": False, "publisher_signed": False, "files": []}
            receipt_path = None
            invoke = lambda: portable.transfer(artifact, digest, SHA, original_manifest)
            manifest_name = "parts-manifest.json"
        artifact.write_bytes(data)
        checksum.write_text(digest + "  " + artifact.name + "\n", encoding="utf-8-sig")
        copied.append(checksum)
        with mock.patch.object(installer, "ROOT", root), \
             mock.patch.object(installer, "revision", return_value=SHA), \
             mock.patch.object(installer, "TRANSFER_PART_BYTES", CHUNK), \
             mock.patch.object(portable, "root", root), \
             mock.patch.dict(os.environ, {"GITHUB_RUN_ID": "12345"}), \
             contextlib.redirect_stdout(io.StringIO()):
            yield SimpleNamespace(root=root, artifact=artifact, output=output, invoke=invoke,
                                  data=data, digest=digest, manifest_name=manifest_name,
                                  copied=copied, original_manifest=original_manifest,
                                  receipt_path=receipt_path)


class CandidateTransferTests(unittest.TestCase):
    def test_both_streams_reconstruct_past_old_four_part_limit_and_at_eight_parts(self):
        # A scaled chunk keeps tests fast while crossing the same 4/8-part
        # boundaries as 80/160 MiB production packages, including a short tail.
        for kind in ("installer", "portable"):
            for size, count in ((CHUNK * 4 + 19, 5), (CHUNK * 8, 8)):
                with self.subTest(kind=kind, parts=count), fixture(kind, size) as f:
                    f.invoke()
                    metadata = f.output / "metadata"
                    manifest = json.loads((metadata / f.manifest_name).read_text(encoding="utf-8"))
                    self.assertEqual(manifest["source_sha"], SHA)
                    self.assertEqual(manifest["bytes"], len(f.data))
                    self.assertEqual(manifest["sha256"], f.digest)
                    self.assertEqual(len(manifest["parts"]), count)
                    recovered = bytearray()
                    for number, part in enumerate(manifest["parts"], 1):
                        self.assertEqual(part["name"], f.artifact.name + f".part{number}")
                        data = (f.output / f"part-{number}" / part["name"]).read_bytes()
                        self.assertEqual(part["bytes"], len(data))
                        self.assertLessEqual(len(data), CHUNK)
                        self.assertEqual(part["sha256"], hashlib.sha256(data).hexdigest())
                        recovered.extend(data)
                    self.assertEqual(bytes(recovered), f.data)
                    self.assertEqual(hashlib.sha256(recovered).hexdigest(), manifest["sha256"])
                    self.assertEqual(len(list(f.output.glob("part-*"))), count)
                    for original in f.copied:
                        self.assertEqual((metadata / original.name).read_bytes(), original.read_bytes())
                    if kind == "installer":
                        self.assertEqual(manifest["installer_verification"], f.original_manifest)
                        self.assertEqual(manifest["run_id"], "12345")
                        self.assertFalse(manifest["published_release"])
                    else:
                        self.assertEqual(json.loads((metadata / "build-manifest.json").read_text()), f.original_manifest)

    def test_both_streams_reject_ninth_part_before_output(self):
        for kind in ("installer", "portable"):
            with self.subTest(kind=kind), fixture(kind, CHUNK * 8 + 1) as f:
                with self.assertRaisesRegex(RuntimeError, "1..8 transfer artifacts"):
                    f.invoke()
                self.assertFalse(f.output.exists())

    def test_both_streams_preserve_checksum_and_source_rejection(self):
        for kind in ("installer", "portable"):
            with self.subTest(kind=kind, mismatch="checksum"), fixture(kind, CHUNK * 4 + 7) as f:
                changed = bytearray(f.data)
                changed[-1] ^= 1
                f.artifact.write_bytes(changed)
                with self.assertRaises((RuntimeError, ValueError)):
                    f.invoke()
                self.assertFalse(f.output.exists())
            with self.subTest(kind=kind, mismatch="source"), fixture(kind, CHUNK * 4 + 7) as f:
                f.original_manifest["source_sha"] = "f" * 40
                if f.receipt_path:
                    f.receipt_path.write_text(json.dumps(f.original_manifest), encoding="utf-8-sig")
                with self.assertRaises((RuntimeError, ValueError)):
                    f.invoke()
                self.assertFalse(f.output.exists())

    def test_workflow_uploads_every_bounded_part_after_security_gate(self):
        self.assertEqual(installer.TRANSFER_PART_BYTES, 20 * 1024 * 1024)
        self.assertEqual(installer.TRANSFER_MAX_PARTS, 8)
        workflow = (ROOT / ".github/workflows/update-candidate.yml").read_text(encoding="utf-8")
        seen = {"installer": [], "candidate": []}
        for block in re.split(r"(?m)^      - ", workflow):
            match = re.search(r"(?m)^          name: Chimera-(installer|candidate)-part-(\d+)$", block)
            if not match:
                continue
            stream, number = match.group(1), int(match.group(2))
            seen[stream].append(number)
            self.assertTrue(block.startswith("uses: actions/upload-artifact@"))
            directory = "installer-transfer" if stream == "installer" else "candidate-transfer"
            path = f"{directory}/part-{number}/*"
            self.assertIn(f"          path: {path}\n", block)
            self.assertIn("          compression-level: 0\n", block)
            self.assertIn("          if-no-files-found: error\n", block)
            self.assertNotIn("always()", block)
            if number > 1:
                self.assertIn(f"if: success() && hashFiles('{path}') != ''", block)
        for stream, numbers in seen.items():
            self.assertEqual(numbers, list(range(1, 9)), stream)
        self.assertLess(workflow.index("run: python Tools/test_candidate_transfer.py"), workflow.index("- name: Configure"))
        scan = workflow.index("- name: Scan candidate Setup with Microsoft Defender")
        split = workflow.index("- name: Prepare verified Setup transfer parts")
        self.assertLess(scan, split)
        security_step = workflow[scan:split]
        self.assertIn("./Tools/Scan-WindowsArtifacts.ps1 -Path", security_step)
        self.assertNotIn("continue-on-error", security_step)
        self.assertNotIn("if:", security_step)
        for stream in seen:
            self.assertGreater(workflow.index(f"name: Chimera-{stream}-part-1"), split)


if __name__ == "__main__":
    unittest.main(verbosity=2)
