#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Stage the macOS or Linux OBS filter package (plan stage F).

linux: <output>/chimera-obs/{bin/64bit/chimera-obs.so,data,README.txt,LICENSE.txt,SOURCE.txt},
       install.sh, INSTALL.txt  (install.sh copies chimera-obs/ to ~/.config/obs-studio/plugins)
macos: <output>/chimera-obs.plugin (README, LICENSE and SOURCE in Contents/Resources),
       install.sh, INSTALL.txt, README.txt. With --build-x86_64 the bundle is Universal
       (Apple silicon and Intel); every binary must run on macOS 12.0.
"""
import argparse
import importlib.util
from pathlib import Path
import re
import shutil
import subprocess

OBS = Path(__file__).resolve().parents[1]
REPOSITORY = "raven-deadwire/SpectralForge-Chimera-Amp-Matrix"
spec = importlib.util.spec_from_file_location("installer_payload", OBS / "tools/installer_payload.py")
installer = importlib.util.module_from_spec(spec)
spec.loader.exec_module(installer)


def module_version() -> str:
    match = re.search(r"project\(chimera-obs VERSION (\d+\.\d+\.\d+)", (OBS / "CMakeLists.txt").read_text(encoding="utf-8"))
    if not match:
        raise SystemExit("OBS/CMakeLists.txt does not declare the module version")
    return match.group(1)


def macos_binary_check(binary: Path, architectures: set) -> None:
    """Every slice present and none built for a macOS newer than the supported one."""
    found = set(subprocess.run(["lipo", "-archs", str(binary)], check=True, capture_output=True, text=True).stdout.split())
    if found != architectures:
        raise SystemExit(f"{binary.name}: architectures {sorted(found)}, expected {sorted(architectures)}")
    oldest = tuple(int(part) for part in installer.MACOS_DEPLOYMENT_TARGET.split("."))
    for arch in sorted(found):
        build = subprocess.run(["vtool", "-arch", arch, "-show-build", str(binary)],
                               check=True, capture_output=True, text=True).stdout
        match = re.search(r"\bminos (\d+)\.(\d+)", build)
        if not match or (int(match.group(1)), int(match.group(2))) > oldest:
            raise SystemExit(f"{binary.name} ({arch}) needs macOS {match.group(0) if match else '?'}, "
                             f"not {installer.MACOS_DEPLOYMENT_TARGET}")


def notices(folder: Path, sha: str, built_for: str) -> None:
    shutil.copyfile(OBS / "LICENSE", folder / "LICENSE.txt")
    shutil.copyfile(OBS / "packaging/README.txt", folder / "README.txt")
    (folder / "SOURCE.txt").write_text(
        "SpectralForge Chimera OBS filter (chimera-obs)\n"
        f"Module version: {module_version()}\n"
        f"Built and tested for: {built_for}\n"
        "License: GNU General Public License, version 2 or later (LICENSE.txt).\n"
        f"Corresponding source: https://github.com/{REPOSITORY}/tree/{sha}/OBS\n"
        "It loads the separately installed SpectralForge Chimera VST3 and contains none of\n"
        "the product's DSP, models, impulse responses or artwork.\n", encoding="utf-8")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("platform", choices=("linux", "macos"))
    parser.add_argument("--build", type=Path, required=True, help="CMake build directory of OBS/")
    parser.add_argument("--output", type=Path, required=True, help="new, empty package directory")
    parser.add_argument("--source-sha", required=True)
    parser.add_argument("--build-x86_64", type=Path, help="macos: x86_64 build directory to join into a Universal bundle")
    parser.add_argument("--built-for", required=True, help='for example "OBS 31.1.2 (macOS Apple)"')
    args = parser.parse_args()
    if args.build_x86_64 and args.platform != "macos":
        raise SystemExit("--build-x86_64 is for the macOS Universal bundle")
    if not re.fullmatch(r"[0-9a-f]{40}", args.source_sha):
        raise SystemExit("--source-sha must be a full commit SHA")
    args.output.mkdir(parents=True, exist_ok=False)
    if args.platform == "linux":
        module = args.output / "chimera-obs"
        (module / "bin/64bit").mkdir(parents=True)
        shutil.copy2(args.build / "chimera-obs.so", module / "bin/64bit/chimera-obs.so")
        shutil.copytree(args.build / "data", module / "data")
        notices(module, args.source_sha, args.built_for)
        shutil.copy2(OBS / "packaging/install-linux.sh", args.output / "install.sh")
        shutil.copyfile(OBS / "packaging/INSTALL-Linux.txt", args.output / "INSTALL.txt")
    else:
        bundle = args.output / "chimera-obs.plugin"
        # ditto keeps the bundle's signature, permissions and extended attributes.
        subprocess.run(["ditto", str(args.build / "chimera-obs.plugin"), str(bundle)], check=True)
        architectures = {"arm64"}
        if args.build_x86_64:
            binary = bundle / "Contents/MacOS/chimera-obs"
            universal = args.output / "chimera-obs-universal"
            subprocess.run(["lipo", "-create", str(binary),
                            str(args.build_x86_64 / "chimera-obs.plugin/Contents/MacOS/chimera-obs"),
                            "-output", str(universal)], check=True)
            universal.chmod(0o755)
            universal.replace(binary)
            architectures.add("x86_64")
        macos_binary_check(bundle / "Contents/MacOS/chimera-obs", architectures)
        notices(bundle / "Contents/Resources", args.source_sha, args.built_for)
        # Resources changed after linking: re-seal the bundle with an ad-hoc signature.
        subprocess.run(["codesign", "--force", "--sign", "-", "--timestamp=none", str(bundle)], check=True)
        subprocess.run(["codesign", "--verify", "--strict", str(bundle)], check=True)
        shutil.copy2(OBS / "packaging/install-macos.sh", args.output / "install.sh")
        shutil.copyfile(OBS / "packaging/INSTALL-macOS.txt", args.output / "INSTALL.txt")
        shutil.copyfile(OBS / "packaging/README.txt", args.output / "README.txt")
    payload = args.output / installer.module_name(args.platform)
    files = installer.inventory(payload)
    if set(files) != installer.owned_files(args.platform):
        raise SystemExit("Staged payload differs from installer-owned inventory")
    (args.output / "payload-manifest.txt").write_text(
        "chimera-obs-v1 " + args.platform + "\n" + "".join(
            f"{entry['sha256']} {entry['mode']:03o} {name}\n" for name, entry in sorted(files.items())), encoding="utf-8")
    installer.verify_payload(payload, files, args.platform)
    # Compile on the build runner, then ship the native executable. Python is
    # a developer/CI tool only; users never compile or install a runtime.
    executable = args.output / "chimera-obs-installer"
    subprocess.run(installer.helper_compile_command(OBS / "packaging/install-module.cpp", executable, args.platform), check=True)
    if args.platform == "macos":
        subprocess.run(["codesign", "--force", "--sign", "-", "--timestamp=none", str(executable)], check=True)
        subprocess.run(["codesign", "--verify", "--strict", str(executable)], check=True)
        macos_binary_check(executable, {"arm64", "x86_64"})
    print(f"staged {args.platform} OBS filter {module_version()} in {args.output}")


if __name__ == "__main__":
    main()
