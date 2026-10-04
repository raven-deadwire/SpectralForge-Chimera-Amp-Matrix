#!/usr/bin/env python3
"""Exercise source -> CMake/resources -> payload -> Inno version contracts."""
from __future__ import annotations

import contextlib
import io
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "Tools"))
import chimera_version as version
import package_installer_candidate as candidate
import package_release as release

SHA = "0123456789abcdef0123456789abcdef01234567"


class PackagingVersionTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="chimera-version-")
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.write("VERSION", "2.17.4\n")

    def write(self, path, text="fixture"):
        path = self.root / path
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text, encoding="utf-8")

    def test_identity_uses_source_version_and_exact_sha(self):
        self.assertEqual(version.identity(self.root, SHA), {
            "version": "2.17.4-preview.0123456789", "product_version": "2.17.4",
            "installer_version": "2.17.4", "source_sha": SHA, "build_id": SHA[:10]})
        for bad in (SHA[:10], SHA.upper(), "", "g" * 40):
            with self.subTest(sha=bad), self.assertRaises(ValueError):
                version.identity(self.root, bad)

    def test_rejects_malformed_and_windows_overflow_versions(self):
        for value in ("", "1.1", "1.1.1.0", "01.1.1", "1.1.1-preview.foo", "1.1.1\n2.0.0", "1.65536.1"):
            self.write("VERSION", value)
            with self.subTest(version=value), self.assertRaises(ValueError):
                version.product_version(self.root)

    def test_rejects_old_missing_mixed_or_wrong_head_payload(self):
        expected = version.identity(self.root, SHA)
        version.validate_manifest(expected, expected, SHA[:10])
        for key in expected:
            for value in (None, "1.0.1", "1.1.0", "f" * 40):
                changed = {**expected, key: value}
                with self.subTest(key=key, value=value), self.assertRaises(ValueError):
                    version.validate_manifest(changed, expected)
            missing = expected.copy()
            missing.pop(key)
            with self.assertRaises(ValueError):
                version.validate_manifest(missing, expected)
        with self.assertRaises(ValueError):
            version.validate_manifest(expected, expected, "f" * 10)

    def test_rejects_stale_configured_build_before_staging(self):
        expected = version.identity(self.root, SHA)
        for key in ("product_version", "source_sha"):
            self.write("build/chimera-build-version.json", json.dumps({**expected, key: "stale"}))
            with self.assertRaisesRegex(ValueError, "reconfigure and rebuild"):
                version.validate_build(self.root / "build", expected)
        self.write("build/chimera-build-version.json", json.dumps(expected))
        version.validate_build(self.root / "build", expected)

    def test_candidate_stage_inventory_and_versions_without_global_mutation(self):
        expected = version.identity(self.root, SHA)
        self.write("build/chimera-build-version.json", json.dumps(expected))
        for path in (
            "build/ChimeraAmpMatrix_artefacts/Release/Standalone/SpectralForge Chimera.exe",
            "build/ChimeraAmpMatrix_artefacts/Release/VST3/SpectralForge Chimera.vst3/Contents/x86_64-win/SpectralForge Chimera.vst3",
            "build/ChimeraRender_artefacts/Release/ChimeraRender.exe", "build/reference-audio/example.wav",
            "Tools/validate_nam.py", "Tools/Trace-Chimera-Session.ps1"):
            self.write(path)
        # Real documents/inventory paths; fixture product bytes are not native-build evidence.
        shutil.copytree(ROOT / "docs", self.root / "docs")
        self.write("COPYRIGHT.txt")
        self.write("build/Testing/Temporary/LastTest.log", "PASS fixture\n")
        original = release.VERSION
        with patch.object(candidate, "ROOT", self.root), patch.object(release, "ROOT", self.root), \
             patch.object(candidate, "revision", return_value=SHA), contextlib.redirect_stdout(io.StringIO()):
            candidate.stage()
        manifest = json.loads((self.root / "installer-candidate/payload/payload-manifest.json").read_text())
        version.validate_manifest(manifest, expected)
        self.assertEqual(release.VERSION, original)
        for entry in manifest["files"]:
            file = self.root / "installer-candidate/payload" / entry["path"]
            self.assertEqual(entry["sha256"], release.sha256(file))
        self.assertFalse(manifest["published_release"])

    def test_cmake_project_and_generated_product_identity(self):
        # Run the real CMake version module and templates, without fetching JUCE.
        shutil.copytree(ROOT / "cmake", self.root / "cmake")
        self.write("CMakeLists.txt", f'''cmake_minimum_required(VERSION 3.22)
include(cmake/ChimeraVersion.cmake)
project(VersionContract VERSION ${{CHIMERA_PRODUCT_VERSION}} LANGUAGES NONE)
set(CHIMERA_SOURCE_SHA "{SHA}")
string(SUBSTRING "${{CHIMERA_SOURCE_SHA}}" 0 10 CHIMERA_BUILD_REVISION)
configure_file(cmake/ChimeraBuildVersion.h.in ChimeraBuildVersion.h @ONLY)
configure_file(cmake/chimera-build-version.json.in chimera-build-version.json @ONLY)
''')
        subprocess.run(["cmake", "-S", str(self.root), "-B", str(self.root / "build")],
                       check=True, capture_output=True, text=True)
        expected = version.identity(self.root, SHA)
        version.validate_build(self.root / "build", expected)
        header = (self.root / "build/ChimeraBuildVersion.h").read_text()
        self.assertIn('"2.17.4"', header)
        self.assertIn('"2.17.4-preview.0123456789"', header)
        self.assertIn("CMAKE_PROJECT_VERSION:STATIC=2.17.4", (self.root / "build/CMakeCache.txt").read_text())
        self.write("VERSION", "1.65536.0")
        result = subprocess.run(["cmake", "-S", str(self.root), "-B", str(self.root / "build")],
                                capture_output=True, text=True)
        self.assertNotEqual(result.returncode, 0)

    def test_repository_consumers_require_version_contract(self):
        # Protect the boundary which failed in both PRs, including removal of bypasses.
        inno = (ROOT / "Installer/Chimera.iss").read_text(encoding="utf-8-sig")
        self.assertNotIn('#define ProductVersion "', inno)
        self.assertIn("#ifndef ProductVersion", inno)
        self.assertIn("AppVersion={#ProductVersion}", inno)
        builder = (ROOT / "Tools/Build-WindowsInstaller.ps1").read_text()
        self.assertIn('"--manifest"', builder)
        self.assertIn('"/DProductVersion=$($identity.product_version)"', builder)
        self.assertIn("Assert-ChimeraBinaryVersion $installer", builder)
        verifier = (ROOT / "Tools/Verify-WindowsInstaller.ps1").read_text(encoding="utf-8-sig")
        self.assertIn("$entry.DisplayVersion -ceq $payload.installer_version", verifier)
        self.assertNotIn('Properties.Name -contains "installer_version"', verifier)
        cmake = (ROOT / "CMakeLists.txt").read_text()
        self.assertIn("project(ChimeraAmpMatrix VERSION ${CHIMERA_PRODUCT_VERSION}", cmake)
        info = (ROOT / "Source/ReleaseInfo.h").read_text()
        self.assertIn("version = CHIMERA_PREVIEW_VERSION", info)

    def windows_stage(self, manifest):
        for file in ("Standalone/SpectralForge Chimera.exe",
                     "VST3/SpectralForge Chimera.vst3/Contents/x86_64-win/SpectralForge Chimera.vst3",
                     "ReferenceTools/ChimeraRender.exe", "WINDOWS_INSTALL.txt", "Verification.txt", "MANUAL.html"):
            self.write("payload/" + file)
        self.write("payload/payload-manifest.json", json.dumps(manifest))

    @unittest.skipUnless(sys.platform == "win32", "PowerShell resource fixtures run in Windows CI")
    def test_windows_juce_and_inno_resource_layouts(self):
        result = subprocess.run(["pwsh", "-NoProfile", "-File", str(ROOT / "Tests/Test-WindowsVersionContract.ps1")],
                                capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("PASS: JUCE and Inno resources accepted", result.stdout)
        self.assertIn("PASS: actual Inno Setup loader version resources match", result.stdout)

    def run_windows_builder(self, *args):
        return subprocess.run(["pwsh", "-NoProfile", "-File", str(ROOT / "Tools/Build-WindowsInstaller.ps1"),
                               "-Stage", str(self.root / "payload"),
                               "-OutputDirectory", str(self.root / "setup"), *args],
                              capture_output=True, text=True)

    @unittest.skipUnless(sys.platform == "win32", "actual PowerShell builder regression runs in Windows CI")
    def test_windows_builder_rejects_mixed_or_missing_payload_version(self):
        expected = version.identity()
        for key, value in (("installer_version", "1.0.1"), ("product_version", "1.1.0"),
                           ("version", "1.0.1-preview." + expected["build_id"]), ("source_sha", "f" * 40),
                           ("installer_version", None)):
            manifest = {**expected, key: value}
            if value is None:
                manifest.pop(key)
            self.windows_stage(manifest)
            result = self.run_windows_builder("-BuildId", expected["build_id"])
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("Installer payload version/source contract failed", result.stderr)
            self.assertFalse((self.root / "setup").exists())

    @unittest.skipUnless(sys.platform == "win32", "actual Windows PE resource check runs in Windows CI")
    def test_windows_builder_rejects_relabelled_binary(self):
        self.windows_stage(version.identity())
        # A real PE executable with Python's version, behind a correct Chimera manifest.
        shutil.copy2(sys.executable, self.root / "payload/Standalone/SpectralForge Chimera.exe")
        result = self.run_windows_builder()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("Binary version differs from payload", result.stderr)
        self.assertFalse((self.root / "setup").exists())


if __name__ == "__main__":
    unittest.main(verbosity=2)
