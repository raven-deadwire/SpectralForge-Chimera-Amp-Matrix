#!/usr/bin/env python3
"""Source- and version-scoped approval for the weekly publication path.

The publisher must supply digest-verified exact-source CI evidence. This module
does not grant approval, contact GitHub, or change a check report. The new
manifest replaces historical release_scope approval flags: a controller can
record approval of a tested SHA without rebuilding that SHA to approve itself.
Release_scope still identifies the product being prepared. Every new version or
source needs its own explicit approval and full policy fingerprints.
"""
from __future__ import annotations

import copy
import hashlib
import json
from pathlib import Path, PurePosixPath
import re

import beta_publication131 as historical
import evaluate_release_gate as gate

ROOT = Path(__file__).resolve().parents[1]
SCHEMA = "spectralforge.chimera.weekly-publication-policy"
STRICT_DECISION = "STRICT_PASS"
GAP_DECISION = "OWNER_ACCEPTED_BETA_WITH_VALIDATION_GAPS"
BETA_PROFILE = "beta_1_3"
FULL_PROFILE = "full_release"
CI_IDS = historical.CI_IDS
GAP_IDS = historical.GAP_IDS
TRANSPOSE_IDS = frozenset({"I2.PITCH_LIVE", "I2.PITCH_DI"})
CI_CHECK_MAP_SHA256 = historical.CI_CHECK_MAP_SHA256
# Version, channel and approval metadata can evolve without weakening a gate.
# Changing any validation rule still requires a reviewed policy/code revision.
VALIDATION_RULES_SHA256 = "8acd522fcf39d08b55bad079a5d706efc8e14e5a38730d3d77a699da610074fc"
require = gate.require


def digest(value):
    return hashlib.sha256(json.dumps(value, sort_keys=True, separators=(",", ":"),
                                     ensure_ascii=False, allow_nan=False).encode("utf-8")).hexdigest()


def _text(value):
    return isinstance(value, str) and bool(value.strip())


def _sha(value, length):
    return isinstance(value, str) and re.fullmatch(r"[0-9a-f]{" + str(length) + "}", value) is not None


def _version(version):
    require(isinstance(version, str), "Explicit canonical release version required")
    match = re.fullmatch(r"((?:0|[1-9][0-9]*)\.(?:0|[1-9][0-9]*)\.(?:0|[1-9][0-9]*))"
                         r"(?:-(beta|rc)\.([1-9][0-9]*))?", version)
    require(match is not None and all(int(n) <= 65535 for n in match[1].split(".")),
            "Canonical beta, RC or stable release version required")
    channel = match[2] + "." + match[3] if match[2] else "stable"
    return match[1], channel, match[2] or "stable"


def _inventory(rows, expected, label):
    require(isinstance(rows, list) and all(isinstance(row, dict) for row in rows),
            f"Malformed {label} inventory")
    ids = [row.get("id") for row in rows]
    require(all(isinstance(cid, str) for cid in ids)
            and len(ids) == len(set(ids)) and set(ids) == expected,
            f"Unexpected, missing or duplicate {label} ID")
    return {row["id"]: row for row in rows}


def _positive_id(value):
    return (type(value) is int and value > 0
            or isinstance(value, str) and re.fullmatch(r"[1-9][0-9]*", value) is not None)


def validate_approval(publication_policy, strict_policy, source_sha, *, version, profile,
                      waivers=None, ci_check_map=None):
    """Validate a fresh approval, without claiming CI or publication readiness.

    A full strict-policy digest binds the approval to its source metadata. An
    additional invariant digest prevents even a re-fingerprinted approval from
    silently removing tests or hard gates. No old beta authorization is read.
    """
    require(_sha(source_sha, 40), "Full lowercase product source SHA required")
    product_version, channel, kind = _version(version)
    require(profile in (BETA_PROFILE, FULL_PROFILE)
            and (kind == "beta" or profile == FULL_PROFILE),
            "RC and stable publication require full_release strict validation")
    require(isinstance(strict_policy, dict), "Malformed strict validation policy")
    scope = strict_policy.get("release_scope", {})
    require(isinstance(scope, dict) and scope.get("version") == version
            and scope.get("product_version") == product_version
            and scope.get("channel") == channel and scope.get("profile") == profile
            and scope.get("preparation_authorized") is True,
            "Source release scope does not match the requested version/profile")
    require(isinstance(publication_policy, dict)
            and publication_policy.get("schema") == SCHEMA
            and type(publication_policy.get("schema_version")) is int
            and publication_policy["schema_version"] == 1
            and type(publication_policy.get("policy_version")) is int
            and publication_policy["policy_version"] == 1
            and publication_policy.get("version") == version
            and publication_policy.get("product_version") == product_version
            and publication_policy.get("profile") == profile
            and publication_policy.get("approved_product_source_sha") == source_sha,
            "Weekly publication policy/version/source identity mismatch")
    decision = publication_policy.get("decision")
    require(decision in (STRICT_DECISION, GAP_DECISION), "Unknown weekly publication decision")
    require(decision != GAP_DECISION or kind == "beta" and profile == BETA_PROFILE,
            "Validation gaps are allowed only for an explicitly accepted beta profile")
    require(publication_policy.get("release_approved") is True
            and publication_policy.get("publication_authorized") is True,
            "Source-specific weekly release approval is missing")
    owner = publication_policy.get("owner_acceptance", {})
    require(isinstance(owner, dict) and owner.get("approved") is True
            and owner.get("publication_authorized") is True
            and type(owner.get("validation_gaps_accepted")) is bool
            and owner["validation_gaps_accepted"] is (decision == GAP_DECISION)
            and all(_text(owner.get(field)) for field in ("instruction", "context", "scope")),
            "Specific owner acceptance of this publication scope is missing")
    require(_sha(publication_policy.get("strict_policy_sha256"), 64)
            and digest(strict_policy) == publication_policy["strict_policy_sha256"],
            "Strict policy differs from the approved fingerprint")
    require(digest({key: value for key, value in strict_policy.items() if key != "release_scope"})
            == VALIDATION_RULES_SHA256, "Validation rules or required gates changed")
    waivers = gate.load_json(ROOT / "Validation/waivers.json") if waivers is None else waivers
    ci_check_map = gate.load_json(ROOT / "Validation/ci-check-map.json") if ci_check_map is None else ci_check_map
    require(_sha(publication_policy.get("ci_check_map_sha256"), 64)
            and digest(ci_check_map) == publication_policy["ci_check_map_sha256"] == CI_CHECK_MAP_SHA256,
            "CI ownership map differs from the approved contract")
    require(waivers == {"schema": "spectralforge.chimera.validation.waivers",
                        "schema_version": 1, "waivers": []}, "Waivers must remain unchanged")
    ci = _inventory(publication_policy.get("required_ci_checks"), CI_IDS, "automatic check")
    expected_ci = {}
    for cid, spec in ci_check_map["checks"].items():
        platforms = (["assembly"] if spec["producer"] == "workflow:assemble-candidate" else
                     ["windows", "linux", "macos"] if spec["platform"] == "all" else [spec["platform"]])
        expected_ci[cid] = {"id": cid, "name": spec["name"], "producer": spec["producer"], "platforms": platforms}
    require(ci == expected_ci, "Automatic check ownership or platform coverage changed")
    owners = {cid: stage for stage in strict_policy["profiles"][profile]["required_stages"]
              for cid in strict_policy["stages"][stage]["required_checks"]}
    expected = CI_IDS | GAP_IDS | (TRANSPOSE_IDS if profile == FULL_PROFILE else frozenset())
    require(set(owners) == expected, "Complete required validation inventory must be retained")
    gaps = _inventory(publication_policy.get("known_validation_gaps"),
                      GAP_IDS if decision == GAP_DECISION else frozenset(), "known gap")
    for cid, entry in gaps.items():
        require(entry.get("stage") == owners[cid]
                and entry.get("stage_name") == strict_policy["stages"][owners[cid]]["name"]
                and entry.get("strict_hard_gate") is (cid in strict_policy["hard_gates"])
                and _text(entry.get("gap_type")) and _text(entry.get("description")),
                f"Gap disclosure does not match strict policy: {cid}")
    return {"product_version": product_version, "kind": kind, "decision": decision,
            "owner": owner, "owners": owners, "ci": ci, "gaps": gaps,
            "waivers": waivers, "ci_check_map": ci_check_map}


def _check_pass(check, cid):
    require(check["execution"].get("executed") is True
            and type(check["execution"].get("exit_code")) is int
            and check["execution"]["exit_code"] == 0
            and isinstance(check.get("assertion"), dict)
            and check["assertion"].get("passed") is True
            and check.get("failure") is None,
            f"Required check failed or is incomplete: {cid}")


def evaluate(publication_policy, strict_policy, checks, strict_verdict,
             source_sha, run_id, run_attempt, *, version, profile,
             waivers=None, ci_check_map=None):
    """Recompute strict evidence and return permission only for its approved scope."""
    approved = validate_approval(publication_policy, strict_policy, source_sha,
                                 version=version, profile=profile,
                                 waivers=waivers, ci_check_map=ci_check_map)
    require(_positive_id(run_id) and _positive_id(run_attempt), "Positive CI run and attempt required")
    revision = {"commit_sha": source_sha, "run_id": str(run_id), "run_attempt": str(run_attempt)}
    owners, ci, gaps = approved["owners"], approved["ci"], approved["gaps"]
    require(isinstance(checks, dict) and set(checks) == set(owners),
            "Complete required check report inventory is missing")
    require(isinstance(strict_verdict, dict)
            and strict_verdict.get("schema") == "spectralforge.chimera.release-gate"
            and type(strict_verdict.get("schema_version")) is int
            and strict_verdict["schema_version"] == 1
            and strict_verdict.get("profile") == profile
            and strict_verdict.get("policy_version") == strict_policy["policy_version"]
            and strict_verdict.get("revision") == revision,
            "Strict verdict source/run/attempt identity mismatch")
    producer = strict_verdict.get("producer", {})
    require(isinstance(producer, dict) and producer.get("name") == "consolidate_release_gate"
            and producer.get("errors") == [], "Strict evidence producer has errors")
    inputs = producer.get("inputs")
    require(isinstance(inputs, list) and len(inputs) == 4
            and all(isinstance(item, dict) for item in inputs)
            and {item.get("kind") for item in inputs} == {"windows", "linux", "macos", "assembly"}
            and all(item.get("revision") == revision and _sha(item.get("manifest_sha256"), 64)
                    for item in inputs), "Missing, duplicate or stale platform evidence")
    for cid, check in checks.items():
        gate.validate_check(check, cid)
        hard = cid in strict_policy["hard_gates"]
        require(check["id"] == cid and check["stage"] == owners[cid]
                and check["policy"].get("required") is True
                and check["policy"].get("hard_gate") is hard
                and check["policy"].get("na_policy") == ("FORBIDDEN" if hard else "ALLOWED_WITH_REASON")
                and check["policy"].get("requires_current_commit") is True
                and check["policy"].get("requires_policy_version") is True
                and check["evidence"].get("commit_sha") == source_sha
                and check["evidence"].get("policy_version") == strict_policy["policy_version"]
                and check["applicability"] == {"applicable": True, "waiver_id": None}
                and check["dependencies"] == [], f"Invalid source/policy binding for {cid}")
        if cid in CI_IDS:
            receipt, expected = check.get("scope", {}), ci[cid]
            platforms = expected["platforms"]
            require(isinstance(receipt, dict) and receipt.get("producer") == expected["producer"]
                    and receipt.get("platforms") == platforms
                    and receipt.get("synthetic_only") is True
                    and receipt.get("run_id") == str(run_id)
                    and receipt.get("run_attempt") == str(run_attempt)
                    and check["evidence"].get("artifacts") == ["evidence/" + p + "/evidence.json" for p in platforms],
                    f"Automatic check provenance or platform coverage changed: {cid}")
            _check_pass(check, cid)
        elif approved["decision"] == GAP_DECISION:
            require(check.get("scope") == {"producer": "unclaimed-baseline", "profile": profile}
                    and check["execution"] == {"executed": False, "exit_code": None}
                    and check.get("assertion") is None
                    and isinstance(check.get("failure"), dict)
                    and check["failure"].get("type") == "ARTIFACT_MISSING",
                    f"Gap is executed, failed, waived or promoted rather than an unclaimed baseline: {cid}")
        else:
            _check_pass(check, cid)
            receipt = check.get("scope", {})
            artifacts = check["evidence"].get("artifacts")
            require(isinstance(receipt, dict) and _text(receipt.get("producer"))
                    and receipt["producer"] != "unclaimed-baseline"
                    and receipt.get("run_id") == str(run_id)
                    and receipt.get("run_attempt") == str(run_attempt)
                    and isinstance(artifacts, list) and artifacts
                    and all(_text(path) and not PurePosixPath(path).is_absolute()
                            and ".." not in PurePosixPath(path).parts and "\\" not in path for path in artifacts),
                    f"Strict check has no current-run evidence producer: {cid}")
    computed = gate.evaluate_release(strict_policy, approved["waivers"], checks, profile, source_sha)
    for field in ("verdict", "ready", "counts", "hard_gates", "checks", "stages", "blockers"):
        require(digest(strict_verdict.get(field)) == digest(computed[field]),
                "Strict gate summary differs from check reports: " + field)
    statuses = {row["id"]: row for row in computed["checks"]}
    if approved["decision"] == GAP_DECISION:
        require(all(statuses[cid]["computed_status"] == "PASS" for cid in CI_IDS)
                and all(statuses[cid]["computed_status"] == "BLOCKED"
                        and statuses[cid]["reason_code"] == "NOT_EXECUTED" for cid in GAP_IDS)
                and computed["counts"]["checks"] == {"total": 128, "pass": 21, "blocked": 107, "not_applicable": 0}
                and computed["hard_gates"] == {"total": 63, "pass": 21, "blocked": 42}
                and computed["verdict"] == "BLOCKED" and computed["ready"] is False,
                "Unexpected beta validation-gap inventory or strict result")
        limitations = ("Owner-accepted beta with explicitly disclosed validation gaps. The strict gate remains "
                       "BLOCKED; no missing check is represented as executed or PASS.")
    else:
        require(computed["verdict"] == "PASS" and computed["ready"] is True
                and computed["counts"]["checks"] == {"total": len(owners), "pass": len(owners),
                                                        "blocked": 0, "not_applicable": 0}
                and computed["hard_gates"]["blocked"] == 0 and not computed["blockers"],
                "All required strict checks must PASS before publication")
        limitations = "All required checks in the named strict profile passed for the approved source and CI run."
    return {
        "schema": "spectralforge.chimera.weekly-publication-decision", "schema_version": 1,
        "policy_version": 1, "version": version, "product_version": approved["product_version"],
        "profile": profile, "revision": revision, "decision": approved["decision"],
        "publication_allowed": True, "all_required_checks_passed": approved["decision"] == STRICT_DECISION,
        "release_approved": True, "publication_authorized": True,
        "owner_acceptance": copy.deepcopy(approved["owner"]),
        "publication_policy_sha256": digest(publication_policy),
        "strict_validation": {key: copy.deepcopy(computed[key]) for key in ("verdict", "ready", "counts", "hard_gates")},
        "strict_gate_sha256": digest(strict_verdict), "strict_policy_sha256": digest(strict_policy),
        "ci_check_map_sha256": digest(approved["ci_check_map"]),
        "automatic_checks": [copy.deepcopy(statuses[cid]) for cid in sorted(CI_IDS)],
        "known_validation_gaps": [{**copy.deepcopy(gaps[cid]), "computed_status": "BLOCKED",
                                    "reason_code": "NOT_EXECUTED", "source_sha": source_sha,
                                    "executed": False} for cid in sorted(gaps)],
        "limitations": limitations,
    }


def render_markdown(decision):
    """Render the actual decision and all accepted gaps without relabeling them."""
    counts = decision["strict_validation"]["counts"]["checks"]
    hard = decision["strict_validation"]["hard_gates"]
    rows = [f"# Chimera {decision['version']} publication validation", "",
            f"Decision: `{decision['decision']}`.", "", decision["limitations"], "",
            f"Source: `{decision['revision']['commit_sha']}`.", "",
            f"Profile: `{decision['profile']}`; CI run `{decision['revision']['run_id']}`, "
            f"attempt `{decision['revision']['run_attempt']}`.", "",
            f"Strict validation: {counts['pass']} PASS / {counts['blocked']} BLOCKED / "
            f"{counts['not_applicable']} NOT_APPLICABLE; {hard['blocked']} strict hard gates remain BLOCKED."]
    if decision["known_validation_gaps"]:
        rows += ["", "| Check | Stage | Strict hard gate | Remaining evidence gap |", "| --- | --- | --- | --- |"]
        for item in decision["known_validation_gaps"]:
            values = [item["id"], item["stage_name"], "Yes" if item["strict_hard_gate"] else "No", item["description"]]
            rows.append("| " + " | ".join(str(value).replace("|", "\\|").replace("\n", " ") for value in values) + " |")
    return "\n".join(rows) + "\n"
