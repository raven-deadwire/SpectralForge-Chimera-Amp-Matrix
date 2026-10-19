#!/usr/bin/env python3
"""Ownership/architecture negative tests and real-bundle install lifecycle."""
from pathlib import Path
import json
import os
import plistlib
import shutil
import struct
import subprocess
import sys
import tempfile
import unittest
import zipfile
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "Tools"))
import aax_native as aax


def fixture(directory: Path, target="windows", revision=b"first") -> Path:
    directory.mkdir(parents=True)
    bundle = directory / aax.BUNDLE
    if target == "windows":
        binary = bundle / "Contents/x64" / aax.BUNDLE
        data = bytearray(256)
        data[:2] = b"MZ"
        struct.pack_into("<I", data, 60, 128)
        data[128:132] = b"PE\0\0"
        struct.pack_into("<H", data, 132, 0x8664)
        struct.pack_into("<H", data, 150, 0x2000)
        struct.pack_into("<H", data, 152, 0x20b)
    else:
        binary = bundle / "Contents/MacOS" / aax.PRODUCT
        data = bytearray(256)
        struct.pack_into(">II", data, 0, 0xcafebabe, 2)
        struct.pack_into(">IIIII", data, 8, 0x1000007, 3, 128, 32, 0)
        struct.pack_into(">IIIII", data, 28, 0x100000c, 0, 192, 32, 0)
        data[128:132] = data[192:196] = b"\xcf\xfa\xed\xfe"
    binary.parent.mkdir(parents=True)
    binary.write_bytes(data + revision)
    if target == "macos":
        (bundle / "Contents/Info.plist").write_bytes(plistlib.dumps({
            "CFBundleIdentifier": "audio.spectralforge.chimera", "CFBundleExecutable": aax.PRODUCT}))
    aax.write_json(directory / aax.RECEIPT, {"schema": aax.SCHEMA,
        "platform": target, "source_sha": "a" * 40, "pace_signed": False,
        "normal_host_accepted": False, "release_approved": False,
        **aax.verify_bundle(bundle, target)})
    return directory


class AAXPackagingTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name).resolve()
        self.package = fixture(self.root / "package")
        self.installed = self.root / "system/Plug-Ins"

    def tearDown(self):
        self.temp.cleanup()

    def test_install_update_remove_preserves_user_data_and_other_plugins(self):
        sibling = self.root / "system/Presets/personal.chimera"
        sibling.parent.mkdir(parents=True); sibling.write_bytes(b"personal")
        self.installed.mkdir()
        other = self.installed / "Other.aaxplugin"; other.mkdir()
        (other / "user.wav").write_bytes(b"IR")
        aax.install(self.package, self.installed)
        updated = fixture(self.root / "updated", revision=b"second")
        aax.install(updated, self.installed)
        self.assertEqual(aax.verify_package(self.installed), aax.verify_package(updated))
        aax.uninstall(self.installed); aax.uninstall(self.installed)
        self.assertFalse((self.installed / aax.BUNDLE).exists())
        self.assertEqual(sibling.read_bytes(), b"personal")
        self.assertEqual((other / "user.wav").read_bytes(), b"IR")

    def test_refuse_unowned_or_modified_installation(self):
        aax.install(self.package, self.installed)
        marker = self.installed / aax.BUNDLE / "unexpected.wav"
        marker.write_bytes(b"user")
        for operation in (lambda: aax.install(self.package, self.installed), lambda: aax.uninstall(self.installed)):
            with self.assertRaises(ValueError): operation()
        marker.unlink()
        (self.installed / aax.RECEIPT).unlink()
        with self.assertRaises(ValueError): aax.install(self.package, self.installed)

    def test_reject_payload_tampering_and_signed_claim(self):
        path = self.package / aax.BUNDLE / "Contents/x64" / aax.BUNDLE
        path.write_bytes(path.read_bytes() + b"changed")
        with self.assertRaises(ValueError): aax.install(self.package, self.installed)
        receipt = json.loads((self.package / aax.RECEIPT).read_text())
        receipt["pace_signed"] = True
        aax.write_json(self.package / aax.RECEIPT, receipt)
        with self.assertRaises(ValueError): aax.verify_package(self.package)

    def test_reject_wrong_pe_architecture(self):
        binary = self.package / aax.BUNDLE / "Contents/x64" / aax.BUNDLE
        data = bytearray(binary.read_bytes()); struct.pack_into("<H", data, 132, 0xaa64)
        binary.write_bytes(data)
        with self.assertRaises(ValueError): aax.verify_bundle(self.package / aax.BUNDLE, "windows")

    def test_universal_and_missing_slice(self):
        package = fixture(self.root / "mac", "macos")
        self.assertEqual(aax.verify_package(package)["architectures"], ["arm64", "x86_64"])
        binary = package / aax.BUNDLE / "Contents/MacOS" / aax.PRODUCT
        binary.write_bytes(binary.read_bytes()[:200])
        with self.assertRaises(ValueError): aax.verify_bundle(package / aax.BUNDLE, "macos")

    def test_symlink_parent_refused(self):
        link = self.root / "link"
        try: link.symlink_to(self.package, target_is_directory=True)
        except OSError: self.skipTest("Symlink creation unavailable")
        with self.assertRaises(ValueError): aax.verify_package(link)

    def test_replacement_failure_restores_previous_bundle(self):
        aax.install(self.package, self.installed)
        updated = fixture(self.root / "updated", revision=b"second")
        with mock.patch.object(aax.os, "replace", side_effect=OSError("locked receipt")):
            with self.assertRaises(OSError): aax.install(updated, self.installed)
        self.assertEqual(aax.verify_package(self.installed), aax.verify_package(self.package))


    def test_failed_rollback_rename_retains_previous_bytes(self):
        aax.install(self.package, self.installed)
        old_files = aax.verify_package(self.installed)["files"]
        old_receipt = (self.installed / aax.RECEIPT).read_bytes()
        updated = fixture(self.root / "updated", revision=b"second")
        rename = Path.rename

        def fail_restore(path, destination):
            if path.name == "previous.aaxplugin":
                raise PermissionError("injected rollback rename failure")
            return rename(path, destination)

        with mock.patch.object(aax.os, "replace", side_effect=OSError("injected receipt failure")), \
                mock.patch.object(Path, "rename", fail_restore):
            with self.assertRaises((OSError, RuntimeError)):
                aax.install(updated, self.installed)
        backups = list(self.installed.glob(".chimera-aax-*/previous.aaxplugin"))
        self.assertEqual(len(backups), 1, "Rollback failure discarded the only old bundle")
        actual = {p.relative_to(backups[0]).as_posix(): aax.digest(p)
                  for p in backups[0].rglob("*") if p.is_file()}
        self.assertEqual(actual, old_files)
        self.assertEqual((backups[0].parent / "previous-receipt.json").read_bytes(), old_receipt)

    def test_failed_rollback_cleanup_retains_previous_bytes(self):
        aax.install(self.package, self.installed)
        expected = aax.verify_package(self.installed)["binary_sha256"]
        updated = fixture(self.root / "updated", revision=b"second")
        remove = shutil.rmtree

        def fail_candidate_removal(path, *args, **kwargs):
            if Path(path) == self.installed / aax.BUNDLE:
                raise PermissionError("injected candidate cleanup failure")
            return remove(path, *args, **kwargs)

        with mock.patch.object(aax.os, "replace", side_effect=OSError("injected receipt failure")), \
                mock.patch.object(aax.shutil, "rmtree", fail_candidate_removal):
            with self.assertRaises((OSError, RuntimeError)) as failure:
                aax.install(updated, self.installed)
        backups = list(self.installed.glob(".chimera-aax-*/previous.aaxplugin"))
        self.assertEqual(len(backups), 1, "Failed cleanup discarded the only old bundle")
        self.assertEqual(aax.digest(backups[0] / "Contents/x64" / aax.BUNDLE), expected)
        self.assertIn(str(backups[0].parent), str(failure.exception))

    def test_backup_move_failure_preserves_old_installation(self):
        aax.install(self.package, self.installed)
        expected = aax.verify_package(self.installed)
        updated = fixture(self.root / "updated", revision=b"second")
        rename = Path.rename

        def fail_backup(path, destination):
            if path == self.installed / aax.BUNDLE:
                raise PermissionError("injected backup move failure")
            return rename(path, destination)

        with mock.patch.object(Path, "rename", fail_backup):
            with self.assertRaises(OSError): aax.install(updated, self.installed)
        self.assertEqual(aax.verify_package(self.installed), expected)
        self.assertEqual(list(self.installed.glob(".chimera-aax-*")), [])

    def test_candidate_move_failure_restores_old_installation(self):
        aax.install(self.package, self.installed)
        expected = aax.verify_package(self.installed)
        updated = fixture(self.root / "updated", revision=b"second")
        rename = Path.rename

        def fail_candidate(path, destination):
            if path.name == aax.BUNDLE and path.parent.name.startswith(".chimera-aax-"):
                raise PermissionError("injected candidate move failure")
            return rename(path, destination)

        with mock.patch.object(Path, "rename", fail_candidate):
            with self.assertRaises(OSError): aax.install(updated, self.installed)
        self.assertEqual(aax.verify_package(self.installed), expected)
        self.assertEqual(list(self.installed.glob(".chimera-aax-*")), [])

    def test_interrupted_update_restores_old_installation(self):
        aax.install(self.package, self.installed)
        expected = aax.verify_package(self.installed)
        updated = fixture(self.root / "updated", revision=b"second")
        with mock.patch.object(aax.os, "replace", side_effect=KeyboardInterrupt("injected interrupt")):
            with self.assertRaises(KeyboardInterrupt): aax.install(updated, self.installed)
        self.assertEqual(aax.verify_package(self.installed), expected)
        self.assertEqual(list(self.installed.glob(".chimera-aax-*")), [])

    def test_failed_fresh_install_removes_only_candidate(self):
        self.installed.mkdir(parents=True)
        sentinel = self.installed / "other.txt"
        sentinel.write_bytes(b"not ours")
        with mock.patch.object(aax.os, "replace", side_effect=OSError("injected receipt failure")):
            with self.assertRaises(OSError): aax.install(self.package, self.installed)
        self.assertIsNone(aax.installed_receipt(self.installed))
        self.assertEqual(sentinel.read_bytes(), b"not ours")
        self.assertEqual(list(self.installed.glob(".chimera-aax-*")), [])

    def test_post_install_verification_failure_restores_previous(self):
        aax.install(self.package, self.installed)
        expected = aax.verify_package(self.installed)
        updated = fixture(self.root / "updated", revision=b"second")
        changed = aax.verify_package(updated)["binary_sha256"]
        verify = aax.verify_package

        def reject_installed_candidate(path):
            result = verify(path)
            if path == self.installed and result["binary_sha256"] == changed:
                raise ValueError("injected post-install verification failure")
            return result

        with mock.patch.object(aax, "verify_package", reject_installed_candidate):
            with self.assertRaises(ValueError): aax.install(updated, self.installed)
        self.assertEqual(aax.verify_package(self.installed), expected)
        self.assertEqual(list(self.installed.glob(".chimera-aax-*")), [])

    def test_failed_rollback_retains_receipt_if_bundle_already_restored(self):
        aax.install(self.package, self.installed)
        expected = aax.verify_package(self.installed)
        prior = (self.installed / aax.RECEIPT).read_bytes()
        updated = fixture(self.root / "updated", revision=b"second")
        write = Path.write_bytes

        def fail_receipt_restore(path, data):
            if path == self.installed / aax.RECEIPT:
                raise PermissionError("injected receipt restore failure")
            return write(path, data)

        with mock.patch.object(aax.os, "replace", side_effect=OSError("injected receipt failure")), \
                mock.patch.object(Path, "write_bytes", fail_receipt_restore):
            with self.assertRaises((OSError, RuntimeError)) as failure:
                aax.install(updated, self.installed)
        receipts = list(self.installed.glob(".chimera-aax-*/previous-receipt.json"))
        self.assertEqual(len(receipts), 1)
        self.assertEqual(receipts[0].read_bytes(), prior)
        self.assertIn(str(receipts[0].parent), str(failure.exception))
        self.assertEqual(aax.verify_package(self.installed), expected)



def native_packages(directory: Path, system=False):
    packages = [p for p in directory.iterdir() if p.is_dir() and (p / aax.RECEIPT).exists()]
    if len(packages) != 1: raise ValueError("Expected one actual native package")
    original = packages[0]
    receipt = aax.verify_package(original)
    with tempfile.TemporaryDirectory() as temp:
        root = Path(temp).resolve()
        package = root / "extracted"
        # Validate the delivered ZIP, including Unix file modes, not just staging.
        with zipfile.ZipFile(original.with_name(original.name + ".zip")) as archive:
            archive.extractall(package)
            for entry in archive.infolist():
                mode = entry.external_attr >> 16
                if mode: (package / entry.filename).chmod(mode & 0o777)
        assert aax.verify_package(package) == receipt
        destination = aax.system_root(receipt["platform"]) if system else root / "Avid/Audio/Plug-Ins"
        if system and ((destination / aax.BUNDLE).exists() or (destination / aax.RECEIPT).exists()):
            raise ValueError("System test requires a clean disposable runner; existing installation preserved")
        sentinel = root / "SpectralForge/Chimera/IRs/personal.wav"
        sentinel.parent.mkdir(parents=True); sentinel.write_bytes(b"personal-preservation-sentinel")
        command = [sys.executable, str(package / "aax_native.py")]
        install = command + ["install", "--package", str(package), "--root", str(destination), "--acknowledge-developer-only"]
        remove = command + ["uninstall", "--platform", receipt["platform"], "--root", str(destination), "--acknowledge-developer-only"]
        subprocess.run(install, check=True)
        assert aax.verify_package(destination) == receipt
        subprocess.run(install, check=True)
        assert aax.verify_package(destination) == receipt
        subprocess.run(remove, check=True)
        assert not (destination / aax.BUNDLE).exists()
        assert sentinel.read_bytes() == b"personal-preservation-sentinel"
    aax.write_json(directory / ("system-install-results.json" if system else "native-install-results.json"), {
        "source_sha": receipt["source_sha"], "platform": receipt["platform"],
        "binary_sha256": receipt["binary_sha256"],
        "install": "PASS", "same_build_update": "PASS", "uninstall": "PASS",
        "personal_data_preserved": True, "installation_scope": str(destination) if system else "isolated temporary root",
        "system_installation": "PASS" if system else "NOT_TESTED", "pro_tools_host": "NOT_TESTED", "pace_signed": False})
    print("PASS actual native ZIP install/update/remove; " + ("system root" if system else "isolated root") + ", no host acceptance")


if __name__ == "__main__":
    if len(sys.argv) == 3 and sys.argv[1] == "--native-packages":
        native_packages(Path(sys.argv[2]))
    elif len(sys.argv) == 3 and sys.argv[1] == "--system-packages":
        native_packages(Path(sys.argv[2]), system=True)
    else:
        unittest.main()
