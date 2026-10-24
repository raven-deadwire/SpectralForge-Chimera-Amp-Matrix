#!/usr/bin/env python3
"""Exercise source -> CMake/resources -> payload -> Inno version contracts."""
from __future__ import annotations

import contextlib
import hashlib
import io
import json
import re
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest
import zipfile
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "Tools"))
import chimera_version as version
import package_installer_candidate as candidate
import package_release as release
import package_update_candidate as portable
import check_public_names as public_names

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

    def test_public_document_inventory_and_injected_name_regression(self):
        public_names.validate_document_names(release.PUBLIC_DOCUMENTS)
        policy = public_names.load_policy()
        self.assertEqual(set(release.PUBLIC_DOCUMENTS), set(policy["public_documents"]))
        public_names.validate_source()
        with self.assertRaisesRegex(RuntimeError, "Original amp/effect name"):
            public_names.validate_text('<label title="BOSS SD-1">Yellow Asym</label>',
                                       "injected public tooltip")
        public_names.validate_text("A DRIVE / B DRIVE / Mid-forward soft clipping", "functional controls")

    def test_beta_identity_and_malformed_channels(self):
        self.write("RELEASE_CHANNEL", "beta.1\n")
        expected = version.identity(self.root, SHA)
        self.assertEqual(expected["version"], "2.17.4-beta.1")
        self.assertEqual(expected["installer_version"], "2.17.4")
        self.assertEqual(version.display_version(self.root, SHA), "Open Beta 2.17.4 (beta.1)")
        for bad in ("release", "beta.0", "beta.01", "rc.0", "rc.01", "stable.1", "beta.1\npreview", "../beta.1"):
            self.write("RELEASE_CHANNEL", bad)
            with self.subTest(channel=bad), self.assertRaises(ValueError):
                version.identity(self.root, SHA)

    def test_publishable_versions_do_not_accept_tags_previews_or_metadata(self):
        for value, expected in (("1.3.1-beta.2", ("1.3.1", "beta.2")),
                                ("1.3.1-rc.10", ("1.3.1", "rc.10")),
                                ("1.3.1", ("1.3.1", "stable"))):
            self.assertEqual(version.parse_release_version(value), expected)
        for bad in ("v1.3.1-beta.2", "1.3.1-beta", "1.3.1-beta.0", "1.3.1-beta.01", "1.3.1-beta.2.extra",
                    "1.3.1-rc", "1.3.1-rc.0", "1.3.1-rc.01", "1.3.1-stable", "1.3.1-alpha.1",
                    "1.3.1-preview." + SHA[:10], "1.3.1+build.1", "1.3.1-beta.2+" + SHA[:10],
                    "1.3.1\n", " 1.3.1", "01.3.1", "1.3.65536", "1.3.1-beta." + "9" * 120):
            with self.subTest(value=bad), self.assertRaises(ValueError):
                version.parse_release_version(bad)

    def test_beta_rc_and_stable_have_one_numeric_identity_and_compatible_feeds(self):
        for channel, package, display, feeds in (
                ("beta.2", "2.17.4-beta.2", "Open Beta 2.17.4 (beta.2)", ("update-beta.json",)),
                ("rc.1", "2.17.4-rc.1", "2.17.4 Release Candidate 1", ("update-rc.json", "update-beta.json")),
                ("stable", "2.17.4", "2.17.4", ("update-stable.json", "update-beta.json"))):
            with self.subTest(channel=channel):
                self.write("RELEASE_CHANNEL", channel)
                actual = version.identity(self.root, SHA)
                self.assertEqual(actual["version"], package)
                self.assertEqual(actual["installer_version"], "2.17.4")
                self.assertEqual(version.display_version(self.root, SHA), display)
                self.assertEqual(version.release_manifest_names(package), feeds)
                self.assertEqual(version.release_channel_for_version(package), channel.split(".")[0])
                self.assertEqual(version.validate_target_version(package, self.root, SHA), actual)
                with self.assertRaisesRegex(ValueError, "Target version differs from source"):
                    version.validate_target_version("2.17.5-beta.1", self.root, SHA)

    def test_target_assertion_cannot_promote_or_relabel_existing_source(self):
        for channel, targets in (("preview", ("2.17.4-beta.2", "2.17.4")),
                                 ("beta.1", ("2.17.4-beta.2", "2.17.4-rc.1", "2.17.4")),
                                 ("stable", ("2.17.4-beta.1", "2.17.4-rc.1"))):
            self.write("RELEASE_CHANNEL", channel)
            for target in targets:
                with self.subTest(channel=channel, target=target), self.assertRaises(ValueError):
                    version.validate_target_version(target, self.root, SHA)

    def make_release_installers(self, package: str):
        self.write("dist/.fixture", "metadata test only; these bytes are not installer acceptance evidence")
        for suffix in ("win64-Setup.exe", "macos-universal.pkg", "linux-x86_64.deb"):
            path = f"dist/SpectralForge-Chimera-{package}-{suffix}"
            self.write(path, "test installer bytes")
            release.checksum(self.root / path)
        return self.root / "dist"

    def test_release_manifests_match_exact_channel_names_urls_and_source(self):
        for channel in ("beta.2", "rc.1", "stable"):
            with self.subTest(channel=channel):
                shutil.rmtree(self.root / "dist", ignore_errors=True)
                self.write("RELEASE_CHANNEL", channel)
                expected = version.identity(self.root, SHA)
                dist = self.make_release_installers(expected["version"])
                with patch.object(release, "ROOT", self.root), patch.object(version, "revision", return_value=SHA):
                    feeds = release.manifest(dist, target_version=expected["version"], source_sha=SHA)
                self.assertEqual(tuple(path.name for path in feeds), version.release_manifest_names(expected["version"]))
                for path in feeds:
                    feed = json.loads(path.read_text())
                    self.assertEqual(feed["channel"], path.stem.removeprefix("update-"))
                    self.assertEqual(feed["version"], expected["version"])
                    self.assertEqual(feed["source_sha"], SHA)
                    self.assertEqual({asset["platform"] for asset in feed["assets"]}, {"windows", "macos", "linux"})
                    for asset in feed["assets"]:
                        self.assertIn("-" + expected["version"] + "-", asset["name"])
                        self.assertEqual(asset["url"], f"https://github.com/{release.REPO}/releases/download/v{expected['version']}/{asset['name']}")
                        self.assertEqual(asset["sha256"], release.sha256(dist / asset["name"]))
                if channel != "beta.2":
                    legacy = json.loads((dist / "update-beta.json").read_text())
                    canonical = json.loads(feeds[0].read_text())
                    self.assertEqual(legacy["assets"], canonical["assets"])
                    self.assertEqual(legacy["releaseUrl"], canonical["releaseUrl"])

    def test_manifest_rejects_bad_source_target_checksums_or_mixed_downloads(self):
        self.write("RELEASE_CHANNEL", "beta.2")
        package = version.identity(self.root, SHA)["version"]
        dist = self.make_release_installers(package)
        with patch.object(release, "ROOT", self.root), patch.object(version, "revision", return_value=SHA):
            with self.assertRaisesRegex(ValueError, "Target version differs"):
                release.manifest(dist, target_version="2.17.4-beta.3")
            with self.assertRaisesRegex(ValueError, "source SHA differs"):
                release.manifest(dist, source_sha="f" * 40)
            self.write("dist/SpectralForge-Chimera-2.17.4-beta.1-win64-Setup.exe", "stale")
            with self.assertRaisesRegex(RuntimeError, "mixed-version"):
                release.manifest(dist)
            (dist / "SpectralForge-Chimera-2.17.4-beta.1-win64-Setup.exe").unlink()
            installer = dist / f"SpectralForge-Chimera-{package}-win64-Setup.exe"
            sidecar = installer.with_name(installer.name + ".sha256.txt")
            for contents in ("", "0" * 64 + "  " + installer.name, release.sha256(installer) + "  wrong.exe"):
                sidecar.write_text(contents)
                with self.subTest(checksum=contents), self.assertRaisesRegex(RuntimeError, "SHA256"):
                    release.manifest(dist)
            self.assertFalse((dist / "update-beta.json").exists())
            self.assertFalse((dist / "SHA256SUMS.txt").exists())

    def test_source_preview_candidate_assembly_remains_private_and_source_derived(self):
        self.write("RELEASE_CHANNEL", "preview")
        expected = version.identity(self.root, SHA)
        dist = self.make_release_installers(expected["version"])
        with patch.object(release, "ROOT", self.root), patch.object(version, "revision", return_value=SHA):
            feeds = release.manifest(dist, source_sha=SHA)
            self.assertEqual(tuple(path.name for path in feeds), ("update-beta.json",))
            feed = json.loads(feeds[0].read_text())
            self.assertEqual(feed["version"], "2.17.4-preview." + SHA[:10])
            self.assertEqual(feed["source_sha"], SHA)
            # Candidate assembly does not promote a preview to a public target.
            with self.assertRaises(ValueError):
                release.manifest(dist, target_version=expected["version"], source_sha=SHA)
            with self.assertRaisesRegex(ValueError, "Target version differs"):
                release.manifest(dist, target_version="2.17.4-beta.1", source_sha=SHA)
        with self.assertRaises(ValueError):
            version.parse_release_version(feed["version"])

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
        for key in ("product_version", "source_sha", "version"):
            self.write("build/chimera-build-version.json", json.dumps({**expected, key: "stale"}))
            with self.assertRaisesRegex(ValueError, "reconfigure and rebuild"):
                version.validate_build(self.root / "build", expected)
        self.write("build/chimera-build-version.json", json.dumps(expected))
        version.validate_build(self.root / "build", expected)

    def test_obs_filter_stage_carries_license_guide_source_and_module_version(self):
        build, stage = self.root / "build", self.root / "stage"
        self.write("build/obs-module/bin/64bit/chimera-obs.dll", "module fixture")
        self.write("build/obs-module/data/locale/en-US.ini", 'Chimera="SpectralForge Chimera"\n')
        self.write("build/obs-module/data/locale/ko-KR.ini", 'Chimera="SpectralForge Chimera"\n')
        self.assertTrue(release.obs_filter(stage, build, SHA, required=True))
        target = stage / "OBS/chimera-obs"
        self.assertEqual((target / "bin/64bit/chimera-obs.dll").read_text(), "module fixture")
        self.assertTrue((target / "data/locale/en-US.ini").is_file())
        self.assertEqual((target / "LICENSE.txt").read_bytes(), (ROOT / "OBS/LICENSE").read_bytes())
        self.assertEqual((target / "README.txt").read_bytes(), (ROOT / "OBS/packaging/README.txt").read_bytes())
        source = (target / "SOURCE.txt").read_text(encoding="utf-8")
        self.assertIn(f"Module version: {release.obs_module_version()}\n", source)
        self.assertIn(f"/tree/{SHA}/OBS", source)
        # The installer removes exactly these files when the task is deselected.
        iss = (ROOT / "Installer/Chimera.iss").read_text(encoding="utf-8-sig")
        removed = set(re.findall(r'Type: files; Name: "\{#ObsPluginDir\}\\([^"]+)"; Check: ObsFilterDeselected', iss))
        staged = {str(p.relative_to(target)).replace("/", "\\") for p in target.rglob("*") if p.is_file()}
        self.assertEqual(removed, staged | {r"data\default-vst3-path.txt"})
        self.assertFalse(release.obs_filter(self.root / "other", self.root / "missing", SHA, required=False))
        with self.assertRaises(RuntimeError):
            release.obs_filter(self.root / "other", self.root / "missing", SHA, required=True)

    def test_obs_filter_rejects_incomplete_or_unowned_module_files(self):
        for relative in release.OBS_MODULE_FILES:
            self.write("build/obs-module/" + relative)
        module = self.root / "build/obs-module"
        (module / "data/locale/ko-KR.ini").unlink()
        with self.assertRaisesRegex(RuntimeError, "inventory mismatch"):
            release.obs_filter(self.root / "stage", self.root / "build", SHA, required=True)
        self.write("build/obs-module/data/locale/ko-KR.ini")
        self.write("build/obs-module/data/user-settings.json")
        with self.assertRaisesRegex(RuntimeError, "inventory mismatch"):
            release.obs_filter(self.root / "stage", self.root / "build", SHA, required=True)

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
        shutil.copytree(ROOT / "Assets/IRs", self.root / "Assets/IRs")
        for path in ("CMakeLists.txt", "Source/IRReferenceCatalog.h", "Validation/ir-distribution-policy.json",
                     "Validation/public-presentation-policy.json"):
            target = self.root / path
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(ROOT / path, target)
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

    def prepare_portable_fixture(self):
        """Real staging inputs with synthetic executable bytes, not installer evidence."""
        expected = version.identity(self.root, SHA)
        self.write("build/chimera-build-version.json", json.dumps(expected))
        self.write("build/ChimeraAmpMatrix_artefacts/Release/Standalone/SpectralForge Chimera.exe",
                   "Synthetic standalone bytes; never executed or distributed.\n")
        self.write("build/ChimeraAmpMatrix_artefacts/Release/VST3/SpectralForge Chimera.vst3/Contents/x86_64-win/SpectralForge Chimera.vst3",
                   "Synthetic plugin bytes; no native compatibility claim.\n")
        for name in release.PUBLIC_DOCUMENTS:
            destination = self.root / "docs" / name
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(ROOT / "docs" / name, destination)
        shutil.copytree(ROOT / "Assets/IRs", self.root / "Assets/IRs")
        for relative in ("CMakeLists.txt", "Source/IRReferenceCatalog.h",
                         "Validation/ir-distribution-policy.json", "Validation/public-presentation-policy.json",
                         "COPYRIGHT.txt"):
            destination = self.root / relative
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(ROOT / relative, destination)
        # Retain a private research document and raw evidence containing names.
        # The product package must omit them without rewriting these originals.
        self.write("docs/AMP_NATIVE_DSP.md", "Private research: Fender Twin Reverb / BOSS SD-1.\n")
        self.write("build/Testing/Temporary/LastTest.log",
                   "PASS synthetic fixture only\nReference bank: Fender Twin Reverb / BOSS SD-1\n")
        return expected, self.root / "build/Testing/Temporary/LastTest.log"

    def test_portable_main_writes_exact_source_evidence_pointer_and_archive(self):
        expected, raw_log = self.prepare_portable_fixture()
        raw_before = raw_log.read_bytes()
        globals_before = release.VERSION, dict(release.IDENTITY)
        with patch.object(portable, "root", self.root), patch.object(release, "ROOT", self.root), \
             patch.object(portable.subprocess, "check_output", return_value=SHA + "\n") as revision, \
             patch.object(portable, "transfer") as transfer, \
             patch.dict(portable.os.environ, {"GITHUB_RUN_ID": "456", "GITHUB_RUN_ATTEMPT": "2"}), \
             contextlib.redirect_stdout(io.StringIO()):
            portable.main()
        revision.assert_called_once_with(["git", "rev-parse", "HEAD"], cwd=self.root, text=True)
        transfer.assert_called_once()
        archive_path, digest, transferred_source, transferred_manifest = transfer.call_args.args
        self.assertEqual(transferred_source, SHA)
        self.assertEqual(hashlib.sha256(archive_path.read_bytes()).hexdigest(), digest)
        self.assertEqual(archive_path.with_name(archive_path.name + ".sha256").read_text().split(),
                         [digest, archive_path.name])
        with zipfile.ZipFile(archive_path) as archive:
            names = set(archive.namelist())
            self.assertIn("CTest.log", names)
            self.assertIn("SpectralForge Chimera.exe", names)
            self.assertTrue(set(release.PUBLIC_DOCUMENTS) <= names)
            self.assertNotIn("Testing/Temporary/LastTest.log", names)
            self.assertTrue(set(public_names.load_policy()["research_documents_not_shipped"]).isdisjoint(names))
            pointer = archive.read("CTest.log").decode("utf-8")
            self.assertIn("Version: " + expected["version"], pointer)
            self.assertIn("Source commit: " + SHA, pointer)
            self.assertIn("Raw CTest log SHA-256: " + hashlib.sha256(raw_before).hexdigest(), pointer)
            self.assertIn("Raw CTest log bytes: " + str(len(raw_before)), pointer)
            self.assertIn("/actions/runs/456", pointer)
            self.assertIn("CI run attempt: 2", pointer)
            self.assertNotIn("Fender", pointer)
            self.assertNotIn("BOSS", pointer)
            manifest = json.loads(archive.read("build-manifest.json"))
            self.assertEqual(manifest, transferred_manifest)
            version.validate_manifest(manifest, expected)
            for field in ("published_release", "publisher_signed", "daw_verified"):
                self.assertFalse(manifest[field])
            for entry in manifest["files"]:
                contents = archive.read(entry["path"])
                self.assertEqual(entry["bytes"], len(contents))
                self.assertEqual(entry["sha256"], hashlib.sha256(contents).hexdigest())
        self.assertEqual(raw_log.read_bytes(), raw_before)
        self.assertIn("Fender", (self.root / "docs/AMP_NATIVE_DSP.md").read_text())
        self.assertEqual((release.VERSION, release.IDENTITY), globals_before)

    def test_portable_main_rejects_original_name_in_packaged_manual_before_transfer(self):
        _, raw_log = self.prepare_portable_fixture()
        raw_before = raw_log.read_bytes()
        with (self.root / "docs/MANUAL.html").open("a", encoding="utf-8") as stream:
            stream.write('\n<button title="BOSS SD-1">Yellow Asym</button>\n')
        with patch.object(portable, "root", self.root), patch.object(release, "ROOT", self.root), \
             patch.object(portable.subprocess, "check_output", return_value=SHA + "\n"), \
             patch.object(portable, "transfer") as transfer, contextlib.redirect_stdout(io.StringIO()), \
             self.assertRaisesRegex(RuntimeError, "Original amp/effect name.*MANUAL.html"):
            portable.main()
        transfer.assert_not_called()
        self.assertEqual(list((self.root / "candidate").glob("*.zip")), [])
        self.assertEqual(raw_log.read_bytes(), raw_before)

    def test_cmake_project_and_generated_product_identity(self):
        # Run the real CMake version module and templates, without fetching JUCE.
        shutil.copytree(ROOT / "cmake", self.root / "cmake")
        self.write("CMakeLists.txt", f'''cmake_minimum_required(VERSION 3.22)
include(cmake/ChimeraVersion.cmake)
project(VersionContract VERSION ${{CHIMERA_PRODUCT_VERSION}} LANGUAGES NONE)
set(CHIMERA_SOURCE_SHA "{SHA}")
string(SUBSTRING "${{CHIMERA_SOURCE_SHA}}" 0 10 CHIMERA_BUILD_REVISION)
chimera_set_package_identity()
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
        for channel, update_channel in (("beta.2", "beta"), ("rc.1", "rc"), ("stable", "stable")):
            self.write("RELEASE_CHANNEL", channel + "\n")
            subprocess.run(["cmake", "-S", str(self.root), "-B", str(self.root / "build")],
                           check=True, capture_output=True, text=True)
            expected = version.identity(self.root, SHA)
            version.validate_build(self.root / "build", expected)
            header = (self.root / "build/ChimeraBuildVersion.h").read_text()
            self.assertIn('"' + expected["version"] + '"', header)
            self.assertIn('"' + version.display_version(self.root, SHA) + '"', header)
            self.assertIn(f'#define CHIMERA_UPDATE_CHANNEL "{update_channel}"', header)
            self.assertIn(f'#define CHIMERA_UPDATE_MANIFEST "update-{update_channel}.json"', header)
        for bad_channel in ("beta.0", "beta.01", "rc.0", "rc.01", "stable.1", "beta.1+meta"):
            self.write("RELEASE_CHANNEL", bad_channel)
            result = subprocess.run(["cmake", "-S", str(self.root), "-B", str(self.root / "build")],
                                    capture_output=True, text=True)
            self.assertNotEqual(result.returncode, 0)
        self.write("RELEASE_CHANNEL", "preview\n")
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

    def test_installed_product_identity_and_user_data_paths_are_immutable(self):
        # These values are persisted by hosts/OS installers and existing user
        # libraries; version/channel changes must never create another product.
        cmake = (ROOT / "CMakeLists.txt").read_text()
        self.assertIn("BUNDLE_ID audio.spectralforge.chimera", cmake)
        self.assertIn("PLUGIN_MANUFACTURER_CODE SpFg PLUGIN_CODE Chm1", cmake)
        self.assertIn('PRODUCT_NAME "SpectralForge Chimera"', cmake)
        inno = (ROOT / "Installer/Chimera.iss").read_text(encoding="utf-8-sig")
        self.assertIn("AppId=SpectralForge.ChimeraAmpMatrix\n", inno)
        self.assertIn("DefaultDirName={autopf}\\SpectralForge\\Chimera Amp Matrix\n", inno)
        for rule in inno.splitlines():
            if rule.startswith("Source:") and "Chimera-Personal-IRs" in rule:
                self.assertIn("onlyifdoesntexist", rule)
                self.assertIn("uninsneveruninstall", rule)
        packaging = (ROOT / "Tools/package_release.py").read_text()
        self.assertIn('"--identifier", "audio.spectralforge.chimera"', packaging)
        self.assertIn("Package: chimera-amp-matrix", packaging)
        self.assertIn('getChildFile("SpectralForge/Chimera")', (ROOT / "Source/IRCollection.h").read_text())
        self.assertIn('getChildFile("SpectralForge/Chimera/ir-library.json")',
                      (ROOT / "Source/IRUserPreferences.h").read_text())
        self.assertIn('getChildFile("SpectralForge/Chimera")', (ROOT / "Source/ReleaseSupport.cpp").read_text())

    @unittest.skipUnless(shutil.which("dpkg"), "Debian version comparison runs on Linux")
    def test_debian_upgrade_order_matches_semver(self):
        packages = ("1.3.1-beta.1", "1.3.1-beta.2", "1.3.1-beta.10", "1.3.1-rc.1", "1.3.1", "1.3.2-beta.1")
        for previous, current in zip(packages, packages[1:]):
            subprocess.run(["dpkg", "--compare-versions", previous.replace("-", "~", 1), "lt",
                            current.replace("-", "~", 1)], check=True, capture_output=True, text=True)

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
