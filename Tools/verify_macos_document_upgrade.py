#!/usr/bin/env python3
"""Run real Apple PKG document fixtures on a disposable macOS CI runner only.

This proves whether pkgbuild/installer execute the actual compiled native hooks
and preserve synthetic documents through upgrade and repair. It does not install
released Chimera app/plugin binaries or certify DAW/audio/product acceptance.
The fixed product document path is required by the native PKG hook contract, so
any existing product documents, receipts, or bundles cause an immediate refusal.
"""
from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import platform
import shutil
import subprocess
import tempfile

import legacy_document_cleanup as cleanup
from test_legacy_document_cleanup import (
    CURRENT_DOCUMENTS, OLD_DOCUMENTS, USER_EDIT, digest,
    fixture_inventory, write_documents,
)

ROOT = Path(__file__).resolve().parents[1]
DOCROOT = Path("/Library/Application Support/SpectralForge/Chimera/Documentation")
FIXTURE_ID = "audio.spectralforge.chimera.document-upgrade-fixture"


def require(condition: bool, message: str) -> None:
    if not condition:
        raise RuntimeError(message)


def verify(output: Path, *, disposable_ci: bool) -> int:
    output.mkdir(parents=True, exist_ok=True)
    report = {
        "schema": "spectralforge.macos-document-package-fixture.v1",
        "success": False,
        "platform": platform.system(),
        "run_id": os.environ.get("GITHUB_RUN_ID"),
        "source_sha": None,
        "scope": "Real Apple PKG installers with synthetic documents and compiled native lifecycle hooks; document-only removal.",
        "native_installer_invocations": 0,
        "native_installer_successes": 0,
        "historical_release_installer_executed": False,
        "native_product_installation_verified": False,
        "daw_audio_verified": False,
        "publication_approved": False,
        "checks": [],
    }

    def run(label: str, *command: str, allowed: tuple[int, ...] = (0,)) -> subprocess.CompletedProcess:
        is_installer = Path(command[0]).name == "installer"
        process = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        if is_installer:
            report["native_installer_invocations"] += 1
        try:
            stdout, stderr = process.communicate(timeout=60)
        except subprocess.TimeoutExpired:
            process.kill()
            stdout, stderr = process.communicate()
            (output / (label + ".log")).write_text(stdout + stderr, encoding="utf-8")
            raise RuntimeError(f"Native fixture command timed out: {label}")
        result = subprocess.CompletedProcess(command, process.returncode, stdout, stderr)
        (output / (label + ".log")).write_text(stdout + stderr, encoding="utf-8")
        if is_installer and result.returncode == 0:
            report["native_installer_successes"] += 1
        require(result.returncode in allowed,
                f"Native fixture command failed ({result.returncode}): {label}\n{stderr}")
        return result

    def passed(message: str) -> None:
        report["checks"].append(message)
        print("PASS: " + message)

    def expect(relative: str, content: bytes) -> None:
        require((DOCROOT / relative).read_bytes() == content,
                f"Installed fixture bytes differ: {relative}")

    def saved(relative: str, content: bytes) -> None:
        backup = Path(str(DOCROOT) + ".preserved") / digest(content) / relative
        require(backup.read_bytes() == content, f"Protected fixture copy differs: {relative}")

    try:
        require(platform.system() == "Darwin", "Native Apple package fixtures were not executed: macOS is required.")
        require(disposable_ci and os.environ.get("GITHUB_ACTIONS") == "true"
                and os.environ.get("CI") == "true" and os.environ.get("RUNNER_OS") == "macOS"
                and bool(os.environ.get("RUNNER_TEMP")),
                "Refusing system-path fixture installation outside an explicit disposable macOS CI run.")
        require(os.geteuid() == 0, "Run this guarded disposable-CI fixture with administrator privileges.")
        for name in ("cc", "pkgbuild", "pkgutil", "installer", "codesign", "lipo"):
            require(shutil.which(name) is not None, f"Native fixture tool unavailable: {name}")
        for path in (DOCROOT, Path(str(DOCROOT) + ".preserved"), Path(str(DOCROOT) + ".pending"),
                     Path("/Applications/SpectralForge Chimera.app"),
                     Path("/Library/Audio/Plug-Ins/VST3/SpectralForge Chimera.vst3"),
                     Path("/Library/Audio/Plug-Ins/Components/SpectralForge Chimera.component")):
            require(not path.exists() and not path.is_symlink(),
                    f"Refusing to touch an existing product or fixture path: {path}")
        for identifier in ("audio.spectralforge.chimera", FIXTURE_ID):
            result = run("receipt-preflight-" + identifier, "pkgutil", "--pkg-info", identifier, allowed=(0, 1))
            require(result.returncode != 0, f"Refusing existing package receipt: {identifier}")
        report["source_sha"] = subprocess.check_output(
            ["git", "-c", f"safe.directory={ROOT}", "-C", str(ROOT), "rev-parse", "HEAD"],
            text=True,
        ).strip()
        with tempfile.TemporaryDirectory(prefix="chimera-macos-pkg-fixture-",
                                         dir=Path(os.environ["RUNNER_TEMP"]).resolve()) as temporary:
            work = Path(temporary).resolve()
            inventory = work / "fixture-inventory.json"
            inventory.write_text(json.dumps(fixture_inventory()), encoding="utf-8")
            current = work / "current-documents"
            write_documents(current, CURRENT_DOCUMENTS)
            helper = cleanup.build_helper(work / "document-maintenance", platform="macos",
                                          inventory=inventory, current_documents=current)
            run("native-helper-architectures", "lipo", str(helper), "-verify_arch", "arm64", "x86_64")
            run("native-helper-ad-hoc-sign", "codesign", "--force", "--sign", "-", str(helper))
            run("native-helper-code-seal", "codesign", "--verify", "--strict", str(helper))
            packages = {}
            for label, documents in (("old", OLD_DOCUMENTS), ("new", CURRENT_DOCUMENTS)):
                stage = work / (label + "-payload")
                write_documents(stage / str(DOCROOT).lstrip("/"), documents)
                package = work / (label + ".pkg")
                arguments = ["pkgbuild", "--root", str(stage), "--identifier", FIXTURE_ID,
                             "--version", "1.3.1", "--install-location", "/"]
                if label == "new":
                    scripts = work / "native-hooks"
                    scripts.mkdir()
                    for phase in ("preinstall", "postinstall"):
                        shutil.copy2(helper, scripts / phase)
                    arguments += ["--scripts", str(scripts)]
                run("build-" + label, *arguments, str(package))
                packages[label] = package

            run("01-fresh-install", "installer", "-pkg", str(packages["new"]), "-target", "/")
            for relative, content in CURRENT_DOCUMENTS.items():
                expect(relative, content)
            run("02-fresh-document-removal", str(helper), "remove-documents", str(DOCROOT))
            require(not (DOCROOT / "MANUAL.html").exists(), "Fresh document removal left an exact owned manual.")
            for relative in ("THIRD_PARTY_NOTICES.md", "COPYRIGHT.txt"):
                expect(relative, CURRENT_DOCUMENTS[relative])
            run("03-forget-fresh-fixture-receipt", "pkgutil", "--forget", FIXTURE_ID)
            passed("Real Apple PKG fresh installation executes native hooks; document-only removal preserves notices.")

            run("04-historical-fixture-install", "installer", "-pkg", str(packages["old"]), "-target", "/")
            for relative, content in OLD_DOCUMENTS.items():
                expect(relative, content)
            write_documents(DOCROOT, {"AMP_VALIDATION.md": USER_EDIT, "MANUAL.html": USER_EDIT,
                                      "personal.chimera": USER_EDIT, "personal.wav": USER_EDIT,
                                      "existing-session.song": USER_EDIT})
            run("05-upgrade-native-hooks", "installer", "-pkg", str(packages["new"]), "-target", "/")
            expect("AMP_VALIDATION.md", USER_EDIT)
            expect("MANUAL.html", CURRENT_DOCUMENTS["MANUAL.html"])
            saved("MANUAL.html", USER_EDIT)
            require(not (DOCROOT / "MODELS_AND_REFERENCE.md").exists(), "Unchanged retired fixture remains after PKG upgrade.")
            require(not (DOCROOT / "AMP_NATIVE_DSP.md").exists(), "Unchanged native research fixture remains after PKG upgrade.")
            passed("Real Apple PKG upgrade removes exact retired files and preserves edited legacy and current document bytes.")

            repair_edit = b"User notes before the same-version PKG repair.\n"
            write_documents(DOCROOT, {"MANUAL.html": repair_edit})
            run("06-same-version-repair", "installer", "-pkg", str(packages["new"]), "-target", "/")
            expect("MANUAL.html", CURRENT_DOCUMENTS["MANUAL.html"])
            saved("MANUAL.html", repair_edit)
            saved("MANUAL.html", USER_EDIT)
            passed("Same-version Apple PKG repair installs current documentation and retains both user revisions.")

            write_documents(DOCROOT, {"MANUAL.html": repair_edit})
            run("07-edited-document-removal", str(helper), "remove-documents", str(DOCROOT))
            run("08-repeat-document-removal", str(helper), "remove-documents", str(DOCROOT))
            expect("MANUAL.html", repair_edit)
            expect("AMP_VALIDATION.md", USER_EDIT)
            for relative in ("personal.chimera", "personal.wav", "existing-session.song"):
                expect(relative, USER_EDIT)
            for relative in ("THIRD_PARTY_NOTICES.md", "COPYRIGHT.txt"):
                expect(relative, CURRENT_DOCUMENTS[relative])
            run("09-forget-fixture-receipt", "pkgutil", "--forget", FIXTURE_ID)
            passed("Repeated document-only removal preserves modified docs, license notices, personal presets, IRs and project fixtures.")
            # Preserved fixture documents remain deliberately. The disposable
            # runner is discarded; no recursive product-directory purge occurs.
            report["success"] = True
        return 0
    except Exception as error:
        report["error"] = str(error)
        print("FAIL / NOT EXECUTED: " + str(error))
        return 1
    finally:
        (output / "MacOSDocumentUpgradeVerification.json").write_text(
            json.dumps(report, indent=2) + "\n", encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--disposable-ci", action="store_true")
    parser.add_argument("--output", type=Path, default=ROOT / "build/document-upgrade/macos-pkg-fixture")
    args = parser.parse_args()
    return verify(args.output, disposable_ci=args.disposable_ci)


if __name__ == "__main__":
    raise SystemExit(main())
