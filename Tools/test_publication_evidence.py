#!/usr/bin/env python3
"""Raw archive integrity regressions; all fixtures are synthetic test evidence."""
import copy
import hashlib
import json
from pathlib import Path
import stat
from types import SimpleNamespace
import unittest
from unittest.mock import Mock, patch
import warnings
import zipfile

import consolidate_release_gate as consolidator
import evaluate_release_gate as gate
import release_evidence as evidence
import test_release_consolidator as fixtures
import verify_publication_evidence as verification

HEAD, RUN, ATTEMPT = fixtures.HEAD, fixtures.RUN, fixtures.ATTEMPT


def make_raw_archive(test_case, *, version="1.3.1-beta.2", profile="beta_1_3",
                     source_sha=HEAD, run_id=RUN, run_attempt=ATTEMPT):
    """Build a reusable raw-platform archive fixture, never product acceptance.

    Returns fixture, archive, output, verdict, checks, policy, ci_check_map and
    source/run identities. The caller owns it through test_case.addCleanup.
    Tests needing additional strict reports can edit output/checks and regenerate
    the verdict; the actual embedded 21-check raw receipts remain available.
    """
    policy = gate.load_json(evidence.ROOT / "Validation/release-policy.json")
    ci_check_map = gate.load_json(evidence.ROOT / "Validation/ci-check-map.json")
    pieces = version.split("-", 1)
    policy["release_scope"] = {
        "version": version, "product_version": pieces[0], "profile": profile,
        "channel": pieces[1] if len(pieces) == 2 else "stable",
        "preparation_authorized": True, "publication_authorized": False, "release_approved": False,
        "deferred_checks": ["I2.PITCH_LIVE", "I2.PITCH_DI"] if profile == "beta_1_3" else [],
    }
    fixture = fixtures.Consolidation("test_actual_automatic_receipts_pass_only_owned_checks")
    test_case.addCleanup(fixture.doCleanups)
    with patch.object(fixtures, "HEAD", source_sha), patch.object(fixtures, "RUN", str(run_id)), \
         patch.object(fixtures, "ATTEMPT", str(run_attempt)), \
         patch.object(evidence, "configuration", return_value=(policy, ci_check_map["checks"])):
        fixture.setUp()
    for kind in evidence.KINDS:
        fixture.mutate(kind, lambda doc: doc.update(profile=profile))
    output = fixture.root / "publication-output"
    with patch.object(evidence, "configuration", return_value=(policy, ci_check_map["checks"])), \
         patch.object(evidence, "source_profile", return_value=profile):
        verdict = consolidator.consolidate(fixture.inputs, output, fixture.revision, profile)
    return SimpleNamespace(fixture=fixture, archive=fixture.archive(output), output=output,
                           verdict=verdict, checks=gate.read_checks(output / "checks"), policy=policy,
                           ci_check_map=ci_check_map, source_sha=source_sha,
                           run_id=run_id, run_attempt=run_attempt, profile=profile)


class EmbeddedEvidence(unittest.TestCase):
    def setUp(self):
        self.raw = make_raw_archive(self)
        with zipfile.ZipFile(self.raw.archive) as bundle:
            self.files = {entry.filename: bundle.read(entry) for entry in bundle.infolist()}
        self.extra = []
        self.modes = {}
        self.original = copy.deepcopy(self.files)

    def write_archive(self):
        path = self.raw.fixture.root / "mutated.zip"
        with warnings.catch_warnings(), zipfile.ZipFile(path, "w") as bundle:
            warnings.simplefilter("ignore", UserWarning)
            for name, data in self.files.items():
                entry = zipfile.ZipInfo(name)
                if name in self.modes:
                    entry.external_attr = self.modes[name] << 16
                bundle.writestr(entry, data)
            for name, data in self.extra:
                bundle.writestr(name, data)
        return path

    def verify(self, **overrides):
        args = {"source_sha": self.raw.source_sha, "run_id": self.raw.run_id,
                "run_attempt": self.raw.run_attempt, "profile": self.raw.profile,
                "ci_check_map": self.raw.ci_check_map}
        args.update(overrides)
        with zipfile.ZipFile(self.write_archive()) as bundle:
            return verification.verify_embedded_evidence(bundle, self.raw.verdict, self.raw.policy,
                                                          self.raw.checks, **args)

    def rejected(self, **overrides):
        with self.assertRaises(gate.ValidationError):
            self.verify(**overrides)

    def store_verdict(self):
        self.files["validation/release-gate.json"] = json.dumps(self.raw.verdict).encode()

    def manifest_bytes(self, kind, data, *, rehash=True):
        self.files[f"validation/evidence/{kind}/evidence.json"] = data
        if rehash:
            row = next(item for item in self.raw.verdict["producer"]["inputs"] if item["kind"] == kind)
            row["manifest_sha256"] = hashlib.sha256(data).hexdigest()
            self.store_verdict()

    def mutate_manifest(self, kind, change, *, rehash=True):
        doc = json.loads(self.files[f"validation/evidence/{kind}/evidence.json"])
        change(doc)
        self.manifest_bytes(kind, json.dumps(doc).encode(), rehash=rehash)

    def mutate_raw(self, kind, key, change, *, rehash=False):
        doc = json.loads(self.files[f"validation/evidence/{kind}/evidence.json"])
        path = f"validation/evidence/{kind}/" + doc["files"][key]["path"]
        data = change(self.files[path])
        self.files[path] = data
        if rehash:
            self.mutate_manifest(kind, lambda value: value["files"][key].update(
                sha256=hashlib.sha256(data).hexdigest()))

    def test_complete_raw_receipts_reproduce_all_21_ci_checks_without_mutation(self):
        original = copy.deepcopy((self.raw.verdict, self.raw.policy, self.raw.checks, self.raw.ci_check_map))
        self.assertIsNone(self.verify())
        self.assertEqual(self.raw.verdict["counts"]["checks"]["pass"], 21)
        self.assertFalse(self.raw.verdict["ready"])
        self.assertEqual(original, (self.raw.verdict, self.raw.policy, self.raw.checks, self.raw.ci_check_map))
        self.assertEqual(self.files, self.original)

    def test_deleted_or_changed_native_evidence_cannot_be_replaced_by_pass_json(self):
        for kind in evidence.KINDS:
            with self.subTest(kind=kind):
                name = f"validation/evidence/{kind}/evidence.json"
                original = self.files.pop(name)
                self.rejected()
                self.files[name] = original + b" "
                self.rejected()
                self.files[name] = original

    def test_manifest_digest_must_match_the_embedded_bytes(self):
        self.raw.verdict["producer"]["inputs"][0]["manifest_sha256"] = "b" * 64
        self.store_verdict()
        self.rejected()

    def test_raw_bytes_are_checked_even_when_outer_archive_is_valid(self):
        self.mutate_raw("linux", "dsp_log", lambda data: data + b"\nchanged\n")
        self.rejected()

    def test_rehashed_raw_test_failure_cannot_keep_a_stale_pass_check(self):
        self.mutate_raw("linux", "ctest", lambda data: data.replace(
            b'name="ChimeraTests" status="run"/>',
            b'name="ChimeraTests" status="run"><failure/></testcase>'), rehash=True)
        with self.assertRaisesRegex(gate.ValidationError, "not supported by embedded raw evidence"):
            self.verify()

    def test_rehashed_skipped_test_cannot_keep_a_stale_pass_check(self):
        self.mutate_raw("macos", "ctest", lambda data: data.replace(
            b'name="ChimeraTests" status="run"/>',
            b'name="ChimeraTests" status="run"><skipped/></testcase>'), rehash=True)
        self.rejected()

    def test_rehashed_malformed_raw_xml_is_a_validation_failure(self):
        self.mutate_raw("linux", "ctest", lambda _: b"<testsuite><unclosed", rehash=True)
        with self.assertRaisesRegex(gate.ValidationError, "Malformed embedded raw evidence"):
            self.verify()

    def test_rehashed_package_or_defender_failure_blocks_publication(self):
        self.mutate_raw("linux", "linux_package", lambda data: data.replace(b"PASS:", b"FAIL:"), rehash=True)
        self.rejected()
        self.files = copy.deepcopy(self.original)
        self.raw.verdict = json.loads(self.files["validation/release-gate.json"])
        self.mutate_raw("windows", "scan_status", lambda data: json.dumps(
            {**json.loads(data), "AMServiceEnabled": False}).encode(), rehash=True)
        self.rejected()

    def test_rehashed_assembly_source_version_or_attempt_mismatch_is_rejected(self):
        for field, value in (("revision", "b" * 40), ("runId", "999"), ("runAttempt", "999"),
                             ("version", "1.3.1-beta.1")):
            with self.subTest(field=field):
                self.files = copy.deepcopy(self.original)
                self.raw.verdict = json.loads(self.files["validation/release-gate.json"])
                self.mutate_raw("assembly", "candidate", lambda data: json.dumps(
                    {**json.loads(data), field: value}).encode(), rehash=True)
                self.rejected()

    def test_rehashed_native_manifest_cannot_claim_a_different_run_or_profile(self):
        for field, value in (("profile", "full_release"), ("kind", "macos"), ("policy_version", 999)):
            with self.subTest(field=field):
                self.files = copy.deepcopy(self.original)
                self.raw.verdict = json.loads(self.files["validation/release-gate.json"])
                self.mutate_manifest("windows", lambda doc: doc.update({field: value}))
                self.rejected()
        self.files = copy.deepcopy(self.original)
        self.raw.verdict = json.loads(self.files["validation/release-gate.json"])
        self.mutate_manifest("windows", lambda doc: doc["revision"].update(run_attempt="999"))
        self.rejected()

    def test_new_ci_map_fingerprint_cannot_reassign_a_platform(self):
        self.raw.ci_check_map["checks"]["A12.01"]["platform"] = "windows"
        self.rejected()

    def test_aggregated_ci_check_must_equal_the_raw_regenerated_check(self):
        self.raw.checks["A12.01"]["scope"]["platforms"] = ["windows"]
        self.rejected()

    def test_claimed_pass_evidence_must_be_an_actual_embedded_file(self):
        self.raw.checks["B8.RESULT"]["execution"] = {"executed": True, "exit_code": 0}
        self.raw.checks["B8.RESULT"]["evidence"]["artifacts"] = ["evidence/manual/missing.json"]
        self.rejected()

    def test_empty_raw_evidence_is_not_a_valid_rehashed_receipt(self):
        self.mutate_raw("windows", "installer_log", lambda _: b"", rehash=True)
        self.rejected()

    def test_extra_unlisted_raw_file_is_rejected(self):
        self.files["validation/evidence/windows/raw/unlisted.txt"] = b"unexpected"
        self.rejected()

    def test_raw_manifest_cannot_escape_its_platform_or_alias_a_file(self):
        for value in ("../linux/raw/ctest-results.xml", "/outside", "raw\\ctest-results.xml"):
            with self.subTest(path=value):
                self.files = copy.deepcopy(self.original)
                self.raw.verdict = json.loads(self.files["validation/release-gate.json"])
                self.mutate_manifest("windows", lambda doc: doc["files"]["ctest"].update(path=value))
                self.rejected()

    def test_archive_paths_symlinks_and_duplicate_entries_are_rejected(self):
        for name in ("../escape", "/absolute", "validation/./evidence/x", "validation//evidence/x", "C:/drive"):
            with self.subTest(name=name):
                self.extra = [(name, b"invalid")]
                self.rejected()
        self.extra = [("validation/release-gate.json", self.files["validation/release-gate.json"])]
        self.rejected()
        self.extra = []
        self.modes["validation/evidence/windows/evidence.json"] = stat.S_IFLNK | 0o777
        self.rejected()

    def test_duplicate_json_keys_are_rejected_even_after_manifest_rehash(self):
        name = "validation/evidence/windows/evidence.json"
        value = self.files[name].decode().replace('"kind": "windows"', '"kind": "linux", "kind": "windows"')
        self.manifest_bytes("windows", value.encode())
        self.rejected()

    def test_archive_bounds_are_checked_before_any_bytes_are_read(self):
        oversized = zipfile.ZipInfo("validation/release-gate.json")
        oversized.file_size = verification.MAX_ARCHIVE_BYTES + 1
        bundle = Mock()
        bundle.infolist.return_value = [oversized]
        with self.assertRaises(gate.ValidationError):
            verification.verify_embedded_evidence(bundle, self.raw.verdict, self.raw.policy,
                self.raw.checks, HEAD, RUN, ATTEMPT, profile=self.raw.profile)
        bundle.read.assert_not_called()

    def test_rc_and_stable_raw_ci_do_not_claim_unexecuted_strict_checks_passed(self):
        for version in ("1.3.2-rc.1", "1.3.2"):
            with self.subTest(version=version):
                raw = make_raw_archive(self, version=version, profile="full_release")
                with zipfile.ZipFile(raw.archive) as bundle:
                    verification.verify_embedded_evidence(bundle, raw.verdict, raw.policy, raw.checks,
                        raw.source_sha, raw.run_id, raw.run_attempt, profile=raw.profile)
                self.assertEqual(raw.verdict["counts"]["checks"],
                                 {"total": 130, "pass": 21, "blocked": 109, "not_applicable": 0})
                self.assertFalse(raw.verdict["ready"])


if __name__ == "__main__":
    unittest.main()
