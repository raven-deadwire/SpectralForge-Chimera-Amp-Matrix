#!/usr/bin/env python3
"""Review or publish an explicitly versioned, exact-source Chimera release.

The default is an offline review. This program never changes a version, builds a
binary, grants approval, or moves a tag. The publication controller may add only
the separately reviewed approval receipt after the product source was tested.
"""
from __future__ import annotations

import argparse
from dataclasses import dataclass
import json
import os
from pathlib import Path, PurePosixPath
import re
import subprocess
import sys
import tempfile
import time
from urllib.parse import quote
import zipfile

import chimera_version
import check_release_calendar
import beta_lifecycle
import check_ir_distribution
import check_public_names
import evaluate_release_gate as gate
import publish_open_beta as transport
import weekly_publication
from package_release import PUBLIC_DOCUMENTS
from verify_publication_evidence import verify_embedded_evidence
# These version-independent verification primitives retain the shipped CI and
# archive contracts. No version, branch, approval or publishing entry point from
# the historical publisher is used.
from publish_beta131 import (CIPending, WORKFLOWS, GATE_ARTIFACT_NAME,
    CANDIDATE_MAX_EXPANDED_BYTES, SUFFIXES, verified_run,
    verified_jobs, verified_artifact, ci_identity, parse_json, read_json, check_archive)

# Reservation contracts are required release evidence alongside the native jobs.
WORKFLOWS = {**WORKFLOWS, "release-publisher.yml": (
    "publisher-contracts", "calendar-contracts (ubuntu-24.04)",
    "calendar-contracts (windows-latest)", "calendar-contracts (macos-15)", "calendar-live")}

ROOT = Path(__file__).resolve().parents[1]
APPROVAL_PATH = "Validation/weekly-publication-policy.json"
WORKFLOW_PATH = ".github/workflows/publish-beta131.yml"
CONTROL_PATHS = frozenset({APPROVAL_PATH})
DOCUMENTS = ("SHA256SUMS.txt", "MANUAL.html", "INSTALLATION.md", "THIRD_PARTY_NOTICES.md",
             "candidate-source.json", "InstallerVerification.txt",
             "macOS-package-verification.txt", "Linux-package-verification.txt")
# User documentation is a reviewed product surface. Keep historical reference
# documents in the repository, independently of the unchanged raw gate evidence.
SOURCE_DOCUMENTS = tuple(name for name in PUBLIC_DOCUMENTS if name not in DOCUMENTS)
require, sha256 = transport.require, transport.sha256


@dataclass(frozen=True)
class Release:
    version: str
    source_sha: str

    def __post_init__(self):
        chimera_version.parse_release_version(self.version)
        require(isinstance(self.source_sha, str)
                and re.fullmatch(r"[0-9a-f]{40}", self.source_sha) is not None,
                "A full lowercase product source SHA is required")

    @property
    def product_version(self):
        return chimera_version.parse_release_version(self.version)[0]

    @property
    def source_channel(self):
        return chimera_version.parse_release_version(self.version)[1]

    @property
    def channel(self):
        return chimera_version.release_channel_for_version(self.version)

    @property
    def profile(self):
        return "beta_1_3" if self.channel == "beta" else "full_release"

    @property
    def tag(self):
        return "v" + self.version

    @property
    def confirm(self):
        return "publish-" + self.tag + "@" + self.source_sha

    @property
    def artifact_name(self):
        return f"SpectralForge-Chimera-{self.version}-Release-Candidate"

    @property
    def release_url(self):
        return f"https://github.com/{transport.REPO}/releases/tag/{self.tag}"

    @property
    def download_url(self):
        return f"https://github.com/{transport.REPO}/releases/download/{self.tag}/"

    @property
    def manifest_names(self):
        return chimera_version.release_manifest_names(self.version)


def git(*args):
    return subprocess.check_output(["git", *args], cwd=ROOT, text=True).strip()


def verify_controller_source(source, controller):
    require(all(isinstance(value, str) and re.fullmatch(r"[0-9a-f]{40}", value)
                for value in (source, controller)), "Full product and controller SHAs are required")
    require(git("rev-parse", "HEAD") == controller, "Checked-out publication controller differs")
    subprocess.run(["git", "merge-base", "--is-ancestor", source, controller], cwd=ROOT, check=True)
    require(not git("status", "--porcelain", "--untracked-files=no"), "Publication controller has local changes")
    changes = git("diff", "--name-status", "--no-renames", source, controller)
    paths = []
    for line in changes.splitlines():
        status, path = line.split("\t", 1)
        require(status in ("A", "M") and path in CONTROL_PATHS,
                f"Untested product or publisher change in controller: {path}")
        require(git("ls-tree", controller, "--", path).startswith("100644 blob "),
                f"Unexpected publication controller file mode: {path}")
        paths.append(path)
    return {"product_source_sha": source, "publication_controller_sha": controller,
            "controller_changed_paths": sorted(paths)}


def release_policy(release):
    # Stop unsupported mandatory scope before CI lookup, archive staging or
    # remote mutations. validate_approval also enforces this for direct callers.
    weekly_publication.require_supported_acceptance_scope(release.version)
    # Read the selected commit itself. identity(ROOT, arbitrary_sha) alone would
    # attach the requested SHA to the current worktree's version, not prove it.
    source_version = git("show", f"{release.source_sha}:VERSION")
    source_channel = git("show", f"{release.source_sha}:RELEASE_CHANNEL")
    require(source_version == release.product_version and source_channel == release.source_channel,
            "Target version differs from the selected source VERSION/RELEASE_CHANNEL")
    chimera_version.validate_target_version(release.version, ROOT, release.source_sha)
    policy = parse_json(git("show", f"{release.source_sha}:Validation/release-policy.json"))
    require(policy == read_json(ROOT / "Validation/release-policy.json"),
            "Controller validation policy differs from tested product source")
    scope = policy.get("release_scope", {})
    require(scope.get("version") == release.version and scope.get("profile") == release.profile
            and scope.get("channel") == release.source_channel,
            "Release scope identity/profile mismatch")
    require(scope.get("preparation_authorized") is True, "Release preparation is not authorized")
    def checks(profile):
        return {cid for stage in policy["profiles"][profile]["required_stages"]
                for cid in policy["stages"][stage]["required_checks"]}
    full, selected = checks("full_release"), checks(release.profile)
    deferred = {"I2.PITCH_LIVE", "I2.PITCH_DI"} if release.channel == "beta" else set()
    require(selected and full - selected == deferred and not selected - full
            and set(scope.get("deferred_checks", [])) == deferred,
            "Release must retain all applicable acceptance checks")
    if release.channel == "beta":
        require(selected == checks("beta_1_2"), "Weekly beta acceptance inventory changed")
    return policy


def approval(release, policy):
    require(not (ROOT / "RELEASE_REVIEW_PENDING").exists(), "RELEASE_REVIEW_PENDING")
    receipt = read_json(ROOT / APPROVAL_PATH)
    weekly_publication.validate_approval(receipt, policy, release.source_sha,
        version=release.version, profile=release.profile,
        waivers=read_json(ROOT / "Validation/waivers.json"),
        ci_check_map=read_json(ROOT / "Validation/ci-check-map.json"))
    return receipt


def verify_ci(api, release):
    runs, pending = {}, []
    for workflow, expected in WORKFLOWS.items():
        available = api.pages(f"/actions/workflows/{workflow}/runs?head_sha={release.source_sha}", "workflow_runs")
        try:
            run = verified_run(available, release.source_sha)
        except CIPending:
            pending.append(workflow)
            continue
        require(run.get("repository", {}).get("full_name") == transport.REPO,
                "Verification repository mismatch")
        # A same-SHA fork run is not the repository's release evidence.
        require(run.get("head_repository", {}).get("full_name") == transport.REPO,
                "Verification head repository mismatch")
        jobs = api.pages(f"/actions/runs/{run['id']}/jobs?filter=latest", "jobs")
        verified_jobs(jobs, expected)
        runs[workflow] = run
    if pending:
        raise CIPending("Exact-source verification is still running: " + ", ".join(pending))
    build = runs["build.yml"]
    artifacts = api.pages(f"/actions/runs/{build['id']}/artifacts", "artifacts")
    return {"runs": runs,
        "candidate": verified_artifact(artifacts, release.artifact_name, release.source_sha, build["id"]),
        "gate": verified_artifact(artifacts, GATE_ARTIFACT_NAME, release.source_sha, build["id"])}


def wait_for_ci(api, release, seconds, check_current_controller):
    require(type(seconds) is int and 0 <= seconds <= 5400, "Invalid CI wait duration")
    deadline = time.monotonic() + seconds
    while True:
        check_current_controller()
        try:
            return verify_ci(api, release)
        except CIPending:
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                raise
            print(f"Waiting for exact-source CI at {release.source_sha[:10]}; publication remains blocked", flush=True)
            time.sleep(min(30, remaining))


def verify_release_gate_archive(archive, release, run_id, run_attempt, policy, receipt=None):
    require(type(run_id) is int and run_id > 0 and type(run_attempt) is int and run_attempt > 0,
            "Gate requires positive run and attempt identities")
    revision = {"commit_sha": release.source_sha, "run_id": str(run_id), "run_attempt": str(run_attempt)}
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
        require(verdict.get("schema") == "spectralforge.chimera.release-gate"
                and type(verdict.get("schema_version")) is int and verdict["schema_version"] == 1
                and verdict.get("profile") == release.profile and verdict.get("revision") == revision
                and verdict.get("policy_version") == policy["policy_version"], "Release-gate identity mismatch")
        producer = verdict.get("producer", {})
        require(producer.get("name") == "consolidate_release_gate" and producer.get("errors") == [],
                "Consolidated release acceptance evidence is absent")
        inputs = producer.get("inputs")
        require(isinstance(inputs, list) and len(inputs) == 4
                and all(isinstance(item, dict) for item in inputs)
                and {item.get("kind") for item in inputs} == {"windows", "linux", "macos", "assembly"}
                and all(item.get("revision") == revision
                        and isinstance(item.get("manifest_sha256"), str)
                        and re.fullmatch(r"[0-9a-f]{64}", item["manifest_sha256"]) for item in inputs),
                "Missing, duplicate or stale platform evidence")
        owners = {cid: stage for stage in policy["profiles"][release.profile]["required_stages"]
                  for cid in policy["stages"][stage]["required_checks"]}
        check_root, checks = PurePosixPath(matches[0]).parent / "checks", {}
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
                    and check["evidence"].get("commit_sha") == release.source_sha
                    and check["evidence"].get("policy_version") == policy["policy_version"],
                    "Release check policy or source mismatch")
            checks[cid] = check
        require(set(checks) == set(owners), "Release check inventory mismatch")
        waivers = read_json(ROOT / "Validation/waivers.json")
        computed = gate.evaluate_release(policy, waivers, checks, release.profile, release.source_sha)
        for field in ("verdict", "ready", "counts", "hard_gates", "checks", "stages", "blockers"):
            require(json.dumps(verdict.get(field), sort_keys=True) == json.dumps(computed[field], sort_keys=True),
                    "Release-gate summary differs from its underlying evidence")
        verify_embedded_evidence(bundle, verdict, policy, checks, release.source_sha, run_id, run_attempt,
            profile=release.profile, ci_check_map=read_json(ROOT / "Validation/ci-check-map.json"))
        decision = None if receipt is None else weekly_publication.evaluate(receipt, policy, checks, verdict,
            release.source_sha, run_id, run_attempt, version=release.version, profile=release.profile,
            waivers=waivers, ci_check_map=read_json(ROOT / "Validation/ci-check-map.json"))
        return verdict, decision


def prepare_assets(candidate, release, run_id, run_attempt):
    source = read_json(candidate / "candidate-source.json")
    require(source.get("version") == release.version and source.get("tag") == release.tag
            and source.get("revision") == release.source_sha and str(source.get("runId")) == str(run_id)
            and str(source.get("runAttempt")) == str(run_attempt)
            and source.get("published") is False and source.get("publisherSigned") is False
            and source.get("macOSNotarized") is False, "Candidate source/version/run identity mismatch")
    names = {f"SpectralForge-Chimera-{release.version}-{suffix}" for suffix in SUFFIXES}
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
    expected = {("windows", "x86_64"): "win64-Setup.exe", ("macos", "universal"): "macos-universal.pkg",
                ("linux", "x86_64"): "linux-x86_64.deb"}
    canonical = None
    for feed in release.manifest_names:
        manifest = read_json(candidate / feed)
        feed_channel = feed.removeprefix("update-").removesuffix(".json")
        require(type(manifest.get("schema")) is int and manifest["schema"] == 1
                and manifest.get("version") == release.version and manifest.get("source_sha") == release.source_sha
                and manifest.get("channel") == feed_channel
                and manifest.get("releaseUrl") == release.release_url, "Update manifest identity mismatch")
        require(isinstance(manifest.get("assets"), list) and len(manifest["assets"]) == 3,
                "Update manifest must contain three platforms")
        seen = set()
        for asset in manifest["assets"]:
            key = (asset["platform"], asset["arch"])
            require(key in expected and key not in seen, "Unexpected or duplicate update platform")
            seen.add(key)
            name = f"SpectralForge-Chimera-{release.version}-{expected[key]}"
            require(asset["name"] == name and asset["url"] == release.download_url + name
                    and asset["sha256"] == sums[name] and type(asset["size"]) is int
                    and asset["size"] == (candidate / name).stat().st_size,
                    f"Update asset mismatch: {name}")
        normalized = {key: value for key, value in manifest.items() if key != "channel"}
        require(canonical is None or normalized == canonical, "Compatibility update feed differs from canonical feed")
        canonical = normalized
    with zipfile.ZipFile(candidate / f"SpectralForge-Chimera-{release.version}-win64.zip") as archive:
        require(archive.testzip() is None, "Public Windows ZIP failed CRC verification")
        require(len(archive.namelist()) == len(set(archive.namelist())), "Duplicate portable ZIP entries")
        check_ir_distribution.validate_zip_payload(archive)
        payload = parse_json(archive.read("payload-manifest.json").decode("utf-8-sig"))
        if (ROOT / beta_lifecycle.POLICY_PATH).is_file():
            require("chimera-beta-lifecycle.json" in archive.namelist(), "Packaged beta metadata is missing")
            metadata = parse_json(archive.read("chimera-beta-lifecycle.json").decode("utf-8"))
            beta_lifecycle.validate_metadata(metadata, ROOT, release.source_sha, publication=True)
            require(archive.read("BETA_LIFECYCLE.txt").decode("utf-8") == beta_lifecycle.notice(metadata),
                    "Packaged beta notice differs from compiled policy")
        chimera_version.validate_manifest(payload, chimera_version.identity(ROOT, release.source_sha))
    assets = {name: candidate / name for name in sorted(names | set(DOCUMENTS) | set(release.manifest_names))}
    assets.update({name: ROOT / "docs" / name for name in SOURCE_DOCUMENTS})
    if (ROOT / beta_lifecycle.POLICY_PATH).is_file():
        for name in ("chimera-beta-lifecycle.json", "BETA_LIFECYCLE.txt"):
            with zipfile.ZipFile(candidate / f"SpectralForge-Chimera-{release.version}-win64.zip") as archive:
                (candidate / name).write_bytes(archive.read(name))
            assets[name] = candidate / name
    assets["Trace-Chimera-Session.ps1"] = ROOT / "Tools/Trace-Chimera-Session.ps1"
    assets["COPYRIGHT.txt"] = ROOT / "COPYRIGHT.txt"
    for name, path in assets.items():
        require(path.is_file() and not path.is_symlink() and path.stat().st_size > 0, f"Missing public file: {name}")
        if name in PUBLIC_DOCUMENTS or name == "COPYRIGHT.txt":
            check_public_names.validate_document(path, name=name)
    notes = assets["OPEN_BETA_RELEASE_NOTES.md"].read_text(encoding="utf-8").strip()
    require(release.version in notes and "Studio One" in notes and "CAB" in notes,
            "Current version release notes and retained host/CAB disclosure are required")
    require((candidate / "OPEN_BETA_RELEASE_NOTES.md").read_bytes()
            == assets["OPEN_BETA_RELEASE_NOTES.md"].read_bytes(), "Packaged release notes differ from source")
    return notes, assets


def configure_transport(release, artifact=None):
    transport.VERSION, transport.TAG, transport.HEAD = release.version, release.tag, release.source_sha
    transport.TITLE = f"SpectralForge Chimera {release.version}"
    transport.RELEASE_URL, transport.DOWNLOAD_URL = release.release_url, release.download_url
    if artifact is not None:
        transport.ARCHIVE_SIZE = artifact["size_in_bytes"]
        transport.ARCHIVE_SHA = artifact["digest"].split(":", 1)[1]


def render_public_release_notes(notes, release):
    # Source documents are immutable source links; the manual is a verified
    # release asset. Reject unresolved links rather than publish broken notes.
    def resolve(match):
        name = match.group(1)
        if name == "MANUAL.html":
            return "](" + release.download_url + name + ")"
        path = PurePosixPath(name)
        require(not path.is_absolute() and ".." not in path.parts and "\\" not in name
                and (ROOT / "docs" / name).is_file(), "Unresolved relative release-note link")
        return f"](https://github.com/{transport.REPO}/blob/{release.source_sha}/docs/{name})"
    return re.sub(r"\]\((?!https?://|#)([^)]+)\)", resolve, notes)


def dry_run(args, release, policy, proof):
    blockers = ["LIVE_EXACT_SOURCE_CI_NOT_VERIFIED", "LIVE_RELEASE_RESERVATIONS_NOT_VERIFIED"]
    try:
        check_release_calendar.check_repository(ROOT, target_version=release.version)
    except (ValueError, RuntimeError, KeyError, TypeError, OSError) as exc:
        blockers.append("RELEASE_CALENDAR_BLOCKED: " + str(exc))
    try:
        approval(release, policy)
    except (RuntimeError, ValueError, KeyError, OSError) as exc:
        blockers.append("APPROVAL_REQUIRED: " + str(exc))
    result = {"version": release.version, "source_sha": release.source_sha, "profile": release.profile,
        "mode": "OFFLINE_DRY_RUN", "candidate": "NOT_SUPPLIED", "consolidated_acceptance": "NOT_SUPPLIED",
        "publication_ready": False, "published": False, "blockers": blockers, **proof}
    require(not args.candidate_zip or (args.artifact_json and args.run_id and args.run_attempt),
            "Offline ZIP verification requires artifact metadata, run ID and attempt")
    require(not args.candidate_dir or (args.run_id and args.run_attempt),
            "Candidate review requires run ID and attempt")
    require(not args.gate_archive or (args.run_id and args.run_attempt), "Gate review requires run ID and attempt")
    with tempfile.TemporaryDirectory(prefix="chimera-release-review-") as temporary:
        candidate = args.candidate_dir
        if args.candidate_zip:
            artifact = read_json(args.artifact_json)
            verified_artifact([artifact], release.artifact_name, release.source_sha, args.run_id)
            configure_transport(release, artifact)
            candidate = Path(temporary) / "candidate"
            candidate.mkdir()
            transport.extract_candidate(args.candidate_zip, candidate, max_expanded_bytes=CANDIDATE_MAX_EXPANDED_BYTES)
        if candidate:
            _, assets = prepare_assets(candidate, release, args.run_id, args.run_attempt)
            result.update(candidate="LOCAL_PACKAGE_CHECKS_PASS", public_asset_count=len(assets))
        if args.gate_archive:
            verdict, _ = verify_release_gate_archive(args.gate_archive, release, args.run_id,
                                                     args.run_attempt, policy)
            result["consolidated_acceptance"] = verdict["verdict"]
    print(json.dumps(result, indent=2, ensure_ascii=False))
    return result


def publish(args, release, policy, proof):
    require(not any((args.candidate_dir, args.candidate_zip, args.artifact_json, args.gate_archive,
                     args.run_id, args.run_attempt)), "Live publication cannot consume local override evidence")
    controller = os.environ.get("GITHUB_SHA")
    require(args.confirm == release.confirm and os.environ.get("GITHUB_EVENT_NAME") == "workflow_dispatch"
            and os.environ.get("GITHUB_REPOSITORY") == transport.REPO
            and controller == proof["publication_controller_sha"],
            "Publication requires manual confirmation of this exact version and source")
    receipt = approval(release, policy)
    api = transport.GitHub()
    repository = api.json("")
    branch = repository.get("default_branch")
    require(repository.get("full_name") == transport.REPO and isinstance(branch, str) and branch
            and os.environ.get("GITHUB_REF") == "refs/heads/" + branch,
            "Publication controller must run on the repository default branch")
    run_id = os.environ.get("GITHUB_RUN_ID", "")
    require(re.fullmatch(r"[1-9][0-9]*", run_id), "Positive publisher workflow run ID required")
    invocation = api.json(f"/actions/runs/{run_id}")
    require(invocation.get("head_sha") == controller and invocation.get("head_branch") == branch
            and invocation.get("event") == "workflow_dispatch" and invocation.get("path") == WORKFLOW_PATH,
            "Publication workflow identity mismatch")

    def check_current_controller():
        current_repository = api.json("")
        require(current_repository.get("full_name") == transport.REPO
                and current_repository.get("default_branch") == branch,
                "Publication default branch changed during preparation")
        require(api.json("/git/ref/heads/" + quote(branch, safe="/"))["object"]["sha"] == controller,
                "A newer publication controller supersedes this invocation")

    check_release_calendar.check_repository(ROOT, api=api, target_version=release.version)
    ci = wait_for_ci(api, release, args.wait_ci_seconds, check_current_controller)
    build = ci["runs"]["build.yml"]
    with tempfile.TemporaryDirectory(prefix="chimera-release-") as temporary:
        working = Path(temporary)
        gate_archive = working / "release-gate.zip"
        api.download(f"/actions/artifacts/{ci['gate']['id']}/zip", gate_archive)
        check_archive(gate_archive, ci["gate"])
        verdict, decision = verify_release_gate_archive(gate_archive, release, build["id"],
                                                       build["run_attempt"], policy, receipt)
        require(decision.get("publication_allowed") is True, "Publication decision does not authorize release")
        archive = working / "candidate.zip"
        api.download(f"/actions/artifacts/{ci['candidate']['id']}/zip", archive)
        configure_transport(release, ci["candidate"])
        candidate = working / "candidate"
        candidate.mkdir()
        transport.extract_candidate(archive, candidate, max_expanded_bytes=CANDIDATE_MAX_EXPANDED_BYTES)
        notes, assets = prepare_assets(candidate, release, build["id"], build["run_attempt"])
        notes = render_public_release_notes(notes, release)
        decision.update(proof)
        decision["ci"] = {name: {key: run[key] for key in ("id", "run_attempt", "head_sha")}
                          for name, run in ci["runs"].items()}
        decision["artifacts"] = {name: {key: ci[name][key] for key in ("id", "name", "digest", "size_in_bytes")}
                                 for name in ("candidate", "gate")}
        for name, value in (("release-gate.json", verdict), ("publication-decision.json", decision)):
            path = working / name
            path.write_text(json.dumps(value, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
            assets[name] = path
        assets["release-acceptance-evidence.zip"] = gate_archive
        disclosure = working / "VALIDATION_SCOPE.md"
        disclosure.write_text(weekly_publication.render_markdown(decision), encoding="utf-8")
        assets[disclosure.name] = disclosure
        notes += (f"\n\nBuild: [`{release.source_sha[:10]}`](https://github.com/{transport.REPO}/commit/{release.source_sha})"
                  f" · [Verified CI](https://github.com/{transport.REPO}/actions/runs/{build['id']})."
                  "\n\nThe source-bound publication decision, complete strict gate and validation scope "
                  "are attached as `publication-decision.json`, `release-gate.json`, "
                  "`release-acceptance-evidence.zip` and `VALIDATION_SCOPE.md`.")

        def recheck():
            check_current_controller()
            check_release_calendar.check_repository(ROOT, api=api, target_version=release.version)
            require(ci_identity(verify_ci(api, release)) == ci_identity(ci),
                    "Verification runs or artifacts changed during preparation")
            require(verify_controller_source(release.source_sha, controller) == proof
                    and release_policy(release) == policy and approval(release, policy) == receipt,
                    "Product source, controller or approval changed during preparation")

        # Check before creating/staging anything and again after uploads, inside
        # transport.publish immediately before its draft -> public request.
        recheck()
        result = transport.publish(api, notes, assets, prerelease=release.channel != "stable",
                                   manifest_names=release.manifest_names, before_publish=recheck)
        print(f"Published and publicly verified: {result['html_url']}")
        return result


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument("--dry-run", action="store_true", help="offline review; the default")
    mode.add_argument("--publish", action="store_true", help="separately approved manual workflow only")
    parser.add_argument("--target-version", required=True, help="exact unprefixed beta.N, rc.N or stable version")
    parser.add_argument("--source-sha", required=True, help="exact tested 40-character lowercase product SHA")
    parser.add_argument("--confirm")
    parser.add_argument("--wait-ci-seconds", type=int, default=0)
    inputs = parser.add_mutually_exclusive_group()
    inputs.add_argument("--candidate-dir", type=Path)
    inputs.add_argument("--candidate-zip", type=Path)
    parser.add_argument("--artifact-json", type=Path)
    parser.add_argument("--gate-archive", type=Path)
    parser.add_argument("--run-id", type=int)
    parser.add_argument("--run-attempt", type=int)
    args = parser.parse_args(argv)
    require(args.publish or args.wait_ci_seconds == 0, "CI waiting requires live publication mode")
    release = Release(args.target_version, args.source_sha)
    proof = verify_controller_source(release.source_sha, git("rev-parse", "HEAD"))
    policy = release_policy(release)
    return publish(args, release, policy, proof) if args.publish else dry_run(args, release, policy, proof)


if __name__ == "__main__":
    try:
        main()
    except (RuntimeError, ValueError, KeyError, TypeError, OSError, subprocess.SubprocessError,
            zipfile.BadZipFile) as error:
        print(f"Release preparation/publication stopped: {error}", file=sys.stderr)
        sys.exit(1)
