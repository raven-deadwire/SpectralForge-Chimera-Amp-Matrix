#!/usr/bin/env python3
"""Stage verified binaries and build OS-native, unsigned beta installers.

Run only after CTest passes. Does not publish, sign, fetch or install anything.
"""
from __future__ import annotations
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import shutil
import subprocess
import tarfile
import tempfile
import zipfile

ROOT = Path(__file__).resolve().parents[1]
VERSION = "1.0.0-beta.1"
PRODUCT = "SpectralForge Chimera"
PREFIX = f"SpectralForge-Chimera-{VERSION}"
REPO = "raven-deadwire/SpectralForge-Chimera-Amp-Matrix"


def run(*args: str, **kwargs) -> str:
    result = subprocess.run(args, text=True, capture_output=True, **kwargs)
    if result.stdout:
        print(result.stdout.rstrip())
    if result.returncode:
        raise RuntimeError(f"Command failed ({result.returncode}): {args}\n{result.stderr}")
    return result.stdout


def copy(source: Path, destination: Path) -> None:
    if not source.exists():
        raise RuntimeError(f"Missing required release input: {source}")
    destination.parent.mkdir(parents=True, exist_ok=True)
    if source.is_dir():
        shutil.copytree(source, destination, symlinks=True)
    else:
        shutil.copy2(source, destination)


def sha256(path: Path) -> str:
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def checksum(path: Path) -> None:
    path.with_name(path.name + ".sha256.txt").write_text(f"{sha256(path)}  {path.name}\n", encoding="utf-8")


def documents(destination: Path, build: Path) -> None:
    destination.mkdir(parents=True, exist_ok=True)
    copy(ROOT / "COPYRIGHT.txt", destination / "COPYRIGHT.txt")
    for name in ("MANUAL.html", "OPEN_BETA_RELEASE_NOTES.md", "THIRD_PARTY_NOTICES.md",
                 "AMP_VALIDATION.md", "AMP_VOICES_OPEN_BETA.md", "OPEN_BETA_NAM_VALIDATION.md", "PRESETS.md", "EXTERNAL_BASS_IRS.md", "UPDATES.md", "MODELS_AND_REFERENCE.md", "FX_AND_IR_DESIGN.md",
                 "WINDOWS_INSTALL.txt", "INSTALLATION.md", "NAM_REFERENCE_RESULTS.md"):
        copy(ROOT / "docs" / name, destination / name)
    copy(build / "Testing/Temporary/LastTest.log", destination / "Verification.txt")
    copy(ROOT / "docs/reference", destination / "reference")


def verify_stage(stage: Path, required: list[str]) -> None:
    for name in required:
        path = stage / name
        if not path.is_file() or not path.stat().st_size:
            raise RuntimeError(f"Empty or missing package file: {name}")
    # Personal IR/NAM files must never leak into public release payloads.
    for path in stage.rglob("*"):
        if path.suffix.lower() == ".nam" or "Chimera-Personal-IRs" in path.parts:
            raise RuntimeError(f"Restricted capture payload found: {path}")
        if path.suffix.lower() in (".wav", ".aif", ".aiff") and "reference-audio" not in path.parts:
            raise RuntimeError(f"Unexpected external audio payload: {path}")
    inventory = [{"path": p.relative_to(stage).as_posix(), "bytes": p.stat().st_size, "sha256": sha256(p)}
                 for p in sorted(stage.rglob("*")) if p.is_file() and not p.is_symlink()]
    (stage / "payload-manifest.json").write_text(json.dumps({"version": VERSION, "files": inventory}, indent=2) + "\n", encoding="utf-8")


def windows(build: Path, dist: Path) -> None:
    stage = dist / f"{PREFIX}-win64"
    stage.mkdir(parents=True, exist_ok=False)
    artefacts = build / "ChimeraAmpMatrix_artefacts/Release"
    copy(artefacts / "VST3", stage / "VST3")
    copy(artefacts / "Standalone", stage / "Standalone")
    documents(stage, build)
    # Existing installer validation exercises reference tools and their opt-in component.
    for name in ("GRAPHICS_DSP_UPDATE.md", "WINDOWS_SIGNING.md", "ARTWORK_PROMPTS.json"):
        copy(ROOT / "docs" / name, stage / name)
    copy(ROOT / "Tools/validate_nam.py", stage / "ReferenceTools/validate_nam.py")
    copy(build / "ChimeraRender_artefacts/Release/ChimeraRender.exe", stage / "ReferenceTools/ChimeraRender.exe")
    copy(build / "reference-audio", stage / "reference-audio")
    (stage / "README.txt").write_text(
        f"{PRODUCT} — Open Beta 1.0 ({VERSION})\n\n"
        "Close Chimera and your DAW before installing or updating.\n"
        "Run the Setup installer for standard VST3 registration and the standalone app.\n"
        "Read MANUAL.html for Korean/English usage and INSTALLATION.md for platform details.\n"
        "This build is unsigned; no verified publisher signature or Store certification is claimed.\n"
        "Public factory IRs are embedded and credited in THIRD_PARTY_NOTICES.md.\n"
        "Personal third-party IR packs are separate and are not redistributed here.\n"
        "Check the adjacent SHA256 before installing. Verification.txt records automated tests.\n",
        encoding="utf-8")
    verify_stage(stage, ["Standalone/SpectralForge Chimera.exe", "MANUAL.html", "Verification.txt",
                         "VST3/SpectralForge Chimera.vst3/Contents/x86_64-win/SpectralForge Chimera.vst3"])
    archive = dist / f"{PREFIX}-win64.zip"
    with zipfile.ZipFile(archive, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as output:
        for path in sorted(stage.rglob("*")):
            if path.is_file():
                output.write(path, path.relative_to(stage))
    checksum(archive)


def macos(build: Path, dist: Path) -> None:
    if platform.system() != "Darwin":
        raise RuntimeError("macOS packages must be produced and validated on macOS")
    artefacts = build / "ChimeraAmpMatrix_artefacts/Release"
    stage = dist / f"{PREFIX}-macos-payload"
    stage.mkdir(parents=True, exist_ok=False)
    bundles = [
        (artefacts / f"Standalone/{PRODUCT}.app", stage / f"Applications/{PRODUCT}.app"),
        (artefacts / f"VST3/{PRODUCT}.vst3", stage / f"Library/Audio/Plug-Ins/VST3/{PRODUCT}.vst3"),
        (artefacts / f"AU/{PRODUCT}.component", stage / f"Library/Audio/Plug-Ins/Components/{PRODUCT}.component"),
    ]
    for source, destination in bundles:
        copy(source, destination)
        binary = destination / "Contents/MacOS" / PRODUCT
        run("lipo", str(binary), "-verify_arch", "arm64", "x86_64")
        # Ad-hoc signatures provide code seals for Apple Silicon, not a publisher identity.
        run("codesign", "--force", "--deep", "--sign", "-", str(destination))
        run("codesign", "--verify", "--deep", "--strict", str(destination))
    import plistlib
    app_info = plistlib.loads((stage / f"Applications/{PRODUCT}.app/Contents/Info.plist").read_bytes())
    if not app_info.get("NSMicrophoneUsageDescription"):
        raise RuntimeError("macOS standalone is missing its microphone permission description")
    docdir = stage / "Library/Application Support/SpectralForge/Chimera/Documentation"
    documents(docdir, build)
    verify_stage(stage, [f"Applications/{PRODUCT}.app/Contents/MacOS/{PRODUCT}",
                         "Library/Application Support/SpectralForge/Chimera/Documentation/MANUAL.html"])
    (stage / "payload-manifest.json").rename(docdir / "payload-manifest.json")
    # pkgbuild's component relocatability is disabled: plugins must remain in host-scanned folders.
    with tempfile.TemporaryDirectory(prefix="chimera-pkg-") as temporary:
        components = Path(temporary) / "components.plist"
        run("pkgbuild", "--analyze", "--root", str(stage), str(components))
        import plistlib
        info = plistlib.loads(components.read_bytes())
        for item in info:
            item["BundleIsRelocatable"] = False
            item["BundleOverwriteAction"] = "upgrade"
        components.write_bytes(plistlib.dumps(info))
        package = dist / f"{PREFIX}-macos-universal.pkg"
        run("pkgbuild", "--root", str(stage), "--component-plist", str(components),
            "--identifier", "audio.spectralforge.chimera", "--version", "1.0.0", "--install-location", "/", str(package))
        expanded = Path(temporary) / "expanded"
        run("pkgutil", "--expand-full", str(package), str(expanded))
        unpacked = expanded / "Payload"
        for _, installed in bundles:
            original = installed / "Contents/MacOS" / PRODUCT
            extracted = unpacked / original.relative_to(stage)
            if not extracted.is_file() or sha256(extracted) != sha256(original):
                raise RuntimeError(f"macOS package payload differs: {original}")
    checksum(package)
    (dist / "macOS-package-verification.txt").write_text(
        "PASS: Standalone, VST3 and AU contain arm64 + x86_64 slices.\n"
        "PASS: Built bundle code-seal verification and expanded pkg binary hashes.\n"
        "PASS: Fixed system install paths and offline manual packaged.\n"
        "LIMIT: Developer ID signing, notarization, real hardware audio and actual install are not certified by these checks.\n",
        encoding="utf-8")


def linux(build: Path, dist: Path) -> None:
    if platform.system() != "Linux" or platform.machine() not in ("x86_64", "amd64"):
        raise RuntimeError("Linux packages require a Linux x86_64 builder")
    artefacts = build / "ChimeraAmpMatrix_artefacts/Release"
    stage = dist / f"{PREFIX}-linux-payload"
    stage.mkdir(parents=True, exist_ok=False)
    executable = stage / "usr/bin/chimera-amp-matrix"
    plugin = stage / f"usr/lib/vst3/{PRODUCT}.vst3"
    copy(artefacts / f"Standalone/{PRODUCT}", executable)
    copy(artefacts / f"VST3/{PRODUCT}.vst3", plugin)
    executable.chmod(0o755)
    docdir = stage / "usr/share/doc/chimera-amp-matrix"
    documents(docdir, build)
    copy(ROOT / "Assets/Artwork/spectralforge-emblem.png", stage / "usr/share/pixmaps/chimera-amp-matrix.png")
    desktop = stage / "usr/share/applications/chimera-amp-matrix.desktop"
    desktop.parent.mkdir(parents=True, exist_ok=True)
    desktop.write_text("[Desktop Entry]\nType=Application\nName=SpectralForge Chimera\n"
                       "Comment=Guitar and bass amp matrix — Open Beta 1.0\n"
                       "Exec=chimera-amp-matrix\nIcon=chimera-amp-matrix\nTerminal=false\n"
                       "Categories=AudioVideo;Audio;\n", encoding="utf-8")
    vstbinary = plugin / "Contents/x86_64-linux/SpectralForge Chimera.so"
    verify_stage(stage, ["usr/bin/chimera-amp-matrix", "usr/share/doc/chimera-amp-matrix/MANUAL.html",
                         f"usr/lib/vst3/{PRODUCT}.vst3/Contents/x86_64-linux/SpectralForge Chimera.so"])
    (stage / "payload-manifest.json").rename(docdir / "payload-manifest.json")
    with tempfile.TemporaryDirectory(prefix="chimera-deb-") as temporary:
        work = Path(temporary)
        (work / "debian").mkdir()
        (work / "debian/control").write_text("Source: chimera-amp-matrix\nSection: sound\nPriority: optional\nMaintainer: RavenForge <noreply@ravenforge.audio>\n\nPackage: chimera-amp-matrix\nArchitecture: amd64\nDescription: guitar and bass amp matrix\n", encoding="utf-8")
        dependencies = run("dpkg-shlibdeps", "-O", "-e" + str(executable.resolve()),
                           "-e" + str(vstbinary.resolve()), cwd=work).strip()
        depends = next((line.removeprefix("shlibs:Depends=") for line in dependencies.splitlines()
                        if line.startswith("shlibs:Depends=")), "")
        if not depends or "libc6" not in depends:
            raise RuntimeError("Cannot produce explicit Debian runtime dependencies")
        control = stage / "DEBIAN/control"
        control.parent.mkdir()
        control.write_text(f"Package: chimera-amp-matrix\nVersion: 1.0.0~beta.1\nSection: sound\nPriority: optional\nArchitecture: amd64\nMaintainer: RavenForge <noreply@ravenforge.audio>\nDepends: {depends}\nHomepage: https://github.com/{REPO}\nDescription: SpectralForge Chimera Open Beta 1.0\n Guitar and bass amp suite, standalone and VST3, with offline manual.\n", encoding="utf-8")
        package = dist / f"{PREFIX}-linux-x86_64.deb"
        run("dpkg-deb", "--build", "--root-owner-group", str(stage), str(package))
        extracted = work / "extracted"
        run("dpkg-deb", "--extract", str(package), str(extracted))
        for source in (executable, vstbinary, docdir / "MANUAL.html"):
            if sha256(source) != sha256(extracted / source.relative_to(stage)):
                raise RuntimeError(f"Debian package payload differs: {source}")
        dependency_report = run("ldd", str(executable)) + run("ldd", str(vstbinary))
        if "not found" in dependency_report:
            raise RuntimeError("Native Linux payload has unresolved shared libraries")
    checksum(package)
    portable = dist / f"{PREFIX}-linux-x86_64.tar.gz"
    with tarfile.open(portable, "w:gz", format=tarfile.PAX_FORMAT) as archive:
        # Runtime libraries remain explicit OS dependencies; this is not a static AppImage.
        archive.add(stage / "usr", arcname=f"{PREFIX}-linux-x86_64/usr")
        archive.add(ROOT / "Tools/install-linux.sh", arcname=f"{PREFIX}-linux-x86_64/install.sh")
        archive.add(ROOT / "docs/INSTALLATION.md", arcname=f"{PREFIX}-linux-x86_64/INSTALLATION.md")
        runtime = f"Runtime packages (Debian/Ubuntu): {depends}\nRequires glibc baseline compatible with this build.\n"
        info = tarfile.TarInfo(f"{PREFIX}-linux-x86_64/RUNTIME-DEPENDENCIES.txt")
        import io
        content = runtime.encode("utf-8")
        info.size = len(content)
        archive.addfile(info, io.BytesIO(content))
    checksum(portable)
    (dist / "Linux-package-verification.txt").write_text(
        "PASS: dpkg-deb extraction hashes match standalone, VST3 and manual.\n"
        "PASS: No unresolved builder runtime libraries.\nRuntime dependency metadata: " + depends + "\n"
        "LIMIT: Desktop audio, DAW scanning and installation on other distributions require manual verification.\n" + dependency_report,
        encoding="utf-8")


def manifest(dist: Path) -> None:
    release_url = f"https://github.com/{REPO}/releases/tag/v{VERSION}"
    assets = []
    for suffix, system, arch in (("win64-Setup.exe", "windows", "x86_64"),
                                 ("macos-universal.pkg", "macos", "universal"),
                                 ("linux-x86_64.deb", "linux", "x86_64")):
        name = f"{PREFIX}-{suffix}"
        path = dist / name
        if not path.is_file():
            raise RuntimeError(f"Release manifest requires all verified OS installers: {name}")
        sidecar = path.with_name(name + ".sha256.txt")
        if not sidecar.exists() or sidecar.read_text(encoding="utf-8-sig").split()[0].lower() != sha256(path):
            raise RuntimeError(f"Missing/mismatched verified SHA256: {name}")
        assets.append({"platform": system, "arch": arch, "name": name,
                       "url": f"https://github.com/{REPO}/releases/download/v{VERSION}/{name}",
                       "sha256": sha256(path), "size": path.stat().st_size})
    (dist / "update-beta.json").write_text(json.dumps({"schema": 1, "version": VERSION, "channel": "beta",
                                                      "releaseUrl": release_url, "assets": assets}, indent=2) + "\n", encoding="utf-8")
    archives = sorted(p for p in dist.iterdir() if p.is_file() and p.suffix.lower() in (".exe", ".pkg", ".deb", ".gz", ".zip"))
    (dist / "SHA256SUMS.txt").write_text("".join(f"{sha256(p)}  {p.name}\n" for p in archives), encoding="utf-8")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("platform", choices=("windows", "macos", "linux", "manifest"))
    parser.add_argument("--build", type=Path, default=ROOT / "build")
    parser.add_argument("--dist", type=Path, default=ROOT / "dist")
    args = parser.parse_args()
    args.dist.mkdir(parents=True, exist_ok=True)
    if args.platform == "manifest":
        manifest(args.dist)
    else:
        globals()[args.platform](args.build, args.dist)


if __name__ == "__main__":
    main()
