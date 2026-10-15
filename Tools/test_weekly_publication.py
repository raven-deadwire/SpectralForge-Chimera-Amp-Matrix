#!/usr/bin/env python3
"""Synthetic approval/evidence regression; never real product acceptance."""
import copy
from pathlib import Path
import unittest
from unittest.mock import patch

import evaluate_release_gate as gate
import release_evidence as evidence
import test_release_consolidator as fixtures
import weekly_publication as weekly

HEAD, RUN, ATTEMPT = fixtures.HEAD, fixtures.RUN, fixtures.ATTEMPT


class WeeklyPublication(unittest.TestCase):
    def setUp(self):
        self.version, self.profile = "1.3.1-beta.2", weekly.BETA_PROFILE
        self.strict_policy = gate.load_json(weekly.ROOT / "Validation/release-policy.json")
        self.mapping = gate.load_json(weekly.ROOT / "Validation/ci-check-map.json")
        self.waivers = gate.load_json(weekly.ROOT / "Validation/waivers.json")
        self.template = gate.load_json(weekly.ROOT / "Tools/fixtures/weekly-publication-policy-unapproved.json")
        self.set_scope()
        # Use the raw three-platform and assembly fixture producer for a
        # synthetic beta.2 identity. These receipts are unit-test inputs only.
        self.fixture = fixtures.Consolidation("test_actual_automatic_receipts_pass_only_owned_checks")
        with patch.object(evidence, "configuration", return_value=(self.strict_policy, self.mapping["checks"])), \
             patch.object(evidence, "source_profile", return_value=weekly.BETA_PROFILE):
            self.fixture.setUp()
            self.addCleanup(self.fixture.doCleanups)
            self.strict, output = self.fixture.run_gate()
        self.checks = gate.read_checks(output / "checks")
        self.policy = self.approval(weekly.GAP_DECISION)

    def set_scope(self):
        parts = self.version.split("-", 1)
        self.strict_policy["release_scope"] = {
            "version": self.version, "product_version": parts[0], "profile": self.profile,
            "channel": parts[1] if len(parts) == 2 else "stable", "preparation_authorized": True,
            # Old booleans do not grant or revoke the separate fresh approval.
            "release_approved": False, "publication_authorized": False,
        }

    def approval(self, decision):
        policy = copy.deepcopy(self.template)
        policy.update(version=self.version, product_version=self.version.split("-", 1)[0],
                      profile=self.profile, approved_product_source_sha=HEAD,
                      strict_policy_sha256=weekly.digest(self.strict_policy),
                      ci_check_map_sha256=weekly.digest(self.mapping), decision=decision,
                      release_approved=True, publication_authorized=True)
        policy["owner_acceptance"] = {
            "approved": True, "publication_authorized": True,
            "validation_gaps_accepted": decision == weekly.GAP_DECISION,
            "instruction": "Synthetic test approval for this exact version and source only.",
            "context": "Unit-test fixture; never product acceptance.",
            "scope": self.version + " / " + HEAD,
        }
        if decision == weekly.GAP_DECISION:
            # Reuse only gap descriptions, never historical approval metadata.
            historical = gate.load_json(weekly.ROOT / "Validation/beta-publication-policy-1.3.1.json")
            policy["known_validation_gaps"] = copy.deepcopy(historical["known_validation_gaps"])
        return policy

    def evaluate(self, **overrides):
        args = dict(version=self.version, profile=self.profile,
                    waivers=self.waivers, ci_check_map=self.mapping)
        args.update(overrides)
        return weekly.evaluate(self.policy, self.strict_policy, self.checks, self.strict,
                               HEAD, RUN, ATTEMPT, **args)

    def rejected(self, **overrides):
        with self.assertRaises(gate.ValidationError):
            self.evaluate(**overrides)

    def recompute(self):
        self.strict.update(gate.evaluate_release(self.strict_policy, self.waivers,
                                                self.checks, self.profile, HEAD))
        self.strict["revision"].update(run_id=RUN, run_attempt=ATTEMPT)

    def strict_pass_fixture(self, version, profile):
        # Explicitly fabricated success reports exercise strict decision logic;
        # no native/listening/DAW validation is claimed by this test helper.
        self.version, self.profile = version, profile
        self.set_scope()
        for stage in self.strict_policy["profiles"][profile]["required_stages"]:
            for cid in self.strict_policy["stages"][stage]["required_checks"]:
                if cid not in weekly.CI_IDS:
                    self.checks[cid] = evidence.make_report(
                        self.strict_policy, cid, HEAD, passed=True,
                        scope={"producer": "synthetic-test:strict-check", "run_id": RUN,
                               "run_attempt": ATTEMPT}, artifacts=["evidence/fixture/" + cid + ".json"])
        self.recompute()
        self.policy = self.approval(weekly.STRICT_DECISION)

    def test_template_is_unapproved_and_has_no_inherited_version_source_or_gaps(self):
        self.assertEqual(self.template["schema"], weekly.SCHEMA)
        for field in ("version", "product_version", "profile", "approved_product_source_sha",
                      "strict_policy_sha256", "ci_check_map_sha256"):
            self.assertIsNone(self.template[field], field)
        for field in ("release_approved", "publication_authorized"):
            self.assertIs(self.template[field], False, field)
        for field in ("approved", "publication_authorized", "validation_gaps_accepted"):
            self.assertIs(self.template["owner_acceptance"][field], False, field)
        self.assertEqual(self.template["decision"], weekly.STRICT_DECISION)
        self.assertEqual(self.template["known_validation_gaps"], [])
        self.policy = copy.deepcopy(self.template)
        self.rejected()

    def test_fresh_beta_approval_preserves_the_full_strict_blocked_result(self):
        original = copy.deepcopy((self.policy, self.strict_policy, self.checks, self.strict))
        decision = self.evaluate()
        self.assertTrue(decision["publication_allowed"])
        self.assertFalse(decision["all_required_checks_passed"])
        self.assertEqual(decision["version"], "1.3.1-beta.2")
        self.assertEqual(decision["strict_validation"]["verdict"], "BLOCKED")
        self.assertEqual(decision["strict_validation"]["counts"]["checks"],
                         {"total": 128, "pass": 21, "blocked": 107, "not_applicable": 0})
        self.assertEqual(decision["strict_validation"]["hard_gates"]["blocked"], 42)
        self.assertEqual({row["id"] for row in decision["known_validation_gaps"]}, weekly.GAP_IDS)
        self.assertTrue(all(row["executed"] is False for row in decision["known_validation_gaps"]))
        self.assertEqual(original, (self.policy, self.strict_policy, self.checks, self.strict))
        rendered = weekly.render_markdown(decision)
        self.assertIn("1.3.1-beta.2", rendered)
        self.assertIn("21 PASS / 107 BLOCKED", rendered)
        self.assertTrue(all("| " + cid + " |" in rendered for cid in weekly.GAP_IDS))

    def test_strict_is_the_default_and_does_not_accept_legacy_gaps(self):
        self.policy = self.approval(weekly.STRICT_DECISION)
        self.rejected()

    def test_copied_historical_beta_approval_cannot_authorize_weekly_publication(self):
        self.policy = gate.load_json(weekly.ROOT / "Validation/beta-publication-policy-1.3.1.json")
        self.policy["approved_product_source_sha"] = HEAD
        self.rejected()
        self.policy.update(schema=weekly.SCHEMA, version=self.version,
                           strict_policy_sha256=weekly.digest(self.strict_policy))
        self.rejected()

    def test_approval_is_bound_to_target_version_profile_and_source(self):
        for field, value in (("version", "1.3.1-beta.1"), ("version", "1.3.2-beta.2"),
                             ("product_version", "1.3.0"), ("profile", "pull_request"),
                             ("approved_product_source_sha", "b" * 40),
                             ("approved_product_source_sha", None)):
            with self.subTest(field=field, value=value):
                original = self.policy[field]
                self.policy[field] = value
                self.rejected()
                self.policy[field] = original

    def test_source_scope_must_match_even_when_all_approval_fields_match(self):
        for field, value in (("version", "1.3.1-beta.1"), ("product_version", "1.3.0"),
                             ("profile", "pull_request"), ("channel", "beta.1"),
                             ("preparation_authorized", False), ("preparation_authorized", "true")):
            with self.subTest(field=field):
                original = self.strict_policy["release_scope"][field]
                self.strict_policy["release_scope"][field] = value
                self.policy["strict_policy_sha256"] = weekly.digest(self.strict_policy)
                self.rejected()
                self.strict_policy["release_scope"][field] = original
                self.policy["strict_policy_sha256"] = weekly.digest(self.strict_policy)

    def test_new_manifest_is_the_approval_authority_not_old_release_scope_flags(self):
        self.assertFalse(self.strict_policy["release_scope"]["release_approved"])
        self.assertFalse(self.strict_policy["release_scope"]["publication_authorized"])
        approved = weekly.validate_approval(self.policy, self.strict_policy, HEAD,
                                            version=self.version, profile=self.profile)
        self.assertEqual(approved["decision"], weekly.GAP_DECISION)
        self.strict_policy["release_scope"].update(release_approved=True, publication_authorized=True)
        self.policy["strict_policy_sha256"] = weekly.digest(self.strict_policy)
        self.policy["release_approved"] = False
        self.rejected()

    def test_approval_flags_must_be_boolean_true_and_gap_acceptance_explicit(self):
        for container, fields in ((self.policy, ("release_approved", "publication_authorized")),
                                  (self.policy["owner_acceptance"], ("approved", "publication_authorized",
                                                                     "validation_gaps_accepted"))):
            for field in fields:
                for value in (False, "true", 1, None):
                    with self.subTest(field=field, value=value):
                        old = container[field]
                        container[field] = value
                        self.rejected()
                        container[field] = old

    def test_owner_instruction_context_and_scope_are_not_inferred(self):
        for field in ("instruction", "context", "scope"):
            for value in (None, "", "   ", True):
                with self.subTest(field=field, value=value):
                    old = self.policy["owner_acceptance"][field]
                    self.policy["owner_acceptance"][field] = value
                    self.rejected()
                    self.policy["owner_acceptance"][field] = old

    def test_previews_malformed_versions_and_nonfull_sha_are_rejected(self):
        for version in ("v1.3.1-beta.2", "1.3.1-preview.abcdef", "1.3.1-beta.0", "1.3.1-beta.02",
                        "01.3.1-beta.2", "1.3.1-beta.2+extra", "1.3.1-RC.1", "65536.0.0", None):
            with self.subTest(version=version):
                self.rejected(version=version)
        for source in (None, HEAD[:10], "A" * 40, "a" * 39, "g" * 40):
            with self.subTest(source=source), self.assertRaises(gate.ValidationError):
                weekly.validate_approval(self.policy, self.strict_policy, source,
                                         version=self.version, profile=self.profile)

    def test_approval_policy_and_map_fingerprints_cannot_be_stale_or_missing(self):
        for field in ("strict_policy_sha256", "ci_check_map_sha256"):
            for value in (None, "b" * 64, "A" * 64, "a" * 63):
                with self.subTest(field=field, value=value):
                    old = self.policy[field]
                    self.policy[field] = value
                    self.rejected()
                    self.policy[field] = old

    def test_refingerprinting_cannot_weaken_validation_gates_or_ci_ownership(self):
        original = copy.deepcopy(self.strict_policy)
        self.strict_policy["hard_gates"].remove("B14.RESULT")
        self.policy["strict_policy_sha256"] = weekly.digest(self.strict_policy)
        self.rejected()
        self.strict_policy = original
        self.policy["strict_policy_sha256"] = weekly.digest(self.strict_policy)
        self.mapping["checks"]["A12.01"]["platform"] = "windows"
        self.policy["ci_check_map_sha256"] = weekly.digest(self.mapping)
        self.rejected()

    def test_incomplete_or_reduced_ci_inventory_is_rejected(self):
        original = copy.deepcopy(self.policy["required_ci_checks"])
        for rows in (original[:-1], original + [original[0]]):
            self.policy["required_ci_checks"] = rows
            self.rejected()
        self.policy["required_ci_checks"] = original
        next(row for row in original if row["id"] == "A12.01")["platforms"] = ["windows"]
        self.rejected()

    def test_complete_gap_disclosure_and_hard_gate_membership_are_required(self):
        original = copy.deepcopy(self.policy["known_validation_gaps"])
        for rows in (original[:-1], original + [original[0]]):
            self.policy["known_validation_gaps"] = rows
            self.rejected()
        self.policy["known_validation_gaps"] = original
        next(row for row in original if row["id"] == "B14.RESULT")["strict_hard_gate"] = False
        self.rejected()

    def test_failure_in_every_ci_or_gap_check_is_rejected(self):
        for cid in sorted(weekly.CI_IDS | weekly.GAP_IDS):
            with self.subTest(cid=cid):
                old, verdict = copy.deepcopy(self.checks[cid]), copy.deepcopy(self.strict)
                self.checks[cid].update(execution={"executed": True, "exit_code": 1},
                                        assertion={"passed": False},
                                        failure={"type": "TEST_FAILURE", "message": "Synthetic observed failure"})
                self.recompute()
                self.rejected()
                self.checks[cid], self.strict = old, verdict

    def test_observed_failure_cannot_be_hidden_as_unexecuted_gap(self):
        self.checks["B12.RESULT"]["failure"] = {"type": "REGRESSION", "message": "Observed DAW crash"}
        self.rejected()

    def test_gap_cannot_be_promoted_to_pass(self):
        self.checks["B8.RESULT"].update(execution={"executed": True, "exit_code": 0},
                                        assertion={"passed": True}, failure=None)
        self.recompute()
        self.rejected()

    def test_raw_platform_ctest_failure_still_blocks_fresh_beta_approval(self):
        self.fixture.raw_change("linux", "ctest", lambda value: value.replace(
            'name="ChimeraTests" status="run"/>',
            'name="ChimeraTests" status="run"><failure/></testcase>'))
        with patch.object(evidence, "configuration", return_value=(self.strict_policy, self.mapping["checks"])), \
             patch.object(evidence, "source_profile", return_value=weekly.BETA_PROFILE):
            self.strict, output = self.fixture.run_gate()
        self.checks = gate.read_checks(output / "checks")
        self.rejected()

    def test_verdict_and_ci_check_source_run_attempt_are_all_bound(self):
        for field, value in (("commit_sha", "b" * 40), ("run_id", "999"), ("run_attempt", "999")):
            with self.subTest(field=field):
                old = self.strict["revision"][field]
                self.strict["revision"][field] = value
                self.rejected()
                self.strict["revision"][field] = old
        for field, value in (("run_id", "999"), ("run_attempt", "999"), ("platforms", ["windows"])):
            with self.subTest(ci_field=field):
                old = self.checks["A12.01"]["scope"][field]
                self.checks["A12.01"]["scope"][field] = value
                self.rejected()
                self.checks["A12.01"]["scope"][field] = old
        self.checks["B8.RESULT"]["evidence"]["commit_sha"] = "b" * 40
        self.rejected()

    def test_nonpositive_boolean_and_nonnumeric_run_identities_are_rejected(self):
        for value in (0, -1, True, False, None, "0", "01", "unknown", 1.5):
            for field in ("run_id", "run_attempt"):
                with self.subTest(field=field, value=value), self.assertRaises(gate.ValidationError):
                    weekly.evaluate(self.policy, self.strict_policy, self.checks, self.strict, HEAD,
                                    value if field == "run_id" else RUN,
                                    value if field == "run_attempt" else ATTEMPT,
                                    version=self.version, profile=self.profile)

    def test_platform_producer_missing_duplicate_stale_or_failed_is_rejected(self):
        original = copy.deepcopy(self.strict["producer"])
        changes = [lambda p: p["inputs"].pop(),
                   lambda p: p["inputs"].__setitem__(0, copy.deepcopy(p["inputs"][1])),
                   lambda p: p["inputs"][0]["revision"].update(run_attempt="999"),
                   lambda p: p["inputs"][0].update(manifest_sha256=""),
                   lambda p: p["errors"].append({"kind": "windows", "reason": "Missing receipt"})]
        for change in changes:
            self.strict["producer"] = copy.deepcopy(original)
            change(self.strict["producer"])
            self.rejected()

    def test_missing_check_stale_policy_disabled_source_binding_and_na_are_rejected(self):
        original = copy.deepcopy(self.checks)
        changes = [lambda: self.checks.pop("B8.RESULT"),
                   lambda: self.checks["B8.RESULT"]["evidence"].update(policy_version=999),
                   lambda: self.checks["B8.RESULT"]["policy"].update(requires_current_commit=False),
                   lambda: self.checks["B8.RESULT"].update(applicability={"applicable": False, "waiver_id": "new"})]
        for change in changes:
            self.checks = copy.deepcopy(original)
            change()
            self.rejected()
        self.checks = original
        self.waivers["waivers"].append({"id": "new", "check_id": "B8.RESULT", "approved": True})
        self.rejected()

    def test_summary_only_pass_is_rejected(self):
        self.strict.update(verdict="PASS", ready=True)
        self.strict["counts"]["checks"] = {"total": 128, "pass": 128, "blocked": 0, "not_applicable": 0}
        self.rejected()

    def test_boolean_exit_code_or_observed_failure_cannot_be_success(self):
        self.checks["A12.01"]["execution"]["exit_code"] = False
        self.rejected()
        self.checks["A12.01"]["execution"]["exit_code"] = 0
        self.checks["A12.01"]["failure"] = {"type": "REGRESSION", "message": "Observed failure despite zero exit"}
        self.rejected()

    def test_beta_can_publish_with_a_complete_strict_pass(self):
        self.strict_pass_fixture("1.3.2-beta.10", weekly.BETA_PROFILE)
        result = self.evaluate()
        self.assertTrue(result["all_required_checks_passed"])
        self.assertEqual(result["strict_validation"]["counts"]["checks"]["pass"], 128)
        self.assertEqual(result["known_validation_gaps"], [])
        self.assertIn("128 PASS / 0 BLOCKED", weekly.render_markdown(result))

    def test_rc_and_stable_require_full_strict_pass_including_transpose(self):
        for version in ("1.3.2-rc.1", "1.3.2"):
            with self.subTest(version=version):
                self.strict_pass_fixture(version, weekly.FULL_PROFILE)
                result = self.evaluate()
                self.assertTrue(result["all_required_checks_passed"])
                self.assertEqual(result["strict_validation"]["counts"]["checks"]["pass"], 130)
                self.checks["I2.PITCH_DI"]["execution"] = {"executed": False, "exit_code": None}
                self.recompute()
                self.rejected()

    def test_rc_and_stable_never_use_beta_profile_or_gap_exception(self):
        for version in ("1.3.2-rc.1", "1.3.2"):
            for profile in (weekly.BETA_PROFILE, weekly.FULL_PROFILE):
                with self.subTest(version=version, profile=profile):
                    self.version, self.profile = version, profile
                    self.set_scope()
                    self.policy = self.approval(weekly.GAP_DECISION)
                    self.rejected()

    def test_strict_pass_requires_a_current_producer_and_evidence_for_former_gaps(self):
        self.strict_pass_fixture("1.3.2-beta.2", weekly.BETA_PROFILE)
        original = copy.deepcopy(self.checks["B8.RESULT"])
        changes = [lambda c: c["scope"].update(producer="unclaimed-baseline"),
                   lambda c: c["scope"].update(run_id="999"),
                   lambda c: c["scope"].update(run_attempt="999"),
                   lambda c: c["evidence"].update(artifacts=[]),
                   lambda c: c["evidence"].update(artifacts=["../other-run.json"])]
        for change in changes:
            self.checks["B8.RESULT"] = copy.deepcopy(original)
            change(self.checks["B8.RESULT"])
            self.rejected()

    def test_unknown_or_boolean_schema_and_policy_versions_are_rejected(self):
        for field, value in (("schema", "spectralforge.chimera.beta-publication-policy"),
                             ("schema_version", True), ("policy_version", True),
                             ("decision", "AUTOMATIC_APPROVAL")):
            with self.subTest(field=field):
                old = self.policy[field]
                self.policy[field] = value
                self.rejected()
                self.policy[field] = old


if __name__ == "__main__":
    unittest.main()
