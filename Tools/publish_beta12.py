#!/usr/bin/env python3
"""Publish Beta 1.2 only from successful verification of this exact source SHA.

Reuse the established draft/upload/hash/public-discovery implementation. Never
replace old assets, move existing tags, or include personal capture bundles.
"""
import json
import os
from pathlib import Path
import tempfile
import zipfile
import evaluate_release_gate as release_gate
import release_evidence

import publish_open_beta as transport
import chimera_version

ROOT = Path(__file__).resolve().parents[1]
IDENTITY = chimera_version.identity(ROOT)
VERSION = IDENTITY["version"]
TITLE = "SpectralForge Chimera — Open Beta 1.2 · Náströnd"
BRANCH = "release/open-beta-1.2.0"
ARTIFACT_NAME = f"SpectralForge-Chimera-{VERSION}-Release-Candidate"
SUFFIXES = ("win64-Setup.exe", "win64.zip", "macos-universal.pkg",
            "linux-x86_64.deb", "linux-x86_64.tar.gz")
DOCUMENTS = ("SHA256SUMS.txt", "update-beta.json", "MANUAL.html", "INSTALLATION.md",
             "THIRD_PARTY_NOTICES.md", "candidate-source.json", "InstallerVerification.txt",
             "macOS-package-verification.txt", "Linux-package-verification.txt")
require = transport.require
sha256 = transport.sha256


def prepare_assets(candidate, head, run_id):
    source = json.loads((candidate / "candidate-source.json").read_text(encoding="utf-8"))
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
    manifest = json.loads((candidate / "update-beta.json").read_text(encoding="utf-8"))
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
                 "NATIVE_NAM_CALIBRATION.md", "NASTROND_CHANNEL_FEEDBACK.md"):
        assets[name] = ROOT / "docs" / name
    assets["Trace-Chimera-Session.ps1"] = ROOT / "Tools/Trace-Chimera-Session.ps1"
    assets["COPYRIGHT.txt"] = ROOT / "COPYRIGHT.txt"
    for name, path in assets.items():
        require(path.is_file() and path.stat().st_size > 0, f"Missing public file: {name}")
    notes = assets["OPEN_BETA_RELEASE_NOTES.md"].read_text(encoding="utf-8").strip()
    require(VERSION in notes and "Studio One" in notes, "Incomplete release notes")
    return notes, assets


def verified_run(runs, head, events):
    matches = [run for run in runs if run["head_sha"] == head and run["event"] in events]
    require(bool(matches), "Exact-source verification run is missing")
    latest = max(matches, key=lambda run: run["id"])
    require(latest["status"] == "completed" and latest["conclusion"] == "success",
            "Latest exact-source verification run has not passed")
    return latest["id"]


def verify_release_gate_archive(archive, head, profile, run_id, run_attempt):
    """Recompute the consolidated gate with this source's policy before publishing."""
    try:
        policy = release_gate.load_json(ROOT / "Validation/release-policy.json")
        with zipfile.ZipFile(archive) as bundle:
            require(bundle.testzip() is None, "Release-gate ZIP failed CRC verification")
            names = bundle.namelist()
            require(len(names) == len(set(names)), "Duplicate release-gate ZIP entries")
            matches = [name for name in names if Path(name).name == "release-gate.json"]
            require(len(matches) == 1, "Expected one release-gate.json in validation artifact")
            def read_json(name):
                return json.loads(bundle.read(name).decode("utf-8-sig"),
                    object_pairs_hook=release_gate.unique_object,
                    parse_constant=release_gate.invalid_constant)
            verdict = read_json(matches[0])
            expected_revision = release_evidence.identity(head, run_id, run_attempt)
            require(isinstance(verdict, dict) and
                    verdict.get("schema") == "spectralforge.chimera.release-gate" and
                    type(verdict.get("schema_version")) is int and verdict["schema_version"] == 1 and
                    verdict.get("profile") == profile and
                    verdict.get("revision") == expected_revision and
                    verdict.get("policy_version") == policy["policy_version"],
                    "Release-gate identity mismatch")
            require(verdict.get("producer", {}).get("name") == "consolidate_release_gate" and
                    verdict["producer"].get("errors") == [], "Release gate has no valid consolidator provenance")
            root = Path(matches[0]).parent
            checks = {}
            for name in names:
                if Path(name).parent != root / "checks" or not name.endswith(".json"):
                    continue
                check = read_json(name)
                release_gate.validate_check(check, name)
                require(check["id"] not in checks, "Duplicate release check")
                # Source and policy binding cannot be disabled inside a report.
                require(check["evidence"].get("commit_sha") == head and
                        check["evidence"].get("policy_version") == policy["policy_version"] and
                        check["policy"].get("requires_current_commit") is True and
                        check["policy"].get("requires_policy_version") is True,
                        "Release check identity mismatch")
                stage, _ = release_evidence.producer.locate_check(policy, check["id"])
                require(check["stage"] == stage and check["policy"].get("required") is True
                        and check["policy"].get("hard_gate") is (check["id"] in policy["hard_gates"]),
                        "Release check policy mismatch")
                checks[check["id"]] = check
            expected = {cid for stage in policy["profiles"][profile]["required_stages"]
                        for cid in policy["stages"][stage]["required_checks"]}
            require(set(checks) == expected, "Release check inventory mismatch")
            computed = release_gate.evaluate_release(policy,
                release_gate.load_json(ROOT / "Validation/waivers.json"), checks, profile, head)
            for field in ("verdict", "ready", "counts", "hard_gates", "checks", "stages", "blockers"):
                require(json.dumps(verdict.get(field), sort_keys=True) == json.dumps(computed[field], sort_keys=True),
                        "Release-gate summary differs from check evidence")
            require(computed["verdict"] == "PASS" and computed["ready"] is True
                    and computed["counts"]["checks"]["blocked"] == 0
                    and computed["hard_gates"]["blocked"] == 0 and not computed["blockers"],
                    "Release-gate verdict is not PASS")
    except (KeyError, TypeError, ValueError, AttributeError, release_gate.ValidationError,
            zipfile.BadZipFile) as exc:
        raise RuntimeError("Malformed release-gate artifact") from exc
    return verdict


def main():
    head = os.environ["GITHUB_SHA"]
    require(IDENTITY["product_version"] == "1.2.0" and VERSION == "1.2.0-beta.1", "Publisher is pinned to Open Beta 1.2 identity")
    publication_run_id = int(os.environ["GITHUB_RUN_ID"])
    require(os.environ["GITHUB_REPOSITORY"] == transport.REPO
            and os.environ["GITHUB_REF"] == "refs/heads/" + BRANCH,
            "Publication requires the authorized Beta 1.2 branch")
    api = transport.GitHub()
    run = api.json(f"/actions/runs/{publication_run_id}")
    require(run["head_sha"] == head and run["head_branch"] == BRANCH
            and run["event"] in ("push", "workflow_dispatch"), "Release workflow identity mismatch")
    current = api.json("/git/ref/heads/" + BRANCH)
    require(current["object"]["sha"] == head, "A newer release source supersedes this build")
    # Reuse immutable artifacts from the already verified exact-head build.
    # No rebuild, relabeling, stale-head fallback or missing-platform fallback.
    builds = api.json(f"/actions/workflows/build.yml/runs?head_sha={head}&per_page=100")["workflow_runs"]
    run_id = verified_run(builds, head, ("pull_request", "workflow_dispatch"))
    build_run = api.json(f"/actions/runs/{run_id}")
    require(build_run["head_sha"] == head, "Build run source mismatch")
    run_attempt = build_run["run_attempt"]
    jobs = api.pages(f"/actions/runs/{run_id}/jobs", "jobs")
    for suffix in ("build (windows-latest)", "build (ubuntu-22.04)", "build (macos-15)", "assemble-candidate", "consolidate-release-gate"):
        matches = [job for job in jobs if job["name"].split(" / ")[-1] == suffix]
        require(len(matches) == 1 and matches[0]["status"] == "completed"
                and matches[0]["conclusion"] == "success", f"Required build gate failed: {suffix}")
    # The distinct Setup workflow also exercises preparation contracts and real
    # install/repair/uninstall. Its exact head must pass before publication.
    candidate_runs = api.json(f"/actions/workflows/update-candidate.yml/runs?head_sha={head}&event=pull_request&per_page=100")["workflow_runs"]
    candidate_run_id = verified_run(candidate_runs, head, ("pull_request",))
    candidate_jobs = api.pages(f"/actions/runs/{candidate_run_id}/jobs", "jobs")
    for name in ("windows-candidate", "preparation-contracts"):
        matches = [job for job in candidate_jobs if job["name"] == name]
        require(len(matches) == 1 and matches[0]["status"] == "completed"
                and matches[0]["conclusion"] == "success", f"Candidate gate failed: {name}")
    artifacts = api.pages(f"/actions/runs/{run_id}/artifacts", "artifacts")
    gate_matches = [artifact for artifact in artifacts
                    if artifact["name"] == "Chimera-A-stage-release-gate"]
    require(len(gate_matches) == 1, "Expected one exact-source release-gate artifact")
    gate_artifact = gate_matches[0]
    require(not gate_artifact["expired"]
            and gate_artifact["workflow_run"]["head_sha"] == head
            and gate_artifact["workflow_run"]["id"] == run_id
            and gate_artifact.get("digest", "").startswith("sha256:"),
            "Release-gate artifact provenance mismatch")
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
    with tempfile.TemporaryDirectory(prefix="chimera-beta12-") as temporary:
        working = Path(temporary)
        gate_archive = working / "release-gate.zip"
        api.download(f"/actions/artifacts/{gate_artifact['id']}/zip", gate_archive)
        require(sha256(gate_archive) == gate_artifact["digest"].split(":", 1)[1],
                "Release-gate artifact digest mismatch")
        verify_release_gate_archive(gate_archive, head, "beta_1_2", run_id, run_attempt)
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
                stream.write(f"## Open Beta 1.2 published\n\n{release['html_url']}\n\nSource `{head}`; all three platform builds and package checks passed.\n")


if __name__ == "__main__":
    main()
