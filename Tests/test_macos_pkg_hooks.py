#!/usr/bin/env python3
"""Execute packaged shell hooks with OS calls replaced in temporary test copies.

No system paths are changed. These are branch/failure tests, not macOS Installer
or privilege-transition acceptance; the real PKG is tested on the macOS runner.
"""
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "Tools"))
import package_release


@unittest.skipUnless(os.name == "posix", "POSIX shell hooks")
class HookTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="chimera-pkg-hooks-")
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.log = self.root / "calls.jsonl"
        self.home = self.root / "user home"
        self.home.mkdir()
        self.env = dict(os.environ, CALL_LOG=str(self.log), TEST_USER="musician",
                        TEST_UID="501", TEST_HOME=str(self.home))
        stub = self.root / "stub"
        stub.write_text("#!" + sys.executable + "\n" + '''import json, os, pathlib, sys
name = pathlib.Path(sys.argv[0]).name
with open(os.environ["CALL_LOG"], "a") as f:
    f.write(json.dumps([name, sys.argv[1:]]) + "\\n")
if name == "stat": print(os.environ["TEST_USER"])
if name == "id": print(os.environ["TEST_UID"])
if name == "dscl": print("NFSHomeDirectory: " + os.environ["TEST_HOME"])
default = "1" if name == "pgrep" else "0"
sys.exit(int(os.environ.get("STATUS_" + name.replace("-", "_"), default)))
''')
        stub.chmod(0o755)
        for name in ("stat", "id", "dscl", "codesign", "launchctl", "pgrep", "document-maintenance"):
            shutil.copy2(stub, self.root / name)

    def run_hook(self, source, volume="/", **env):
        text = (ROOT / source).read_text(encoding="utf-8")
        for absolute in ("/usr/bin/stat", "/usr/bin/id", "/usr/bin/dscl", "/usr/bin/codesign",
                         "/bin/launchctl", "/usr/bin/pgrep"):
            text = text.replace(absolute, str(self.root / Path(absolute).name))
        hook = self.root / "hook.sh"
        hook.write_text(text, encoding="utf-8")
        self.log.unlink(missing_ok=True)
        result = subprocess.run(["sh", str(hook), "package.pkg", "/", volume],
                                env=dict(self.env, **env), capture_output=True, text=True, timeout=10)
        calls = [json.loads(line) for line in self.log.read_text().splitlines()] if self.log.exists() else []
        return result, calls

    def test_preinstall_blocks_running_and_unknown_process_before_maintenance(self):
        for status in ("0", "2", "127"):
            with self.subTest(status=status):
                result, calls = self.run_hook("Tools/macos-pkg-preinstall.sh", STATUS_pgrep=status)
                self.assertNotEqual(result.returncode, 0)
                self.assertNotIn("document-maintenance", [name for name, _ in calls])

    def test_preinstall_continues_native_document_preservation(self):
        result, calls = self.run_hook("Tools/macos-pkg-preinstall.sh")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual([name for name, _ in calls], ["pgrep", "pgrep", "document-maintenance"])
        self.assertEqual(calls[-1][1], ["prepare-upgrade", "//Library/Application Support/SpectralForge/Chimera/Documentation"])
        result, _ = self.run_hook("Tools/macos-pkg-preinstall.sh", STATUS_document_maintenance="1")
        self.assertNotEqual(result.returncode, 0)

    def test_obs_component_guard_needs_no_document_helper(self):
        (self.root / "document-maintenance").unlink()
        result, calls = self.run_hook("Tools/macos-pkg-preinstall.sh")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual([name for name, _ in calls], ["pgrep", "pgrep"])

    def test_postinstall_no_console_or_other_volume_is_deferred(self):
        for user in ("", "root", "loginwindow", "_mbsetupuser"):
            with self.subTest(user=user):
                result, calls = self.run_hook("OBS/packaging/macos-pkg-postinstall.sh", TEST_USER=user)
                self.assertEqual(result.returncode, 0, result.stderr)
                self.assertNotIn("launchctl", [name for name, _ in calls])
        result, calls = self.run_hook("OBS/packaging/macos-pkg-postinstall.sh", volume="/Volumes/Other")
        self.assertEqual(result.returncode, 0)
        self.assertEqual(calls, [])

    def test_postinstall_identity_and_signature_errors_never_launch(self):
        for env in ({"TEST_UID": "0"}, {"TEST_UID": "bad"}, {"TEST_HOME": "relative"},
                    {"TEST_HOME": "/nonexistent-chimera-fixture"}, {"STATUS_codesign": "1"},
                    {"STATUS_stat": "1"}, {"STATUS_id": "1"}, {"STATUS_dscl": "1"}):
            with self.subTest(env=env):
                result, calls = self.run_hook("OBS/packaging/macos-pkg-postinstall.sh", **env)
                self.assertNotEqual(result.returncode, 0)
                self.assertNotIn("launchctl", [name for name, _ in calls])

    def test_postinstall_uses_console_identity_clean_environment_and_propagates_failure(self):
        result, calls = self.run_hook("OBS/packaging/macos-pkg-postinstall.sh")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual([name for name, _ in calls], ["stat", "id", "dscl", "codesign", "launchctl"])
        self.assertEqual(calls[-1][1], ["asuser", "501", "/usr/bin/sudo", "-u", "musician",
            "/usr/bin/env", "-i", "HOME=" + str(self.home), "PATH=/usr/bin:/bin:/usr/sbin:/sbin",
            "/Library/Application Support/SpectralForge/Chimera/OBS/chimera-obs-installer"])
        result, _ = self.run_hook("OBS/packaging/macos-pkg-postinstall.sh", STATUS_launchctl="1")
        self.assertNotEqual(result.returncode, 0)


class DistributionTests(unittest.TestCase):
    @unittest.skipUnless(shutil.which("node"), "Node executes the Installer JS logic fixtures")
    def test_preselection_and_running_obs_logic(self):
        tree = ET.fromstring(package_release.macos_distribution("1.3.2"))
        script = tree.find("script").text
        # Execute the production JS with only the Apple host API mocked. Native
        # installer -showChoicesXML additionally checks Apple's interpretation.
        harness = '''const assert = require('node:assert/strict');
let paths = new Set(), apps = [], my = {result: {}};
let system = {env: {HOME: '/Users/musician'},
 files: {fileExistsAtPath: p => paths.has(p)},
 applications: {fromIdentifier: id => {assert.equal(id, 'com.obsproject.obs-studio'); return apps;}}};
''' + script + '''
assert.equal(obsPreselected(), false);
paths.add('/Applications/OBS.app'); assert.equal(obsPreselected(), true); paths.clear();
paths.add('/Users/musician/Library/Application Support/obs-studio/plugins/chimera-obs.plugin');
assert.equal(obsPreselected(), true);
system.env.HOME = undefined; assert.equal(obsPreselected(), false);
assert.equal(obsClosed(), true);
apps = [{}]; assert.equal(obsClosed(), false); assert.equal(my.result.type, 'Fatal');
'''
        subprocess.run([shutil.which("node"), "-e", harness], check=True, timeout=10)


if __name__ == "__main__":
    unittest.main()
