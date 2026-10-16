#!/usr/bin/env python3
"""Negative contracts for diagnostic ledgers; no new product acceptance claims."""
import copy
import unittest
import audit_acceptance_definitions as audit

class DefinitionEvidence(unittest.TestCase):
    def setUp(self): self.data=copy.deepcopy(audit.load_ledger())
    def rejected(self,method=audit.validate_registry):
        with self.assertRaises(audit.triage.gate.ValidationError): method(self.data)
    def test_complete_ledger_replays_original_gate(self):
        result=audit.audit(self.data)
        self.assertEqual(result['strict_counts'],{'total':128,'pass':21,'blocked':107,'not_applicable':0})
        self.assertTrue(all(result['known_gaps'].values()))
    def test_missing_id(self):
        self.data['rows'].pop();self.rejected()
    def test_duplicate_id(self):
        self.data['rows'][-1]=self.data['rows'][0];self.rejected()
    def test_forged_original_definition(self):
        self.data['rows'][0]['original_definition']='guessed';self.rejected()
    def test_pass_or_na_is_not_a_proposal(self):
        for status in ('PASS','NOT_APPLICABLE'):
            with self.subTest(status=status):
                self.data['rows'][0]['gate_status']=status;self.rejected()
    def test_unapproved_scope_cannot_be_approved(self):
        self.data['rows'][0]['decision']='APPROVED';self.rejected()
    def test_hard_gate_cannot_be_removed(self):
        row=next(r for r in self.data['rows'] if r['strict_hard_gate'])
        row['strict_hard_gate']=False;self.rejected()
    def test_unknown_evidence(self):
        self.data['rows'][0]['evidence_ids']=['unavailable'];self.rejected()
    def test_policy_fingerprint(self):
        self.data['protected_files']['Validation/release-policy.json']='0'*64
        self.rejected(audit.verify_sources_and_policy)
    def test_raw_archive_digest(self):
        self.data['evidence']['E.upgrade']['archive_sha256']='0'*64
        self.rejected(audit.verify_reused_evidence)
    def test_cross_source_transfer(self):
        self.data['evidence']['E.pr41-calculator']['product_source_sha']=audit.PRODUCT
        self.rejected(audit.verify_reused_evidence)
    def test_ctest_identity_and_scope(self):
        record=self.data['evidence']['E.ChimeraTests']['records'][0]
        record['test']='ChimeraNonexistentTests';self.rejected(audit.verify_reused_evidence)
    def test_platform_coverage_cannot_be_silently_reduced(self):
        self.data['evidence']['E.ChimeraTests']['records'].pop()
        self.rejected(audit.verify_reused_evidence)
    def test_excerpts_must_belong_to_the_named_case(self):
        self.data['evidence']['E.ChimeraTests']['records'][0]['assertion_excerpts']=['PASS invented condition']
        self.rejected(audit.verify_reused_evidence)
    def test_receipt_must_match_raw_bytes(self):
        self.data['evidence']['E.upgrade']['receipt']['daw_project_acceptance']=True
        self.rejected(audit.verify_reused_evidence)
    def test_historical_workflow_failure_not_hidden(self):
        self.data['evidence']['E.pr41-calculator']['whole_workflow_conclusion']='success'
        self.rejected(audit.verify_reused_evidence)

if __name__=='__main__': unittest.main(verbosity=2)
