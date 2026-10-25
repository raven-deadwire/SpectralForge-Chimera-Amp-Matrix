#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Transactional per-user installer. Python 3.9+, standard library only.

No environment-variable test bypasses. Tests inject failures in isolated copies
or patch the imported module. Existing installations are never copied over or
removed before a verified replacement is ready on the same filesystem.
"""
import argparse
import contextlib
import ctypes
import fcntl
import hashlib
import json
import os
from pathlib import Path
import plistlib
import re
import shutil
import signal
import stat
import subprocess
import sys

LINUX_FILES = frozenset(("bin/64bit/chimera-obs.so", "data/locale/en-US.ini",
                         "data/locale/ko-KR.ini", "README.txt", "LICENSE.txt", "SOURCE.txt"))
MAC_FILES = frozenset(("Contents/MacOS/chimera-obs", "Contents/Info.plist",
                       "Contents/_CodeSignature/CodeResources",
                       "Contents/Resources/locale/en-US.ini", "Contents/Resources/locale/ko-KR.ini",
                       "Contents/Resources/README.txt", "Contents/Resources/LICENSE.txt",
                       "Contents/Resources/SOURCE.txt"))
SIGNALS = (signal.SIGINT, signal.SIGTERM, signal.SIGHUP)


class InstallError(RuntimeError):
    pass


def owned_files(platform):
    return MAC_FILES if platform == "macos" else LINUX_FILES


def module_name(platform):
    return "chimera-obs.plugin" if platform == "macos" else "chimera-obs"


def require_obs_closed(platform):
    """pgrep 1 alone proves no match; missing tool, timeout and errors block."""
    names = ("OBS", "obs") if platform == "macos" else ("obs", "obs-studio")
    for name in names:
        try:
            result = subprocess.run(["pgrep", "-x", name], stdout=subprocess.DEVNULL,
                                    stderr=subprocess.PIPE, timeout=10, check=False)
        except (OSError, subprocess.TimeoutExpired) as exc:
            raise InstallError("Cannot determine OBS process state; no installation changes authorized.") from exc
        if result.returncode == 0:
            raise InstallError("OBS Studio is running. Close OBS and keep it closed during installation/removal.")
        if result.returncode != 1:
            raise InstallError("Cannot determine OBS process state (pgrep failed); installation/removal blocked.")


def inventory(folder):
    """Reject links/special files rather than traversing outside our module."""
    if folder.is_symlink() or not folder.is_dir():
        raise InstallError("Expected a real directory: " + str(folder))
    result = {}
    for base, dirs, files in os.walk(folder, followlinks=False):
        for name in dirs + files:
            path = Path(base) / name
            mode = path.lstat().st_mode
            if stat.S_ISLNK(mode) or not (stat.S_ISDIR(mode) or stat.S_ISREG(mode)):
                raise InstallError("Links and special files are not supported: " + str(path))
            if stat.S_ISREG(mode):
                result[path.relative_to(folder).as_posix()] = {
                    "sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
                    "mode": stat.S_IMODE(mode),
                }
    return result


def read_manifest(package, platform):
    manifest = json.loads((package / "payload-manifest.json").read_text(encoding="utf-8"))
    if manifest.get("platform") != platform or manifest.get("schema") != 1:
        raise InstallError("Invalid package manifest identity")
    files = manifest.get("files", {})
    if set(files) != owned_files(platform):
        raise InstallError("Package inventory does not match the installer-owned file list")
    for entry in files.values():
        if (not isinstance(entry, dict) or not re.fullmatch(r"[0-9a-f]{64}", entry.get("sha256", ""))
                or type(entry.get("mode")) is not int or entry["mode"] not in (0o644, 0o755)):
            raise InstallError("Invalid package file hash or permissions")
    return files


def verify_payload(folder, manifest, platform, allow_extra=False):
    actual = inventory(folder)
    if (not allow_extra and set(actual) != set(manifest)) or any(actual.get(k) != v for k, v in manifest.items()):
        raise InstallError("Payload integrity check failed: " + str(folder))
    binary = "Contents/MacOS/chimera-obs" if platform == "macos" else "bin/64bit/chimera-obs.so"
    if not actual[binary]["mode"] & 0o111:
        raise InstallError("Module binary is not executable")
    if platform == "macos":
        with (folder / "Contents/Info.plist").open("rb") as stream:
            info = plistlib.load(stream)
        if info.get("CFBundleExecutable") != "chimera-obs" or info.get("CFBundleIdentifier") != "com.ravenforge.chimera-obs":
            raise InstallError("Unexpected macOS bundle identity")
        subprocess.run(["codesign", "--verify", "--deep", "--strict", str(folder)], check=True, timeout=30)


def safe_directories(path):
    # The home/XDG root is resolved by the caller; do not follow links below it.
    for part in reversed((path,) + tuple(path.parents)):
        if part.is_symlink():
            raise InstallError("Refusing a symlink in the installation path: " + str(part))
        if part.exists() and not part.is_dir():
            raise InstallError("Expected an installation directory: " + str(part))
    path.mkdir(parents=True, exist_ok=True)


def sync_directory(path):
    fd = os.open(path, os.O_RDONLY | os.O_DIRECTORY)
    try:
        os.fsync(fd)
    finally:
        os.close(fd)


def sync_tree(path):
    for base, dirs, files in os.walk(path, topdown=False):
        for name in files:
            with (Path(base) / name).open("rb") as stream:
                os.fsync(stream.fileno())
        sync_directory(Path(base))


def identity(path):
    if not path.exists():
        return None
    if path.is_symlink() or not path.is_dir():
        raise InstallError("Unexpected transaction path: " + str(path))
    st = path.stat()
    return [st.st_dev, st.st_ino]


def exchange(first, second):
    """Atomic nonempty-directory exchange. Never fall back to delete + copy.

    Darwin renamex_np(RENAME_SWAP); Linux renameat2(RENAME_EXCHANGE).
    Unsupported filesystems/libcs fail with the installed target untouched.
    """
    libc = ctypes.CDLL(None, use_errno=True)
    if sys.platform == "darwin":
        fn = libc.renamex_np
        fn.argtypes = (ctypes.c_char_p, ctypes.c_char_p, ctypes.c_uint)
        args = (os.fsencode(first), os.fsencode(second), 2)
    else:
        fn = libc.renameat2
        fn.argtypes = (ctypes.c_int, ctypes.c_char_p, ctypes.c_int, ctypes.c_char_p, ctypes.c_uint)
        args = (-100, os.fsencode(first), -100, os.fsencode(second), 2)
    fn.restype = ctypes.c_int
    if fn(*args) != 0:
        error = ctypes.get_errno()
        raise OSError(error, "Atomic directory exchange failed: " + os.strerror(error))


def write_journal(work, value):
    temporary = work / "journal.tmp"
    with temporary.open("w", encoding="utf-8") as stream:
        json.dump(value, stream)
        stream.flush()
        os.fsync(stream.fileno())
    os.replace(temporary, work / "journal.json")
    sync_directory(work)


def recover(work, target, force_rollback=False):
    """Recover even if a kill occurred between exchange and journal update.

    Inodes identify which side of the swap persisted; do not infer success
    merely from the presence of a directory or discard an ambiguous backup.
    """
    if not work.exists():
        return
    if work.is_symlink() or not work.is_dir():
        raise InstallError("Unsafe transaction directory: " + str(work))
    journal_path = work / "journal.json"
    candidate = work / target.name
    if journal_path.exists():
        if journal_path.is_symlink():
            raise InstallError("Unsafe transaction journal")
        journal = json.loads(journal_path.read_text(encoding="utf-8"))
        old, new = journal["old"], journal["new"]
        current, staged = identity(target), identity(candidate)
        if journal["phase"] == "prepared" or force_rollback:
            if current == new:
                if old is None and staged is None:
                    os.rename(target, candidate)
                elif staged == old and old is not None:
                    exchange(candidate, target)
                else:
                    raise InstallError("Ambiguous backup; retained for recovery: " + str(work))
                sync_directory(target.parent)
                sync_directory(work)
            elif current != old or staged != new:
                raise InstallError("Unexpected installation change; recovery files retained: " + str(work))
        elif journal["phase"] != "committed":
            raise InstallError("Unknown transaction state; files retained: " + str(work))
        elif current != new and not (current is None and journal.get("uninstall")):
            raise InstallError("Committed target changed; backup retained: " + str(work))
    # No journal means staging never reached the exchange. A committed journal
    # means the verified target is authoritative and candidate is the old copy.
    shutil.rmtree(work)
    sync_directory(target.parent)


@contextlib.contextmanager
def locked(plugins):
    # Keep the empty lock file after release: unlinking it allows two inodes
    # (and thus two simultaneous locks). The kernel releases flock on SIGKILL.
    fd = os.open(plugins / ".chimera-obs-install.lock", os.O_CREAT | os.O_RDWR | os.O_NOFOLLOW, 0o600)
    try:
        if not stat.S_ISREG(os.fstat(fd).st_mode):
            raise InstallError("Invalid installer lock")
        try:
            fcntl.flock(fd, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except BlockingIOError as exc:
            raise InstallError("Another Chimera OBS install/removal is in progress.") from exc
        yield
    finally:
        os.close(fd)


def copy_tree(source, destination, platform):
    if platform == "macos":
        subprocess.run(["ditto", str(source), str(destination)], check=True)
    else:
        shutil.copytree(source, destination, copy_function=shutil.copy2)


def clear_quarantine(folder):
    # Operate on the staged copy only. Unexpected xattr errors abort safely.
    for base, dirs, files in os.walk(folder):
        for path in [Path(base)] + [Path(base) / name for name in files]:
            if "com.apple.quarantine" in os.listxattr(path):
                os.removexattr(path, "com.apple.quarantine")


def remove_owned(folder, platform):
    for name in owned_files(platform):
        path = folder / name
        if path.is_dir():
            raise InstallError("An owned file path is a user directory; preserving it: " + str(path))
        if path.exists():
            path.unlink()
    directories = {parent for name in owned_files(platform)
                   for parent in (folder / name).parents if parent != folder and folder in parent.parents}
    for path in sorted(directories, key=lambda p: len(p.parts), reverse=True):
        if path.is_dir() and not os.listdir(path):
            path.rmdir()


def install(package, plugins, platform, uninstall=False):
    if os.geteuid() == 0:
        raise InstallError("Run this as the user who runs OBS, not as root.")
    require_obs_closed(platform)
    target = plugins / module_name(platform)
    if target.is_symlink():
        raise InstallError("Refusing a symlink installation target")
    source = package / module_name(platform)
    manifest = None
    if not uninstall:
        manifest = read_manifest(package, platform)
        verify_payload(source, manifest, platform)
    safe_directories(plugins)
    with locked(plugins):
        work = plugins / ".chimera-obs-transaction"
        require_obs_closed(platform)
        recover(work, target)
        old_inventory = inventory(target) if target.exists() else None
        if uninstall and old_inventory is None:
            return
        # Signed macOS bundles cannot be merged with unsealed user files.
        # Refuse upgrades with such additions; uninstall still preserves them.
        if platform == "macos" and not uninstall and old_inventory is not None:
            if set(old_inventory) - owned_files(platform):
                raise InstallError("User files inside the macOS bundle must be moved out before updating; nothing removed.")
        candidate = work / target.name
        work.mkdir(mode=0o700)
        try:
            if uninstall:
                copy_tree(target, candidate, platform)
                remove_owned(candidate, platform)
            elif platform == "linux" and old_inventory is not None:
                copy_tree(target, candidate, platform)
                remove_owned(candidate, platform)
                for name in sorted(manifest):
                    path = candidate / name
                    path.parent.mkdir(parents=True, exist_ok=True)
                    shutil.copy2(source / name, path)
            else:
                copy_tree(source, candidate, platform)
            if not uninstall:
                if platform == "macos":
                    clear_quarantine(candidate)
                verify_payload(candidate, manifest, platform, allow_extra=(platform == "linux"))
            # Unknown files, including user notes/settings, must survive exactly.
            expected = {k: v for k, v in (old_inventory or {}).items() if k not in owned_files(platform)}
            if not uninstall:
                expected.update(manifest)
            if inventory(candidate) != expected:
                raise InstallError("Staged installation does not preserve the expected file inventory")
            sync_tree(candidate)
            require_obs_closed(platform)
            if (inventory(target) if target.exists() else None) != old_inventory:
                raise InstallError("Installation changed during staging; retry with OBS closed")
            journal = {"phase": "prepared", "old": identity(target), "new": identity(candidate),
                       "uninstall": uninstall}
            write_journal(work, journal)
            sync_directory(plugins)
            if journal["old"] is None:
                os.rename(candidate, target)
            else:
                exchange(candidate, target)
            sync_directory(plugins)
            sync_directory(work)
            if inventory(target) != expected:
                raise InstallError("Installed payload differs from the verified candidate")
            if not uninstall:
                verify_payload(target, manifest, platform, allow_extra=(platform == "linux"))
            journal["phase"] = "committed"
            write_journal(work, journal)
        except BaseException:
            # Do not let a second catchable signal interrupt rollback. If disk
            # failure prevents rollback, retain the old copy and journal.
            previous_mask = signal.pthread_sigmask(signal.SIG_BLOCK, SIGNALS)
            try:
                recover(work, target, force_rollback=True)
            except BaseException as exc:
                raise InstallError("Recovery could not finish. Previous copy/journal retained at " + str(work)) from exc
            finally:
                signal.pthread_sigmask(signal.SIG_SETMASK, previous_mask)
            raise
        # Cleanup failure after durable commit is not an installation failure.
        # Keep the journal so the next invocation can finish cleanup safely.
        try:
            recover(work, target)
            if uninstall and not os.listdir(target):
                target.rmdir()
                sync_directory(plugins)
        except OSError as exc:
            print("Installation committed; cleanup will be retried next run: " + str(exc), file=sys.stderr)


def interrupted(signum, frame):
    raise InstallError("Installation interrupted by signal " + str(signum))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--platform", choices=("macos", "linux"), required=True)
    parser.add_argument("--uninstall", action="store_true")
    args = parser.parse_args()
    if sys.platform != ("darwin" if args.platform == "macos" else "linux"):
        parser.error("This package is for " + args.platform)
    home = Path.home().resolve()
    if args.platform == "macos":
        plugins = home / "Library/Application Support/obs-studio/plugins"
    else:
        config = Path(os.environ.get("XDG_CONFIG_HOME", str(home / ".config")))
        if not config.is_absolute():
            parser.error("XDG_CONFIG_HOME must be absolute")
        plugins = config / "obs-studio/plugins"
    for signum in SIGNALS:
        signal.signal(signum, interrupted)
    try:
        install(Path(__file__).resolve().parent, plugins, args.platform, args.uninstall)
    except (OSError, ValueError, KeyError, InstallError, subprocess.SubprocessError, AttributeError) as exc:
        print("Chimera OBS installation/removal failed: " + str(exc), file=sys.stderr)
        return 1
    print(("Removed owned Chimera OBS files from " if args.uninstall else "Installed Chimera OBS filter to ") + str(plugins))
    return 0


if __name__ == "__main__":
    sys.exit(main())
