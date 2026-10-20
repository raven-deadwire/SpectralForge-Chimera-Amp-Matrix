#!/usr/bin/env python3
"""Publish source-approved, immutable beta.1 preset maintenance distributions.

Existing Product CI builds the five packages and exports all 48 factory sounds.
This controller only verifies and transports that exact build. It never changes
the beta product version, an existing release/tag/asset, or an update feed.
"""
from __future__ import annotations

import argparse
import json
import mimetypes
import os
from pathlib import Path
import subprocess
import sys
import tempfile
from urllib.parse import quote, urlencode
import zipfile

import preset_maintenance as contract
import publish_release as official
import publish_open_beta as transport
from publish_beta131 import (verified_artifact, check_archive, ci_identity, read_json,
                             parse_json, SUFFIXES, CANDIDATE_MAX_EXPANDED_BYTES)

ROOT = Path(__file__).resolve().parents[1]
require, sha256 = transport.require, transport.sha256


def write_json(path, value):
    path.write_text(json.dumps(value, ensure_ascii=False, indent=2, allow_nan=False) + "\n", encoding="utf-8")
    return path


def base_release_snapshot(api):
    release = api.json(f"/releases/{contract.BASE_RELEASE_ID}")
    require(release.get("tag_name") == "v" + contract.BASE_VERSION
            and release.get("draft") is False and release.get("prerelease") is True,
            "Published beta.1 baseline is missing or changed")
    target = api.json("/git/ref/tags/v" + contract.BASE_VERSION)["object"]
    for _ in range(5):
        if target["type"] != "tag":
            break
        target = api.json("/git/tags/" + target["sha"])["object"]
    require(target == {"sha": contract.BASE_SOURCE, "type": "commit",
                       "url": f"https://api.github.com/repos/{transport.REPO}/git/commits/{contract.BASE_SOURCE}"},
            "Published beta.1 tag no longer identifies its approved source")
    assets = api.pages(f"/releases/{contract.BASE_RELEASE_ID}/assets")
    names = [asset["name"] for asset in assets]
    require(len(names) == len(set(names)), "Duplicate baseline release asset")
    decision = [asset for asset in assets if asset["name"] == "publication-decision.json"]
    require(len(decision) == 1 and decision[0].get("digest") == "sha256:" + contract.BASE_DECISION_SHA256,
            "Published beta.1 acceptance record changed")
    return {"release": {key: release.get(key) for key in ("id", "tag_name", "name", "body", "draft", "prerelease", "published_at")},
            "tag_source_sha": contract.BASE_SOURCE,
            "assets": sorted(({key: asset.get(key) for key in ("id", "name", "size", "state", "digest", "browser_download_url")}
                              for asset in assets), key=lambda row: row["name"])}


def export_artifacts(api, ci, source):
    run = ci["runs"]["build.yml"]
    artifacts = api.pages(f"/actions/runs/{run['id']}/artifacts", "artifacts")
    return {platform: verified_artifact(artifacts, "Chimera-integration-validation-" + label, source, run["id"])
            for platform, label in contract.PLATFORMS.items()}


def artifact_identity(artifacts):
    return {platform: {key: item[key] for key in ("id", "name", "digest", "size_in_bytes")}
            for platform, item in sorted(artifacts.items())}


def maintenance_assets(candidate_assets, pack, policy, working):
    """Rename public download labels only; package bytes are never rebuilt here."""
    assets = {name: path for name, path in candidate_assets.items()
              if name not in ("update-beta.json", "SHA256SUMS.txt")}
    binaries = []
    for suffix in SUFFIXES:
        old = f"SpectralForge-Chimera-{contract.BASE_VERSION}-{suffix}"
        name = f"SpectralForge-Chimera-{contract.BASE_VERSION}-presets-{policy['preset_revision']}-{suffix}"
        path = assets.pop(old)
        assets[name] = path
        binaries.append(name)
    assets[pack.name] = pack
    sums = []
    for name in (*binaries, pack.name):
        line = sha256(assets[name]) + "  " + name + "\n"
        sums.append(line)
        sidecar = working / (name + ".sha256.txt")
        sidecar.write_text(line, encoding="utf-8")
        assets[sidecar.name] = sidecar
    checksums = working / "SHA256SUMS.txt"
    checksums.write_text("".join(sums), encoding="utf-8")
    assets[checksums.name] = checksums
    require(len(binaries) == 5 and not any(name.startswith("update-") for name in assets),
            "Maintenance may contain exactly five packages and no update feed")
    return assets, binaries


def validation_scope(report):
    strict = report["strict_validation"]
    rows = ["# Factory preset maintenance validation", "",
        f"Product: `{contract.BASE_VERSION}`. Preset revision: `{report['preset_revision']}`.", "",
        "The new installation packages embed the 48 revised factory presets. All seven required workflows, "
        "the five exact-source packages, and the factory export/fresh-state restore on Windows, Linux and macOS "
        "passed the automated verification recorded in the attached manifest.", "",
        "This maintenance scope changes preset settings and their documentation. Engine source bytes, serializer, "
        "product identity, installer implementation, expiry and existing product policies remain the published beta.1 baseline. "
        "The rebuilt executable bytes necessarily differ because they contain the new preset table and source identity.", "",
        "The strict product gate remains **BLOCKED: 21 PASS / 107 unexecuted BLOCKED checks**, including "
        "42 blocked strict hard gates. No missing check is relabelled as passed. The prior beta.1 acceptance "
        "receipt remains bound to its original source; this receipt authorizes only the tested preset maintenance "
        "distribution and does not accept new validation gaps.", "",
        "Actual DAW/device acceptance, real DI/listening acceptance, signing/notarization, beta.2 and unfinished "
        "P0 work are not claimed. Windows remains unsigned and macOS remains unnotarized. The expiry remains "
        f"**{contract.BASE_EXPIRY}**; this maintenance does not extend it.", "",
        "## Retained, individually disclosed beta.1 gaps", ""]
    for gap in strict["known_validation_gaps"]:
        rows.append(f"- **{gap['id']}** — BLOCKED / NOT_EXECUTED. {gap['description']}")
    return "\n".join(rows) + "\n"


def release_notes(policy, source, binaries, pack_name, build_run):
    download = f"https://github.com/{transport.REPO}/releases/download/{policy['release_tag']}/"
    labels = ("Windows installer", "Windows portable", "macOS universal installer",
              "Linux Debian package", "Linux portable archive")
    rows = [f"# SpectralForge Chimera factory preset maintenance {policy['preset_revision']}", "",
        "Factory dynamics and EQ finishing is embedded in these rebuilt installation packages. "
        "All 48 factory presets retain their identities and routing, with revised Tone/Final EQ, "
        "dynamic EQ, common compression, gate timing/range and live limiter settings.", "",
        f"Product identity remains **{contract.BASE_VERSION}**. This is a separate immutable preset maintenance "
        "distribution. Close Chimera and its DAW hosts, install over the existing beta.1 installation, reopen, "
        "then select a factory preset to load its revised settings. Existing saved projects and user presets "
        "retain their stored settings. SETTINGS > Beta / Updates identifies this build as "
        f"`{source[:10]}`.", "",
        "## Downloads", ""]
    rows.extend(f"- [{label}]({download}{name})" for label, name in zip(labels, binaries))
    rows.extend([f"- [Optional 48-preset .chimera pack]({download}{pack_name})", "",
        "The installers already contain the new factory presets. The optional pack can also be opened with "
        "Chimera's OPEN action. SHA-256 checksums and sidecars are attached.", "",
        "## Validation and retained beta limitations", "",
        "All seven required exact-source workflows and the ordinary Product package verification passed. "
        "The 48 exported states were saved by the product serializer, read from disk, and restored into fresh "
        "processors on Windows, Linux and macOS. The export contains no personal IR data or MIDI mappings.", "",
        "The strict product report remains **21 PASS / 107 unexecuted BLOCKED**, including 42 blocked strict "
        "hard gates. The attached `VALIDATION_SCOPE.md` retains every existing gap. Actual Studio One/DAW, "
        "real DI/listening acceptance, publisher signing and macOS notarization are not claimed. "
        "Windows is unsigned; macOS is unnotarized. CAB/routing and the shipped engine code remain unchanged.", "",
        f"The approved beta expiry remains **{contract.BASE_EXPIRY}**. Beta.2, unfinished P0 work, existing "
        "release assets, update feeds and the latest-release selection are unchanged.", "",
        f"[Exact product source](https://github.com/{transport.REPO}/commit/{source}) · "
        f"[Verified Product build](https://github.com/{transport.REPO}/actions/runs/{build_run})", "",
        "`preset-maintenance-source.json` binds the base engine, exact tested source, preset recipe, source "
        "approval, seven CI runs, three export artifacts and all five package hashes. The complete strict "
        "gate and raw acceptance evidence are attached separately."])
    return "\n".join(rows).strip()


def publish_immutable(api, policy, source, notes, assets, recheck):
    """Same immutable staging/verification rules as the product transport.

    The existing product transport requires an update-channel manifest. This
    namespace deliberately has no such feed, so only the staging shell differs;
    its tag, existing-asset and public-byte verifiers are reused unchanged.
    """
    contract.validate_policy(policy, source=source, publication=True)
    transport.TAG, transport.HEAD = policy["release_tag"], source
    transport.TITLE = f"SpectralForge Chimera {contract.BASE_VERSION} · Factory presets {policy['preset_revision']}"
    transport.RELEASE_URL = f"https://github.com/{transport.REPO}/releases/tag/{transport.TAG}"
    transport.DOWNLOAD_URL = f"https://github.com/{transport.REPO}/releases/download/{transport.TAG}/"
    require(transport.TAG.startswith("presets-") and "preset-maintenance-source.json" in assets
            and not any(name.startswith("update-") for name in assets), "Invalid immutable maintenance namespace")
    recheck()
    transport.verify_tag(api)
    matches = [item for item in api.pages("/releases") if item["tag_name"] == transport.TAG]
    require(len(matches) <= 1, "Multiple releases use the maintenance tag")
    if matches:
        release = matches[0]
        require(release.get("name") == transport.TITLE and release.get("body", "").strip() == notes
                and release.get("prerelease") is True, "Existing maintenance release metadata differs; no replacement allowed")
        transport.verify_release_assets(api, release, assets, complete=not release["draft"])
        if not release["draft"]:
            transport.verify_tag(api, required=True)
            recheck()
            return transport.public_check(api, assets, manifest_names=("preset-maintenance-source.json",))
    else:
        release = api.json("/releases", method="POST", payload={
            "tag_name": transport.TAG, "target_commitish": source, "name": transport.TITLE,
            "body": notes, "draft": True, "prerelease": True, "make_latest": "false"})
    existing = transport.verify_release_assets(api, release, assets, complete=False)
    upload = release["upload_url"].split("{", 1)[0]
    require(upload == f"https://uploads.github.com/repos/{transport.REPO}/releases/{release['id']}/assets",
            "Unexpected maintenance upload endpoint")
    for name, path in assets.items():
        if name in existing:
            continue
        with path.open("rb") as stream:
            uploaded = api.json(upload + "?" + urlencode({"name": name}), method="POST", data=stream,
                headers={"Content-Type": mimetypes.guess_type(name)[0] or "application/octet-stream",
                         "Content-Length": str(path.stat().st_size)})
        transport.verify_asset(api, uploaded, path)
        print("Uploaded and SHA-256 verified " + name, flush=True)
    transport.verify_release_assets(api, release, assets, complete=True)
    recheck()
    transport.verify_tag(api)
    api.json(f"/releases/{release['id']}", method="PATCH", payload={
        "draft": False, "prerelease": True, "make_latest": "false", "target_commitish": source})
    transport.verify_tag(api, required=True)
    result = transport.public_check(api, assets, manifest_names=("preset-maintenance-source.json",))
    recheck()
    return result


def publish(policy, wait_seconds):
    source = policy.get("approved_product_source_sha")
    contract.validate_policy(policy, source=source, publication=True)
    controller = contract.git("rev-parse", "HEAD")
    proof = contract.verify_controller(source, controller, policy)
    scope = contract.verify_source_scope(source, policy)
    require(not (ROOT / "RELEASE_REVIEW_PENDING").exists(), "RELEASE_REVIEW_PENDING")
    event = os.environ.get("GITHUB_EVENT_NAME")
    require(event in ("push", "workflow_dispatch") and os.environ.get("GITHUB_REPOSITORY") == transport.REPO
            and os.environ.get("GITHUB_REF") == "refs/heads/main" and os.environ.get("GITHUB_SHA") == controller,
            "Maintenance publication requires the approved default-branch controller")
    api = transport.GitHub()
    repository = api.json("")
    require(repository.get("full_name") == transport.REPO and repository.get("default_branch") == "main",
            "Unexpected publication repository/default branch")
    invocation_id = os.environ.get("GITHUB_RUN_ID", "")
    require(invocation_id.isdecimal() and int(invocation_id) > 0, "Positive publisher workflow run ID required")
    invocation = api.json("/actions/runs/" + invocation_id)
    require(invocation.get("head_sha") == controller and invocation.get("head_branch") == "main"
            and invocation.get("event") == event and invocation.get("path") == contract.WORKFLOW_PATH,
            "Maintenance publisher invocation identity mismatch")
    release = official.Release(contract.BASE_VERSION, source)
    strict_policy = official.release_policy(release)
    historical = read_json(ROOT / contract.OLD_APPROVAL_PATH)
    baseline = base_release_snapshot(api)
    latest = api.optional("/releases/latest")
    latest_id = None if latest is None else latest["id"]

    def current_controller():
        require(api.json("").get("default_branch") == "main"
                and api.json("/git/ref/heads/main")["object"]["sha"] == controller,
                "A newer default-branch controller supersedes this publication")

    ci = official.wait_for_ci(api, release, wait_seconds, current_controller)
    exports = export_artifacts(api, ci, source)
    build = ci["runs"]["build.yml"]
    with tempfile.TemporaryDirectory(prefix="chimera-preset-maintenance-") as temporary:
        working = Path(temporary)
        gate_archive = working / "release-acceptance-evidence.zip"
        api.download(f"/actions/artifacts/{ci['gate']['id']}/zip", gate_archive)
        check_archive(gate_archive, ci["gate"])
        verdict, no_new_product_decision = official.verify_release_gate_archive(gate_archive, release,
            build["id"], build["run_attempt"], strict_policy)
        require(no_new_product_decision is None, "Maintenance must not manufacture a new product-gap approval")
        strict = contract.verify_retained_strict_gate(verdict, historical)
        archive = working / "candidate.zip"
        api.download(f"/actions/artifacts/{ci['candidate']['id']}/zip", archive)
        check_archive(archive, ci["candidate"])
        official.configure_transport(release, ci["candidate"])
        candidate = working / "candidate"
        candidate.mkdir()
        transport.extract_candidate(archive, candidate, max_expanded_bytes=CANDIDATE_MAX_EXPANDED_BYTES)
        _, candidate_assets = official.prepare_assets(candidate, release, build["id"], build["run_attempt"])
        manifests = {}
        for platform, artifact in exports.items():
            path = working / (platform + "-integration.zip")
            api.download(f"/actions/artifacts/{artifact['id']}/zip", path)
            check_archive(path, artifact)
            manifests[platform] = contract.read_integration_archive(path, source, platform, working / platform)
        bank_digest = contract.compare_platform_packs(manifests)
        pack = contract.create_pack(working / "linux",
            working / f"SpectralForge-Chimera-Factory-Presets-{policy['preset_revision']}.zip",
            policy, source, ROOT / "docs/PRESET_DYNAMICS_EQ.md")
        assets, binaries = maintenance_assets(candidate_assets, pack, policy, working)
        assets["PRESET_DYNAMICS_EQ.md"] = ROOT / "docs/PRESET_DYNAMICS_EQ.md"
        assets["preset-maintenance-policy.json"] = ROOT / contract.POLICY_PATH
        for platform in contract.PLATFORMS:
            assets[f"factory-preset-export-{platform}.json"] = working / platform / contract.PACK_MANIFEST
        assets["release-acceptance-evidence.zip"] = gate_archive
        assets["release-gate.json"] = write_json(working / "release-gate.json", verdict)
        baseline_decision = working / "base-beta1-publication-decision.json"
        base_asset = next(item for item in baseline["assets"] if item["name"] == "publication-decision.json")
        api.download(base_asset["browser_download_url"], baseline_decision, authenticated=False)
        require(sha256(baseline_decision) == contract.BASE_DECISION_SHA256, "Downloaded beta.1 receipt digest differs")
        assets[baseline_decision.name] = baseline_decision
        report = {"schema": "spectralforge.chimera.preset-maintenance-distribution.v1",
            "preset_revision": policy["preset_revision"], "release_tag": policy["release_tag"],
            "base_product_version": contract.BASE_VERSION, "base_product_source_sha": contract.BASE_SOURCE,
            "product_source_sha": source, "source_proof": scope, "controller_proof": proof,
            "owner_authorization": policy["owner_authorization"], "strict_validation": strict,
            "strict_product_release_ready": False, "new_validation_gaps_accepted": False,
            "beta_expires_at_utc": contract.BASE_EXPIRY, "preset_count": 48,
            "three_platform_parameter_bank_sha256": bank_digest,
            "cross_platform_normalized_parameter_tolerance": 1e-6,
            "ci": {name: {key: run[key] for key in ("id", "run_attempt", "head_sha")}
                   for name, run in ci["runs"].items()},
            "artifacts": {**{name: {key: ci[name][key] for key in ("id", "name", "digest", "size_in_bytes")}
                               for name in ("candidate", "gate")}, "factory_exports": artifact_identity(exports)},
            "factory_export_manifests_sha256": {platform: sha256(working / platform / contract.PACK_MANIFEST)
                                                for platform in contract.PLATFORMS},
            "download_packages": [{"name": name, "size": assets[name].stat().st_size, "sha256": sha256(assets[name])}
                                  for name in (*binaries, pack.name)],
            "baseline_release_snapshot_sha256": contract.weekly_publication.digest(baseline),
            "update_feed_changed": False, "product_version_changed": False, "expiry_changed": False,
            "publisher_signed": False, "macos_notarized": False, "actual_daw_acceptance": False,
            "listening_acceptance": False}
        assets["preset-maintenance-source.json"] = write_json(working / "preset-maintenance-source.json", report)
        disclosure = working / "VALIDATION_SCOPE.md"
        disclosure.write_text(validation_scope(report), encoding="utf-8")
        assets[disclosure.name] = disclosure
        notes = release_notes(policy, source, binaries, pack.name, build["id"])
        official.check_public_names.validate_text(notes, "preset maintenance public release notes",
            policy=official.check_public_names.load_policy(ROOT), exceptions=[])

        def recheck():
            current_controller()
            require(ci_identity(official.verify_ci(api, release)) == ci_identity(ci)
                    and artifact_identity(export_artifacts(api, ci, source)) == artifact_identity(exports),
                    "Exact-source CI or artifacts changed during publication preparation")
            require(contract.verify_controller(source, controller, read_json(ROOT / contract.POLICY_PATH)) == proof
                    and contract.verify_source_scope(source, policy) == scope
                    and official.release_policy(release) == strict_policy,
                    "Maintenance source, policy or controller changed")
            # Uploading never grants extra beta time. Re-evaluate the original
            # approved lifecycle at the final public transition as well.
            official.beta_lifecycle.validate_metadata(read_json(assets["chimera-beta-lifecycle.json"]),
                                                       ROOT, source, publication=True)
            require(base_release_snapshot(api) == baseline, "An existing beta.1 release/tag/asset changed")
            current_latest = api.optional("/releases/latest")
            require((None if current_latest is None else current_latest["id"]) == latest_id,
                    "Latest product release changed during maintenance publication")

        result = publish_immutable(api, policy, source, notes, assets, recheck)
        print("Published and publicly verified: " + result["html_url"])
        if os.environ.get("GITHUB_STEP_SUMMARY"):
            with open(os.environ["GITHUB_STEP_SUMMARY"], "a", encoding="utf-8") as summary:
                summary.write(f"## Factory preset maintenance published\n\n[{policy['release_tag']}]({result['html_url']})\n\n"
                    f"Five rebuilt packages and 48 presets, exact source `{source}`. "
                    "Seven required workflows and all three factory exports passed. "
                    "Product version/expiry retained; strict product gate remains 21 PASS / 107 unexecuted BLOCKED.\n")
        return result


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--check-source", action="store_true", help="read-only Git source/policy verification")
    mode.add_argument("--approval-state", action="store_true", help="read receipt; never publish")
    mode.add_argument("--publish", action="store_true", help="approved default-branch workflow only")
    parser.add_argument("--source-sha")
    parser.add_argument("--wait-ci-seconds", type=int, default=0)
    args = parser.parse_args(argv)
    require(args.publish or args.wait_ci_seconds == 0, "CI waiting is only available to the publication controller")
    require(not args.publish or args.source_sha is None, "Live source must come from the reviewed receipt")
    policy = contract.validate_policy(read_json(ROOT / contract.POLICY_PATH))
    if args.check_source:
        source = args.source_sha or contract.git("rev-parse", "HEAD")
        result = contract.verify_source_scope(source, policy)
        print(json.dumps({"mode": "SOURCE_SCOPE_VERIFIED", "published": False, **result}, indent=2))
        return result
    if args.approval_state:
        ready = policy["approved_product_source_sha"] is not None
        if ready:
            contract.verify_controller(policy["approved_product_source_sha"], contract.git("rev-parse", "HEAD"), policy)
        if os.environ.get("GITHUB_OUTPUT"):
            with open(os.environ["GITHUB_OUTPUT"], "a", encoding="utf-8") as output:
                output.write("ready=" + str(ready).lower() + "\n")
        print("Exact-source maintenance receipt is ready" if ready else
              "Prepared: exact tested source approval is not yet recorded; no publication attempted")
        return {"ready": ready, "published": False}
    return publish(policy, args.wait_ci_seconds)


if __name__ == "__main__":
    try:
        main()
    except (RuntimeError, ValueError, KeyError, TypeError, OSError, subprocess.SubprocessError, zipfile.BadZipFile) as error:
        print(f"Preset maintenance stopped: {error}", file=sys.stderr)
        sys.exit(1)
