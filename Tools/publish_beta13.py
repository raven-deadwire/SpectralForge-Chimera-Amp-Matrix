#!/usr/bin/env python3
"""Prepare or publish the immutable Open Beta 1.3 candidate.

The default is an offline dry run. Publication requires an explicit manual
workflow, source-specific authorization, all three verification workflows and a
recomputed consolidated acceptance gate. The producer retains missing manual and
unowned checks as BLOCKED; a green build alone cannot publish this release.
"""
from __future__ import annotations

import argparse
import json
import os
from pathlib import Path, PurePosixPath
import re
import subprocess
import sys
import tempfile
import zipfile

import chimera_version
import evaluate_release_gate as gate
import publish_open_beta as transport

ROOT = Path(__file__).resolve().parents[1]
PRODUCT_VERSION = "1.3.0"
VERSION = "1.3.0-beta.1"
PROFILE = "beta_1_3"
TAG = "v" + VERSION
TITLE = "SpectralForge Chimera — Open Beta 1.3 · CAB + Niflheimr"
BRANCH = "release/open-beta-1.3.0"
ARTIFACT_NAME = f"SpectralForge-Chimera-{VERSION}-Release-Candidate"
GATE_ARTIFACT_NAME = "Chimera-A-stage-release-gate"
RELEASE_URL = f"https://github.com/{transport.REPO}/releases/tag/{TAG}"
DOWNLOAD_URL = f"https://github.com/{transport.REPO}/releases/download/{TAG}/"
SUFFIXES = ("win64-Setup.exe", "win64.zip", "macos-universal.pkg",
            "linux-x86_64.deb", "linux-x86_64.tar.gz")
DOCUMENTS = ("SHA256SUMS.txt", "update-beta.json", "MANUAL.html", "INSTALLATION.md",
             "THIRD_PARTY_NOTICES.md", "candidate-source.json", "InstallerVerification.txt",
             "macOS-package-verification.txt", "Linux-package-verification.txt")
SOURCE_DOCUMENTS = ("OPEN_BETA_RELEASE_NOTES.md", "STUDIO_ONE_TEARDOWN.md",
                    "AMP_NATIVE_DSP.md", "POST_NATIVE_DSP.md", "PEDAL_BOARD_DSP.md",
                    "EXTERNAL_BASS_IRS.md", "NATIVE_NAM_CALIBRATION.md",
                    "NASTROND_CHANNEL_FEEDBACK.md", "NIFLHEIMR_NATIVE_PROTOTYPE.md",
                    "CAB_INTEGRATION.md", "CAB_EXPANSION_V2.md", "CAB_LAYOUT_V3.md")
WORKFLOWS = {
    "build.yml": ("validation-contract", "build (windows-latest)",
                  "build (ubuntu-22.04)", "build (macos-15)", "assemble-candidate",
                  "consolidate-release-gate"),
    "update-candidate.yml": ("windows-candidate", "preparation-contracts"),
    "cab-panel.yml": ("cab-panel (windows-latest)", "cab-panel (ubuntu-22.04)",
                      "cab-panel (macos-15)"),
}
require = transport.require
sha256 = transport.sha256


def unique_object(pairs):
    result = {}
    for key, value in pairs:
        require(key not in result, f"Duplicate JSON key: {key}")
        result[key] = value
    return result


def parse_json(value):
    def invalid_constant(value):
        raise ValueError(f"Non-finite JSON constant: {value}")
    return json.loads(value, object_pairs_hook=unique_object, parse_constant=invalid_constant)


def read_json(path):
    return parse_json(Path(path).read_text(encoding="utf-8-sig"))


def release_policy(head):
    require(re.fullmatch(r"[0-9a-f]{40}", head or "") is not None,
            "A full lowercase source SHA is required")
    identity = chimera_version.identity(ROOT, head)
    require(identity["product_version"] == PRODUCT_VERSION and identity["version"] == VERSION,
            "Publisher is pinned to Open Beta 1.3 identity")
    policy = read_json(ROOT / "Validation/release-policy.json")
    scope = policy["release_scope"]
    require(scope.get("version") == VERSION and scope.get("profile") == PROFILE
            and scope.get("channel") == "beta.1", "Release scope identity mismatch")
    def checks(profile):
        return {cid for stage in policy["profiles"][profile]["required_stages"]
                for cid in policy["stages"][stage]["required_checks"]}
    deferred = {"I2.PITCH_LIVE", "I2.PITCH_DI"}
    current, full = checks(PROFILE), checks("full_release")
    require(current and full - current == deferred and not current - full
            and current == checks("beta_1_2")
            and set(scope.get("deferred_checks", [])) == deferred,
            "Beta 1.3 must preserve existing acceptance gates and transpose deferral")
    return policy


def publication_blockers(policy):
    scope = policy["release_scope"]
    blockers = []
    if scope.get("publication_authorized") is not True:
        blockers.append("PUBLICATION_NOT_AUTHORIZED")
    if scope.get("release_approved") is not True:
        blockers.append("RELEASE_ACCEPTANCE_NOT_APPROVED")
    if (ROOT / "RELEASE_REVIEW_PENDING").exists():
        blockers.append("RELEASE_REVIEW_PENDING")
    return blockers


def verified_run(runs, head, events=("pull_request", "workflow_dispatch")):
    matches = [run for run in runs if run.get("head_sha") == head and run.get("event") in events]
    require(bool(matches), "Exact-source verification run is missing")
    require(all(type(run.get("id")) is int and run["id"] > 0 for run in matches),
            "Malformed verification run identity")
    latest = max(matches, key=lambda run: run["id"])
    require(latest.get("status") == "completed" and latest.get("conclusion") == "success",
            "Latest exact-source verification run has not passed")
    require(type(latest.get("run_attempt")) is int and latest["run_attempt"] > 0,
            "Verification run attempt is missing")
    return latest


def verified_jobs(jobs, expected):
    for name in expected:
        matches = [job for job in jobs if job.get("name", "").split(" / ")[-1] == name]
        require(len(matches) == 1 and matches[0].get("status") == "completed"
                and matches[0].get("conclusion") == "success",
                f"Required verification job failed or is missing: {name}")


def verified_artifact(artifacts, name, head, run_id):
    matches = [artifact for artifact in artifacts if artifact.get("name") == name]
    require(len(matches) == 1, f"Expected one exact-source artifact: {name}")
    artifact = matches[0]
    run = artifact.get("workflow_run", {})
    require(artifact.get("expired") is False and run.get("head_sha") == head
            and run.get("id") == run_id and type(artifact.get("id")) is int
            and artifact["id"] > 0 and type(artifact.get("size_in_bytes")) is int
            and artifact["size_in_bytes"] > 0
            and re.fullmatch(r"sha256:[0-9a-f]{64}", artifact.get("digest", "")) is not None,
            f"Artifact provenance mismatch: {name}")
    return artifact


def verify_ci(api, head):
    runs = {}
    for workflow, expected in WORKFLOWS.items():
        available = api.pages(f"/actions/workflows/{workflow}/runs?head_sha={head}", "workflow_runs")
        run = verified_run(available, head)
        require(run.get("repository", {}).get("full_name") == transport.REPO,
                "Verification repository mismatch")
        jobs = api.pages(f"/actions/runs/{run['id']}/jobs?filter=latest", "jobs")
        verified_jobs(jobs, expected)
        runs[workflow] = run
    build = runs["build.yml"]
    artifacts = api.pages(f"/actions/runs/{build['id']}/artifacts", "artifacts")
    return {
        "runs": runs,
        "candidate": verified_artifact(artifacts, ARTIFACT_NAME, head, build["id"]),
        "gate": verified_artifact(artifacts, GATE_ARTIFACT_NAME, head, build["id"]),
    }


def ci_identity(ci):
    return ({name: (run["id"], run["run_attempt"]) for name, run in ci["runs"].items()},
            tuple((ci[name]["id"], ci[name]["digest"]) for name in ("candidate", "gate")))


def verify_release_gate_archive(archive, head, profile, run_id, run_attempt, policy=None):
    """Consume the consolidated format; seeded or summary-only gates are rejected."""
    require(str(run_id).isdigit() and int(run_id) > 0 and str(run_attempt).isdigit() and int(run_attempt) > 0,
            "Gate requires positive run and attempt identities")
    require(profile == PROFILE, "Release-gate profile identity mismatch")
    policy = read_json(ROOT / "Validation/release-policy.json") if policy is None else policy
    try:
        with zipfile.ZipFile(archive) as bundle:
            entries = bundle.infolist()
            names = [entry.filename for entry in entries]
            require(len(names) == len(set(names)), "Duplicate release-gate ZIP entries")
            require(sum(entry.file_size for entry in entries) <= 16 * 1024 * 1024,
                    "Release-gate archive is unexpectedly large")
            require(all(not PurePosixPath(name).is_absolute() and ".." not in PurePosixPath(name).parts
                        and "\\" not in name for name in names), "Unsafe release-gate ZIP entry")
            require(bundle.testzip() is None, "Release-gate ZIP failed CRC verification")
            matches = [name for name in names if PurePosixPath(name).name == "release-gate.json"]
            require(len(matches) == 1, "Expected one consolidated release-gate.json")
            verdict = parse_json(bundle.read(matches[0]).decode("utf-8-sig"))
            revision = {"commit_sha": head, "run_id": str(run_id), "run_attempt": str(run_attempt)}
            require(verdict.get("schema") == "spectralforge.chimera.release-gate"
                    and type(verdict.get("schema_version")) is int and verdict["schema_version"] == 1
                    and verdict.get("profile") == PROFILE and verdict.get("revision") == revision
                    and verdict.get("policy_version") == policy["policy_version"],
                    "Release-gate identity mismatch")
            require(verdict.get("producer", {}).get("name") == "consolidate_release_gate"
                    and verdict["producer"].get("errors") == [],
                    "Consolidated release acceptance evidence is absent")
            owners = {cid: stage for stage in policy["profiles"][PROFILE]["required_stages"]
                      for cid in policy["stages"][stage]["required_checks"]}
            check_root = PurePosixPath(matches[0]).parent / "checks"
            checks = {}
            for name in names:
                if PurePosixPath(name).parent != check_root or not name.endswith(".json"):
                    continue
                check = parse_json(bundle.read(name).decode("utf-8-sig"))
                gate.validate_check(check, name)
                cid = check["id"]
                require(cid in owners and cid not in checks, "Unexpected or duplicate release check")
                require(check["stage"] == owners[cid] and check["policy"].get("required") is True
                        and check["policy"].get("hard_gate") is (cid in policy["hard_gates"])
                        and check["policy"].get("requires_current_commit") is True
                        and check["policy"].get("requires_policy_version") is True
                        and check["evidence"].get("commit_sha") == head
                        and check["evidence"].get("policy_version") == policy["policy_version"],
                        "Release check policy or source mismatch")
                checks[cid] = check
            require(set(checks) == set(owners), "Release check inventory mismatch")
            computed = gate.evaluate_release(policy, read_json(ROOT / "Validation/waivers.json"),
                                             checks, PROFILE, head)
            for field in ("verdict", "ready", "counts", "hard_gates", "checks", "stages", "blockers"):
                require(json.dumps(verdict.get(field), sort_keys=True) == json.dumps(computed[field], sort_keys=True),
                        "Release-gate summary differs from its underlying evidence")
            require(computed["verdict"] == "PASS" and computed["ready"] is True
                    and computed["counts"]["checks"]["blocked"] == 0
                    and computed["hard_gates"]["blocked"] == 0 and not computed["blockers"],
                    "Consolidated release-gate verdict is not PASS")
    except (KeyError, TypeError, ValueError, AttributeError, zipfile.BadZipFile) as exc:
        raise RuntimeError("Malformed consolidated release-gate archive") from exc
    return verdict


def prepare_assets(candidate, head, run_id):
    source = read_json(candidate / "candidate-source.json")
    require(source.get("version") == VERSION and source.get("tag") == TAG
            and source.get("revision") == head and str(source.get("runId")) == str(run_id)
            and source.get("published") is False and source.get("publisherSigned") is False
            and source.get("macOSNotarized") is False, "Candidate source identity mismatch")
    names = {f"SpectralForge-Chimera-{VERSION}-{suffix}" for suffix in SUFFIXES}
    sums = {}
    for line in (candidate / "SHA256SUMS.txt").read_text(encoding="utf-8-sig").splitlines():
        digest, name = line.split(maxsplit=1)
        name = name.lstrip("*").strip()
        require(name not in sums and re.fullmatch(r"[0-9a-fA-F]{64}", digest), "Invalid checksum entry")
        sums[name] = digest.lower()
    require(set(sums) == names, "Exactly five public binary packages are required")
    for name, digest in sums.items():
        require(sha256(candidate / name) == digest, f"Binary checksum mismatch: {name}")
        sidecar = (candidate / (name + ".sha256.txt")).read_text(encoding="utf-8-sig").split()
        require(len(sidecar) == 2 and sidecar[0].lower() == digest and sidecar[1].lstrip("*") == name,
                f"Sidecar checksum mismatch: {name}")
    manifest = read_json(candidate / "update-beta.json")
    require(manifest.get("schema") == 1 and manifest.get("version") == VERSION
            and manifest.get("channel") == "beta" and manifest.get("releaseUrl") == RELEASE_URL,
            "Update manifest identity mismatch")
    expected = {("windows", "x86_64"): "win64-Setup.exe", ("macos", "universal"): "macos-universal.pkg",
                ("linux", "x86_64"): "linux-x86_64.deb"}
    require(len(manifest["assets"]) == 3, "Update manifest must contain three platforms")
    seen = set()
    for asset in manifest["assets"]:
        key = (asset["platform"], asset["arch"])
        require(key in expected and key not in seen, "Unexpected or duplicate update platform")
        seen.add(key)
        name = f"SpectralForge-Chimera-{VERSION}-{expected[key]}"
        require(asset["name"] == name and asset["url"] == DOWNLOAD_URL + name
                and asset["sha256"] == sums[name] and asset["size"] == (candidate / name).stat().st_size,
                f"Update asset mismatch: {name}")
    with zipfile.ZipFile(candidate / f"SpectralForge-Chimera-{VERSION}-win64.zip") as archive:
        require(archive.testzip() is None, "Public Windows ZIP failed CRC verification")
        require(len(archive.namelist()) == len(set(archive.namelist())), "Duplicate portable ZIP entries")
        for name in archive.namelist():
            parts = PurePosixPath(name).parts
            require("Chimera-Personal-IRs" not in parts and not name.lower().endswith(".nam"),
                    "Private capture found in public package")
            require(not name.lower().endswith((".wav", ".aif", ".aiff")) or "reference-audio" in parts,
                    "Unexpected external audio in public package")
        payload = parse_json(archive.read("payload-manifest.json").decode("utf-8-sig"))
        require(payload.get("version") == VERSION and payload.get("source_sha") == head,
                "Portable payload identity mismatch")
    assets = {name: candidate / name for name in sorted(names | set(DOCUMENTS))}
    assets.update({name: ROOT / "docs" / name for name in SOURCE_DOCUMENTS})
    assets["Trace-Chimera-Session.ps1"] = ROOT / "Tools/Trace-Chimera-Session.ps1"
    assets["COPYRIGHT.txt"] = ROOT / "COPYRIGHT.txt"
    for name, path in assets.items():
        require(path.is_file() and path.stat().st_size > 0, f"Missing public file: {name}")
    notes = assets["OPEN_BETA_RELEASE_NOTES.md"].read_text(encoding="utf-8").strip()
    require(VERSION in notes and "Studio One" in notes and "CAB" in notes, "Incomplete 1.3 release notes")
    require((candidate / "OPEN_BETA_RELEASE_NOTES.md").read_bytes()
            == assets["OPEN_BETA_RELEASE_NOTES.md"].read_bytes(), "Packaged release notes differ from source")
    return notes, assets


def configure_transport(head, artifact=None):
    transport.VERSION, transport.TAG, transport.TITLE, transport.HEAD = VERSION, TAG, TITLE, head
    transport.RELEASE_URL, transport.DOWNLOAD_URL = RELEASE_URL, DOWNLOAD_URL
    if artifact is not None:
        transport.ARCHIVE_SIZE = artifact["size_in_bytes"]
        transport.ARCHIVE_SHA = artifact["digest"].split(":", 1)[1]


def check_archive(path, artifact):
    require(path.stat().st_size == artifact["size_in_bytes"]
            and sha256(path) == artifact["digest"].split(":", 1)[1], "Artifact size or SHA-256 mismatch")


def dry_run(args, head, policy):
    """Local files can be checked without credentials, network reads or writes."""
    result = {"version": VERSION, "source_sha": head, "mode": "OFFLINE_DRY_RUN",
              "candidate": "NOT_SUPPLIED", "consolidated_acceptance": "NOT_SUPPLIED",
              "publication_ready": False, "published": False,
              "blockers": publication_blockers(policy) + ["LIVE_EXACT_SOURCE_CI_NOT_VERIFIED"]}
    require(not args.candidate_zip or (args.artifact_json and args.run_id),
            "Offline ZIP verification requires --artifact-json and --run-id")
    require(not args.candidate_dir or args.run_id, "Candidate directory verification requires --run-id")
    require(not args.gate_archive or (args.run_id and args.run_attempt),
            "Gate verification requires --run-id and --run-attempt")
    with tempfile.TemporaryDirectory(prefix="chimera-beta13-review-") as temporary:
        candidate = args.candidate_dir
        if args.candidate_zip:
            artifact = read_json(args.artifact_json)
            verified_artifact([artifact], ARTIFACT_NAME, head, args.run_id)
            configure_transport(head, artifact)
            candidate = Path(temporary) / "candidate"
            candidate.mkdir()
            transport.extract_candidate(args.candidate_zip, candidate)
        if candidate:
            _, assets = prepare_assets(candidate, head, args.run_id)
            result.update(candidate="LOCAL_PACKAGE_CHECKS_PASS", public_asset_count=len(assets))
        if args.gate_archive:
            computed = verify_release_gate_archive(args.gate_archive, head, PROFILE, args.run_id, args.run_attempt, policy)
            result["consolidated_acceptance"] = computed["verdict"]
    print(json.dumps(result, indent=2, ensure_ascii=False))
    return result


def publish(args, head, policy):
    require(not publication_blockers(policy),
            "Publication is blocked by release preparation scope: " + ", ".join(publication_blockers(policy)))
    require(not any((args.candidate_dir, args.candidate_zip, args.artifact_json, args.gate_archive,
                     args.run_id, args.run_attempt)), "Live publication cannot consume local override evidence")
    require(args.source_sha == head and args.confirm == "publish-" + TAG
            and os.environ.get("GITHUB_EVENT_NAME") == "workflow_dispatch"
            and os.environ.get("GITHUB_REPOSITORY") == transport.REPO
            and os.environ.get("GITHUB_REF") == "refs/heads/" + BRANCH
            and os.environ.get("GITHUB_SHA") == head,
            "Publication requires the manually confirmed 1.3 release branch and exact source")
    require(chimera_version.revision(ROOT) == head, "Checked-out source differs from accepted source")
    subprocess.run(["git", "diff", "--exit-code"], cwd=ROOT, check=True)
    api = transport.GitHub()
    invocation = api.json(f"/actions/runs/{int(os.environ['GITHUB_RUN_ID'])}")
    require(invocation.get("head_sha") == head and invocation.get("head_branch") == BRANCH
            and invocation.get("event") == "workflow_dispatch", "Publication workflow identity mismatch")
    def check_current_branch():
        require(api.json("/git/ref/heads/" + BRANCH)["object"]["sha"] == head,
                "A newer release source supersedes this candidate")
    check_current_branch()
    ci = verify_ci(api, head)
    run = ci["runs"]["build.yml"]
    with tempfile.TemporaryDirectory(prefix="chimera-beta13-") as temporary:
        working = Path(temporary)
        gate_archive = working / "release-gate.zip"
        api.download(f"/actions/artifacts/{ci['gate']['id']}/zip", gate_archive)
        check_archive(gate_archive, ci["gate"])
        verify_release_gate_archive(gate_archive, head, PROFILE, run["id"], run["run_attempt"], policy)
        candidate_archive = working / "candidate.zip"
        api.download(f"/actions/artifacts/{ci['candidate']['id']}/zip", candidate_archive)
        configure_transport(head, ci["candidate"])
        candidate = working / "candidate"
        candidate.mkdir()
        transport.extract_candidate(candidate_archive, candidate)
        notes, assets = prepare_assets(candidate, head, run["id"])
        notes += (f"\n\nBuild: [`{head[:10]}`](https://github.com/{transport.REPO}/commit/{head})"
                  f" · [Verified CI](https://github.com/{transport.REPO}/actions/runs/{run['id']}).")
        # Recheck after downloads, immediately before the first mutating request.
        check_current_branch()
        require(ci_identity(verify_ci(api, head)) == ci_identity(ci),
                "Verification runs or artifacts changed during preparation")
        require(not publication_blockers(release_policy(head)), "Release scope changed during preparation")
        release = transport.publish(api, notes, assets)
        print(f"Published and publicly verified: {release['html_url']}")
        return release


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument("--dry-run", action="store_true", help="offline local review; the default")
    mode.add_argument("--publish", action="store_true", help="requires separately authorized manual workflow")
    parser.add_argument("--source-sha")
    parser.add_argument("--confirm")
    inputs = parser.add_mutually_exclusive_group()
    inputs.add_argument("--candidate-dir", type=Path)
    inputs.add_argument("--candidate-zip", type=Path)
    parser.add_argument("--artifact-json", type=Path)
    parser.add_argument("--gate-archive", type=Path)
    parser.add_argument("--run-id", type=int)
    parser.add_argument("--run-attempt", type=int)
    args = parser.parse_args(argv)
    head = args.source_sha or os.environ.get("GITHUB_SHA") or chimera_version.revision(ROOT)
    policy = release_policy(head)
    return publish(args, head, policy) if args.publish else dry_run(args, head, policy)


if __name__ == "__main__":
    try:
        main()
    except (RuntimeError, ValueError, KeyError, OSError, subprocess.SubprocessError, zipfile.BadZipFile) as error:
        print(f"Release preparation/publication stopped: {error}", file=sys.stderr)
        sys.exit(1)
