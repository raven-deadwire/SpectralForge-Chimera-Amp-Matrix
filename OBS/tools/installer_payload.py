# SPDX-License-Identifier: GPL-2.0-or-later
"""Build/test-only payload helpers. Never distributed or invoked by install.sh."""
import hashlib
import os
from pathlib import Path
import plistlib
import stat
import subprocess

LINUX_FILES = frozenset(("bin/64bit/chimera-obs.so", "data/locale/en-US.ini",
                         "data/locale/ko-KR.ini", "README.txt", "LICENSE.txt", "SOURCE.txt"))
MAC_FILES = frozenset(("Contents/MacOS/chimera-obs", "Contents/Info.plist",
                       "Contents/_CodeSignature/CodeResources",
                       "Contents/Resources/locale/en-US.ini", "Contents/Resources/locale/ko-KR.ini",
                       "Contents/Resources/README.txt", "Contents/Resources/LICENSE.txt",
                       "Contents/Resources/SOURCE.txt"))

# Oldest macOS the OBS 31 release and the Chimera product support; without it
# the compiler targets the build machine's own macOS version.
MACOS_DEPLOYMENT_TARGET = "12.0"


def helper_compile_command(source, output, platform):
    """The native install helper runs on users' machines with only the OS libraries:
    one Universal binary on macOS, the C++ runtime linked in on Linux."""
    command = ["c++", "-std=c++17", "-O2", "-Wall", "-Wextra", "-Werror"]
    if platform == "macos":
        command += ["-arch", "arm64", "-arch", "x86_64", "-mmacosx-version-min=" + MACOS_DEPLOYMENT_TARGET]
    else:
        command += ["-static-libstdc++", "-static-libgcc"]
    return command + [str(source), "-o", str(output)]


class InstallError(RuntimeError):
    pass


def owned_files(platform):
    return MAC_FILES if platform == "macos" else LINUX_FILES


def module_name(platform):
    return "chimera-obs.plugin" if platform == "macos" else "chimera-obs"


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


