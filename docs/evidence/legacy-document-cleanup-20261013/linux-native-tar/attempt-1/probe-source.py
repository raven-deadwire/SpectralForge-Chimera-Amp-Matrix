#!/usr/bin/env python3
"""Run the real TAR installer only in an empty, unchanged account HOME.

The exact public beta.1 archive supplies the baseline. The successor is an
internal fixture with the same application/plugin bytes, current documents,
and the current native helper/install.sh. The application is never launched.
HOME is inherited without modification and must match the account database.
Any existing Chimera target, unsafe parent, or missing library blocks the run.
Only this probe's finite, hash-verified files are removed during restoration.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import platform
import pwd
import re
import shutil
import stat
import subprocess
import tarfile
import tempfile

import legacy_document_cleanup as cleanup
from package_release import PUBLIC_DOCUMENTS

ROOT = Path(__file__).resolve().parents[1]
BASELINE_NAME = "SpectralForge-Chimera-1.3.1-beta.1-linux-x86_64.tar.gz"
BASELINE_SHA256 = "7b2bd5ecb84b882340af019af3b4c8aeca48f5af9393909167ab22e0c6a290a9"
BASELINE_SIZE = 234056208
BASELINE_SOURCE = "7b20cd9d1f90e3e2969c250712b199e893cf50cd"
PREFIX = "SpectralForge-Chimera-1.3.1-beta.1-linux-x86_64"
DOCREL = ".local/share/doc/chimera-amp-matrix"
DOCSTAGE = "usr/share/doc/chimera-amp-matrix"
TARGETS = (
    ".local/bin/chimera-amp-matrix",
    ".vst3/SpectralForge Chimera.vst3",
    DOCREL, DOCREL + ".preserved", DOCREL + ".pending",
    ".local/share/applications/chimera-amp-matrix.desktop",
    ".local/share/pixmaps/chimera-amp-matrix.png",
    ".local/libexec/chimera-document-maintenance",
)


def digest(content: bytes) -> str:
    return hashlib.sha256(content).hexdigest()


def file_digest(path: Path) -> str:
    with path.open("rb") as source:
        return hashlib.file_digest(source, "sha256").hexdigest()


def require(condition: bool, message: str) -> None:
    if not condition:
        raise RuntimeError(message)


def input_hashes() -> dict[str, str]:
    paths = ["Tools/verify_linux_tar_document_upgrade.py", "Tools/install-linux.sh",
             "Tools/legacy_document_cleanup.c", "Tools/legacy_document_cleanup.py",
             "Tools/package_release.py", "Validation/legacy-document-inventory.json",
             "Validation/public-presentation-policy.json", "COPYRIGHT.txt",
             "docs/reference/artwork-manifest.json", "docs/reference/manifest.json",
             *("docs/" + name for name in PUBLIC_DOCUMENTS)]
    return {name: file_digest(ROOT / name) for name in paths}


def input_blobs(paths: dict[str, str]) -> dict[str, str]:
    result = {}
    for name in paths:
        content = (ROOT / name).read_bytes()
        header = b"blob " + str(len(content)).encode("ascii") + b"\0"
        result[name] = hashlib.sha1(header + content, usedforsecurity=False).hexdigest()
    return result


def parent_names() -> list[str]:
    names = set()
    for target in TARGETS:
        names.update(str(p) for p in PurePosixPath(target).parents if str(p) != ".")
    return sorted(names, key=lambda name: (len(PurePosixPath(name).parts), name))


def inspect_home(home: Path) -> dict:
    require(home.is_absolute() and home != Path("/"), "Invalid account HOME")
    for path in reversed((home, *home.parents)):
        require(path.is_dir() and not path.is_symlink(), "Unsafe account HOME ancestor")
    parents = {}
    for name in parent_names():
        path = home / name
        if not os.path.lexists(path):
            parents[name] = None
            continue
        info = path.lstat()
        require(stat.S_ISDIR(info.st_mode), "Installation parent is linked or not a directory: " + name)
        parents[name] = {"device": info.st_dev, "inode": info.st_ino,
                         "uid": info.st_uid, "gid": info.st_gid,
                         "mode": stat.S_IMODE(info.st_mode)}
    present = [name for name in TARGETS if os.path.lexists(home / name)]
    legacy = ".vst3/Chimera Amp Matrix.vst3"
    if os.path.lexists(home / legacy):
        present.append(legacy)
    return {"home": str(home), "existing_chimera_paths": present, "parents": parents}


def extract_verified_archive(archive: Path, destination: Path) -> Path:
    require(archive.is_file() and not archive.is_symlink(), "Baseline archive is absent or linked")
    require(archive.stat().st_size == BASELINE_SIZE and file_digest(archive) == BASELINE_SHA256,
            "Baseline is not the original public beta.1 TAR archive")
    with tarfile.open(archive, "r:gz") as source:
        seen = set()
        for member in source:
            path = PurePosixPath(member.name)
            require(not path.is_absolute() and ".." not in path.parts and path.parts[0] == PREFIX,
                    "Unsafe baseline archive member")
            require(member.name not in seen, "Duplicate baseline archive member")
            seen.add(member.name)
            target = destination.joinpath(*path.parts)
            if member.isdir():
                target.mkdir(parents=True, exist_ok=True)
            else:
                require(member.isfile(), "Unexpected link or special file in original TAR")
                target.parent.mkdir(parents=True, exist_ok=True)
                with source.extractfile(member) as reader, target.open("xb") as writer:
                    shutil.copyfileobj(reader, writer)
                target.chmod(member.mode & 0o777)
    return destination / PREFIX


def pin_parent(home_fd: int, relative: str) -> tuple[int, str]:
    parts = PurePosixPath(relative).parts
    parent = os.dup(home_fd)
    try:
        for part in parts[:-1]:
            next_fd = os.open(part, os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW,
                              dir_fd=parent)
            os.close(parent)
            parent = next_fd
        return parent, parts[-1]
    except Exception:
        os.close(parent)
        raise


def restore_owned_paths(home: Path, allowed: dict[str, set[tuple[int, str]]],
                        initial: dict, report: dict) -> None:
    """Remove only declared fixture bytes, using pinned parents and quarantine."""
    # The production helper retains captured inodes as well as snapshots.
    # Add only its bounded per-invocation path shape underneath an already
    # declared hash directory, with the same declared filename and byte hashes.
    # Unknown contents or additional names still fail the complete preflight.
    preserved = home / (DOCREL + ".preserved")
    retained = []
    for relative, fingerprints in list(allowed.items()):
        prefix = DOCREL + ".preserved/"
        if not relative.startswith(prefix):
            continue
        sha, name = relative[len(prefix):].split("/", 1)
        parent = preserved / sha
        if not os.path.lexists(parent):
            continue
        require(parent.is_dir() and not parent.is_symlink(), "Unsafe preserved hash directory")
        for candidate in parent.iterdir():
            match = re.fullmatch(r"\.open-inode-([1-9][0-9]*)-([0-9]+)", candidate.name)
            if match is None:
                continue
            require(int(match[2]) < len(PUBLIC_DOCUMENTS) + 3,
                    "Unexpected helper retention index")
            require(candidate.is_dir() and not candidate.is_symlink(), "Unsafe retained-inode directory")
            target = candidate / name
            if os.path.lexists(target):
                target_relative = target.relative_to(home).as_posix()
                allowed.setdefault(target_relative, set()).update(fingerprints)
                retained.append(target_relative)
    report["declared_retained_inode_housekeeping_paths"] = sorted(set(retained))
    permitted_dirs = set(TARGETS[1:5])
    for name in allowed:
        permitted_dirs.update(str(p) for p in PurePosixPath(name).parents if str(p) != ".")
    files, directories = [], set()
    for relative in TARGETS:
        path = home / relative
        if not os.path.lexists(path):
            continue
        info = path.lstat()
        require(not stat.S_ISLNK(info.st_mode), "Refusing to restore through a linked fixture target")
        if stat.S_ISREG(info.st_mode):
            files.append(relative)
        elif stat.S_ISDIR(info.st_mode):
            directories.add(relative)
            for base, names, leaves in os.walk(path, followlinks=False):
                for name in names:
                    child = Path(base) / name
                    child_relative = child.relative_to(home).as_posix()
                    require(child_relative in permitted_dirs and not child.is_symlink(),
                            "Unknown/linked directory appeared; it will not be deleted: " + child_relative)
                    directories.add(child_relative)
                files.extend((Path(base) / name).relative_to(home).as_posix() for name in leaves)
        else:
            raise RuntimeError("Unknown special file appeared in a fixture target")
    # Validate the complete finite file set before deleting any fixture file.
    for relative in files:
        path = home / relative
        info = path.lstat()
        require(stat.S_ISREG(info.st_mode) and info.st_nlink == 1,
                "Refusing to delete a linked/special fixture file: " + relative)
        require((info.st_size, file_digest(path)) in allowed.get(relative, set()),
                "Unknown or modified bytes appeared; they will not be deleted: " + relative)
    home_fd = os.open(home, os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW)
    removed = []
    try:
        for index, relative in enumerate(sorted(files)):
            parent, name = pin_parent(home_fd, relative)
            temporary = ".chimera-probe-cleanup-" + str(os.getpid()) + "-" + str(index)
            captured = -1
            try:
                before = os.stat(name, dir_fd=parent, follow_symlinks=False)
                os.mkdir(temporary, 0o700, dir_fd=parent)
                captured = os.open(temporary, os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW,
                                   dir_fd=parent)
                os.rename(name, "candidate", src_dir_fd=parent, dst_dir_fd=captured)
                verified = False
                try:
                    candidate = os.open("candidate", os.O_RDONLY | os.O_NOFOLLOW, dir_fd=captured)
                    with os.fdopen(candidate, "rb") as source:
                        info = os.fstat(source.fileno())
                        fingerprint = (info.st_size, hashlib.file_digest(source, "sha256").hexdigest())
                    verified = (stat.S_ISREG(info.st_mode) and info.st_nlink == 1
                                and (info.st_dev, info.st_ino) == (before.st_dev, before.st_ino)
                                and fingerprint in allowed.get(relative, set()))
                finally:
                    if not verified:
                        # Exclusive link restoration cannot overwrite a file
                        # that appeared after capture. Retain the candidate if
                        # restoration is obstructed, and report incomplete cleanup.
                        os.link("candidate", name, src_dir_fd=captured, dst_dir_fd=parent,
                                follow_symlinks=False)
                        os.unlink("candidate", dir_fd=captured)
                require(verified, "Fixture bytes changed during restoration: " + relative)
                os.unlink("candidate", dir_fd=captured)
                removed.append(relative)
            finally:
                if captured >= 0:
                    os.close(captured)
                    os.rmdir(temporary, dir_fd=parent)
                os.close(parent)
        for relative in sorted(directories, key=lambda name: len(PurePosixPath(name).parts), reverse=True):
            parent, name = pin_parent(home_fd, relative)
            try:
                os.rmdir(name, dir_fd=parent)
            finally:
                os.close(parent)
        for relative, state in reversed(list(initial["parents"].items())):
            if state is None and os.path.lexists(home / relative):
                parent, name = pin_parent(home_fd, relative)
                try:
                    os.rmdir(name, dir_fd=parent)  # Never remove a non-empty ancestor.
                finally:
                    os.close(parent)
    finally:
        os.close(home_fd)
        report["fixture_housekeeping_removed_files"] = removed


def verify(archive: Path, output: Path) -> int:
    output = output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    report = {
        "schema": "spectralforge.linux-tar-document-fixture.v1", "success": False,
        "scope": "Real public TAR install.sh baseline in an initially empty actual account HOME, followed by an internal document fixture.",
        "baseline": {"name": BASELINE_NAME, "size": BASELINE_SIZE,
                     "sha256": BASELINE_SHA256, "source_sha": BASELINE_SOURCE},
        "actual_baseline_shell_install_verified": False,
        "fixture_shell_upgrade_verified": False, "fixture_shell_repair_verified": False,
        "document_only_removal_verified": False, "application_uninstaller_verified": False,
        "next_release_product_installer_verified": False,
        "application_or_plugin_executed": False, "daw_audio_verified": False,
        "home_override_used": False, "dependencies_installed": False,
        "accounts_or_global_settings_changed": False, "publication_approved": False,
        "home_scoped_targets_restored": False, "source_inputs_unchanged": None,
        "shell_install_invocations": 0, "shell_install_successes": 0,
        "checks": [], "byte_assertions": [],
        "limitations": [
            "HOME is the real account HOME; an existing Chimera target blocks execution.",
            "The successor fixture retains original beta.1 app/plugin binaries and is not a beta.2 product installer.",
            "ldd checks linked library resolution; application startup and DAW/audio behavior are not tested.",
            "Removal exercises the document helper only; final removal of other probe-created files is test housekeeping, not a product uninstaller.",
            "Restoration covers the finite initially absent Chimera paths and new empty ancestors; directory timestamps naturally change during installation.",
        ],
    }
    initial_inputs, allowed = {}, {}
    initial = None
    mutated = False
    home = None

    def run(label: str, command: list[str], install: bool = False) -> subprocess.CompletedProcess:
        if install:
            report["shell_install_invocations"] += 1
        # No env argument: HOME and all other account variables are inherited.
        result = subprocess.run(command, capture_output=True, text=True, timeout=180)
        (output / (label + ".log")).write_text(result.stdout + result.stderr, encoding="utf-8")
        if install and result.returncode == 0:
            report["shell_install_successes"] += 1
        require(result.returncode == 0, "Command failed: " + label + "\n" + result.stderr[-4000:])
        return result

    def passed(message: str) -> None:
        report["checks"].append(message)
        print("PASS: " + message, flush=True)

    def allow(relative: str, content: bytes) -> None:
        allowed.setdefault(relative, set()).add((len(content), digest(content)))

    def allow_document(relative: str, content: bytes) -> None:
        allow(DOCREL + "/" + relative, content)
        allow(DOCREL + ".preserved/" + digest(content) + "/" + relative, content)

    def expect(path: Path, content: bytes, label: str) -> None:
        require(path.is_file() and not path.is_symlink() and path.read_bytes() == content,
                "Installed/preserved bytes differ: " + label)
        report["byte_assertions"].append({"check": label, "size": len(content), "sha256": digest(content)})

    try:
        require(platform.system() == "Linux", "Native TAR verification requires Linux")
        require("HOME" in os.environ and os.environ["HOME"], "Account HOME is unset")
        home = Path(os.environ["HOME"])
        require(home == Path(pwd.getpwuid(os.getuid()).pw_dir),
                "HOME differs from the account database; refusing an overridden HOME")
        for tool in ("sh", "ldd", "readelf", "cc", "git", "install", "cp", "mkdir", "grep", "sed"):
            require(shutil.which(tool) is not None, "Missing native probe tool: " + tool)
        initial = inspect_home(home)
        report["initial_home_state"] = initial
        require(not initial["existing_chimera_paths"], "Existing Chimera target blocks the native TAR probe")
        initial_inputs = input_hashes()
        report["source_input_sha256"] = initial_inputs
        report["source_input_git_blob_sha1"] = input_blobs(initial_inputs)
        report["fixture_source_sha"] = subprocess.check_output(["git", "-C", str(ROOT), "rev-parse", "HEAD"], text=True).strip()
        report["fixture_source_dirty"] = bool(subprocess.check_output(["git", "-C", str(ROOT), "status", "--porcelain"], text=True).strip())
        with tempfile.TemporaryDirectory(prefix="chimera-tar-document-fixture-") as temporary:
            work = Path(temporary)
            baseline = extract_verified_archive(archive, work / "original")
            original_script = (baseline / "install.sh").read_bytes()
            (output / "baseline-install.sh.txt").write_bytes(original_script)
            report["baseline_install_script_sha256"] = digest(original_script)
            program = baseline / "usr/bin/chimera-amp-matrix"
            headers = run("00-elf-program-headers", ["readelf", "--program-headers", str(program)])
            require("/lib64/ld-linux-x86-64.so.2" in headers.stdout, "Unexpected original ELF interpreter")
            libraries = run("00-ldd-standalone", ["ldd", str(program)])
            missing = [line.strip() for line in libraries.stdout.splitlines() if "not found" in line]
            report["ldd_missing_libraries"] = missing
            require(not missing, "Missing runtime libraries block the native TAR probe")
            run("00-baseline-shell-syntax", ["sh", "-n", str(baseline / "install.sh")])
            original_docs = {p.relative_to(baseline / DOCSTAGE).as_posix(): p.read_bytes()
                             for p in (baseline / DOCSTAGE).rglob("*") if p.is_file()}
            require(len(original_docs) == 20, "Original TAR document inventory differs")
            original_runtime = {p.relative_to(baseline).as_posix(): file_digest(p)
                                for p in (baseline / "usr").rglob("*") if p.is_file()
                                and not p.is_relative_to(baseline / DOCSTAGE)}
            fixture = work / "internal-document-fixture"
            shutil.copytree(baseline, fixture)
            shutil.rmtree(fixture / DOCSTAGE)
            docstage = fixture / DOCSTAGE
            docstage.mkdir(parents=True)
            for name in PUBLIC_DOCUMENTS:
                shutil.copy2(ROOT / "docs" / name, docstage / name)
            shutil.copy2(ROOT / "COPYRIGHT.txt", docstage / "COPYRIGHT.txt")
            (docstage / "Verification.txt").write_text(
                "Internal TAR document lifecycle fixture only.\n"
                "Original beta.1 application/plugin bytes are unchanged and are not executed.\n"
                "No beta.2 product release or DAW acceptance is claimed.\n", encoding="utf-8")
            current_docs = {p.name: p.read_bytes() for p in docstage.iterdir() if p.is_file()}
            helper = cleanup.build_helper(fixture / "usr/libexec/chimera-document-maintenance",
                                          platform="linux", current_documents=docstage)
            # Match production ordering: the helper is built before this
            # copy-only metadata file, which includes the helper's own hash.
            manifest = {
                "internal_document_fixture_only": True,
                "source_sha": report["fixture_source_sha"],
                "next_release_product_installer_verified": False,
                "files": [{"path": p.relative_to(fixture).as_posix(),
                           "bytes": p.stat().st_size, "sha256": file_digest(p)}
                          for p in sorted((fixture / "usr").rglob("*")) if p.is_file()],
            }
            (docstage / "payload-manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
            current_docs["payload-manifest.json"] = (docstage / "payload-manifest.json").read_bytes()
            shutil.copy2(ROOT / "Tools/install-linux.sh", fixture / "install.sh")
            run("00-fixture-shell-syntax", ["sh", "-n", str(fixture / "install.sh")])
            (output / "fixture-install.sh.txt").write_bytes((fixture / "install.sh").read_bytes())
            for name, fingerprint in original_runtime.items():
                require(file_digest(fixture / name) == fingerprint, "Fixture changed historical runtime: " + name)
            report["fixture"] = {"native_helper_sha256": file_digest(helper),
                                 "install_script_sha256": file_digest(fixture / "install.sh"),
                                 "current_document_count": len(current_docs),
                                 "historical_non_document_payload_sha256": original_runtime,
                                 "historical_non_document_payload_unchanged": True}

            runtime = {TARGETS[0]: program.read_bytes(),
                       TARGETS[6]: (baseline / "usr/share/pixmaps/chimera-amp-matrix.png").read_bytes(),
                       TARGETS[7]: helper.read_bytes()}
            vst = baseline / "usr/lib/vst3/SpectralForge Chimera.vst3"
            for path in vst.rglob("*"):
                if path.is_file():
                    runtime[TARGETS[1] + "/" + path.relative_to(vst).as_posix()] = path.read_bytes()
            launch = str(home / TARGETS[0])
            for before, after in (("\\", "\\\\"), ('"', '\\"'), ("`", "\\`"), ("$", "\\$"), ("%", "%%")):
                launch = launch.replace(before, after)
            desktop = (baseline / "usr/share/applications/chimera-amp-matrix.desktop").read_text()
            runtime[TARGETS[5]] = "".join(('Exec="' + launch + '"' if line.startswith("Exec=") else line) + "\n"
                                          for line in desktop.splitlines()).encode()
            for name, content in runtime.items():
                allow(name, content)
            for documents in (original_docs, current_docs):
                for name, content in documents.items():
                    allow_document(name, content)
            edited_legacy = original_docs["AMP_VALIDATION.md"] + b"\nUser's retained TAR legacy annotations.\n"
            edited_manual = original_docs["MANUAL.html"] + b"\n<!-- User's retained TAR manual annotations. -->\n"
            repair_manual = current_docs["MANUAL.html"] + b"\n<!-- Another user revision before TAR repair. -->\n"
            for name, data in (("AMP_VALIDATION.md", edited_legacy), ("MANUAL.html", edited_manual), ("MANUAL.html", repair_manual)):
                allow_document(name, data)
            personal = {"personal.chimera": b"Synthetic personal preset for the TAR probe.\n",
                        "personal.wav": b"RIFF\0\0\0\0WAVEsynthetic-private-IR\n",
                        "existing-project.song": b"Synthetic pre-existing project in the TAR fixture.\n",
                        "user-notes.md": b"Personal notes unrelated to package ownership.\n"}
            for name, data in personal.items():
                allow_document(name, data)
            inventory = json.loads((ROOT / "Validation/legacy-document-inventory.json").read_text())
            entries = {entry["path"]: entry for entry in inventory["files"]}
            carryovers = {}
            for name in ("reference/artwork-manifest.json", "reference/manifest.json"):
                data = (ROOT / "docs" / name).read_bytes()
                require(any("linux" in v["platforms"] and v["size"] == len(data) and v["sha256"] == digest(data)
                            for v in entries[name]["variants"]), "Carryover is not measured public Linux bytes")
                carryovers[name] = data
                allow_document(name, data)
            require(initial_inputs == input_hashes(), "Source changed before native execution")
            require(inspect_home(home) == initial, "Account installation paths changed during preflight")
            passed("Original archive/script, real account HOME, absent product targets, and all linked libraries passed preflight without dependency or environment changes.")

            mutated = True
            run("01-public-baseline-install", ["sh", str(baseline / "install.sh")], install=True)
            docs = home / DOCREL
            for name, data in original_docs.items():
                expect(docs / name, data, "baseline/document/" + name)
            for name, data in runtime.items():
                if name != TARGETS[7]:
                    expect(home / name, data, "baseline/runtime/" + name)
            report["actual_baseline_shell_install_verified"] = True
            passed("Actual public beta.1 install.sh completed in the unchanged real HOME; all 20 documents and installed runtime bytes match.")
            (docs / "AMP_VALIDATION.md").write_bytes(edited_legacy)
            (docs / "MANUAL.html").write_bytes(edited_manual)
            for name, data in {**personal, **carryovers}.items():
                destination = docs / name
                destination.parent.mkdir(parents=True, exist_ok=True)
                destination.write_bytes(data)
            run("02-internal-fixture-upgrade", ["sh", str(fixture / "install.sh")], install=True)
            expect(docs / "AMP_VALIDATION.md", edited_legacy, "upgrade/edited-legacy")
            expect(docs / "MANUAL.html", current_docs["MANUAL.html"], "upgrade/current-manual")
            preserved = Path(str(docs) + ".preserved")
            for name, data in (("MANUAL.html", edited_manual),
                               ("COPYRIGHT.txt", original_docs["COPYRIGHT.txt"]),
                               ("THIRD_PARTY_NOTICES.md", original_docs["THIRD_PARTY_NOTICES.md"])):
                expect(preserved / digest(data) / name, data, "upgrade/preserved/" + name)
            retired = [name for name in original_docs if entries[name]["retire"] and name != "AMP_VALIDATION.md"]
            for name in (*retired, "reference/artwork-manifest.json"):
                require(not (docs / name).exists(), "Original retired research file survived TAR upgrade: " + name)
            expect(docs / "reference/manifest.json", carryovers["reference/manifest.json"], "upgrade/licensed-reference")
            for name, data in runtime.items():
                expect(home / name, data, "upgrade/runtime/" + name)
            report["fixture_shell_upgrade_verified"] = True
            passed("Current full install.sh upgraded the actual TAR installation, retired exact research bytes, and preserved edits, historical notices, and licensed metadata.")
            (docs / "MANUAL.html").write_bytes(repair_manual)
            run("03-internal-fixture-repair", ["sh", str(fixture / "install.sh")], install=True)
            expect(docs / "MANUAL.html", current_docs["MANUAL.html"], "repair/current-manual")
            for data in (edited_manual, repair_manual):
                expect(preserved / digest(data) / "MANUAL.html", data, "repair/distinct-user-revision/" + digest(data))
            expect(docs / "AMP_VALIDATION.md", edited_legacy, "repair/edited-legacy")
            report["fixture_shell_repair_verified"] = True
            passed("Repeated full TAR installation retained both distinct user manual revisions and the edited legacy document.")
            (docs / "MANUAL.html").write_bytes(repair_manual)
            run("04-installed-helper-document-removal", [str(home / TARGETS[7]), "remove-documents", str(docs)])
            expect(docs / "MANUAL.html", repair_manual, "remove/edited-manual")
            expect(docs / "AMP_VALIDATION.md", edited_legacy, "remove/edited-legacy")
            for name, data in personal.items():
                expect(docs / name, data, "remove/personal/" + name)
            for name in ("COPYRIGHT.txt", "THIRD_PARTY_NOTICES.md"):
                expect(docs / name, current_docs[name], "remove/current-notice/" + name)
                expect(preserved / digest(original_docs[name]) / name, original_docs[name], "remove/prior-notice/" + name)
            expect(docs / "reference/manifest.json", carryovers["reference/manifest.json"], "remove/licensed-reference")
            require(not (docs / "PRESETS.md").exists(), "Document removal retained an unchanged owned current document")
            require(not Path(str(docs) + ".pending").exists(), "Document removal left a pending transaction")
            for name, data in runtime.items():
                expect(home / name, data, "remove/runtime-outside-document-scope/" + name)
            report["document_only_removal_verified"] = True
            passed("The installed native helper removed owned documents while preserving user documents, presets, IRs, projects, licenses, and all runtime files.")
            report["success"] = True
    except Exception as error:
        report["error"] = str(error)
        print("FAIL / NOT VERIFIED: " + str(error), flush=True)
    finally:
        if mutated and initial is not None and home is not None:
            try:
                restore_owned_paths(home, allowed, initial, report)
                final_state = inspect_home(home)
                report["final_home_state"] = final_state
                report["home_scoped_targets_restored"] = final_state == initial
                require(report["home_scoped_targets_restored"], "Scoped HOME state was not restored")
                passed("All finite probe-created files passed hash verification before housekeeping; all Chimera targets and newly created parents returned to their initial absence.")
            except Exception as error:
                report["restoration_error"] = str(error)
                report["success"] = False
                print("FAIL / NOT RESTORED: " + str(error), flush=True)
        if initial_inputs:
            report["source_inputs_unchanged"] = initial_inputs == input_hashes()
            if not report["source_inputs_unchanged"]:
                report["success"] = False
        if home is not None:
            report["home_environment_unchanged"] = os.environ.get("HOME") == str(home)
        (output / "LinuxTarDocumentUpgradeVerification.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    return 0 if report["success"] else 1


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--baseline-tar", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    return verify(args.baseline_tar, args.output)


if __name__ == "__main__":
    raise SystemExit(main())
