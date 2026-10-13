#!/usr/bin/env python3
"""Exercise real dpkg document lifecycles inside fresh isolated filesystem roots.

The baseline is the exact public 1.3.1-beta.1 Debian package. The successor is
an internal document-lifecycle fixture: historical app/plugin bytes are held
fixed, while current documentation and compiled native hooks are substituted.
This cannot certify the next product installer, application runtime, or DAW.
No installed application is executed, and HOME is never overridden.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import re
import shutil
import subprocess
import tempfile

import legacy_document_cleanup as cleanup
from package_release import PUBLIC_DOCUMENTS

ROOT = Path(__file__).resolve().parents[1]
BASELINE_NAME = "SpectralForge-Chimera-1.3.1-beta.1-linux-x86_64.deb"
BASELINE_SHA256 = "5ca9409186e20e94781f9b8865eb549909c613d09ff635b7f2a23745a0f7fefd"
BASELINE_SIZE = 213313724
BASELINE_SOURCE = "7b20cd9d1f90e3e2969c250712b199e893cf50cd"
BASELINE_VERSION = "1.3.1~beta.1"
FIXTURE_VERSION = "1.3.1~beta.2"
PACKAGE = "chimera-amp-matrix"
DOCREL = Path("usr/share/doc/chimera-amp-matrix")


def digest(value: bytes) -> str:
    return hashlib.sha256(value).hexdigest()


def file_digest(path: Path) -> str:
    with path.open("rb") as source:
        return hashlib.file_digest(source, "sha256").hexdigest()


def require(condition: bool, message: str) -> None:
    if not condition:
        raise RuntimeError(message)


def input_hashes() -> dict[str, str]:
    files = ["Tools/legacy_document_cleanup.c", "Tools/legacy_document_cleanup.py",
             "Tools/verify_linux_document_upgrade.py", "Tools/package_release.py",
             "Validation/legacy-document-inventory.json", "Validation/public-presentation-policy.json",
             "COPYRIGHT.txt", *("docs/" + name for name in PUBLIC_DOCUMENTS)]
    return {name: file_digest(ROOT / name) for name in files}


def input_blob_hashes(paths: dict[str, str]) -> dict[str, str]:
    """Identify the actual working-tree bytes independently of local commit IDs."""
    result = {}
    for name in paths:
        content = (ROOT / name).read_bytes()
        header = b"blob " + str(len(content)).encode("ascii") + b"\0"
        result[name] = hashlib.sha1(header + content, usedforsecurity=False).hexdigest()
    return result


def verify(baseline_deb: Path, output: Path) -> int:
    output = output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    report = {
        "schema": "spectralforge.linux-document-package-fixture.v1",
        "success": False,
        "scope": "Actual public beta.1 dpkg baseline and internal beta.2 document fixture in isolated roots; application/plugin bytes held fixed.",
        "platform": platform.system(),
        "fixture_source_sha": None,
        "fixture_source_dirty": None,
        "source_input_sha256": {},
        "source_input_git_blob_sha1": {},
        "source_inputs_unchanged_during_execution": None,
        "baseline_original_package": {"name": BASELINE_NAME, "sha256": BASELINE_SHA256,
                                      "size": BASELINE_SIZE, "source_sha": BASELINE_SOURCE},
        "actual_baseline_install_verified": False,
        "fixture_native_lifecycle_verified": False,
        "next_release_product_installer_verified": False,
        "application_or_plugin_executed": False,
        "daw_audio_verified": False,
        "publication_approved": False,
        "dependency_resolution_verified": False,
        "dependency_force_used": True,
        "script_chrootless_used": True,
        "home_override_used": False,
        "limitations": [
            "dpkg --root uses new isolated package databases and filesystem roots.",
            "--force-depends is necessary because those roots have no OS dependency database; runtime/dependency acceptance is not tested.",
            "--force-script-chrootless executes only the compiled document hooks with the host C runtime; DPKG_ROOT supplies the isolated target.",
            "The successor package is an internal fixture and retains beta.1 application/plugin bytes; it is not a releasable beta.2 installer.",
        ],
        "native_dpkg_invocations": 0,
        "native_dpkg_successes": 0,
        "checks": [],
        "preservation_assertions": [],
    }
    initial_inputs = {}
    host_status = Path("/var/lib/dpkg/status")
    host_status_before = file_digest(host_status) if host_status.is_file() else None

    def run(label: str, *command: str, env: dict | None = None) -> subprocess.CompletedProcess:
        native = Path(command[0]).name == "dpkg"
        if native:
            report["native_dpkg_invocations"] += 1
        result = subprocess.run(command, capture_output=True, text=True, env=env, timeout=180)
        (output / (label + ".log")).write_text(result.stdout + result.stderr, encoding="utf-8")
        if native and result.returncode == 0:
            report["native_dpkg_successes"] += 1
        require(result.returncode == 0,
                f"Command failed ({result.returncode}): {label}\n{result.stderr[-5000:]}")
        return result

    def passed(message: str) -> None:
        report["checks"].append(message)
        print("PASS: " + message, flush=True)

    def dpkg(label: str, target_root: Path, *action: str) -> subprocess.CompletedProcess:
        require(target_root.is_absolute() and target_root.is_dir() and not target_root.is_symlink(),
                "dpkg target must be a fresh absolute fixture directory")
        require(target_root != Path("/") and target_root.parent == work,
                "Refusing dpkg outside this invocation's isolated fixture root")
        # DPKG_ROOT is also set explicitly for a transparent fixture contract;
        # dpkg's --root sets the same value for its native maintainer scripts.
        environment = {**os.environ, "DPKG_ROOT": str(target_root)}
        return run(label, "dpkg", "--root=" + str(target_root),
                   "--force-script-chrootless", "--force-depends",
                   "--path-include=/usr/share/doc/*", *action, env=environment)

    def expect(path: Path, content: bytes, label: str) -> None:
        require(path.is_file() and not path.is_symlink() and path.read_bytes() == content,
                "Preserved/installed bytes differ: " + label)
        report["preservation_assertions"].append({"check": label, "size": len(content), "sha256": digest(content)})

    def saved(docroot: Path, relative: str, content: bytes, label: str) -> None:
        backup = Path(str(docroot) + ".preserved") / digest(content) / relative
        expect(backup, content, label)

    try:
        require(platform.system() == "Linux", "Real isolated dpkg verification requires Linux")
        require(os.geteuid() == 0, "Real isolated dpkg verification requires root in the task environment")
        for tool in ("dpkg", "dpkg-deb", "dpkg-query", "cc", "git"):
            require(shutil.which(tool) is not None, "Missing fixture tool: " + tool)
        require(not baseline_deb.is_symlink() and baseline_deb.is_file(), "Baseline package is missing or linked")
        baseline_deb = baseline_deb.resolve()
        require(baseline_deb.stat().st_size == BASELINE_SIZE and file_digest(baseline_deb) == BASELINE_SHA256,
                "Baseline is not the pinned original public beta.1 package")
        report["fixture_source_sha"] = subprocess.check_output(
            ["git", "-C", str(ROOT), "rev-parse", "HEAD"], text=True).strip()
        report["fixture_source_dirty"] = bool(subprocess.check_output(
            ["git", "-C", str(ROOT), "status", "--porcelain"], text=True).strip())
        initial_inputs = input_hashes()
        report["source_input_sha256"] = initial_inputs
        report["source_input_git_blob_sha1"] = input_blob_hashes(initial_inputs)
        report["tool_versions"] = {
            "dpkg": subprocess.check_output(["dpkg", "--version"], text=True).splitlines()[0],
            "compiler": subprocess.check_output(["cc", "--version"], text=True).splitlines()[0],
            "python": platform.python_version(), "libc": list(platform.libc_ver()),
        }
        with tempfile.TemporaryDirectory(prefix="chimera-dpkg-document-fixture-") as temporary:
            work = Path(temporary).resolve()
            stage = work / "fixture-payload"
            run("01-extract-original-baseline", "dpkg-deb", "--raw-extract", str(baseline_deb), str(stage))
            original_control_files = sorted(path.name for path in (stage / "DEBIAN").iterdir())
            require(original_control_files == ["control"],
                    "Refusing an unexpected executable hook in the pinned baseline")
            control = (stage / "DEBIAN/control").read_text(encoding="utf-8")
            require(re.search(r"^Package: " + re.escape(PACKAGE) + r"$", control, re.M) is not None
                    and re.search(r"^Version: " + re.escape(BASELINE_VERSION) + r"$", control, re.M) is not None,
                    "Pinned baseline package identity differs")
            original_documents = {path.relative_to(stage / DOCREL).as_posix(): path.read_bytes()
                                  for path in (stage / DOCREL).rglob("*") if path.is_file()}
            original_runtime = {path.relative_to(stage).as_posix(): file_digest(path)
                                for path in stage.rglob("*") if path.is_file()
                                and not path.is_relative_to(stage / DOCREL)
                                and not path.is_relative_to(stage / "DEBIAN")}
            require(len(original_documents) == 20, "Actual baseline document inventory differs")
            shutil.rmtree(stage / DOCREL)
            docstage = stage / DOCREL
            docstage.mkdir(parents=True)
            for name in PUBLIC_DOCUMENTS:
                shutil.copy2(ROOT / "docs" / name, docstage / name)
            shutil.copy2(ROOT / "COPYRIGHT.txt", docstage / "COPYRIGHT.txt")
            (docstage / "Verification.txt").write_text(
                "Internal document lifecycle fixture only.\n"
                "The application/plugin payload remains the original public beta.1 bytes.\n"
                "This fixture is not a product release or DSP/DAW acceptance record.\n", encoding="utf-8")
            current_documents = {path.name: path.read_bytes() for path in docstage.iterdir() if path.is_file()}
            helper = cleanup.build_helper(stage / "usr/libexec/chimera-document-maintenance",
                                          platform="linux", current_documents=docstage)
            require(initial_inputs == input_hashes(), "Source inputs changed while compiling the native fixture")
            for phase in ("preinst", "postinst", "prerm", "postrm"):
                shutil.copy2(helper, stage / "DEBIAN" / phase)
            control = re.sub(r"^Version: .*$", "Version: " + FIXTURE_VERSION, control, flags=re.M)
            control = re.sub(r"^Description: .*$", "Description: Internal document-lifecycle fixture with historical application bytes", control, flags=re.M)
            control += "Pre-Depends: libc6\nX-Chimera-Document-Lifecycle-Fixture: true\n"
            (stage / "DEBIAN/control").write_text(control, encoding="utf-8")
            for name, expected_hash in original_runtime.items():
                require(file_digest(stage / name) == expected_hash, "Fixture changed a historical non-document payload: " + name)
            fixture = work / "document-lifecycle-fixture.deb"
            run("02-build-internal-document-fixture", "dpkg-deb", "-Zgzip", "-z1", "--build",
                "--root-owner-group", str(stage), str(fixture))
            report["fixture"] = {"name": fixture.name, "version": FIXTURE_VERSION,
                                 "sha256": file_digest(fixture), "size": fixture.stat().st_size,
                                 "native_helper_sha256": file_digest(helper),
                                 "current_document_count": len(current_documents),
                                 "historical_non_document_payload_sha256": original_runtime,
                                 "historical_non_document_payload_unchanged": True}
            for phase in ("preinst", "postinst", "prerm", "postrm"):
                require(file_digest(stage / "DEBIAN" / phase) == report["fixture"]["native_helper_sha256"],
                        "Packaged native hook bytes differ: " + phase)
            passed("Original beta.1 DEB digest and full historical application/plugin payload are fixed; only current docs and native hooks form the successor fixture.")

            fresh = work / "fresh-root"
            fresh.mkdir()
            dpkg("03-fresh-fixture-install", fresh, "--install", str(fixture))
            fresh_docs = fresh / DOCREL
            for name, data in current_documents.items():
                expect(fresh_docs / name, data, "fresh/current/" + name)
            dpkg("04-fresh-fixture-remove", fresh, "--remove", PACKAGE)
            for name in ("COPYRIGHT.txt", "THIRD_PARTY_NOTICES.md"):
                expect(fresh_docs / name, current_documents[name], "fresh/remove-notice/" + name)
            require(not (fresh_docs / "MANUAL.html").exists(), "Fresh removal retained an unchanged owned manual")
            passed("Actual dpkg fresh fixture installation and removal execute native hooks and preserve required notices.")

            target = work / "upgrade-root 한글"
            target.mkdir()
            dpkg("05-actual-public-baseline-install", target, "--install", str(baseline_deb))
            docs = target / DOCREL
            for name, data in original_documents.items():
                expect(docs / name, data, "actual-baseline/" + name)
            for name, expected_hash in original_runtime.items():
                require(file_digest(target / name) == expected_hash, "Actual baseline installed bytes differ: " + name)
            report["actual_baseline_install_verified"] = True
            passed("Original public beta.1 DEB was installed by real dpkg in an isolated Unicode path; all 20 document files and historical runtime bytes match.")

            edited_legacy = original_documents["AMP_VALIDATION.md"] + b"\nUser's retained legacy annotations.\n"
            edited_manual = original_documents["MANUAL.html"] + b"\n<!-- User's personal manual annotations. -->\n"
            (docs / "AMP_VALIDATION.md").write_bytes(edited_legacy)
            (docs / "MANUAL.html").write_bytes(edited_manual)
            personal = {
                "personal.chimera": b"Synthetic private preset bytes for the isolated fixture.\n",
                "personal.wav": b"RIFF\x00\x00\x00\x00WAVEfixture-personal-IR\n",
                "existing-project.song": b"Synthetic existing project bytes.\n",
                "user-reference-notes.md": b"Personal document unrelated to the installer.\n",
            }
            for name, data in personal.items():
                (docs / name).write_bytes(data)
            outside = {
                "home/fixture-user/Presets/personal.chimera": personal["personal.chimera"],
                "home/fixture-user/IRs/personal.wav": personal["personal.wav"],
                "home/fixture-user/Projects/existing-project.song": personal["existing-project.song"],
            }
            for name, data in outside.items():
                destination = target / name
                destination.parent.mkdir(parents=True, exist_ok=True)
                destination.write_bytes(data)

            # Simulate carryover from an older genuine package, using only
            # repository bytes that exactly match a measured public variant.
            inventory = json.loads((ROOT / "Validation/legacy-document-inventory.json").read_text())
            entries = {entry["path"]: entry for entry in inventory["files"]}
            carryovers = {}
            for name in ("reference/artwork-manifest.json", "reference/manifest.json"):
                data = (ROOT / "docs" / name).read_bytes()
                require(any("linux" in v["platforms"] and v["size"] == len(data) and v["sha256"] == digest(data)
                            for v in entries[name]["variants"]), "Carryover lacks observed original Linux bytes: " + name)
                destination = docs / name
                destination.parent.mkdir(parents=True, exist_ok=True)
                destination.write_bytes(data)
                carryovers[name] = data
            retired = [name for name in original_documents if entries[name]["retire"]]

            dpkg("06-upgrade-with-native-document-hooks", target, "--install", str(fixture))
            expect(docs / "AMP_VALIDATION.md", edited_legacy, "upgrade/edited-legacy-at-original-path")
            expect(docs / "MANUAL.html", current_documents["MANUAL.html"], "upgrade/current-manual")
            saved(docs, "AMP_VALIDATION.md", edited_legacy, "upgrade/edited-legacy-backup")
            saved(docs, "MANUAL.html", edited_manual, "upgrade/edited-manual-backup")
            for name in ("COPYRIGHT.txt", "THIRD_PARTY_NOTICES.md"):
                saved(docs, name, original_documents[name], "upgrade/prior-release-notice-backup/" + name)
            for name in retired:
                if name != "AMP_VALIDATION.md":
                    require(not (docs / name).exists(), "Unchanged retired document survived upgrade: " + name)
            require(not (docs / "reference/artwork-manifest.json").exists(),
                    "Native helper failed to remove an exact older carryover outside dpkg's beta.1 file list")
            expect(docs / "reference/manifest.json", carryovers["reference/manifest.json"], "upgrade/licensed-reference-record")
            passed("Actual dpkg upgrade retires original documents and an older nested carryover, restores edited legacy bytes, and keeps a verified copy of the edited manual.")

            repair_edit = current_documents["MANUAL.html"] + b"\n<!-- A second user revision before repair. -->\n"
            (docs / "MANUAL.html").write_bytes(repair_edit)
            dpkg("07-same-version-repair", target, "--install", str(fixture))
            expect(docs / "MANUAL.html", current_documents["MANUAL.html"], "repair/current-manual")
            saved(docs, "MANUAL.html", edited_manual, "repair/first-user-revision")
            saved(docs, "MANUAL.html", repair_edit, "repair/second-user-revision")
            expect(docs / "AMP_VALIDATION.md", edited_legacy, "repair/edited-legacy")
            passed("Same-version real dpkg repair installs current documentation and preserves both separate user manual revisions.")

            (docs / "MANUAL.html").write_bytes(repair_edit)
            dpkg("08-remove-fixture", target, "--remove", PACKAGE)
            expect(docs / "MANUAL.html", repair_edit, "remove/edited-current-manual-restored")
            expect(docs / "AMP_VALIDATION.md", edited_legacy, "remove/edited-legacy")
            for name in ("COPYRIGHT.txt", "THIRD_PARTY_NOTICES.md"):
                expect(docs / name, current_documents[name], "remove/required-notice/" + name)
            expect(docs / "reference/manifest.json", carryovers["reference/manifest.json"], "remove/licensed-reference-record")
            for name, data in personal.items():
                expect(docs / name, data, "remove/personal-document-directory/" + name)
            for name, data in outside.items():
                expect(target / name, data, "remove/outside-document-scope/" + name)
            for name in retired:
                if name != "AMP_VALIDATION.md":
                    require(not (docs / name).exists(), "Removed retired document reappeared: " + name)
            saved(docs, "MANUAL.html", edited_manual, "remove/first-user-revision-backup")
            saved(docs, "MANUAL.html", repair_edit, "remove/second-user-revision-backup")
            for name in ("COPYRIGHT.txt", "THIRD_PARTY_NOTICES.md"):
                saved(docs, name, original_documents[name], "remove/prior-release-notice-backup/" + name)
            require(not Path(str(docs) + ".pending").exists(), "Successful lifecycle left a pending preservation journal")
            passed("Real dpkg removal restores modified current docs and keeps modified legacy docs, notices, licensed metadata, personal presets, IRs, and projects.")

            dpkg("09-purge-remaining-package-record", target, "--purge", PACKAGE)
            expect(docs / "MANUAL.html", repair_edit, "purge/edited-manual")
            expect(docs / "AMP_VALIDATION.md", edited_legacy, "purge/edited-legacy")
            for name in ("COPYRIGHT.txt", "THIRD_PARTY_NOTICES.md"):
                expect(docs / name, current_documents[name], "purge/notice/" + name)
            for name, data in personal.items():
                expect(docs / name, data, "purge/personal/" + name)
            for name, data in outside.items():
                expect(target / name, data, "purge/outside-document-scope/" + name)
            passed("Removing the remaining dpkg package record also leaves preserved documents and personal files intact.")
            report["fixture_native_lifecycle_verified"] = True
            report["success"] = True
    except Exception as error:
        report["error"] = str(error)
        print("FAIL / NOT VERIFIED: " + str(error), flush=True)
    finally:
        if initial_inputs:
            report["source_inputs_unchanged_during_execution"] = input_hashes() == initial_inputs
        report["host_dpkg_database_unchanged"] = (
            (file_digest(host_status) if host_status.is_file() else None) == host_status_before)
        for invariant in ("source_inputs_unchanged_during_execution", "host_dpkg_database_unchanged"):
            if report[invariant] is False:
                report["success"] = False
                report.setdefault("evidence_invariant_failures", []).append(invariant)
                print("FAIL / NOT VERIFIED: " + invariant, flush=True)
        (output / "LinuxDocumentUpgradeVerification.json").write_text(
            json.dumps(report, indent=2) + "\n", encoding="utf-8")
    return 0 if report["success"] else 1


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--baseline-deb", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    return verify(args.baseline_deb, args.output)


if __name__ == "__main__":
    raise SystemExit(main())
