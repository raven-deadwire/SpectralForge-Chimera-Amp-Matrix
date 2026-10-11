#!/usr/bin/env python3
"""A separate owner-accepted beta decision; never rewrite the strict gate.

Call only with reports from the digest-verified exact-source CI archive. The
publisher remains responsible for raw archive/package provenance and live CI.
This helper recomputes all 128 strict checks and accepts one frozen gap inventory.
"""
from __future__ import annotations

import copy
import hashlib
import json
from pathlib import Path
import re

import evaluate_release_gate as gate

PROFILE = "beta_1_3"
VERSION = "1.3.0-beta.1"
DECISION = "OWNER_ACCEPTED_BETA_WITH_VALIDATION_GAPS"
STRICT_POLICY_SHA256 = "944c02099f1eaa23e6c5b6151becde62e696e18ef466805770de6e91b2f2015f"
CI_CHECK_MAP_SHA256 = "d59f7e55cfcd16eaba5b712b959f10fd4d99846053f31fc821fdd5633f85420f"
ROOT = Path(__file__).resolve().parents[1]
CI_IDS = frozenset(
    [f"A12.{n:02}" for n in range(1, 8)] + [f"A13.{n:02}" for n in range(1, 8)]
    + ["E1.DSP", "E1.STATE_UI", "E1.LEGACY", "I1.PITCH", "I1.GATE",
       "I1.STATE_GAIN_GR", "I1.LIVE_REMOVE"])
LEGACY_COUNTS = {"A1": 6, "A2": 8, "A3": 9, "A4": 5, "A5": 8, "A6": 8,
                 "A7": 7, "A8": 4, "A9": 9, "A10": 6, "A11": 9, "A14": 7}
GAP_IDS = frozenset(
    [f"{stage}.{n:02}" for stage, count in LEGACY_COUNTS.items() for n in range(1, count + 1)]
    + [f"B{n}.RESULT" for n in (1, 2, 3, 5, 6, 8, 10, 11, 12, 13, 14)]
    + [f"B4.H{n}" for n in range(1, 5)]
    + ["E2.TD_EQ", "E2.CALIBRATION", "E2.SAME_DI", "I2.GATE_DI",
       "I2.UI_AUDIO_TIMING", "I2.LIVE_REMOVE_DAW"])
require = gate.require


def digest(value):
    return hashlib.sha256(json.dumps(value, sort_keys=True, separators=(",", ":"),
                                     ensure_ascii=False).encode("utf-8")).hexdigest()


def inventory(rows, expected, label):
    require(isinstance(rows, list) and all(isinstance(row, dict) for row in rows),
            f"Malformed {label} inventory")
    ids = [row.get("id") for row in rows]
    require(all(isinstance(cid, str) for cid in ids)
            and len(ids) == len(set(ids)) and set(ids) == expected,
            f"Unexpected, missing or duplicate {label} ID")
    return {row["id"]: row for row in rows}


def evaluate(publication_policy, strict_policy, checks, strict_verdict,
             source_sha, run_id, run_attempt, *, waivers=None, ci_check_map=None):
    """Return publication permission with disclosed gaps, or raise ValidationError.

    No check is changed, waived or promoted. Even an executed failure among the
    accepted gap IDs is rejected. Future producers require a policy revision.
    """
    require(isinstance(source_sha, str) and re.fullmatch(r"[0-9a-f]{40}", source_sha),
            "Full product source SHA required")
    require(str(run_id).isdigit() and int(run_id) > 0
            and str(run_attempt).isdigit() and int(run_attempt) > 0,
            "Positive CI run and attempt required")
    revision = {"commit_sha": source_sha, "run_id": str(run_id), "run_attempt": str(run_attempt)}
    require(isinstance(publication_policy, dict)
            and publication_policy.get("schema") == "spectralforge.chimera.beta-publication-policy"
            and type(publication_policy.get("schema_version")) is int
            and publication_policy["schema_version"] == 1
            and type(publication_policy.get("policy_version")) is int
            and publication_policy["policy_version"] == 1
            and publication_policy.get("profile") == PROFILE
            and publication_policy.get("version") == VERSION
            and publication_policy.get("product_version") == "1.3.0"
            and publication_policy.get("approved_product_source_sha") == source_sha
            and publication_policy.get("decision") == DECISION
            and publication_policy.get("strict_validation_is_pass") is False,
            "Beta publication policy/source identity mismatch")
    require(digest(strict_policy) == STRICT_POLICY_SHA256
            == publication_policy.get("strict_policy_sha256"),
            "Strict validation policy changed")
    waivers = gate.load_json(ROOT / "Validation/waivers.json") if waivers is None else waivers
    ci_check_map = gate.load_json(ROOT / "Validation/ci-check-map.json") if ci_check_map is None else ci_check_map
    require(digest(ci_check_map) == CI_CHECK_MAP_SHA256
            == publication_policy.get("ci_check_map_sha256"), "CI ownership map changed")
    require(waivers == {"schema": "spectralforge.chimera.validation.waivers",
                        "schema_version": 1, "waivers": []}, "Waivers must remain unchanged")
    owner = publication_policy.get("owner_acceptance", {})
    require(isinstance(owner, dict) and owner.get("approved") is True
            and owner.get("publication_authorized") is True
            and owner.get("instruction") == "그럼 처리를 하렴 좀"
            and owner.get("testing_reported") is True
            and owner.get("individual_check_claims") == []
            and bool(owner.get("context")) and bool(owner.get("scope")),
            "Specific owner acceptance of disclosed beta gaps is missing")
    scope = strict_policy.get("release_scope", {})
    require(scope.get("release_approved") is True and scope.get("publication_authorized") is True,
            "Owner release approval is missing")
    ci = inventory(publication_policy.get("required_ci_checks"), CI_IDS, "automatic check")
    expected_ci = {}
    for cid, spec in ci_check_map["checks"].items():
        platforms = (["assembly"] if spec["producer"] == "workflow:assemble-candidate" else
                     ["windows", "linux", "macos"] if spec["platform"] == "all" else [spec["platform"]])
        expected_ci[cid] = {"id": cid, "name": spec["name"], "producer": spec["producer"], "platforms": platforms}
    require(ci == expected_ci, "Automatic check ownership or platform coverage changed")
    gaps = inventory(publication_policy.get("known_validation_gaps"), GAP_IDS, "known gap")
    owners = {cid: stage for stage in strict_policy["profiles"][PROFILE]["required_stages"]
              for cid in strict_policy["stages"][stage]["required_checks"]}
    require(set(owners) == CI_IDS | GAP_IDS and len(owners) == 128
            and isinstance(checks, dict) and set(checks) == set(owners),
            "Complete fixed 128-check inventory required")
    require(isinstance(strict_verdict, dict)
            and strict_verdict.get("schema") == "spectralforge.chimera.release-gate"
            and type(strict_verdict.get("schema_version")) is int
            and strict_verdict["schema_version"] == 1
            and strict_verdict.get("profile") == PROFILE
            and strict_verdict.get("policy_version") == strict_policy["policy_version"]
            and strict_verdict.get("revision") == revision,
            "Strict verdict source/run/attempt identity mismatch")
    producer = strict_verdict.get("producer", {})
    require(producer.get("name") == "consolidate_release_gate"
            and producer.get("errors") == [], "Strict evidence producer has errors")
    inputs = producer.get("inputs")
    require(isinstance(inputs, list) and len(inputs) == 4
            and all(isinstance(item, dict) for item in inputs)
            and {item.get("kind") for item in inputs} == {"windows", "linux", "macos", "assembly"}
            and all(item.get("revision") == revision
                    and isinstance(item.get("manifest_sha256"), str)
                    and re.fullmatch(r"[0-9a-f]{64}", item.get("manifest_sha256", "")) for item in inputs),
            "Missing, duplicate or stale platform evidence")
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
            receipt = check.get("scope", {})
            expected = ci[cid]
            platforms = expected.get("platforms")
            require(isinstance(platforms, list) and platforms
                    and len(platforms) == len(set(platforms))
                    and set(platforms) <= {"windows", "linux", "macos", "assembly"}
                    and receipt.get("producer") == expected.get("producer")
                    and receipt.get("platforms") == platforms
                    and receipt.get("synthetic_only") is True
                    and receipt.get("run_id") == str(run_id)
                    and receipt.get("run_attempt") == str(run_attempt)
                    and check["evidence"].get("artifacts") == ["evidence/" + p + "/evidence.json" for p in platforms]
                    and check["execution"].get("executed") is True
                    and type(check["execution"].get("exit_code")) is int
                    and check["execution"]["exit_code"] == 0
                    and check.get("assertion") == {"passed": True}
                    and check.get("failure") is None, f"Automatic check failed or is incomplete: {cid}")
        else:
            entry = gaps[cid]
            require(entry.get("stage") == owners[cid]
                    and entry.get("stage_name") == strict_policy["stages"][owners[cid]]["name"]
                    and entry.get("strict_hard_gate") is hard
                    and bool(entry.get("gap_type")) and bool(entry.get("description")),
                    f"Gap disclosure does not match strict policy: {cid}")
            require(check.get("scope") == {"producer": "unclaimed-baseline", "profile": PROFILE}
                    and check["execution"] == {"executed": False, "exit_code": None}
                    and check.get("assertion") is None
                    and isinstance(check.get("failure"), dict)
                    and check["failure"].get("type") == "ARTIFACT_MISSING",
                    f"Gap is executed, failed, waived or promoted rather than an unclaimed baseline: {cid}")
    computed = gate.evaluate_release(strict_policy, waivers, checks, PROFILE, source_sha)
    for field in ("verdict", "ready", "counts", "hard_gates", "checks", "stages", "blockers"):
        require(digest(strict_verdict.get(field)) == digest(computed[field]),
                "Strict gate summary differs from check reports: " + field)
    statuses = {row["id"]: row for row in computed["checks"]}
    require(all(statuses[cid]["computed_status"] == "PASS" for cid in CI_IDS)
            and all(statuses[cid]["computed_status"] == "BLOCKED"
                    and statuses[cid]["reason_code"] == "NOT_EXECUTED" for cid in GAP_IDS)
            and computed["counts"]["checks"] == {"total": 128, "pass": 21, "blocked": 107, "not_applicable": 0}
            and computed["hard_gates"] == {"total": 63, "pass": 21, "blocked": 42}
            and computed["verdict"] == "BLOCKED" and computed["ready"] is False,
            "Unexpected strict validation outcome")
    return {
        "schema": "spectralforge.chimera.beta-publication-decision", "schema_version": 1,
        "policy_version": 1, "version": VERSION, "profile": PROFILE, "revision": revision,
        "decision": DECISION, "publication_allowed": True, "all_required_checks_passed": False,
        "owner_acceptance": copy.deepcopy(owner),
        "strict_validation": {key: copy.deepcopy(computed[key])
                              for key in ("verdict", "ready", "counts", "hard_gates")},
        "strict_gate_sha256": digest(strict_verdict), "strict_policy_sha256": STRICT_POLICY_SHA256,
        "ci_check_map_sha256": CI_CHECK_MAP_SHA256,
        "automatic_checks": [copy.deepcopy(statuses[cid]) for cid in sorted(CI_IDS)],
        "known_validation_gaps": [{**copy.deepcopy(gaps[cid]), "computed_status": "BLOCKED",
                                   "reason_code": "NOT_EXECUTED", "source_sha": source_sha,
                                   "executed": False} for cid in sorted(GAP_IDS)],
        "limitations": "Owner-accepted open beta with disclosed validation gaps. The strict gate remains BLOCKED; no missing check is represented as executed or PASS.",
    }


def render_markdown(decision):
    """Render all gaps for the public release alongside the machine-readable decision."""
    rows = ["# Chimera 1.3 beta validation disclosure", "",
            "Decision: `" + decision["decision"] + "`.", "",
            decision["limitations"], "",
            "Source: `" + decision["revision"]["commit_sha"] + "`.", "",
            "Strict validation: 21 PASS / 107 BLOCKED; 42 strict hard gates remain BLOCKED.", "",
            "| Check | Stage | Strict hard gate | Remaining evidence gap |",
            "| --- | --- | --- | --- |"]
    for item in decision["known_validation_gaps"]:
        rows.append("| " + " | ".join([item["id"], item["stage_name"],
                    "Yes" if item["strict_hard_gate"] else "No", item["description"]]) + " |")
    return "\n".join(rows) + "\n"
