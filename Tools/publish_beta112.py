#!/usr/bin/env python3
"""Publish Beta 1.1.2 only from the successful build jobs of this exact release run.

Reuse the established draft/upload/hash/public-discovery implementation. Never
replace old assets, move existing tags, or include personal capture bundles.
"""
import json
import os
from pathlib import Path
import tempfile
import zipfile

import publish_open_beta as transport
import chimera_version

ROOT = Path(__file__).resolve().parents[1]
IDENTITY = chimera_version.identity(ROOT)
VERSION = IDENTITY["version"]
TITLE = "SpectralForge Chimera — Open Beta 1.1.2 · Day-one patch"
BRANCH = "release/open-beta-1.1.2"
ARTIFACT_NAME = f"SpectralForge-Chimera-{VERSION}-Release-Candidate"
SUFFIXES = ("win64-Setup.exe", "win64.zip", "macos-universal.pkg",
            "linux-x86_64.deb", "linux-x86_64.tar.gz")
DOCUMENTS = ("SHA256SUMS.txt", "update-beta.json", "MANUAL.html", "INSTALLATION.md",
             "THIRD_PARTY_NOTICES.md", "candidate-source.json", "InstallerVerification.txt",
             "macOS-package-verification.txt", "Linux-package-verification.txt")
require = transport.require
sha256 = transport.sha256


def prepare_assets(candidate, head, run_id):
    source = json.loads((candidate / "candidate-source.json").read_text())
    require(source["version"] == VERSION and source["tag"] == "v" + VERSION
            and source["revision"] == head and str(source["runId"]) == str(run_id)
            and source["published"] is False and source["publisherSigned"] is False
            and source["macOSNotarized"] is False, "Candidate source identity mismatch")
    names = {f"SpectralForge-Chimera-{VERSION}-{suffix}" for suffix in SUFFIXES}
    sums = {}
    for line in (candidate / "SHA256SUMS.txt").read_text(encoding="utf-8-sig").splitlines():
        digest, name = line.split(maxsplit=1)
        name = name.lstrip("*").strip()
        require(name not in sums, "Duplicate checksum entry")
        sums[name] = digest.lower()
    require(set(sums) == names, "Exactly five public binary packages are required")
    for name, digest in sums.items():
        require(sha256(candidate / name) == digest, f"Binary checksum mismatch: {name}")
        sidecar = (candidate / (name + ".sha256.txt")).read_text(encoding="utf-8-sig").split()
        require(sidecar[0].lower() == digest, f"Sidecar checksum mismatch: {name}")
    manifest = json.loads((candidate / "update-beta.json").read_text())
    require(manifest["schema"] == 1 and manifest["version"] == VERSION
            and manifest["channel"] == "beta" and manifest["releaseUrl"] == transport.RELEASE_URL,
            "Update manifest identity mismatch")
    expected = {("windows", "x86_64"): "win64-Setup.exe",
                ("macos", "universal"): "macos-universal.pkg",
                ("linux", "x86_64"): "linux-x86_64.deb"}
    require(len(manifest["assets"]) == 3, "Update manifest must contain three platforms")
    seen = set()
    for asset in manifest["assets"]:
        key = (asset["platform"], asset["arch"])
        require(key in expected and key not in seen, "Unexpected or duplicate platform")
        seen.add(key)
        name = f"SpectralForge-Chimera-{VERSION}-{expected[key]}"
        require(asset["name"] == name and asset["url"] == transport.DOWNLOAD_URL + name
                and asset["sha256"] == sums[name] and asset["size"] == (candidate / name).stat().st_size,
                f"Update asset mismatch: {name}")
    # The portable public package must not contain any private IR/NAM captures.
    with zipfile.ZipFile(candidate / f"SpectralForge-Chimera-{VERSION}-win64.zip") as archive:
        require(archive.testzip() is None, "Public Windows ZIP failed CRC verification")
        for name in archive.namelist():
            parts = Path(name).parts
            require("Chimera-Personal-IRs" not in parts and not name.lower().endswith(".nam"),
                    "Private capture found in public package")
            require(not name.lower().endswith((".wav", ".aif", ".aiff")) or "reference-audio" in parts,
                    "Unexpected external audio in public package")
        payload = json.loads(archive.read("payload-manifest.json"))
        require(payload["version"] == VERSION, "Portable payload version mismatch")
    assets = {name: candidate / name for name in sorted(names | set(DOCUMENTS))}
    for name in ("OPEN_BETA_RELEASE_NOTES.md", "STUDIO_ONE_TEARDOWN.md", "AMP_NATIVE_DSP.md",
                 "POST_NATIVE_DSP.md", "PEDAL_BOARD_DSP.md", "EXTERNAL_BASS_IRS.md",
                 "NATIVE_NAM_CALIBRATION.md"):
        assets[name] = ROOT / "docs" / ("OPEN_BETA_1_1_2_RELEASE_NOTES.md" if name == "OPEN_BETA_RELEASE_NOTES.md" else name)
    assets["Trace-Chimera-Session.ps1"] = ROOT / "Tools/Trace-Chimera-Session.ps1"
    assets["COPYRIGHT.txt"] = ROOT / "COPYRIGHT.txt"
    for name, path in assets.items():
        require(path.is_file() and path.stat().st_size > 0, f"Missing public file: {name}")
    notes = assets["OPEN_BETA_RELEASE_NOTES.md"].read_text().strip()
    require(VERSION in notes and "Studio One" in notes, "Incomplete release notes")
    return notes, assets


def main():
    head = os.environ["GITHUB_SHA"]
    require(IDENTITY["product_version"] == "1.1.2" and VERSION == "1.1.2-beta.1", "Publisher is pinned to Open Beta 1.1.2 identity")
    run_id = int(os.environ["GITHUB_RUN_ID"])
    require(os.environ["GITHUB_REPOSITORY"] == transport.REPO
            and os.environ["GITHUB_REF"] == "refs/heads/" + BRANCH,
            "Publication requires the authorized Beta 1.1.2 branch")
    api = transport.GitHub()
    run = api.json(f"/actions/runs/{run_id}")
    require(run["head_sha"] == head and run["head_branch"] == BRANCH
            and run["event"] in ("push", "workflow_dispatch"), "Release workflow identity mismatch")
    current = api.json("/git/ref/heads/" + BRANCH)
    require(current["object"]["sha"] == head, "A newer release source supersedes this build")
    jobs = api.pages(f"/actions/runs/{run_id}/jobs", "jobs")
    for suffix in ("build (windows-latest)", "build (ubuntu-22.04)", "build (macos-15)", "assemble-candidate"):
        matches = [job for job in jobs if job["name"].split(" / ")[-1] == suffix]
        require(len(matches) == 1 and matches[0]["status"] == "completed"
                and matches[0]["conclusion"] == "success", f"Required build gate failed: {suffix}")
    # The distinct Setup workflow also exercises preparation contracts and real
    # install/repair/uninstall. Its exact head must pass before publication.
    candidate_runs = api.json(f"/actions/workflows/update-candidate.yml/runs?head_sha={head}&event=pull_request&per_page=100")["workflow_runs"]
    accepted = [run for run in candidate_runs if run["head_sha"] == head
                and run["status"] == "completed" and run["conclusion"] == "success"]
    require(bool(accepted), "Exact-source Windows candidate/preparation workflow has not passed")
    candidate_jobs = api.pages(f"/actions/runs/{accepted[0]['id']}/jobs", "jobs")
    for name in ("windows-candidate", "preparation-contracts"):
        matches = [job for job in candidate_jobs if job["name"] == name]
        require(len(matches) == 1 and matches[0]["status"] == "completed"
                and matches[0]["conclusion"] == "success", f"Candidate gate failed: {name}")
    artifacts = api.pages(f"/actions/runs/{run_id}/artifacts", "artifacts")
    matches = [artifact for artifact in artifacts if artifact["name"] == ARTIFACT_NAME]
    require(len(matches) == 1, "Expected one release candidate artifact")
    artifact = matches[0]
    require(not artifact["expired"] and artifact["workflow_run"]["head_sha"] == head
            and artifact["workflow_run"]["id"] == run_id
            and artifact.get("digest", "").startswith("sha256:"), "Candidate provenance mismatch")
    transport.VERSION = VERSION
    transport.TAG = "v" + VERSION
    transport.TITLE = TITLE
    transport.HEAD = head
    transport.RELEASE_URL = f"https://github.com/{transport.REPO}/releases/tag/v{VERSION}"
    transport.DOWNLOAD_URL = f"https://github.com/{transport.REPO}/releases/download/v{VERSION}/"
    transport.ARCHIVE_SIZE = artifact["size_in_bytes"]
    transport.ARCHIVE_SHA = artifact["digest"].split(":", 1)[1]
    with tempfile.TemporaryDirectory(prefix="chimera-beta112-") as temporary:
        working = Path(temporary)
        archive = working / "candidate.zip"
        api.download(f"/actions/artifacts/{artifact['id']}/zip", archive)
        candidate = working / "candidate"
        candidate.mkdir()
        transport.extract_candidate(archive, candidate)
        notes, assets = prepare_assets(candidate, head, run_id)
        # Include immutable CI provenance without changing the reviewed release notes file.
        notes += f"\n\nBuild: [`{head[:10]}`](https://github.com/{transport.REPO}/commit/{head}) · [Verified CI](https://github.com/{transport.REPO}/actions/runs/{run_id})."
        release = transport.publish(api, notes, assets)
        print(f"Published and publicly verified: {release['html_url']}")
        if os.environ.get("GITHUB_STEP_SUMMARY"):
            with open(os.environ["GITHUB_STEP_SUMMARY"], "a") as stream:
                stream.write(f"## Open Beta 1.1.2 published\n\n{release['html_url']}\n\nSource `{head}`; all three platform builds and package checks passed.\n")


if __name__ == "__main__":
    main()
