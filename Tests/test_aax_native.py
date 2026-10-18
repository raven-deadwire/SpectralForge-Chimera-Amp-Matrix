#!/usr/bin/env python3
"""Ownership/architecture negative tests and real-bundle install lifecycle."""
from pathlib import Path
import json
import os
import plistlib
import shutil
import struct
import sys
import tempfile
import unittest
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


def native_packages(directory: Path):
    packages = [p for p in directory.iterdir() if p.is_dir() and (p / aax.RECEIPT).exists()]
    if len(packages) != 1: raise ValueError("Expected one actual native package")
    package = packages[0]
    receipt = aax.verify_package(package)
    with tempfile.TemporaryDirectory() as temp:
        root = Path(temp).resolve()
        destination = root / "Avid/Audio/Plug-Ins"
        sentinel = root / "SpectralForge/Chimera/IRs/personal.wav"
        sentinel.parent.mkdir(parents=True); sentinel.write_bytes(b"personal-preservation-sentinel")
        aax.install(package, destination)
        assert aax.verify_package(destination) == receipt
        aax.install(package, destination)
        assert aax.verify_package(destination) == receipt
        aax.uninstall(destination)
        assert not (destination / aax.BUNDLE).exists()
        assert sentinel.read_bytes() == b"personal-preservation-sentinel"
    aax.write_json(directory / "native-install-results.json", {
        "source_sha": receipt["source_sha"], "platform": receipt["platform"],
        "binary_sha256": receipt["binary_sha256"],
        "install": "PASS", "same_build_update": "PASS", "uninstall": "PASS",
        "personal_data_preserved": True, "installation_scope": "isolated temporary root",
        "system_installation": "NOT_TESTED", "pro_tools_host": "NOT_TESTED", "pace_signed": False})
    print("PASS actual native bundle install/update/remove; isolated root, no host acceptance")


if __name__ == "__main__":
    if len(sys.argv) == 3 and sys.argv[1] == "--native-packages":
        native_packages(Path(sys.argv[2]))
    else:
        unittest.main()
