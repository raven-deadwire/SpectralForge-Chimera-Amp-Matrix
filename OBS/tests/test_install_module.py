#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Exercise the shipped native installer; Python is only the CI test driver.

Faults are injected into a separately compiled temporary source copy. The shipped
binary has no test flags, runtime overrides or root bypass. --package tests the
real packaged executable and payload. No OBS GUI/hardware acceptance is claimed.
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

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("payload", ROOT / "tools/installer_payload.py")
payload = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(payload)
PLATFORM = "macos" if sys.platform == "darwin" else "linux"
PACKAGE = None
NAME = payload.module_name(PLATFORM)
SOURCE = ROOT / "packaging/install-module.cpp"


def write(path, content=b"user data\x00\xff"):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(content)


def manifest(package):
    files = payload.inventory(package / NAME)
    (package / "payload-manifest.txt").write_text("chimera-obs-v1 " + PLATFORM + "\n" + "".join(
        f"{v['sha256']} {v['mode']:03o} {k}\n" for k, v in sorted(files.items())))


def compile_native(source, binary):
    subprocess.run(payload.helper_compile_command(source, binary, PLATFORM), check=True)
    if PLATFORM == "macos":
        subprocess.run(["codesign", "--force", "--sign", "-", "--timestamp=none", str(binary)], check=True, capture_output=True)


class InstallTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.fixture_temp = tempfile.TemporaryDirectory(prefix="chimera-native-fixture-")
        cls.addClassCleanup(cls.fixture_temp.cleanup)
        folder = Path(cls.fixture_temp.name).resolve()
        cls.fixture = folder / "package"
        if PACKAGE:
            shutil.copytree(PACKAGE, cls.fixture)
        else:
            module = cls.fixture / NAME
            for name in payload.owned_files(PLATFORM):
                write(module / name, ("new fixture " + name).encode())
            binary = module / ("Contents/MacOS/chimera-obs" if PLATFORM == "macos" else "bin/64bit/chimera-obs.so")
            binary.chmod(0o755)
            if PLATFORM == "macos":
                source = folder / "module.c"
                source.write_text("int fixture(void) { return 42; }\n")
                subprocess.run(["cc", "-bundle", str(source), "-o", str(binary)], check=True)
                (module / "Contents/Info.plist").write_bytes(plistlib.dumps({
                    "CFBundleExecutable": "chimera-obs", "CFBundleIdentifier": "com.ravenforge.chimera-obs",
                    "CFBundlePackageType": "BNDL", "CFBundleVersion": "0.1.0"}))
                (module / "Contents/_CodeSignature/CodeResources").unlink()
                subprocess.run(["codesign", "--force", "--sign", "-", str(module)], check=True, capture_output=True)
            shutil.copy2(ROOT / f"packaging/install-{PLATFORM}.sh", cls.fixture / "install.sh")
            compile_native(SOURCE, cls.fixture / "chimera-obs-installer")
            manifest(cls.fixture)
        cls.production = cls.fixture / "chimera-obs-installer"
        original = SOURCE.read_text()
        # Local sandbox runs as uid 0 and cannot switch uid. Its ordinary-path
        # tests use a separate source copy; CI must run unmodified as non-root.
        cls.local_root = os.geteuid() == 0
        if cls.local_root and os.environ.get("CI"):
            raise RuntimeError("CI must run as an ordinary user; no root bypass accepted")
        base = original
        if cls.local_root:
            print("LOCAL ONLY: ordinary-user path uses a temporary root-check substitution; CI tests the shipped binary.", flush=True)
            base = base.replace('require(geteuid() != 0,', 'require(true,')
            local = folder / "local.cpp"
            local.write_text(base)
            cls.local_binary = folder / "local-installer"
            compile_native(local, cls.local_binary)
        instrumented = base.replace('void checkpoint(const char*) {', r'''void checkpoint(const char* phase) {
    if (const char* failPhase = std::getenv("CHIMERA_TEST_FAIL"); failPhase && std::string(failPhase) == phase)
        throw std::runtime_error(std::string("Injected failure: ") + phase);
    if (const char* pausePhase = std::getenv("CHIMERA_TEST_PAUSE"); pausePhase && std::string(pausePhase) == phase) {
        std::ofstream(std::getenv("CHIMERA_TEST_MARKER")) << "ready\n";
        while (!caughtSignal) std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }''')
        instrumented = instrumented.replace('void exchange(const fs::path& a, const fs::path& b) {', '''void exchange(const fs::path& a, const fs::path& b) {
    static int attempts = 0; ++attempts;
    if (std::getenv("CHIMERA_TEST_EXCHANGE_FAIL") || (std::getenv("CHIMERA_TEST_ROLLBACK_FAIL") && attempts == 2))
        fail("Injected atomic exchange failure");''')
        instrumented = instrumented.replace('void copyTree(const fs::path& source, const fs::path& dest) {', '''void copyTree(const fs::path& source, const fs::path& dest) {
    if (std::getenv("CHIMERA_TEST_PARTIAL_COPY")) {
        fs::create_directory(dest); std::ofstream(dest / "partial") << "incomplete";
        fail("Injected partial copy failure");
    }''')
        instrumented = instrumented.replace('checkpoint("after-copy");', '''checkpoint("after-copy");
        if (std::getenv("CHIMERA_TEST_CORRUPT_COPY")) {
            fs::create_directories((candidate / *owned.begin()).parent_path());
            std::ofstream(candidate / *owned.begin()) << "truncated";
        }''')
        instrumented = instrumented.replace('fs::rename(temp, work / "journal.txt"); syncDirectory(work);', '''fs::rename(temp, work / "journal.txt");
    checkpoint(journal.phase == "committed" ? "commit-sync" : "prepare-sync"); syncDirectory(work);''')
        faults = folder / "faults.cpp"
        faults.write_text(instrumented)
        cls.fault_binary = folder / "fault-installer"
        compile_native(faults, cls.fault_binary)
        process_source = folder / "obs.c"
        process_source.write_text('#include <stdio.h>\n#include <unistd.h>\nint main(void) { puts("ready"); fflush(stdout); for (;;) pause(); }\n')
        cls.obs_process = folder / "obs"
        subprocess.run(["cc", str(process_source), "-o", str(cls.obs_process)], check=True)

    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="chimera installer spaces-")
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name).resolve()
        self.package = self.root / "download package"
        shutil.copytree(self.fixture, self.package)
        if self.local_root:
            shutil.copy2(self.local_binary, self.package / "chimera-obs-installer")
        shutil.copy2(self.fault_binary, self.package / "fault-installer")
        self.home = self.root / "home"
        self.config = self.home / ("Library/Application Support" if PLATFORM == "macos" else ".config")
        self.plugins = self.config / "obs-studio/plugins"
        self.target = self.plugins / NAME
        self.work = self.plugins / ".chimera-obs-transaction"
        self.protected = [self.config / "obs-studio/basic", self.config / "obs-studio/global.ini",
                          self.plugins / "other-plugin", self.home / "Chimera",
                          self.home / ("Library/Audio/Plug-Ins/VST3" if PLATFORM == "macos" else ".vst3")]
        for path in (self.protected[0] / "scenes/live.json", self.protected[0] / "profiles/stream/basic.ini",
                     self.protected[1], self.protected[2] / "bin/other.so", self.protected[3] / "presets/custom.json",
                     self.protected[3] / "IRs/user.wav", self.protected[4] / "SpectralForge Chimera.vst3/Contents/binary"):
            write(path)
        self.before = self.protected_digest()
        self.addCleanup(lambda: self.assertEqual(self.before, self.protected_digest(), "User data changed"))
        self.env = dict(os.environ, HOME=str(self.home), XDG_CONFIG_HOME=str(self.config))

    def protected_digest(self):
        return {str(p): hashlib.sha256(p.read_bytes()).hexdigest()
                for root in self.protected for p in ([root] if root.is_file() else root.rglob("*")) if p.is_file()}

    def run_install(self, uninstall=False, fault=None, ok=True, env=None, shell=False):
        environment = dict(self.env, **(env or {}), **(fault or {}))
        args = (["/bin/sh", str(self.package / "install.sh")] if shell else
                [str(self.package / ("fault-installer" if fault else "chimera-obs-installer"))])
        result = subprocess.run(args + (["--uninstall"] if uninstall else []), env=environment,
                                capture_output=True, text=True, timeout=35)
        if ok:
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        else:
            self.assertNotEqual(result.returncode, 0, result.stdout + result.stderr)
        return result

    def seed_old(self, extra=False):
        shutil.copytree(self.package / NAME, self.target)
        (self.target / ("Contents/Resources/README.txt" if PLATFORM == "macos" else "README.txt")).write_text("previous working version")
        if PLATFORM == "macos":
            subprocess.run(["codesign", "--force", "--sign", "-", str(self.target)], check=True, capture_output=True)
        if extra:
            write(self.target / "user-notes.txt")
            (self.target / "user-empty-directory").mkdir()
        return payload.inventory(self.target)

    def assert_old(self, old):
        self.assertEqual(payload.inventory(self.target) if self.target.exists() else None, old)
        self.assertFalse(self.work.exists(), "Incomplete staging/backup was not cleaned")

    def test_fresh_upgrade_repair_uninstall(self):
        for _ in range(3):
            self.run_install()
        write(self.target / "user-notes.txt")
        (self.target / "user-empty-directory").mkdir()
        self.run_install(uninstall=True)
        self.assertEqual(set(payload.inventory(self.target)), {"user-notes.txt"})
        self.assertTrue((self.target / "user-empty-directory").is_dir())
        self.assertFalse(self.work.exists())
        self.run_install(uninstall=True)

    def test_update_preserves_or_refuses_user_additions(self):
        old = self.seed_old(extra=True)
        self.run_install(ok=PLATFORM != "macos")
        if PLATFORM == "macos":
            self.assert_old(old)
        else:
            self.assertNotEqual(payload.inventory(self.target), old)
        self.assertEqual((self.target / "user-notes.txt").read_bytes(), b"user data\x00\xff")
        self.assertTrue((self.target / "user-empty-directory").is_dir())

    def test_empty_user_directory_preserved(self):
        old = self.seed_old()
        (self.target / "empty-user-directory").mkdir()
        self.run_install(ok=PLATFORM != "macos")
        if PLATFORM == "macos":
            self.assert_old(old)
        self.assertTrue((self.target / "empty-user-directory").is_dir())

    def test_no_python_or_compiler_on_user_path(self):
        path = self.root / "system tools"
        path.mkdir()
        for name in ("dirname", "pgrep", "cp", "sha256sum"):
            real = shutil.which(name)
            if real:
                (path / name).symlink_to(real)
        marker = self.root / "unexpected-runtime"
        for name in ("python", "python3", "pypy", "c++", "cc", "clang", "g++"):
            tool = path / name
            tool.write_text(f"#!/bin/sh\necho unexpected > '{marker}'\nexit 99\n")
            tool.chmod(0o755)
        for remove in (False, False, True):
            self.run_install(uninstall=remove, env={"PATH": str(path)}, shell=True)
        self.assertFalse(marker.exists(), "Installer invoked Python/compiler")
        self.assertFalse(list(self.fixture.glob("*.py")), "Python files shipped to users")
        magic = self.production.read_bytes()[:4]
        self.assertIn(magic, (b"\x7fELF", b"\xcf\xfa\xed\xfe", b"\xca\xfe\xba\xbe"))

    def test_production_refuses_root(self):
        command = [str(self.production)]
        if not self.local_root:
            command = ["sudo", "-n", "env", "HOME=" + str(self.home)] + command
        result = subprocess.run(command, env=self.env, capture_output=True, text=True, timeout=20)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("not as root", result.stderr)
        self.assertFalse(self.target.exists())

    def test_real_obs_process_blocks_all_modes(self):
        process = subprocess.Popen([str(self.obs_process)], stdout=subprocess.PIPE, text=True)
        try:
            self.assertEqual(process.stdout.readline().strip(), "ready")
            self.assertIn("running", self.run_install(ok=False).stderr)
            old = self.seed_old()
            for remove in (False, True):
                self.assertIn("running", self.run_install(uninstall=remove, ok=False).stderr)
                self.assert_old(old)
        finally:
            process.kill()
            process.communicate(timeout=10)

    def test_process_errors_and_missing_tool_block(self):
        path = self.root / "bad-tools"
        path.mkdir()
        pgrep = path / "pgrep"
        for status in (0, 2, 3, 127):
            pgrep.write_text(f"#!/bin/sh\nexit {status}\n")
            pgrep.chmod(0o755)
            self.run_install(ok=False, env={"PATH": str(path)})
            old = self.seed_old()
            for remove in (False, True):
                self.run_install(uninstall=remove, ok=False, env={"PATH": str(path)})
                self.assert_old(old)
            shutil.rmtree(self.target)
        pgrep.unlink()
        self.assertIn("process state", self.run_install(ok=False, env={"PATH": str(path)}).stderr)
        self.assertFalse(self.target.exists())

    def test_process_check_timeout_blocks(self):
        path = self.root / "hanging-tools"
        path.mkdir()
        pgrep = path / "pgrep"
        pgrep.write_text("#!/bin/sh\nexec /bin/sleep 60\n")
        pgrep.chmod(0o755)
        old = self.seed_old()
        self.assertIn("timed out", self.run_install(ok=False, env={"PATH": str(path)}).stderr)
        self.assert_old(old)

    def test_obs_started_during_staging_blocks_exchange(self):
        path = self.root / "racing-tools"
        path.mkdir()
        pgrep = path / "pgrep"
        count = self.root / "process-check-count"
        pgrep.write_text(f"#!/bin/sh\nn=0\n[ ! -f '{count}' ] || n=$(cat '{count}')\nn=$((n + 1))\necho $n > '{count}'\n[ $n -le 4 ] && exit 1\nexit 0\n")
        pgrep.chmod(0o755)
        old = self.seed_old()
        for remove in (False, True):
            count.unlink(missing_ok=True)
            self.assertIn("running", self.run_install(uninstall=remove, ok=False,
                          env={"PATH": str(path) + os.pathsep + os.environ["PATH"]}).stderr)
            self.assert_old(old)

    @unittest.skipUnless(PLATFORM == "linux", "Linux cp failure path")
    def test_partial_cp_error_or_false_success_preserves_old(self):
        path = self.root / "copy-tools"
        path.mkdir()
        cp = path / "cp"
        old = self.seed_old()
        for status in (0, 1):
            cp.write_text(f'#!/bin/sh\nmkdir -p "$3"\nprintf partial > "$3/partial"\nexit {status}\n')
            cp.chmod(0o755)
            for remove in (False, True):
                self.run_install(uninstall=remove, ok=False,
                                 env={"PATH": str(path) + os.pathsep + os.environ["PATH"]})
                self.assert_old(old)

    def test_corrupt_payload_and_manifest_block_before_copy(self):
        old = self.seed_old()
        notice = self.package / NAME / ("Contents/Resources/README.txt" if PLATFORM == "macos" else "README.txt")
        notice.write_text("damaged download")
        self.run_install(ok=False)
        self.assert_old(old)
        (self.package / "payload-manifest.txt").write_text("chimera-obs-v1 " + PLATFORM + "\n")
        self.run_install(ok=False)
        self.assert_old(old)

    @unittest.skipUnless(PLATFORM == "macos", "macOS signing/quarantine")
    def test_signature_and_quarantine(self):
        binary = self.package / NAME / "Contents/MacOS/chimera-obs"
        subprocess.run(["codesign", "--remove-signature", str(binary)], check=True)
        manifest(self.package)
        old = self.seed_old()
        self.run_install(ok=False)
        self.assert_old(old)
        shutil.rmtree(self.package / NAME)
        shutil.copytree(self.fixture / NAME, self.package / NAME)
        manifest(self.package)
        helper = self.package / "chimera-obs-installer"
        for path in (helper, self.package / NAME, self.protected[1]):
            subprocess.run(["xattr", "-w", "com.apple.quarantine", "0081;test;Chimera;", str(path)], check=True)
        self.run_install(shell=True)
        self.assertNotIn(b"com.apple.quarantine", subprocess.check_output(["xattr", str(helper)]))
        self.assertNotIn(b"com.apple.quarantine", subprocess.check_output(["xattr", str(self.target)]))
        self.assertIn(b"com.apple.quarantine", subprocess.check_output(["xattr", str(self.protected[1])]))

    def test_links_special_files_and_owned_directories_refused(self):
        old = self.seed_old()
        user_link = self.target / "user-link"
        user_link.symlink_to(self.protected[1])
        self.run_install(ok=False)
        self.run_install(uninstall=True, ok=False)
        user_link.unlink()
        self.assert_old(old)
        fifo = self.target / "user-pipe"
        os.mkfifo(fifo)
        self.run_install(ok=False)
        fifo.unlink()
        self.assert_old(old)
        owned = self.target / ("Contents/Resources/README.txt" if PLATFORM == "macos" else "README.txt")
        owned.unlink()
        owned.mkdir()
        write(owned / "user-document")
        changed = payload.inventory(self.target)
        self.run_install(uninstall=True, ok=False)
        self.assert_old(changed)

    def test_symlink_target_and_install_parent_refused(self):
        self.target.symlink_to(self.protected[2], target_is_directory=True)
        self.run_install(ok=False)
        self.run_install(uninstall=True, ok=False)
        self.assertTrue(self.target.is_symlink())
        self.target.unlink()
        relocated = self.root / "real-plugins"
        self.plugins.rename(relocated)
        self.plugins.symlink_to(relocated, target_is_directory=True)
        self.run_install(ok=False)
        self.assertFalse((relocated / NAME).exists())

    def test_copy_integrity_exchange_and_checkpoint_failures_restore(self):
        failures = [{"CHIMERA_TEST_PARTIAL_COPY": "1"}, {"CHIMERA_TEST_CORRUPT_COPY": "1"},
                    {"CHIMERA_TEST_EXCHANGE_FAIL": "1"}] + [
                        {"CHIMERA_TEST_FAIL": phase} for phase in ("before-copy", "after-copy", "before-swap",
                        "after-prepare", "prepare-sync", "after-swap", "before-commit", "commit-sync")]
        old = self.seed_old()
        for fault in failures:
            for remove in (False, True):
                with self.subTest(fault=fault, remove=remove):
                    self.run_install(uninstall=remove, fault=fault, ok=False)
                    self.assert_old(old)

    def test_first_install_failures_leave_no_partial_target(self):
        for phase in ("before-copy", "after-copy", "after-prepare", "after-swap", "commit-sync"):
            self.run_install(fault={"CHIMERA_TEST_FAIL": phase}, ok=False)
            self.assert_old(None)

    def paused(self, phase, uninstall=False):
        marker = self.root / "pause-marker"
        marker.unlink(missing_ok=True)
        env = dict(self.env, CHIMERA_TEST_PAUSE=phase, CHIMERA_TEST_MARKER=str(marker))
        process = subprocess.Popen([str(self.package / "fault-installer")] + (["--uninstall"] if uninstall else []),
                                   env=env, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        self.addCleanup(lambda: process.poll() is None and (process.kill(), process.communicate(timeout=10)))
        deadline = time.monotonic() + 20
        while not marker.exists() and process.poll() is None and time.monotonic() < deadline:
            time.sleep(.01)
        self.assertTrue(marker.exists(), "Installer did not reach pause point")
        return process

    def test_signals_restore_previous_install(self):
        old = self.seed_old()
        for sig in (signal.SIGINT, signal.SIGTERM, signal.SIGHUP):
            for phase in ("after-copy", "after-swap"):
                for remove in (False, True):
                    with self.subTest(signal=sig, phase=phase, remove=remove):
                        process = self.paused(phase, remove)
                        process.send_signal(sig)
                        process.communicate(timeout=15)
                        self.assertNotEqual(process.returncode, 0)
                        self.assert_old(old)

    def test_kill_recovery_native_and_legacy_journals(self):
        for legacy in (False, True):
            for initial in (False, True):
                for remove in ((False, True) if not initial else (False,)):
                    if self.target.exists():
                        shutil.rmtree(self.target)
                    old = None if initial else self.seed_old()
                    process = self.paused("after-swap", remove)
                    process.kill()
                    process.communicate(timeout=10)
                    if legacy:
                        path = self.work / "journal.txt"
                        phase, od, oi, nd, ni, uninstall = path.read_text().split()
                        (self.work / "journal.json").write_text(json.dumps({"phase": phase,
                            "old": [int(od), int(oi)] if int(oi) else None,
                            "new": [int(nd), int(ni)], "uninstall": uninstall == "1"}))
                        path.unlink()
                    self.run_install(fault={"CHIMERA_TEST_FAIL": "after-recovery"}, ok=False)
                    self.assert_old(old)

    def test_failed_rollback_retains_backup_then_recovers(self):
        old = self.seed_old()
        result = self.run_install(fault={"CHIMERA_TEST_FAIL": "after-swap", "CHIMERA_TEST_ROLLBACK_FAIL": "1"}, ok=False)
        self.assertIn("retained", result.stderr)
        self.assertEqual(payload.inventory(self.work / NAME), old)
        self.run_install(fault={"CHIMERA_TEST_FAIL": "after-recovery"}, ok=False)
        self.assert_old(old)

    def test_kill_before_exchange_cleans_staging(self):
        old = self.seed_old()
        for phase in ("before-copy", "before-swap", "after-prepare"):
            process = self.paused(phase)
            process.kill()
            process.communicate(timeout=10)
            self.run_install(fault={"CHIMERA_TEST_FAIL": "after-recovery"}, ok=False)
            self.assert_old(old)

    def test_ambiguous_recovery_retains_backup(self):
        old = self.seed_old()
        self.run_install(fault={"CHIMERA_TEST_FAIL": "after-swap", "CHIMERA_TEST_ROLLBACK_FAIL": "1"}, ok=False)
        installed = self.root / "externally-moved-module"
        self.target.rename(installed)
        self.target.mkdir()
        write(self.target / "external-user-file")
        self.run_install(ok=False)
        self.assertEqual(payload.inventory(self.work / NAME), old)
        self.assertTrue((self.target / "external-user-file").exists())

    def test_concurrent_installer_blocked(self):
        old = self.seed_old()
        process = self.paused("before-copy")
        self.assertIn("in progress", self.run_install(ok=False).stderr)
        self.assertIn("in progress", self.run_install(uninstall=True, ok=False).stderr)
        process.send_signal(signal.SIGTERM)
        process.communicate(timeout=10)
        self.assert_old(old)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--package", type=Path)
    args, rest = parser.parse_known_args()
    PACKAGE = args.package.resolve() if args.package else None
    unittest.main(argv=[sys.argv[0]] + rest, verbosity=2)
