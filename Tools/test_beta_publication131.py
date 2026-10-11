#!/usr/bin/env python3
"""Synthetic decision contracts, never real product acceptance evidence."""
import copy
import json
from pathlib import Path
import unittest

import beta_publication131 as beta
import evaluate_release_gate as gate
import publish_beta131 as publisher
import test_release_consolidator as fixtures

HEAD, RUN, ATTEMPT = fixtures.HEAD, fixtures.RUN, fixtures.ATTEMPT


class BetaPublication(unittest.TestCase):
    def setUp(self):
        # Reuse the raw Windows/Linux/macOS/assembly fixture producer, rather
        # than constructing a summary-only PASS or bypassing strict evaluation.
        self.fixture = fixtures.Consolidation("test_actual_automatic_receipts_pass_only_owned_checks")
        self.fixture.setUp()
        self.addCleanup(self.fixture.doCleanups)
        self.strict, output = self.fixture.run_gate()
        self.output = output
        self.checks = gate.read_checks(output / "checks")
        self.policy = gate.load_json(beta.ROOT / "Validation/beta-publication-policy-1.3.1.json")
        self.policy["approved_product_source_sha"] = HEAD  # Synthetic fixture identity only.
        self.strict_policy = copy.deepcopy(self.fixture.policy)
        self.waivers = gate.load_json(beta.ROOT / "Validation/waivers.json")
        self.mapping = gate.load_json(beta.ROOT / "Validation/ci-check-map.json")

    def evaluate(self):
        return beta.evaluate(self.policy, self.strict_policy, self.checks, self.strict,
                             HEAD, RUN, ATTEMPT, waivers=self.waivers, ci_check_map=self.mapping)

    def recompute(self):
        current = gate.evaluate_release(self.strict_policy, self.waivers, self.checks, beta.PROFILE, HEAD)
        self.strict.update(current)
        self.strict["revision"].update(run_id=RUN, run_attempt=ATTEMPT)

    def rejected(self):
        with self.assertRaises(gate.ValidationError):
            self.evaluate()

    def test_beta_permission_keeps_complete_strict_blocked_verdict(self):
        original = copy.deepcopy((self.policy, self.strict_policy, self.checks, self.strict))
        decision = self.evaluate()
        self.assertEqual(decision["decision"], beta.DECISION)
        self.assertTrue(decision["publication_allowed"])
        self.assertFalse(decision["all_required_checks_passed"])
        self.assertEqual(decision["strict_validation"]["verdict"], "BLOCKED")
        self.assertFalse(decision["strict_validation"]["ready"])
        self.assertEqual(decision["strict_validation"]["counts"]["checks"],
                         {"total": 128, "pass": 21, "blocked": 107, "not_applicable": 0})
        self.assertEqual(decision["strict_validation"]["hard_gates"]["blocked"], 42)
        rows = decision["known_validation_gaps"]
        self.assertEqual({row["id"] for row in rows}, beta.GAP_IDS)
        self.assertTrue(all(not row["executed"] and row["computed_status"] == "BLOCKED" for row in rows))
        markdown = beta.render_markdown(decision)
        self.assertTrue(all("| " + cid + " |" in markdown for cid in beta.GAP_IDS))
        self.assertEqual(original, (self.policy, self.strict_policy, self.checks, self.strict))

    def test_publisher_archive_integration_keeps_strict_default(self):
        archive = self.fixture.archive(self.output)
        original_bytes = archive.read_bytes()
        with self.assertRaisesRegex(RuntimeError, "not PASS"):
            publisher.verify_release_gate_archive(archive, HEAD, beta.PROFILE, RUN, ATTEMPT)
        strict, decision = publisher.verify_release_gate_archive(
            archive, HEAD, beta.PROFILE, RUN, ATTEMPT, publication_policy=self.policy)
        self.assertEqual(strict["verdict"], "BLOCKED")
        self.assertEqual(strict["counts"]["checks"]["blocked"], 107)
        self.assertEqual(decision["decision"], beta.DECISION)
        self.assertTrue(decision["publication_allowed"])
        self.assertEqual(original_bytes, archive.read_bytes())

    def test_publisher_beta_path_rejects_real_fixture_ctest_failure(self):
        self.fixture.raw_change("linux", "ctest", lambda value: value.replace(
            'name="ChimeraTests" status="run"/>',
            'name="ChimeraTests" status="run"><failure/></testcase>'))
        _, output = self.fixture.run_gate()
        with self.assertRaisesRegex(RuntimeError, "Automatic check failed"):
            publisher.verify_release_gate_archive(self.fixture.archive(output), HEAD, beta.PROFILE,
                                                 RUN, ATTEMPT, publication_policy=self.policy)

    def test_every_required_ci_check_is_mandatory(self):
        for cid in beta.CI_IDS:
            with self.subTest(cid=cid):
                old = self.checks.pop(cid)
                self.rejected()
                self.checks[cid] = old

    def test_failure_in_any_check_is_rejected_even_with_matching_summary(self):
        for cid in sorted(beta.CI_IDS | beta.GAP_IDS):
            with self.subTest(cid=cid):
                old = copy.deepcopy(self.checks[cid])
                old_summary = copy.deepcopy(self.strict)
                self.checks[cid].update(execution={"executed": True, "exit_code": 1},
                                        assertion={"passed": False},
                                        failure={"type": "TEST_FAILURE", "message": "Synthetic failure"})
                self.recompute()
                self.rejected()
                self.checks[cid], self.strict = old, old_summary

    def test_unowned_pass_cannot_replace_known_gap(self):
        check = self.checks["B8.RESULT"]
        check.update(execution={"executed": True, "exit_code": 0}, assertion={"passed": True}, failure=None)
        self.recompute()
        self.rejected()

    def test_unexecuted_failure_is_not_accepted_as_known_gap(self):
        self.checks["B12.RESULT"]["failure"] = {"type": "REGRESSION", "message": "Observed crash"}
        self.rejected()

    def test_unknown_check_cannot_replace_expected_gap(self):
        self.checks["A2.99"] = self.checks.pop("A2.01")
        self.checks["A2.99"]["id"] = "A2.99"
        self.recompute()
        self.rejected()

    def test_duplicate_or_missing_gap_declaration_rejected(self):
        old = copy.deepcopy(self.policy["known_validation_gaps"])
        for rows in (old[:-1], old + [old[0]]):
            self.policy["known_validation_gaps"] = rows
            self.rejected()

    def test_source_run_and_attempt_binding(self):
        changes = [("commit_sha", "b" * 40), ("run_id", "999"), ("run_attempt", "999")]
        for key, value in changes:
            with self.subTest(key=key):
                old = self.strict["revision"][key]
                self.strict["revision"][key] = value
                self.rejected()
                self.strict["revision"][key] = old
        self.checks["A12.01"]["scope"]["run_id"] = "999"
        self.rejected()

    def test_stale_individual_check_source_rejected(self):
        self.checks["A1.01"]["evidence"]["commit_sha"] = "b" * 40
        self.rejected()

    def test_report_source_binding_cannot_be_disabled(self):
        self.checks["A1.01"]["policy"]["requires_current_commit"] = False
        self.rejected()

    def test_different_product_source_needs_explicit_policy_revision(self):
        self.policy["approved_product_source_sha"] = "b" * 40
        self.rejected()

    def test_specific_owner_decision_is_required(self):
        original = copy.deepcopy(self.policy["owner_acceptance"])
        for field, value in [("approved", False), ("approved", "true"),
                             ("publication_authorized", False), ("instruction", ""),
                             ("testing_reported", True),
                             ("individual_check_claims", ["B8.RESULT"])]:
            with self.subTest(field=field, value=value):
                self.policy["owner_acceptance"] = copy.deepcopy(original)
                self.policy["owner_acceptance"][field] = value
                self.rejected()

    def test_strict_policy_fingerprint_cannot_be_replaced_in_new_policy(self):
        self.strict_policy["hard_gates"].remove("B14.RESULT")
        self.policy["strict_policy_sha256"] = beta.digest(self.strict_policy)
        self.rejected()

    def test_original_ci_map_fingerprint_cannot_be_replaced(self):
        self.mapping["checks"]["A12.01"]["platform"] = "windows"
        self.policy["ci_check_map_sha256"] = beta.digest(self.mapping)
        self.rejected()

    def test_ci_platform_coverage_cannot_be_reduced(self):
        next(row for row in self.policy["required_ci_checks"] if row["id"] == "A12.01")["platforms"] = ["windows"]
        self.rejected()

    def test_ci_check_boolean_exit_code_is_not_zero_success(self):
        self.checks["A12.01"]["execution"]["exit_code"] = False
        self.rejected()

    def test_summary_only_pass_is_rejected(self):
        self.strict.update(verdict="PASS", ready=True)
        self.strict["counts"]["checks"] = {"total": 128, "pass": 128, "blocked": 0, "not_applicable": 0}
        self.rejected()

    def test_missing_duplicate_stale_or_failed_producer_rejected(self):
        original = copy.deepcopy(self.strict["producer"])
        mutations = [lambda p: p["inputs"].pop(),
                     lambda p: p["inputs"].__setitem__(0, copy.deepcopy(p["inputs"][1])),
                     lambda p: p["inputs"][0]["revision"].update(run_id="999"),
                     lambda p: p["errors"].append({"kind": "windows", "reason": "Missing receipt"})]
        for change in mutations:
            self.strict["producer"] = copy.deepcopy(original)
            change(self.strict["producer"])
            self.rejected()

    def test_gap_hard_gate_membership_cannot_be_hidden(self):
        next(row for row in self.policy["known_validation_gaps"] if row["id"] == "B14.RESULT")["strict_hard_gate"] = False
        self.rejected()

    def test_new_waiver_and_na_report_rejected(self):
        self.waivers["waivers"].append({"id": "new", "check_id": "B8.RESULT", "approved": True})
        self.rejected()
        self.waivers["waivers"] = []
        self.checks["B8.RESULT"]["applicability"] = {"applicable": False, "waiver_id": "new"}
        self.rejected()

    def test_defaults_read_utf8_policy_dependencies(self):
        decision = beta.evaluate(self.policy, self.strict_policy, self.checks, self.strict, HEAD, RUN, ATTEMPT)
        self.assertEqual(decision["ci_check_map_sha256"], beta.CI_CHECK_MAP_SHA256)


if __name__ == "__main__":
    unittest.main()
