#!/usr/bin/env python3
"""Stage verified binaries and build OS-native, unsigned release installers.

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
import chimera_version
import beta_lifecycle
import check_ir_distribution
import check_public_names
import legacy_document_cleanup

ROOT = Path(__file__).resolve().parents[1]
IDENTITY = chimera_version.identity(ROOT)
VERSION = IDENTITY["version"]
PRODUCT = "SpectralForge Chimera"
PREFIX = f"SpectralForge-Chimera-{VERSION}"
REPO = "raven-deadwire/SpectralForge-Chimera-Amp-Matrix"
# Product-facing documents shared by release, preview and publication staging.
# Research mappings, artwork prompts and raw evidence remain in their original
# development/CI locations; they are not installed as product documentation.
PUBLIC_DOCUMENTS = (
    "MANUAL.html", "OPEN_BETA_RELEASE_NOTES.md", "THIRD_PARTY_NOTICES.md",
    "PRESETS.md", "EXTERNAL_BASS_IRS.md", "IR_DISTRIBUTION.md", "UPDATES.md",
    "WINDOWS_INSTALL.txt", "INSTALLATION.md", "STUDIO_ONE_TEARDOWN.md",
    "PUBLIC_PRODUCT_NAMING.md",
)


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


def write_public_verification(destination: Path, build: Path, *, version: dict | None = None) -> None:
    """Identify the untouched test evidence without installing its research text.

    ``destination`` is the output file, not a directory. This is an evidence
    pointer, never a replacement PASS verdict or an approval receipt.
    """
    raw_log = build / "Testing/Temporary/LastTest.log"
    if not raw_log.is_file() or not raw_log.stat().st_size:
        raise RuntimeError(f"Missing required release input: {raw_log}")
    identity = IDENTITY if version is None else version
    lines = [
        f"{PRODUCT} — automated verification record",
        f"Version: {identity['version']}",
        f"Source commit: {identity['source_sha']}",
        "Raw CTest log: Testing/Temporary/LastTest.log",
        f"Raw CTest log SHA-256: {sha256(raw_log)}",
        f"Raw CTest log bytes: {raw_log.stat().st_size}",
    ]
    run_id = os.environ.get("GITHUB_RUN_ID", "")
    attempt = os.environ.get("GITHUB_RUN_ATTEMPT", "")
    if run_id.isdecimal() and int(run_id) > 0:
        lines.append(f"CI run: https://github.com/{REPO}/actions/runs/{run_id}")
        if attempt.isdecimal() and int(attempt) > 0:
            lines.append(f"CI run attempt: {attempt}")
    else:
        lines.append("CI run: not supplied by this build environment")
    lines.extend((
        "",
        "Review the original test logs and source-specific CI validation artifacts for results.",
        "This record does not certify DAW/listening acceptance or authorize publication.",
    ))
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_text("\n".join(lines) + "\n", encoding="utf-8")


def documents(destination: Path, build: Path, *, version: dict | None = None) -> None:
    check_public_names.validate_document_names(PUBLIC_DOCUMENTS)
    destination.mkdir(parents=True, exist_ok=True)
    check_public_names.validate_document(ROOT / "COPYRIGHT.txt")
    copy(ROOT / "COPYRIGHT.txt", destination / "COPYRIGHT.txt")
    for name in PUBLIC_DOCUMENTS:
        check_public_names.validate_document(ROOT / "docs" / name)
        copy(ROOT / "docs" / name, destination / name)
    write_public_verification(destination / "Verification.txt", build, version=version)
    lifecycle_documents(destination, build, version=version)


def lifecycle_documents(destination: Path, build: Path, *, version: dict | None = None) -> None:
    # Only new sources containing the lifecycle contract require it. Historical
    # fixtures and published artifacts remain untouched.
    if (ROOT / beta_lifecycle.POLICY_PATH).is_file():
        release = IDENTITY if version is None else version
        metadata = json.loads((build / "chimera-beta-lifecycle.json").read_text(encoding="utf-8"))
        beta_lifecycle.validate_metadata(metadata, ROOT, release["source_sha"])
        build_identity = json.loads((build / "chimera-build-version.json").read_text(encoding="utf-8"))
        if build_identity.get("beta_lifecycle_sha256") != sha256(build / "chimera-beta-lifecycle.json"):
            raise ValueError("Beta metadata differs from the configured build fingerprint")
        copy(build / "chimera-beta-lifecycle.json", destination / "chimera-beta-lifecycle.json")
        (destination / "BETA_LIFECYCLE.txt").write_text(beta_lifecycle.notice(metadata), encoding="utf-8")


def verify_stage(stage: Path, required: list[str], *, version: dict | None = None) -> None:
    for name in required:
        path = stage / name
        if not path.is_file() or not path.stat().st_size:
            raise RuntimeError(f"Empty or missing package file: {name}")
    check_ir_distribution.validate_stage(stage)
    check_public_names.validate_stage(stage)
    inventory = [{"path": p.relative_to(stage).as_posix(), "bytes": p.stat().st_size, "sha256": sha256(p)}
                 for p in sorted(stage.rglob("*")) if p.is_file() and not p.is_symlink()]
    (stage / "payload-manifest.json").write_text(json.dumps({**(IDENTITY if version is None else version), "files": inventory}, indent=2) + "\n", encoding="utf-8")


def windows(build: Path, dist: Path) -> None:
    check_ir_distribution.validate_source(ROOT)
    stage = dist / f"{PREFIX}-win64"
    stage.mkdir(parents=True, exist_ok=False)
    artefacts = build / "ChimeraAmpMatrix_artefacts/Release"
    copy(artefacts / "VST3", stage / "VST3")
    copy(artefacts / "Standalone", stage / "Standalone")
    documents(stage, build)
    # Preserve the optional diagnostic component exercised by installer checks.
    # Hardware-reference research and artwork prompts stay out of this payload.
    copy(ROOT / "Tools/validate_nam.py", stage / "ReferenceTools/validate_nam.py")
    copy(ROOT / "Tools/Trace-Chimera-Session.ps1", stage / "ReferenceTools/Trace-Chimera-Session.ps1")
    copy(build / "ChimeraRender_artefacts/Release/ChimeraRender.exe", stage / "ReferenceTools/ChimeraRender.exe")
    (stage / "README.txt").write_text(
        f"{PRODUCT} — {chimera_version.display_version(ROOT)} ({VERSION})\n\n"
        "Close Chimera and your DAW before installing or updating.\n"
        "Run the Setup installer for standard VST3 registration and the standalone app.\n"
        "Read MANUAL.html for Korean/English/German usage and INSTALLATION.md for platform details.\n"
        "This build is unsigned; no verified publisher signature or Store certification is claimed.\n"
        "Public factory IRs are embedded and credited in THIRD_PARTY_NOTICES.md.\n"
        "Personal third-party IR packs are separate and are not redistributed here.\n"
        "Check the adjacent SHA256 before installing. Verification.txt identifies the original test evidence.\n",
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
    check_ir_distribution.validate_source(ROOT)
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
    maintenance = stage / "Library/Application Support/SpectralForge/Chimera/Tools/document-maintenance"
    legacy_document_cleanup.build_helper(maintenance, platform="macos", current_documents=docdir)
    run("lipo", str(maintenance), "-verify_arch", "arm64", "x86_64")
    run("codesign", "--force", "--sign", "-", str(maintenance))
    run("codesign", "--verify", "--strict", str(maintenance))
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
        scripts = Path(temporary) / "scripts"
        scripts.mkdir()
        # Native executable hooks work without an optional Python installation.
        # Preinstall protects changed bytes before the system installer unpacks.
        for phase in ("preinstall", "postinstall"):
            copy(maintenance, scripts / phase)
        package = dist / f"{PREFIX}-macos-universal.pkg"
        run("pkgbuild", "--root", str(stage), "--component-plist", str(components),
            "--scripts", str(scripts),
            "--identifier", "audio.spectralforge.chimera", "--version", IDENTITY["product_version"], "--install-location", "/", str(package))
        expanded = Path(temporary) / "expanded"
        run("pkgutil", "--expand-full", str(package), str(expanded))
        unpacked = expanded / "Payload"
        for _, installed in bundles:
            original = installed / "Contents/MacOS" / PRODUCT
            extracted = unpacked / original.relative_to(stage)
            if not extracted.is_file() or sha256(extracted) != sha256(original):
                raise RuntimeError(f"macOS package payload differs: {original}")
        for name in ("chimera-beta-lifecycle.json", "BETA_LIFECYCLE.txt"):
            original = docdir / name
            if original.is_file() and sha256(unpacked / original.relative_to(stage)) != sha256(original):
                raise RuntimeError(f"macOS beta lifecycle payload differs: {name}")
        for phase in ("preinstall", "postinstall"):
            packaged_script = expanded / "Scripts" / phase
            if not packaged_script.is_file() or sha256(packaged_script) != sha256(maintenance):
                raise RuntimeError(f"macOS document lifecycle hook differs: {phase}")
    checksum(package)
    (dist / "macOS-package-verification.txt").write_text(
        "PASS: Standalone, VST3 and AU contain arm64 + x86_64 slices.\n"
        "PASS: Built bundle code-seal verification and expanded pkg binary hashes.\n"
        "PASS: Fixed system install paths and offline manual packaged.\n"
        "LIMIT: Developer ID signing, notarization, real hardware audio and actual install are not certified by these checks.\n",
        encoding="utf-8")


def linux(build: Path, dist: Path) -> None:
    check_ir_distribution.validate_source(ROOT)
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
    maintenance = stage / "usr/libexec/chimera-document-maintenance"
    legacy_document_cleanup.build_helper(maintenance, platform="linux", current_documents=docdir)
    copy(ROOT / "Assets/Artwork/spectralforge-emblem.png", stage / "usr/share/pixmaps/chimera-amp-matrix.png")
    desktop = stage / "usr/share/applications/chimera-amp-matrix.desktop"
    desktop.parent.mkdir(parents=True, exist_ok=True)
    desktop.write_text("[Desktop Entry]\nType=Application\nName=SpectralForge Chimera\n"
                       f"Comment=Guitar and bass amp matrix — {chimera_version.display_version(ROOT)}\n"
                       "Exec=chimera-amp-matrix\nIcon=chimera-amp-matrix\nTerminal=false\n"
                       "Categories=AudioVideo;Audio;\n", encoding="utf-8")
    vstbinary = plugin / "Contents/x86_64-linux/SpectralForge Chimera.so"
    verify_stage(stage, ["usr/bin/chimera-amp-matrix", "usr/share/doc/chimera-amp-matrix/MANUAL.html",
                         "usr/libexec/chimera-document-maintenance",
                         f"usr/lib/vst3/{PRODUCT}.vst3/Contents/x86_64-linux/SpectralForge Chimera.so"])
    (stage / "payload-manifest.json").rename(docdir / "payload-manifest.json")
    with tempfile.TemporaryDirectory(prefix="chimera-deb-") as temporary:
        work = Path(temporary)
        (work / "debian").mkdir()
        (work / "debian/control").write_text("Source: chimera-amp-matrix\nSection: sound\nPriority: optional\nMaintainer: RavenForge <noreply@ravenforge.audio>\n\nPackage: chimera-amp-matrix\nArchitecture: amd64\nDescription: guitar and bass amp matrix\n", encoding="utf-8")
        dependencies = run("dpkg-shlibdeps", "-O", "-e" + str(executable.resolve()),
                           "-e" + str(vstbinary.resolve()), "-e" + str(maintenance.resolve()), cwd=work).strip()
        depends = next((line.removeprefix("shlibs:Depends=") for line in dependencies.splitlines()
                        if line.startswith("shlibs:Depends=")), "")
        if not depends or "libc6" not in depends:
            raise RuntimeError("Cannot produce explicit Debian runtime dependencies")
        helper_dependencies = run("dpkg-shlibdeps", "-O", "-e" + str(maintenance.resolve()), cwd=work).strip()
        pre_depends = next((line.removeprefix("shlibs:Depends=") for line in helper_dependencies.splitlines()
                            if line.startswith("shlibs:Depends=")), "")
        if not pre_depends or "libc6" not in pre_depends:
            raise RuntimeError("Cannot prove document preinst runtime dependencies")
        control = stage / "DEBIAN/control"
        control.parent.mkdir()
        control.write_text(f"Package: chimera-amp-matrix\nVersion: {VERSION.replace('-', '~', 1)}\nSection: sound\nPriority: optional\nArchitecture: amd64\nMaintainer: RavenForge <noreply@ravenforge.audio>\nPre-Depends: {pre_depends}\nDepends: {depends}\nHomepage: https://github.com/{REPO}\nDescription: SpectralForge Chimera {chimera_version.display_version(ROOT)}\n Guitar and bass amp suite, standalone and VST3, with offline manual.\n", encoding="utf-8")
        # General package files are not conffiles: dpkg can delete obsolete
        # edited documents during unpack. Protection must run in preinst.
        for phase in ("preinst", "postinst", "prerm", "postrm"):
            copy(maintenance, control.parent / phase)
        package = dist / f"{PREFIX}-linux-x86_64.deb"
        run("dpkg-deb", "--build", "--root-owner-group", str(stage), str(package))
        extracted = work / "extracted"
        run("dpkg-deb", "--extract", str(package), str(extracted))
        beta_documents = [docdir / name for name in ("chimera-beta-lifecycle.json", "BETA_LIFECYCLE.txt")
                          if (docdir / name).is_file()]
        for source in (executable, vstbinary, docdir / "MANUAL.html", *beta_documents):
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


def manifest(dist: Path, *, target_version: str | None = None,
             source_sha: str | None = None) -> tuple[Path, ...]:
    # Targets are assertions about source-derived package identity, not knobs
    # for renaming already-built bytes from another version or revision.
    expected = chimera_version.identity(ROOT)
    if source_sha is not None and source_sha != expected["source_sha"]:
        raise ValueError("Requested source SHA differs from the checked-out source")
    value = expected["version"] if target_version is None else target_version
    if target_version is None and chimera_version.release_channel(ROOT) == "preview":
        # Private CI assembly also stages preview.<SHA> payloads. Their feed is
        # review evidence only: public target validation rejects this identity.
        names = ("update-beta.json",)
    else:
        chimera_version.validate_target_version(value, ROOT, expected["source_sha"])
        names = chimera_version.release_manifest_names(value)
    prefix = f"SpectralForge-Chimera-{value}"
    release_url = f"https://github.com/{REPO}/releases/tag/v{value}"
    allowed_archives = {f"{prefix}-{suffix}" for suffix in (
        "win64-Setup.exe", "win64.zip", "macos-universal.pkg",
        "linux-x86_64.deb", "linux-x86_64.tar.gz")}
    archives = sorted(p for p in dist.iterdir()
                      if p.is_file() and p.suffix.lower() in (".exe", ".pkg", ".deb", ".gz", ".zip"))
    if any(p.name not in allowed_archives or p.is_symlink() for p in archives):
        raise RuntimeError("Release directory contains an unexpected/mixed-version package")
    if any(p.name not in names for p in dist.glob("update-*.json")):
        raise RuntimeError("Release directory contains a feed for a different channel")
    assets = []
    for suffix, system, arch in (("win64-Setup.exe", "windows", "x86_64"),
                                 ("macos-universal.pkg", "macos", "universal"),
                                 ("linux-x86_64.deb", "linux", "x86_64")):
        name = f"{prefix}-{suffix}"
        path = dist / name
        if not path.is_file() or path.is_symlink() or not 0 < path.stat().st_size <= 1024**3:
            raise RuntimeError(f"Release manifest requires all verified OS installers: {name}")
        sidecar = path.with_name(name + ".sha256.txt")
        checksum_fields = sidecar.read_text(encoding="utf-8-sig").split() if sidecar.is_file() else []
        if sidecar.is_symlink() or checksum_fields != [sha256(path), name]:
            raise RuntimeError(f"Missing/mismatched verified SHA256: {name}")
        assets.append({"platform": system, "arch": arch, "name": name,
                       "url": f"https://github.com/{REPO}/releases/download/v{value}/{name}",
                       "sha256": sha256(path), "size": path.stat().st_size})
    # A stable/RC release keeps its canonical channel. The extra beta receiver
    # feed lets already shipped beta clients discover it without marking a
    # stable GitHub release as a prerelease or relabelling installer bytes.
    feeds = tuple(dist / name for name in names)
    for feed in feeds:
        receiver_channel = feed.stem.removeprefix("update-")
        feed.write_text(json.dumps({"schema": 1, "version": value, "channel": receiver_channel,
                                   "source_sha": expected["source_sha"],
                                   "releaseUrl": release_url, "assets": assets}, indent=2) + "\n", encoding="utf-8")
    (dist / "SHA256SUMS.txt").write_text("".join(f"{sha256(p)}  {p.name}\n" for p in archives), encoding="utf-8")
    return feeds


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("platform", choices=("windows", "macos", "linux", "manifest"))
    parser.add_argument("--build", type=Path, default=ROOT / "build")
    parser.add_argument("--dist", type=Path, default=ROOT / "dist")
    parser.add_argument("--target-version", help="Assert the source-derived package version")
    parser.add_argument("--source-sha", help="Assert the exact checked-out source SHA")
    args = parser.parse_args()
    if args.source_sha is not None and args.source_sha != IDENTITY["source_sha"]:
        parser.error("Requested source SHA differs from the checked-out source")
    if args.target_version is not None:
        chimera_version.validate_target_version(args.target_version, ROOT)
    args.dist.mkdir(parents=True, exist_ok=True)
    if args.platform == "manifest":
        manifest(args.dist, target_version=args.target_version, source_sha=args.source_sha)
    else:
        chimera_version.validate_build(args.build, IDENTITY)
        globals()[args.platform](args.build, args.dist)


if __name__ == "__main__":
    main()
