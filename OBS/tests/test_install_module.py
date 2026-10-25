#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Native macOS/Linux installer fault and preservation tests; no OBS GUI claim.

--package uses the actual CI-staged module. Without it, Linux uses a file
fixture and macOS compiles/signs a small real bundle with the native tools.
Fault injection only exists here, never in the distributed installer.
"""
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import plistlib
import shutil
import signal
import subprocess
import sys
import tempfile
import time
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("installer", ROOT / "packaging/install_module.py")
installer = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(installer)
PLATFORM = "macos" if sys.platform == "darwin" else "linux"
PACKAGE = None


def manifest(package):
    files = installer.inventory(package / installer.module_name(PLATFORM))
    (package / "payload-manifest.json").write_text(json.dumps({"schema": 1, "platform": PLATFORM, "files": files}))


def write(path, content=b"user data\x00\xff"):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(content)


class InstallTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if os.geteuid() == 0:
            raise RuntimeError("Run installer tests as an ordinary user (no root bypass is tested as success)")
        cls.fixture_temp = tempfile.TemporaryDirectory(prefix="chimera-native-fixture-")
        cls.addClassCleanup(cls.fixture_temp.cleanup)
        cls.fixture = Path(cls.fixture_temp.name).resolve() / "package"
        process_source = Path(cls.fixture_temp.name) / "obs.c"
        process_source.write_text('#include <stdio.h>\n#include <unistd.h>\nint main(void) { puts("ready"); fflush(stdout); for (;;) pause(); }\n')
        cls.obs_process = Path(cls.fixture_temp.name) / "obs"
        subprocess.run(["cc", str(process_source), "-o", str(cls.obs_process)], check=True)
        if PACKAGE:
            shutil.copytree(PACKAGE, cls.fixture)
        else:
            payload = cls.fixture / installer.module_name(PLATFORM)
            for name in installer.owned_files(PLATFORM):
                write(payload / name, ("new fixture " + name).encode())
            binary = payload / ("Contents/MacOS/chimera-obs" if PLATFORM == "macos" else "bin/64bit/chimera-obs.so")
            binary.chmod(0o755)
            if PLATFORM == "macos":
                source = Path(cls.fixture_temp.name) / "module.c"
                source.write_text("int fixture(void) { return 42; }\n")
                subprocess.run(["clang", "-bundle", str(source), "-o", str(binary)], check=True)
                (payload / "Contents/Info.plist").write_bytes(plistlib.dumps({
                    "CFBundleExecutable": "chimera-obs", "CFBundleIdentifier": "com.ravenforge.chimera-obs",
                    "CFBundlePackageType": "BNDL", "CFBundleVersion": "0.1.0"}))
                (payload / "Contents/_CodeSignature/CodeResources").unlink()
                subprocess.run(["codesign", "--force", "--sign", "-", str(payload)], check=True)
            shutil.copy2(ROOT / ("packaging/install-macos.sh" if PLATFORM == "macos" else "packaging/install-linux.sh"), cls.fixture / "install.sh")
            shutil.copy2(ROOT / "packaging/install_module.py", cls.fixture / "install_module.py")
            manifest(cls.fixture)
        installer.verify_payload(cls.fixture / installer.module_name(PLATFORM), installer.read_manifest(cls.fixture, PLATFORM), PLATFORM)

    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="chimera installer spaces-")
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name).resolve()
        self.package = self.root / "download package"
        shutil.copytree(self.fixture, self.package)
        self.home = self.root / "home"
        self.config = self.home / ("Library/Application Support" if PLATFORM == "macos" else ".config")
        self.plugins = self.config / "obs-studio/plugins"
        self.target = self.plugins / installer.module_name(PLATFORM)
        self.work = self.plugins / ".chimera-obs-transaction"
        self.protected = [self.config / "obs-studio/basic", self.config / "obs-studio/global.ini",
                          self.plugins / "other-plugin", self.home / "Chimera",
                          self.home / ("Library/Audio/Plug-Ins/VST3" if PLATFORM == "macos" else ".vst3")]
        for path in (self.protected[0] / "scenes/live.json", self.protected[0] / "profiles/stream/basic.ini",
                     self.protected[1], self.protected[2] / "bin/other.so", self.protected[3] / "presets/custom.json",
                     self.protected[3] / "IRs/user.wav", self.protected[4] / "SpectralForge Chimera.vst3/Contents/binary"):
            write(path)
        self.before = self.protected_digest()
        self.addCleanup(lambda: self.assertEqual(self.before, self.protected_digest(), "Scenes/settings/presets/IRs/other plugin/VST3 changed"))

    def protected_digest(self):
        return {str(p): hashlib.sha256(p.read_bytes()).hexdigest()
                for root in self.protected for p in ([root] if root.is_file() else root.rglob("*")) if p.is_file()}

    def run_install(self, uninstall=False):
        installer.install(self.package, self.plugins, PLATFORM, uninstall)

    def seed_old(self, extra=False):
        shutil.copytree(self.package / installer.module_name(PLATFORM), self.target)
        notice = self.target / ("Contents/Resources/README.txt" if PLATFORM == "macos" else "README.txt")
        notice.write_text("previous known-working version")
        if PLATFORM == "macos":
            subprocess.run(["codesign", "--force", "--sign", "-", str(self.target)], check=True, capture_output=True)
        if extra:
            write(self.target / "user-notes.txt")
            (self.target / "user-empty-directory").mkdir()
        return installer.inventory(self.target)

    def assert_old(self, old):
        self.assertEqual(installer.inventory(self.target), old)
        self.assertFalse(self.work.exists(), "Incomplete staging/backup was not cleaned")

    def test_fresh_upgrade_repair_uninstall(self):
        self.run_install()
        self.run_install()
        self.run_install()
        write(self.target / "user-notes.txt")
        (self.target / "user-empty-directory").mkdir()
        self.run_install(uninstall=True)
        self.assertEqual(set(installer.inventory(self.target)), {"user-notes.txt"})
        self.assertTrue((self.target / "user-empty-directory").is_dir())
        self.assertFalse(self.work.exists())
        self.run_install(uninstall=True)

    def test_legacy_upgrade_preserves_user_files_or_refuses_signed_bundle_extras(self):
        old = self.seed_old(extra=True)
        if PLATFORM == "macos":
            with self.assertRaisesRegex(installer.InstallError, "User files"):
                self.run_install()
            self.assert_old(old)
        else:
            self.run_install()
            self.assertEqual((self.target / "user-notes.txt").read_bytes(), b"user data\x00\xff")
            self.assertTrue((self.target / "user-empty-directory").is_dir())
            self.assertNotEqual(installer.inventory(self.target), old)

    def test_root_refused(self):
        with patch.object(installer.os, "geteuid", return_value=0), self.assertRaises(installer.InstallError):
            self.run_install()
        self.assertFalse(self.target.exists())

    def test_real_obs_named_process_blocks_install_update_and_uninstall(self):
        process = subprocess.Popen([str(self.obs_process)], stdout=subprocess.PIPE, text=True)
        try:
            self.assertEqual(process.stdout.readline().strip(), "ready")
            with self.assertRaisesRegex(installer.InstallError, "running"):
                self.run_install()
            old = self.seed_old()
            for uninstall in (False, True):
                with self.assertRaisesRegex(installer.InstallError, "running"):
                    self.run_install(uninstall)
                self.assert_old(old)
        finally:
            process.kill()
            process.communicate(timeout=10)

    def test_shell_entrypoint_with_spaces_and_xdg(self):
        env = dict(os.environ, HOME=str(self.home), XDG_CONFIG_HOME=str(self.config))
        for args in ([], [], ["--uninstall"]):
            result = subprocess.run(["sh", str(self.package / "install.sh")] + args,
                                    env=env, capture_output=True, text=True, timeout=30)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertFalse(self.target.exists())

    def test_process_running_missing_tool_errors_and_timeout_block_all_mutations(self):
        real_run = subprocess.run
        for state in (0, 2, 3, 127, FileNotFoundError("pgrep"), subprocess.TimeoutExpired("pgrep", 10)):
            for mode in ("fresh", "update", "uninstall"):
                with self.subTest(state=state, mode=mode):
                    if self.target.exists():
                        shutil.rmtree(self.target)
                    old = self.seed_old() if mode != "fresh" else None
                    def run(command, *args, **kwargs):
                        if command[0] == "pgrep":
                            if isinstance(state, Exception):
                                raise state
                            return subprocess.CompletedProcess(command, state)
                        return real_run(command, *args, **kwargs)
                    with patch.object(installer.subprocess, "run", side_effect=run), self.assertRaises(installer.InstallError):
                        self.run_install(mode == "uninstall")
                    self.assertEqual(installer.inventory(self.target) if self.target.exists() else None, old)
                    self.assertFalse(self.work.exists())

    def test_process_state_changes_after_staging(self):
        for uninstall in (False, True):
            old = self.seed_old() if not self.target.exists() else installer.inventory(self.target)
            with patch.object(installer, "require_obs_closed", side_effect=[None, None, installer.InstallError("unknown")]), self.assertRaises(installer.InstallError):
                self.run_install(uninstall)
            self.assert_old(old)

    def test_partial_tree_copy_fails_without_touching_old(self):
        old = self.seed_old()
        def partial(source, dest, platform):
            write(dest / "incomplete", b"partial")
            raise OSError("ENOSPC during copy")
        for uninstall in (False, True):
            with patch.object(installer, "copy_tree", side_effect=partial), self.assertRaises(OSError):
                self.run_install(uninstall)
            self.assert_old(old)

    @unittest.skipUnless(PLATFORM == "linux", "Linux file overlay fault")
    def test_partial_overlay_copy_keeps_old(self):
        old = self.seed_old(extra=True)
        original = shutil.copy2
        def fail(source, dest, *args, **kwargs):
            if Path(source).is_relative_to(self.package):
                Path(dest).write_bytes(b"partial overwrite")
                raise OSError("disk full")
            return original(source, dest, *args, **kwargs)
        with patch.object(installer.shutil, "copy2", side_effect=fail), self.assertRaises(OSError):
            self.run_install()
        self.assert_old(old)

    def test_payload_manifest_missing_extra_tampered_and_symlink(self):
        old = self.seed_old()
        payload = self.package / installer.module_name(PLATFORM)
        name = sorted(installer.owned_files(PLATFORM))[0]
        original = (payload / name).read_bytes()
        for kind in ("missing", "hash", "extra", "symlink", "manifest"):
            with self.subTest(kind=kind):
                if kind == "missing":
                    (payload / name).unlink()
                elif kind == "hash":
                    (payload / name).write_bytes(b"corrupted")
                elif kind == "extra":
                    write(payload / "unowned")
                elif kind == "symlink":
                    (payload / name).unlink()
                    (payload / name).symlink_to(self.protected[1])
                else:
                    (self.package / "payload-manifest.json").write_text('{}')
                with self.assertRaises((installer.InstallError, OSError)):
                    self.run_install()
                self.assert_old(old)
                if (payload / name).is_symlink():
                    (payload / name).unlink()
                (payload / name).write_bytes(original)
                if (payload / "unowned").exists():
                    (payload / "unowned").unlink()
                shutil.copy2(self.fixture / "payload-manifest.json", self.package / "payload-manifest.json")

    def test_symlink_target_and_parent_preserved(self):
        self.target.symlink_to(self.protected[2], target_is_directory=True)
        with self.assertRaises(installer.InstallError):
            self.run_install()
        self.target.unlink()
        shutil.rmtree(self.plugins)
        self.plugins.symlink_to(self.root / "outside", target_is_directory=True)
        with self.assertRaises(installer.InstallError):
            self.run_install()
        self.plugins.unlink()
        # Restore protected other-plugin for the per-test digest check.
        write(self.protected[2] / "bin/other.so")

    def test_atomic_exchange_failure_keeps_original(self):
        old = self.seed_old()
        for uninstall in (False, True):
            with patch.object(installer, "exchange", side_effect=OSError("unsupported filesystem")), self.assertRaises(OSError):
                self.run_install(uninstall)
            self.assert_old(old)

    def test_post_exchange_verification_failure_rolls_back(self):
        old = self.seed_old()
        verify = installer.verify_payload
        def fail(folder, *args, **kwargs):
            if folder == self.target:
                raise installer.InstallError("post-exchange verification failure")
            return verify(folder, *args, **kwargs)
        with patch.object(installer, "verify_payload", side_effect=fail), self.assertRaises(installer.InstallError):
            self.run_install()
        self.assert_old(old)

    def test_fresh_post_exchange_failure_removes_incomplete_installation(self):
        verify = installer.verify_payload
        def fail(folder, *args, **kwargs):
            if folder == self.target:
                raise installer.InstallError("verification failed")
            return verify(folder, *args, **kwargs)
        with patch.object(installer, "verify_payload", side_effect=fail), self.assertRaises(installer.InstallError):
            self.run_install()
        self.assertFalse(self.target.exists())
        self.assertFalse(self.work.exists())

    def test_commit_journal_write_failure_rolls_back_even_after_replace(self):
        old = self.seed_old()
        original = installer.write_journal
        def fail(work, value):
            original(work, value)
            if value["phase"] == "committed":
                raise OSError("journal fsync failure")
        with patch.object(installer, "write_journal", side_effect=fail), self.assertRaises(OSError):
            self.run_install()
        self.assert_old(old)

    def test_rollback_failure_retains_backup_and_next_run_recovers(self):
        old = self.seed_old()
        original_exchange = installer.exchange
        verify = installer.verify_payload
        count = 0
        def fail_exchange(a, b):
            nonlocal count
            count += 1
            if count == 2:
                raise OSError("rollback temporarily unavailable")
            return original_exchange(a, b)
        def fail_verify(folder, *args, **kwargs):
            if folder == self.target:
                raise installer.InstallError("verification failed")
            return verify(folder, *args, **kwargs)
        with patch.object(installer, "exchange", side_effect=fail_exchange), patch.object(installer, "verify_payload", side_effect=fail_verify), self.assertRaisesRegex(installer.InstallError, "retained"):
            self.run_install()
        self.assertEqual(installer.inventory(self.work / self.target.name), old)
        with installer.locked(self.plugins):
            installer.recover(self.work, self.target)
        self.assert_old(old)

    def test_cleanup_failure_retried_without_rolling_back_committed_update(self):
        self.seed_old()
        real_remove = shutil.rmtree
        def fail(path, *args, **kwargs):
            if path == self.work:
                raise OSError("temporary cleanup failure")
            return real_remove(path, *args, **kwargs)
        with patch.object(installer.shutil, "rmtree", side_effect=fail):
            self.run_install()
        self.assertEqual(json.loads((self.work / "journal.json").read_text())["phase"], "committed")
        self.run_install()
        self.assertFalse(self.work.exists())

    def fault_process(self, location):
        marker = self.root / "checkpoint"
        runner = self.root / "fault_runner.py"
        runner.write_text('''import importlib.util, pathlib, signal, sys
spec = importlib.util.spec_from_file_location("fault_installer", sys.argv[1])
m = importlib.util.module_from_spec(spec)
spec.loader.exec_module(m)
location, marker, platform = sys.argv[2:5]
name = "copy_tree" if location == "copy" else "exchange"
original = getattr(m, name)
armed = True
def stop(*args, **kwargs):
    global armed
    result = original(*args, **kwargs)
    if armed:
        armed = False
        pathlib.Path(marker).write_text("ready")
        signal.pause()
    return result
setattr(m, name, stop)
sys.argv = [sys.argv[1], "--platform", platform]
sys.exit(m.main())
''')
        env = dict(os.environ, HOME=str(self.home), XDG_CONFIG_HOME=str(self.config))
        process = subprocess.Popen([sys.executable, str(runner), str(self.package / "install_module.py"),
                                    location, str(marker), PLATFORM], env=env, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        deadline = time.monotonic() + 20
        while not marker.exists() and process.poll() is None and time.monotonic() < deadline:
            time.sleep(0.02)
        if not marker.exists():
            process.kill()
            self.fail(str(process.communicate(timeout=10)))
        self.addCleanup(lambda: process.poll() is None and process.kill())
        return process, marker

    def test_native_signals_cleanup_and_sigkill_recovery(self):
        old = self.seed_old()
        for location in ("copy", "exchange"):
            for signum in (signal.SIGTERM, signal.SIGINT, signal.SIGHUP, signal.SIGKILL):
                with self.subTest(location=location, signal=signum):
                    process, marker = self.fault_process(location)
                    process.send_signal(signum)
                    process.communicate(timeout=15)
                    self.assertNotEqual(process.returncode, 0)
                    if signum == signal.SIGKILL:
                        # At every instant an update target is complete (old or
                        # verified new), and the kernel released the lock.
                        self.assertTrue(self.target.is_dir())
                        with installer.locked(self.plugins):
                            installer.recover(self.work, self.target)
                    self.assert_old(old)
                    marker.unlink()

    def test_concurrent_installer_refused(self):
        old = self.seed_old()
        process, marker = self.fault_process("copy")
        with self.assertRaisesRegex(installer.InstallError, "Another"):
            self.run_install()
        self.assertEqual(installer.inventory(self.target), old)
        process.terminate()
        process.communicate(timeout=15)
        self.assert_old(old)

    @unittest.skipUnless(PLATFORM == "macos", "Native macOS codesign/xattr test")
    def test_native_invalid_signature_and_quarantine(self):
        old = self.seed_old()
        payload = self.package / installer.module_name(PLATFORM)
        resource = payload / "Contents/Resources/README.txt"
        resource.write_text("tampered signed resource")
        manifest(self.package)  # Hashes alone cannot authorize an invalid signature.
        with self.assertRaises(subprocess.CalledProcessError):
            self.run_install()
        self.assert_old(old)
        shutil.rmtree(payload)
        shutil.copytree(self.fixture / payload.name, payload)
        manifest(self.package)
        subprocess.run(["/usr/bin/xattr", "-w", "com.apple.quarantine", "0081;fixture;Chimera;", str(payload)], check=True)
        self.run_install()
        self.assertIn("com.apple.quarantine", subprocess.check_output(["/usr/bin/xattr", str(payload)], text=True).splitlines())
        self.assertNotIn("com.apple.quarantine", subprocess.check_output(["/usr/bin/xattr", str(self.target)], text=True).splitlines())


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--package", type=Path)
    args, rest = parser.parse_known_args()
    PACKAGE = args.package.resolve() if args.package else None
    unittest.main(argv=[sys.argv[0]] + rest, verbosity=2)
