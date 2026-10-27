#!/usr/bin/env python3
"""Verify/package/install/remove unsigned AAX development bundles. Never publish.

The packaged copy needs only Python 3.9+. It deliberately cannot claim PACE
verification or overwrite a bundle not owned by this development installer.
"""
from __future__ import annotations
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import plistlib
import shutil
import struct
import subprocess
import tempfile
import zipfile

PRODUCT = "SpectralForge Chimera"
BUNDLE = PRODUCT + ".aaxplugin"
RECEIPT = "SpectralForge-Chimera-AAX-development.json"
SCHEMA = "spectralforge.chimera.aax-development.v1"


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def plain_path(path: Path) -> None:
    # Do not resolve first: that would erase evidence of links/junctions.
    for item in (path.absolute(), *path.absolute().parents):
        if item.is_symlink() or (item.exists() and
                getattr(item.lstat(), "st_file_attributes", 0) & 0x400):
            raise ValueError(f"Linked/reparse path is not supported: {item}")


def inventory(bundle: Path) -> dict:
    plain_path(bundle)
    if not bundle.is_dir() or bundle.name != BUNDLE:
        raise ValueError(f"Expected {BUNDLE} directory")
    result = {}
    for item in sorted(bundle.rglob("*")):
        plain_path(item)
        if item.is_file():
            result[item.relative_to(bundle).as_posix()] = digest(item)
        elif not item.is_dir():
            raise ValueError(f"Unsupported bundle entry: {item}")
    if not result:
        raise ValueError("Empty AAX bundle")
    return result


def verify_bundle(bundle: Path, target: str) -> dict:
    files = inventory(bundle)
    if target == "windows":
        binary = bundle / "Contents/x64" / BUNDLE
        data = binary.read_bytes()
        if len(data) < 64 or data[:2] != b"MZ":
            raise ValueError("AAX Windows binary is not PE")
        offset = struct.unpack_from("<I", data, 60)[0]
        if offset + 26 > len(data) or data[offset:offset+4] != b"PE\0\0":
            raise ValueError("Invalid PE header")
        machine = struct.unpack_from("<H", data, offset+4)[0]
        flags = struct.unpack_from("<H", data, offset+22)[0]
        magic = struct.unpack_from("<H", data, offset+24)[0]
        if machine != 0x8664 or not flags & 0x2000 or magic != 0x20b:
            raise ValueError("AAX requires an x64 PE32+ DLL")
        architectures = ["x86_64"]
    elif target == "macos":
        binary = bundle / "Contents/MacOS" / PRODUCT
        data = binary.read_bytes()
        # JUCE's Universal output is a fat Mach-O containing two 64-bit slices.
        if len(data) < 8 or data[:4] not in (b"\xca\xfe\xba\xbe", b"\xca\xfe\xba\xbf"):
            raise ValueError("AAX macOS binary must be Universal Mach-O")
        count = struct.unpack_from(">I", data, 4)[0]
        stride = 32 if data[:4] == b"\xca\xfe\xba\xbf" else 20
        if count != 2 or len(data) < 8 + count * stride:
            raise ValueError("Expected exactly two Universal slices")
        cpu = set()
        for i in range(count):
            start = 8 + i * stride
            cpu.add(struct.unpack_from(">I", data, start)[0])
            offset, size = struct.unpack_from(">QQ" if stride == 32 else ">II", data, start+8)
            if size < 32 or offset + size > len(data) or data[offset:offset+4] != b"\xcf\xfa\xed\xfe":
                raise ValueError("Missing/truncated 64-bit Mach-O slice")
        if cpu != {0x01000007, 0x0100000c}:
            raise ValueError("Expected x86_64 and arm64")
        info = plistlib.loads((bundle / "Contents/Info.plist").read_bytes())
        if info.get("CFBundleIdentifier") != "audio.spectralforge.chimera" or info.get("CFBundleExecutable") != PRODUCT:
            raise ValueError("Unexpected AAX bundle identity")
        architectures = ["arm64", "x86_64"]
    else:
        raise ValueError("Only windows and macos AAX targets are supported")
    return {"architectures": architectures, "binary_sha256": digest(binary), "files": files}


def write_json(path: Path, value: dict) -> None:
    path.write_text(json.dumps(value, indent=2, sort_keys=True) + "\n", encoding="utf-8")


def verify_package(package: Path) -> dict:
    receipt = json.loads((package / RECEIPT).read_text(encoding="utf-8"))
    if (receipt.get("schema") != SCHEMA or receipt.get("pace_signed") is not False
            or receipt.get("normal_host_accepted") is not False
            or receipt.get("release_approved") is not False):
        raise ValueError("Not an unsigned AAX development package")
    actual = verify_bundle(package / BUNDLE, receipt["platform"])
    for key in actual:
        if receipt.get(key) != actual[key]:
            raise ValueError(f"AAX payload differs from receipt: {key}")
    return receipt


def signing_preflight(package_dir: Path, target: str, source_sha: str,
                      require_approved_beta: bool = False) -> dict:
    """Read-only input check from the candidate's source checkout, not signing.

    A self-consistent development receipt is not proof of build provenance.
    Obtain the package/hash from the trusted exact-source CI run separately.
    This command never reads PACE credentials or executes vendor tools.
    """
    import beta_lifecycle
    import chimera_version
    root = Path(__file__).resolve().parents[1]
    if source_sha != chimera_version.revision(root):
        raise ValueError("Expected source must match the checked-out full SHA")
    identity = chimera_version.identity(root, source_sha)
    plain_path(package_dir)
    for name in (RECEIPT, "chimera-build-version.json", "chimera-beta-lifecycle.json"):
        plain_path(package_dir / name)
    receipt = verify_package(package_dir)
    if receipt["platform"] != target:
        raise ValueError("AAX package platform differs from requested signing target")
    chimera_version.validate_manifest(receipt, identity)
    chimera_version.validate_build(package_dir, identity)
    beta_path = package_dir / "chimera-beta-lifecycle.json"
    beta = json.loads(beta_path.read_text(encoding="utf-8"))
    build = json.loads((package_dir / "chimera-build-version.json").read_text(encoding="utf-8"))
    if build.get("beta_lifecycle_sha256") != digest(beta_path):
        raise ValueError("AAX beta metadata hash differs from configured build")
    beta_lifecycle.validate_metadata(beta, root, source_sha,
                                     publication=require_approved_beta)
    # Deliberately allowlist report fields; never copy arbitrary metadata or
    # local absolute paths into logs. None of these checks grants a release.
    return {"schema": "spectralforge.chimera.aax-signing-preflight.v1",
            **identity, "platform": target,
            "architectures": receipt["architectures"],
            "binary_sha256": receipt["binary_sha256"],
            "receipt_sha256": digest(package_dir / RECEIPT),
            "build_metadata_sha256": digest(package_dir / "chimera-build-version.json"),
            "beta_metadata_sha256": digest(beta_path),
            "beta_mode": beta["mode"], "beta_expiry_approved": beta["expiry_approved"],
            "input_integrity": "PASS", "pace_verification": "NOT_RUN",
            "platform_signature_verification": "NOT_RUN",
            "normal_host_accepted": False, "release_approved": False}


def package(build: Path, output: Path, target: str) -> Path:
    # These source modules are only needed by the builder, not by the installer.
    import beta_lifecycle
    import chimera_version
    root = Path(__file__).resolve().parents[1]
    source_sha = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=root, text=True).strip()
    identity = chimera_version.identity(root, source_sha)
    build_identity = json.loads((build / "chimera-build-version.json").read_text())
    beta_path = build / "chimera-beta-lifecycle.json"
    beta = json.loads(beta_path.read_text())
    beta_lifecycle.validate_metadata(beta, root, source_sha)
    if any(build_identity.get(k) != identity[k] for k in ("version", "product_version", "source_sha")):
        raise ValueError("AAX build/source version mismatch")
    if build_identity["beta_lifecycle_sha256"] != digest(beta_path):
        raise ValueError("AAX beta metadata changed since configure")
    bundle = build / "ChimeraAmpMatrix_artefacts/Release/AAX" / BUNDLE
    verified = verify_bundle(bundle, target)
    name = f"SpectralForge-Chimera-{identity['version']}-{source_sha[:10]}-{target}-AAX-UNSIGNED-DEVELOPMENT"
    output.mkdir(parents=True, exist_ok=True)
    stage = output / name
    stage.mkdir(exist_ok=False)
    shutil.copytree(bundle, stage / BUNDLE)
    for filename in ("chimera-build-version.json", "chimera-beta-lifecycle.json"):
        shutil.copy2(build / filename, stage / filename)
    shutil.copy2(__file__, stage / "aax_native.py")
    (stage / "BETA_LIFECYCLE.txt").write_text(beta_lifecycle.notice(beta), encoding="utf-8")
    write_json(stage / RECEIPT, {"schema": SCHEMA, **identity, **verified,
        "platform": target, "pace_signed": False, "normal_host_accepted": False,
        "release_approved": False})
    (stage / "README.txt").write_text(
        "UNSIGNED AAX NATIVE DEVELOPMENT BUILD - NOT A PUBLIC RELEASE\n"
        "Requires Pro Tools Developer; ordinary Pro Tools will reject this build.\n"
        "macOS ad-hoc code seals are not PACE signatures or notarization.\n"
        "Close all hosts before install, update or removal. Python 3.9+ required.\n"
        "From an administrator terminal (sudo python3 on macOS):\n"
        "  python aax_native.py install --package . --acknowledge-developer-only\n"
        f"  python aax_native.py uninstall --platform {target} --acknowledge-developer-only\n"
        "Install also updates a prior development package at the fixed system AAX path.\n"
        "A bundle without a matching development receipt is never replaced or removed.\n"
        "Presets, personal IRs, projects, VST3 and AU are not owned by this installer.\n"
        "No host loading, PDC or signing acceptance is implied by bundle verification.\n",
        encoding="utf-8")
    verify_package(stage)
    archive = output / (name + ".zip")
    with zipfile.ZipFile(archive, "w", zipfile.ZIP_DEFLATED) as stream:
        for item in sorted(stage.rglob("*")):
            if item.is_file():
                stream.write(item, item.relative_to(stage))
    (output / (archive.name + ".sha256")).write_text(f"{digest(archive)}  {archive.name}\n")
    return stage


def system_root(target: str) -> Path:
    if target == "windows" and platform.system() == "Windows":
        return Path(os.environ.get("CommonProgramW6432", r"C:\Program Files\Common Files")) / "Avid/Audio/Plug-Ins"
    if target == "macos" and platform.system() == "Darwin":
        # /Library is canonical; /tmp and /var can legitimately be symlinks.
        return Path("/Library/Application Support/Avid/Audio/Plug-Ins")
    raise ValueError("System AAX installation requires the matching OS")


def installed_receipt(root: Path) -> dict | None:
    plain_path(root)
    destination = root / BUNDLE
    receipt = root / RECEIPT
    plain_path(receipt)
    if not destination.exists() and not receipt.exists():
        return None
    if not receipt.is_file() or not destination.is_dir():
        raise ValueError("Unowned/incomplete installation; preserve it and inspect manually")
    return verify_package(root)


def install(package_dir: Path, root: Path) -> None:
    new = verify_package(package_dir)
    old = installed_receipt(root)
    if old and old["platform"] != new["platform"]:
        raise ValueError("Installed platform differs")
    root.mkdir(parents=True, exist_ok=True)
    # Manual cleanup is intentional: TemporaryDirectory would delete the only
    # previous bundle if the replacement AND its rollback both fail.
    work = Path(tempfile.mkdtemp(prefix=".chimera-aax-", dir=root)).absolute()
    preserve_work = False
    try:
        staged = work / BUNDLE
        shutil.copytree(package_dir / BUNDLE, staged)
        shutil.copy2(package_dir / RECEIPT, work / RECEIPT)
        verify_package(work)
        destination = root / BUNDLE
        backup = work / "previous.aaxplugin"
        prior_receipt = (root / RECEIPT).read_bytes() if old else None
        if old:
            # Retain receipt bytes even if the bundle is restored successfully
            # but the subsequent receipt write fails.
            (work / "previous-receipt.json").write_bytes(prior_receipt)
        # Preserve before the first rename. A signal can arrive after the OS
        # completed a move but before Python records a boolean flag.
        preserve_work = True
        try:
            if old:
                destination.rename(backup)
            staged.rename(destination)
            os.replace(work / RECEIPT, root / RECEIPT)
            if verify_package(root) != new:
                raise ValueError("Installed AAX differs from the verified package")
            preserve_work = False
        except BaseException as install_error:
            # A handled KeyboardInterrupt must not bypass rollback. Abrupt
            # process termination/power loss are not transactional guarantees.
            preserve_work = True
            try:
                # Same-volume rename is atomic; infer completed moves from
                # their source paths, including an interrupted syscall return.
                candidate_installed = not staged.exists()
                if candidate_installed and destination.exists():
                    shutil.rmtree(destination)
                if backup.exists():
                    backup.rename(destination)
                    (root / RECEIPT).write_bytes(prior_receipt)
                elif not old and candidate_installed:
                    (root / RECEIPT).unlink(missing_ok=True)
                if old and verify_package(root) != old:
                    raise ValueError("Restored AAX differs from the previous installation")
                preserve_work = False
            except BaseException as rollback_error:
                raise RuntimeError(
                    f"AAX installation failed ({install_error!r}); rollback incomplete "
                    f"({rollback_error!r}). Recovery files retained at: {work}. "
                    "Do not delete this directory. Close all hosts and inspect the "
                    "installed bundle, previous.aaxplugin and previous-receipt.json "
                    "before retrying. No successful install or rollback is claimed."
                ) from rollback_error
            raise
    finally:
        # Never best-effort-delete the recovery directory on rollback failure.
        if not preserve_work:
            shutil.rmtree(work)


def uninstall(root: Path) -> None:
    old = installed_receipt(root)
    if old is None:
        return
    # rmtree can delete part of a bundle before failing. Keep a verified full
    # copy until both removal and receipt deletion have committed.
    work = Path(tempfile.mkdtemp(prefix=".chimera-aax-remove-", dir=root)).absolute()
    preserve_work = False
    try:
        destination = root / BUNDLE
        moved = work / BUNDLE
        recovery = work / "recovery"
        recovery.mkdir()
        prior_receipt = (root / RECEIPT).read_bytes()
        shutil.copytree(destination, recovery / BUNDLE)
        (recovery / RECEIPT).write_bytes(prior_receipt)
        if verify_package(recovery) != old:
            raise ValueError("Uninstall recovery copy differs from installed AAX")
        preserve_work = True
        try:
            # A failed or interrupted rename must also enter rollback.
            destination.rename(moved)
            shutil.rmtree(moved)
            (root / RECEIPT).unlink()
            if installed_receipt(root) is not None:
                raise ValueError("AAX removal did not complete")
            preserve_work = False
        except BaseException as remove_error:
            try:
                if not destination.exists():
                    # Keep the verified recovery copy even if restoration is
                    # interrupted. Never retry deletion of a partial original.
                    restore = work / "restore" / BUNDLE
                    shutil.copytree(recovery / BUNDLE, restore)
                    restore.rename(destination)
                (root / RECEIPT).write_bytes(prior_receipt)
                if verify_package(root) != old:
                    raise ValueError("Restored AAX differs after failed removal")
                preserve_work = False
            except BaseException as rollback_error:
                raise RuntimeError(
                    f"AAX removal failed ({remove_error!r}); rollback incomplete "
                    f"({rollback_error!r}). Recovery files retained at: {work}. "
                    "Do not delete this directory. Close all hosts and inspect "
                    "the installed bundle and recovery package before retrying. "
                    "No successful removal or rollback is claimed."
                ) from rollback_error
            raise
    finally:
        if not preserve_work:
            shutil.rmtree(work)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="command", required=True)
    verify = sub.add_parser("verify")
    verify.add_argument("--bundle", type=Path, required=True)
    verify.add_argument("--platform", choices=("windows", "macos"), required=True)
    pack = sub.add_parser("package")
    pack.add_argument("--build", type=Path, required=True)
    pack.add_argument("--output", type=Path, required=True)
    pack.add_argument("--platform", choices=("windows", "macos"), required=True)
    preflight = sub.add_parser("signing-preflight", help="Read-only candidate input checks; never signs")
    preflight.add_argument("--package", type=Path, required=True)
    preflight.add_argument("--platform", choices=("windows", "macos"), required=True)
    preflight.add_argument("--expected-source", required=True)
    preflight.add_argument("--require-approved-beta", action="store_true",
                           help="Also reject qualification-only beta policy for deployment input")
    for action in ("install", "uninstall"):
        p = sub.add_parser(action)
        p.add_argument("--acknowledge-developer-only", action="store_true", required=True)
        p.add_argument("--root", type=Path, help="Override only for isolated development/CI installation")
        if action == "install":
            p.add_argument("--package", type=Path, required=True)
        else:
            p.add_argument("--platform", choices=("windows", "macos"), required=True)
    args = parser.parse_args()
    if args.command == "verify":
        print(json.dumps(verify_bundle(args.bundle, args.platform), indent=2))
    elif args.command == "package":
        print(package(args.build, args.output, args.platform))
    elif args.command == "signing-preflight":
        print(json.dumps(signing_preflight(args.package, args.platform, args.expected_source,
                                          args.require_approved_beta), indent=2))
    elif args.command == "install":
        metadata = verify_package(args.package)
        install(args.package, args.root or system_root(metadata["platform"]))
    else:
        uninstall(args.root or system_root(args.platform))


if __name__ == "__main__":
    main()

