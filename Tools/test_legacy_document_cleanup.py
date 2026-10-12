#!/usr/bin/env python3
"""Regression tests for bounded legacy-document migration.

The native helper tests use synthetic document bytes and directories.  The dpkg
tests invoke the real package manager in an isolated --root with tiny fixture
packages; they do not claim to install a released Chimera binary or to validate
Windows/macOS installers, audio, DAWs, signing, or publication readiness.
"""
from __future__ import annotations

import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

import legacy_document_cleanup as cleanup


OLD_VERSION = "1.3.1~beta.1"
NEW_VERSION = "1.3.1~beta.2"
PACKAGE = "chimera-amp-matrix"
DOC_RELATIVE = Path("usr/share/doc/chimera-amp-matrix")
PLATFORMS = ["linux", "macos", "windows"]

# Synthetic fixture text deliberately contains no original equipment names.
OLD_DOCUMENTS = {
    "AMP_VALIDATION.md": b"Factory diagnostic fixture.\n",
    "MODELS_AND_REFERENCE.md": b"Factory research fixture.\n",
    "AMP_NATIVE_DSP.md": b"Linux-specific research fixture.\n",
    "MANUAL.html": b"<p>Previous fixture manual.</p>\n",
    "THIRD_PARTY_NOTICES.md": b"Fixture attribution: preserve this notice.\n",
    "COPYRIGHT.txt": b"Fixture copyright: preserve this notice.\n",
}
CURRENT_DOCUMENTS = {
    "MANUAL.html": b"<p>Current fixture manual.</p>\n",
    "THIRD_PARTY_NOTICES.md": OLD_DOCUMENTS["THIRD_PARTY_NOTICES.md"],
    "COPYRIGHT.txt": OLD_DOCUMENTS["COPYRIGHT.txt"],
}
WINDOWS_ONLY_VARIANT = b"Windows-specific research fixture.\r\n"
USER_EDIT = b"User annotations. These bytes must survive.\n"


def digest(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def write_documents(root: Path, documents: dict[str, bytes]) -> None:
    for relative, content in documents.items():
        path = root / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(content)


def fixture_inventory() -> dict:
    def variant(path: str, content: bytes, platforms: list[str]) -> dict:
        return {
            "size": len(content),
            "sha256": digest(content),
            "releases": ["fixture-beta.1"],
            "platforms": platforms,
            "evidence": [{
                "kind": "release-package",
                "tag": "fixture-beta.1",
                "source_sha": "1" * 40,
                "archive_sha256": "2" * 64,
                "package_path": path,
                "fixture_only": True,
            }],
        }

    files = []
    for relative, content in OLD_DOCUMENTS.items():
        variants = [variant(relative, content, PLATFORMS)]
        if relative == "AMP_NATIVE_DSP.md":
            variants[0]["platforms"] = ["linux", "macos"]
            variants.append(variant(relative, WINDOWS_ONLY_VARIANT, ["windows"]))
        files.append({
            "path": relative,
            "platforms": PLATFORMS,
            "retire": relative not in CURRENT_DOCUMENTS,
            "preserve_always": relative in {"THIRD_PARTY_NOTICES.md", "COPYRIGHT.txt"},
            "variants": variants,
        })
    return {
        "schema": "spectralforge.legacy-document-inventory.v1",
        "effective_release": "1.3.1-beta.2",
        "fixture_only": True,
        "releases": [{
            "tag": "fixture-beta.1",
            "source_sha": "1" * 40,
            "release_url": "https://example.invalid/synthetic-fixture-only",
        }],
        "files": files,
    }


class NativeFixtureCase(unittest.TestCase):
    """Build once, then exercise native operations in private directories."""

    @classmethod
    def setUpClass(cls) -> None:
        if os.name != "posix" or shutil.which("cc") is None:
            raise unittest.SkipTest("Native POSIX C compiler unavailable; not an installer PASS")
        cls.build_directory = tempfile.TemporaryDirectory(prefix="chimera-cleanup-build-")
        cls.addClassCleanup(cls.build_directory.cleanup)
        cls.build_root = Path(cls.build_directory.name)
        cls.inventory = cls.build_root / "fixture-inventory.json"
        cls.inventory.write_text(json.dumps(fixture_inventory()), encoding="utf-8")
        cls.current_documents = cls.build_root / "current-documents"
        write_documents(cls.current_documents, CURRENT_DOCUMENTS)
        try:
            cls.helper = cleanup.build_helper(
                cls.build_root / "legacy-document-cleanup",
                platform="macos" if sys.platform == "darwin" else "linux",
                inventory=cls.inventory,
                current_documents=cls.current_documents,
            )
        except subprocess.CalledProcessError as error:
            raise RuntimeError(
                f"Native fixture helper failed to build:\n{error.stdout}\n{error.stderr}"
            ) from error

    def setUp(self) -> None:
        self.temporary = tempfile.TemporaryDirectory(prefix="chimera-document-fixture-")
        self.addCleanup(self.temporary.cleanup)
        # macOS may expose its temporary directory through /var -> /private/var.
        # Use the canonical fixture root; linked-path tests add their own links.
        self.work = Path(self.temporary.name).resolve()
        self.docroot = self.work / "Chimera 문서 fixture"

    def helper_run(self, mode: str, *, root: Path | None = None,
                   success: bool = True) -> subprocess.CompletedProcess:
        result = subprocess.run(
            [str(self.helper), mode, str(self.docroot if root is None else root)],
            text=True, capture_output=True, timeout=30,
        )
        detail = f"{mode}: exit={result.returncode}\n{result.stdout}\n{result.stderr}"
        if success:
            self.assertEqual(result.returncode, 0, detail)
        else:
            self.assertNotEqual(result.returncode, 0, detail)
        return result

    def install_old(self) -> None:
        write_documents(self.docroot, OLD_DOCUMENTS)

    def saved_copy(self, relative: str, content: bytes) -> Path:
        return Path(str(self.docroot) + ".preserved") / digest(content) / relative

    def require_saved(self, relative: str, content: bytes) -> None:
        saved = self.saved_copy(relative, content)
        self.assertTrue(saved.is_file(), f"Missing protected copy: {saved}")
        self.assertEqual(saved.read_bytes(), content)

    def emulate_package_unpack(self) -> None:
        # dpkg removes obsolete ordinary package files regardless of edits.
        for relative in OLD_DOCUMENTS.keys() - CURRENT_DOCUMENTS.keys():
            (self.docroot / relative).unlink(missing_ok=True)
        write_documents(self.docroot, CURRENT_DOCUMENTS)


class LegacyDocumentCleanupTests(NativeFixtureCase):
    def test_native_sha256_matches_hashlib_at_block_and_read_boundaries(self) -> None:
        contents = {length: bytes((index * 37 + 11) % 256 for index in range(length))
                    for length in (0, 1, 55, 56, 63, 64, 65, 1024, 16384, 16385)}
        inventory = fixture_inventory()
        entry = next(item for item in inventory["files"] if item["path"] == "AMP_VALIDATION.md")
        entry["variants"] = [{
            "size": length, "sha256": hashlib.sha256(content).hexdigest(),
            "platforms": PLATFORMS, "releases": ["binary-boundary-fixture"],
            "evidence": [{"kind": "native-test-fixture", "fixture_only": True}],
        } for length, content in contents.items()]
        inventory_path = self.work / "boundary-inventory.json"
        inventory_path.write_text(json.dumps(inventory), encoding="utf-8")
        helper = cleanup.build_helper(
            self.work / "boundary-helper",
            platform="macos" if sys.platform == "darwin" else "linux",
            inventory=inventory_path,
            current_documents=self.current_documents,
        )
        for length, content in contents.items():
            with self.subTest(bytes=length):
                write_documents(self.docroot, {"AMP_VALIDATION.md": content})
                result = subprocess.run([str(helper), "cleanup", str(self.docroot)],
                                        text=True, capture_output=True, timeout=30)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                self.assertFalse((self.docroot / "AMP_VALIDATION.md").exists(),
                                 f"Native SHA256 disagrees with hashlib at {length} bytes")

    def test_fresh_install_has_no_retired_documents(self) -> None:
        self.helper_run("prepare-upgrade")
        write_documents(self.docroot, CURRENT_DOCUMENTS)
        self.helper_run("commit-upgrade")
        for relative, content in CURRENT_DOCUMENTS.items():
            self.assertEqual((self.docroot / relative).read_bytes(), content)
        for relative in OLD_DOCUMENTS.keys() - CURRENT_DOCUMENTS.keys():
            self.assertFalse((self.docroot / relative).exists())

    def test_cleanup_removes_only_exact_platform_variants(self) -> None:
        self.install_old()
        (self.docroot / "MODELS_AND_REFERENCE.md").write_bytes(USER_EDIT)
        (self.docroot / "AMP_NATIVE_DSP.md").write_bytes(WINDOWS_ONLY_VARIANT)
        self.helper_run("cleanup")
        self.assertFalse((self.docroot / "AMP_VALIDATION.md").exists())
        self.assertEqual((self.docroot / "MODELS_AND_REFERENCE.md").read_bytes(), USER_EDIT)
        self.assertEqual((self.docroot / "AMP_NATIVE_DSP.md").read_bytes(), WINDOWS_ONLY_VARIANT)
        for relative in CURRENT_DOCUMENTS:
            self.assertEqual((self.docroot / relative).read_bytes(), OLD_DOCUMENTS[relative])

    def test_same_size_changed_content_is_preserved(self) -> None:
        self.install_old()
        original = OLD_DOCUMENTS["AMP_VALIDATION.md"]
        altered = b"X" + original[1:]
        (self.docroot / "AMP_VALIDATION.md").write_bytes(altered)
        self.helper_run("cleanup")
        self.assertEqual((self.docroot / "AMP_VALIDATION.md").read_bytes(), altered)

    def test_unverified_line_ending_variant_is_preserved(self) -> None:
        self.install_old()
        altered = OLD_DOCUMENTS["AMP_VALIDATION.md"].replace(b"\n", b"\r\n")
        (self.docroot / "AMP_VALIDATION.md").write_bytes(altered)
        self.helper_run("cleanup")
        self.assertEqual((self.docroot / "AMP_VALIDATION.md").read_bytes(), altered)

    def test_unknown_user_files_and_forged_runtime_manifest_are_ignored(self) -> None:
        self.install_old()
        sentinels = {
            "personal.chimera": b"User project state.\n",
            "personal.wav": b"User IR bytes.\n",
            "Private/AMP_VALIDATION.md": OLD_DOCUMENTS["AMP_VALIDATION.md"],
            "UserNotes.md": b"Unknown user document.\n",
            "payload-manifest.json": b'{"files":[{"path":"personal.chimera","sha256":"forged"}]}',
        }
        write_documents(self.docroot, sentinels)
        self.helper_run("cleanup")
        for relative, content in sentinels.items():
            self.assertEqual((self.docroot / relative).read_bytes(), content)

    def test_modified_retired_document_restored_after_unpack(self) -> None:
        self.install_old()
        (self.docroot / "AMP_VALIDATION.md").write_bytes(USER_EDIT)
        self.helper_run("prepare-upgrade")
        self.require_saved("AMP_VALIDATION.md", USER_EDIT)
        self.emulate_package_unpack()
        self.helper_run("commit-upgrade")
        self.assertEqual((self.docroot / "AMP_VALIDATION.md").read_bytes(), USER_EDIT)
        self.assertFalse((self.docroot / "MODELS_AND_REFERENCE.md").exists())
        self.require_saved("AMP_VALIDATION.md", USER_EDIT)

    def test_current_manual_edit_saved_while_repair_restores_payload(self) -> None:
        self.install_old()
        (self.docroot / "MANUAL.html").write_bytes(USER_EDIT)
        self.helper_run("prepare-upgrade")
        self.require_saved("MANUAL.html", USER_EDIT)
        self.emulate_package_unpack()
        self.helper_run("commit-upgrade")
        self.assertEqual((self.docroot / "MANUAL.html").read_bytes(), CURRENT_DOCUMENTS["MANUAL.html"])
        self.require_saved("MANUAL.html", USER_EDIT)
        self.helper_run("prepare-upgrade")
        self.emulate_package_unpack()
        self.helper_run("commit-upgrade")
        self.require_saved("MANUAL.html", USER_EDIT)

    def test_repeated_prepare_preserves_original_snapshot(self) -> None:
        self.install_old()
        (self.docroot / "AMP_VALIDATION.md").write_bytes(USER_EDIT)
        self.helper_run("prepare-upgrade")
        self.helper_run("prepare-upgrade")
        self.emulate_package_unpack()
        self.helper_run("commit-upgrade")
        self.helper_run("commit-upgrade")
        self.assertEqual((self.docroot / "AMP_VALIDATION.md").read_bytes(), USER_EDIT)

    def test_abort_restores_missing_user_document_and_is_repeatable(self) -> None:
        self.install_old()
        (self.docroot / "MANUAL.html").write_bytes(USER_EDIT)
        self.helper_run("prepare-upgrade")
        (self.docroot / "MANUAL.html").unlink()
        self.helper_run("abort")
        self.helper_run("abort")
        self.assertEqual((self.docroot / "MANUAL.html").read_bytes(), USER_EDIT)
        self.require_saved("MANUAL.html", USER_EDIT)

    def test_abort_does_not_overwrite_a_new_user_edit(self) -> None:
        self.install_old()
        (self.docroot / "MANUAL.html").write_bytes(USER_EDIT)
        self.helper_run("prepare-upgrade")
        latest = b"A newer user edit during interruption.\n"
        (self.docroot / "MANUAL.html").write_bytes(latest)
        self.helper_run("abort")
        self.assertEqual((self.docroot / "MANUAL.html").read_bytes(), latest)
        self.require_saved("MANUAL.html", USER_EDIT)

    def test_remove_restores_user_edits_and_required_notices(self) -> None:
        write_documents(self.docroot, CURRENT_DOCUMENTS)
        (self.docroot / "MANUAL.html").write_bytes(USER_EDIT)
        sentinels = {"personal.wav": b"Private IR.", "personal.chimera": b"Private project."}
        write_documents(self.docroot, sentinels)
        self.helper_run("prepare-remove")
        for relative in CURRENT_DOCUMENTS:
            (self.docroot / relative).unlink(missing_ok=True)
        self.helper_run("commit-remove")
        self.helper_run("commit-remove")
        self.assertEqual((self.docroot / "MANUAL.html").read_bytes(), USER_EDIT)
        for relative in {"THIRD_PARTY_NOTICES.md", "COPYRIGHT.txt"}:
            self.assertEqual((self.docroot / relative).read_bytes(), CURRENT_DOCUMENTS[relative])
        for relative, content in sentinels.items():
            self.assertEqual((self.docroot / relative).read_bytes(), content)

    def test_remove_documents_preserves_edits_notices_and_unknown_user_files(self) -> None:
        self.install_old()
        write_documents(self.docroot, CURRENT_DOCUMENTS)
        (self.docroot / "MANUAL.html").write_bytes(USER_EDIT)
        (self.docroot / "AMP_VALIDATION.md").write_bytes(USER_EDIT)
        sentinels = {
            "personal.chimera": b"Private preset and project bytes.\n",
            "personal.wav": b"Private IR bytes.\n",
            "Notes/unowned.md": b"User notes outside the explicit rule paths.\n",
        }
        write_documents(self.docroot, sentinels)
        self.helper_run("remove-documents")
        for relative in {"MANUAL.html", "AMP_VALIDATION.md"}:
            self.assertEqual((self.docroot / relative).read_bytes(), USER_EDIT)
            self.require_saved(relative, USER_EDIT)
        for relative in {"THIRD_PARTY_NOTICES.md", "COPYRIGHT.txt"}:
            self.assertEqual((self.docroot / relative).read_bytes(), CURRENT_DOCUMENTS[relative])
        for relative, content in sentinels.items():
            self.assertEqual((self.docroot / relative).read_bytes(), content)
        self.assertFalse((self.docroot / "MODELS_AND_REFERENCE.md").exists())
        self.assertFalse((self.docroot / "AMP_NATIVE_DSP.md").exists())

    def test_remove_documents_exact_payload_removal_is_idempotent(self) -> None:
        self.install_old()
        write_documents(self.docroot, CURRENT_DOCUMENTS)
        self.helper_run("remove-documents")
        self.assertFalse((self.docroot / "MANUAL.html").exists())
        self.assertFalse((self.docroot / "AMP_VALIDATION.md").exists())
        retained = {path.relative_to(self.docroot).as_posix(): path.read_bytes()
                    for path in self.docroot.rglob("*") if path.is_file()}
        self.helper_run("remove-documents")
        self.assertEqual(retained, {path.relative_to(self.docroot).as_posix(): path.read_bytes()
                                   for path in self.docroot.rglob("*") if path.is_file()})
        self.assertEqual(set(retained), {"THIRD_PARTY_NOTICES.md", "COPYRIGHT.txt"})

    def test_symlink_file_refuses_prepare_without_deleting_other_files(self) -> None:
        self.install_old()
        outside = self.work / "outside-user-document.md"
        outside.write_bytes(OLD_DOCUMENTS["AMP_VALIDATION.md"])
        (self.docroot / "AMP_VALIDATION.md").unlink()
        (self.docroot / "AMP_VALIDATION.md").symlink_to(outside)
        self.helper_run("prepare-upgrade", success=False)
        self.assertTrue((self.docroot / "AMP_VALIDATION.md").is_symlink())
        self.assertEqual(outside.read_bytes(), OLD_DOCUMENTS["AMP_VALIDATION.md"])
        self.assertEqual((self.docroot / "MODELS_AND_REFERENCE.md").read_bytes(), OLD_DOCUMENTS["MODELS_AND_REFERENCE.md"])

    def test_hardlink_file_refuses_prepare(self) -> None:
        self.install_old()
        outside = self.work / "hardlink-user-document.md"
        os.link(self.docroot / "AMP_VALIDATION.md", outside)
        self.helper_run("prepare-upgrade", success=False)
        self.assertEqual(outside.read_bytes(), OLD_DOCUMENTS["AMP_VALIDATION.md"])
        self.assertTrue((self.docroot / "AMP_VALIDATION.md").exists())

    def test_symlink_docroot_refuses_prepare(self) -> None:
        self.install_old()
        alias = self.work / "linked-document-root"
        alias.symlink_to(self.docroot, target_is_directory=True)
        self.helper_run("prepare-upgrade", root=alias, success=False)
        for relative, content in OLD_DOCUMENTS.items():
            self.assertEqual((self.docroot / relative).read_bytes(), content)

    def test_linked_parent_refuses_prepare(self) -> None:
        self.install_old()
        alias = self.work / "linked-parent"
        alias.symlink_to(self.work, target_is_directory=True)
        self.helper_run("prepare-upgrade", root=alias / self.docroot.name, success=False)
        self.assertEqual((self.docroot / "AMP_VALIDATION.md").read_bytes(), OLD_DOCUMENTS["AMP_VALIDATION.md"])

    def test_nonregular_candidate_refuses_prepare(self) -> None:
        self.install_old()
        candidate = self.docroot / "AMP_VALIDATION.md"
        candidate.unlink()
        candidate.mkdir()
        (candidate / "personal.chimera").write_bytes(USER_EDIT)
        self.helper_run("prepare-upgrade", success=False)
        self.assertEqual((candidate / "personal.chimera").read_bytes(), USER_EDIT)

    def test_blocked_backup_refuses_prepare_and_preserves_source(self) -> None:
        self.install_old()
        (self.docroot / "MANUAL.html").write_bytes(USER_EDIT)
        blocked = Path(str(self.docroot) + ".preserved")
        blocked.write_bytes(b"User file blocks the protected-copy directory.")
        self.helper_run("prepare-upgrade", success=False)
        self.assertEqual((self.docroot / "MANUAL.html").read_bytes(), USER_EDIT)
        self.assertEqual(blocked.read_bytes(), b"User file blocks the protected-copy directory.")

    def test_backup_path_link_refuses_prepare(self) -> None:
        self.install_old()
        (self.docroot / "MANUAL.html").write_bytes(USER_EDIT)
        outside = self.work / "unrelated-folder"
        outside.mkdir()
        Path(str(self.docroot) + ".preserved").symlink_to(outside, target_is_directory=True)
        self.helper_run("prepare-upgrade", success=False)
        self.assertEqual(list(outside.iterdir()), [])
        self.assertEqual((self.docroot / "MANUAL.html").read_bytes(), USER_EDIT)

    def test_tampered_backup_refuses_restore(self) -> None:
        self.install_old()
        (self.docroot / "AMP_VALIDATION.md").write_bytes(USER_EDIT)
        self.helper_run("prepare-upgrade")
        saved = self.saved_copy("AMP_VALIDATION.md", USER_EDIT)
        saved.write_bytes(b"Corrupt protected bytes.")
        self.helper_run("commit-upgrade", success=False)
        self.assertEqual((self.docroot / "AMP_VALIDATION.md").read_bytes(), USER_EDIT)
        self.assertTrue((self.docroot / "MODELS_AND_REFERENCE.md").exists())

    def test_tampered_journal_refuses_commit_without_deleting_original(self) -> None:
        self.install_old()
        (self.docroot / "AMP_VALIDATION.md").write_bytes(USER_EDIT)
        self.helper_run("prepare-upgrade")
        pending = Path(str(self.docroot) + ".pending")
        records = sorted(pending.glob("*.record"))
        self.assertTrue(records, "Preparation did not persist transaction records")
        records[0].write_bytes(b"invalid transaction metadata\n")
        self.helper_run("commit-upgrade", success=False)
        self.assertEqual((self.docroot / "AMP_VALIDATION.md").read_bytes(), USER_EDIT)
        self.assertEqual((self.docroot / "MODELS_AND_REFERENCE.md").read_bytes(), OLD_DOCUMENTS["MODELS_AND_REFERENCE.md"])
        self.require_saved("AMP_VALIDATION.md", USER_EDIT)


class DpkgDocumentLifecycleTests(NativeFixtureCase):
    """Actual dpkg engine, synthetic payload, isolated installation root."""

    @classmethod
    def setUpClass(cls) -> None:
        if os.name != "posix" or not hasattr(os, "geteuid") or os.geteuid() != 0:
            raise unittest.SkipTest("Isolated real-dpkg lifecycle requires root in a disposable environment")
        if any(shutil.which(tool) is None for tool in ("dpkg", "dpkg-deb")):
            raise unittest.SkipTest("Real dpkg tools unavailable; no installation PASS is claimed")
        super().setUpClass()
        cls.old_package = cls.build_package(OLD_VERSION, OLD_DOCUMENTS, helper=False)
        cls.new_package = cls.build_package(NEW_VERSION, CURRENT_DOCUMENTS, helper=True)

    @classmethod
    def build_package(cls, version: str, documents: dict[str, bytes], *, helper: bool) -> Path:
        stage = cls.build_root / version
        control = stage / "DEBIAN"
        control.mkdir(parents=True)
        (control / "control").write_text(
            f"Package: {PACKAGE}\nVersion: {version}\nArchitecture: all\n"
            "Maintainer: Fixture test <test@example.invalid>\n"
            "Description: isolated document lifecycle fixture, no product binaries\n",
            encoding="utf-8",
        )
        write_documents(stage / DOC_RELATIVE, documents)
        marker = stage / "usr/lib/chimera-fixture/package-marker"
        marker.parent.mkdir(parents=True)
        marker.write_text(version, encoding="utf-8")
        if helper:
            for hook in ("preinst", "postinst", "prerm", "postrm"):
                shutil.copy2(cls.helper, control / hook)
                (control / hook).chmod(0o755)
        package = cls.build_root / (version + ".deb")
        result = subprocess.run(
            ["dpkg-deb", "--build", "--root-owner-group", str(stage), str(package)],
            text=True, capture_output=True, timeout=30,
        )
        if result.returncode:
            raise RuntimeError(f"Fixture package build failed: {result.stdout}\n{result.stderr}")
        return package

    def setUp(self) -> None:
        super().setUp()
        self.install_root = self.work / "isolated-system-root"
        database = self.install_root / "var/lib/dpkg"
        database.mkdir(parents=True)
        (database / "status").write_text("", encoding="utf-8")
        self.docroot = self.install_root / DOC_RELATIVE

    def dpkg(self, *arguments: str, success: bool = True) -> subprocess.CompletedProcess:
        result = subprocess.run(
            ["dpkg", "--root=" + str(self.install_root), "--force-script-chrootless",
             "--path-include=/usr/share/doc/*", *arguments],
            text=True, capture_output=True, timeout=45,
        )
        detail = f"dpkg {arguments}: exit={result.returncode}\n{result.stdout}\n{result.stderr}"
        if success:
            self.assertEqual(result.returncode, 0, detail)
        else:
            self.assertNotEqual(result.returncode, 0, detail)
        return result

    def test_real_dpkg_fresh_install_and_remove_preserve_licenses_and_personal_files(self) -> None:
        self.dpkg("--install", str(self.new_package))
        for relative, content in CURRENT_DOCUMENTS.items():
            self.assertEqual((self.docroot / relative).read_bytes(), content)
        write_documents(self.docroot, {"personal.chimera": USER_EDIT})
        user_ir = self.install_root / "home/test-user/.local/share/SpectralForge/Chimera/IRs/personal.wav"
        user_ir.parent.mkdir(parents=True)
        user_ir.write_bytes(USER_EDIT)
        self.dpkg("--remove", PACKAGE)
        self.assertFalse((self.install_root / "usr/lib/chimera-fixture/package-marker").exists())
        self.assertEqual((self.docroot / "personal.chimera").read_bytes(), USER_EDIT)
        self.assertEqual(user_ir.read_bytes(), USER_EDIT)
        for relative in {"THIRD_PARTY_NOTICES.md", "COPYRIGHT.txt"}:
            self.assertEqual((self.docroot / relative).read_bytes(), CURRENT_DOCUMENTS[relative])

    def test_real_dpkg_upgrade_repair_remove_preserve_user_changes(self) -> None:
        self.dpkg("--install", str(self.old_package))
        # Assert installation despite possible host path-exclude configuration.
        for relative, content in OLD_DOCUMENTS.items():
            self.assertEqual((self.docroot / relative).read_bytes(), content)
        (self.docroot / "AMP_VALIDATION.md").write_bytes(USER_EDIT)
        (self.docroot / "MANUAL.html").write_bytes(USER_EDIT)
        write_documents(self.docroot, {"personal.chimera": USER_EDIT, "personal.wav": USER_EDIT})
        self.dpkg("--install", str(self.new_package))
        self.assertEqual((self.docroot / "AMP_VALIDATION.md").read_bytes(), USER_EDIT)
        self.assertFalse((self.docroot / "MODELS_AND_REFERENCE.md").exists())
        self.assertEqual((self.docroot / "MANUAL.html").read_bytes(), CURRENT_DOCUMENTS["MANUAL.html"])
        self.require_saved("MANUAL.html", USER_EDIT)
        repaired_edit = b"A second manual edit before same-version repair.\n"
        (self.docroot / "MANUAL.html").write_bytes(repaired_edit)
        marker = self.install_root / "usr/lib/chimera-fixture/package-marker"
        marker.unlink()
        self.dpkg("--install", str(self.new_package))
        self.assertEqual(marker.read_text(), NEW_VERSION)
        self.require_saved("MANUAL.html", repaired_edit)
        self.assertEqual((self.docroot / "AMP_VALIDATION.md").read_bytes(), USER_EDIT)
        self.assertEqual((self.docroot / "MANUAL.html").read_bytes(), CURRENT_DOCUMENTS["MANUAL.html"])
        (self.docroot / "MANUAL.html").write_bytes(repaired_edit)
        self.dpkg("--remove", PACKAGE)
        self.assertEqual((self.docroot / "MANUAL.html").read_bytes(), repaired_edit)
        self.assertEqual((self.docroot / "AMP_VALIDATION.md").read_bytes(), USER_EDIT)
        for relative in {"personal.chimera", "personal.wav"}:
            self.assertEqual((self.docroot / relative).read_bytes(), USER_EDIT)
        for relative in {"THIRD_PARTY_NOTICES.md", "COPYRIGHT.txt"}:
            self.assertEqual((self.docroot / relative).read_bytes(), CURRENT_DOCUMENTS[relative])

    def test_real_dpkg_failed_prepare_does_not_unpack_or_lose_user_document(self) -> None:
        self.dpkg("--install", str(self.old_package))
        (self.docroot / "AMP_VALIDATION.md").write_bytes(USER_EDIT)
        blocked = Path(str(self.docroot) + ".preserved")
        blocked.write_bytes(b"Cannot replace this user file with a directory.")
        self.dpkg("--install", str(self.new_package), success=False)
        self.assertEqual((self.docroot / "AMP_VALIDATION.md").read_bytes(), USER_EDIT)
        self.assertEqual((self.docroot / "MODELS_AND_REFERENCE.md").read_bytes(), OLD_DOCUMENTS["MODELS_AND_REFERENCE.md"])
        marker = self.install_root / "usr/lib/chimera-fixture/package-marker"
        self.assertEqual(marker.read_text(), OLD_VERSION)


if __name__ == "__main__":
    unittest.main(verbosity=2)
