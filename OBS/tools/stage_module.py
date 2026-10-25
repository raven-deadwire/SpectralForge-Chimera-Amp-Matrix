#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Stage the macOS or Linux OBS filter package (plan stage F).

linux: <output>/chimera-obs/{bin/64bit/chimera-obs.so,data,README.txt,LICENSE.txt,SOURCE.txt},
       install.sh, INSTALL.txt  (install.sh copies chimera-obs/ to ~/.config/obs-studio/plugins)
macos: <output>/chimera-obs.plugin (README, LICENSE and SOURCE in Contents/Resources),
       install.sh, INSTALL.txt, README.txt
"""
import argparse
import importlib.util
import json
from pathlib import Path
import re
import shutil
import subprocess

OBS = Path(__file__).resolve().parents[1]
REPOSITORY = "raven-deadwire/SpectralForge-Chimera-Amp-Matrix"
spec = importlib.util.spec_from_file_location("install_module", OBS / "packaging/install_module.py")
installer = importlib.util.module_from_spec(spec)
spec.loader.exec_module(installer)


def module_version() -> str:
    match = re.search(r"project\(chimera-obs VERSION (\d+\.\d+\.\d+)", (OBS / "CMakeLists.txt").read_text(encoding="utf-8"))
    if not match:
        raise SystemExit("OBS/CMakeLists.txt does not declare the module version")
    return match.group(1)


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
    parser.add_argument("--built-for", required=True, help='for example "OBS 31.1.2 (macOS Apple)"')
    args = parser.parse_args()
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
    (args.output / "payload-manifest.json").write_text(
        json.dumps({"schema": 1, "platform": args.platform, "files": files}, indent=2) + "\n", encoding="utf-8")
    installer.verify_payload(payload, files, args.platform)
    shutil.copy2(OBS / "packaging/install_module.py", args.output / "install_module.py")
    print(f"staged {args.platform} OBS filter {module_version()} in {args.output}")


if __name__ == "__main__":
    main()
