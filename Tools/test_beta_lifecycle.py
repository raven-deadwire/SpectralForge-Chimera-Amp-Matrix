#!/usr/bin/env python3
from __future__ import annotations

from datetime import datetime, timezone
import json
from pathlib import Path
import tempfile
from copy import deepcopy
from unittest.mock import patch
import unittest

import beta_lifecycle

SHA = "0123456789abcdef0123456789abcdef01234567"


class BetaLifecycleTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="chimera-beta-lifecycle-")
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        (self.root / "VERSION").write_text("1.3.1\n", encoding="utf-8")
        (self.root / "RELEASE_CHANNEL").write_text("beta.2\n", encoding="utf-8")
        self.now = datetime(2026, 10, 13, 6, 0, tzinfo=timezone.utc)

    def test_metadata_binds_explicit_expiry_to_existing_identity(self):
        result = beta_lifecycle.build_metadata(
            self.root, SHA, "2027-01-31T23:59:59Z", self.now)
        self.assertEqual(result["version"], "1.3.1-beta.2")
        self.assertEqual(result["product_version"], "1.3.1")
        self.assertEqual(result["source_sha"], SHA)
        self.assertEqual(result["build_id"], SHA[:10])
        self.assertEqual(result["expires_at_unix"], 1801439999)
        self.assertFalse(result["network_required"])
        self.assertTrue(result["active_instance_grace"])

    def test_malformed_non_utc_or_elapsed_expiry_is_rejected(self):
        for value in ("", "2027-01-31", "2027-01-31T23:59:59+00:00",
                      "2027-01-31T23:59:59.000Z", "2027-02-30T00:00:00Z"):
            with self.subTest(value=value), self.assertRaises(ValueError):
                beta_lifecycle.build_metadata(self.root, SHA, value, self.now)
        for value in ("2026-10-13T06:00:00Z", "2026-10-13T05:59:59Z"):
            with self.subTest(value=value), self.assertRaisesRegex(ValueError, "later"):
                beta_lifecycle.build_metadata(self.root, SHA, value, self.now)

    def test_wrong_source_or_non_beta_channel_is_rejected(self):
        for source in (SHA[:10], SHA.upper(), "g" * 40):
            with self.subTest(source=source), self.assertRaises(ValueError):
                beta_lifecycle.build_metadata(
                    self.root, source, "2027-01-31T23:59:59Z", self.now)
        for channel in ("preview", "rc.1", "stable"):
            (self.root / "RELEASE_CHANNEL").write_text(channel + "\n", encoding="utf-8")
            with self.subTest(channel=channel), self.assertRaisesRegex(ValueError, "beta source"):
                beta_lifecycle.build_metadata(
                    self.root, SHA, "2027-01-31T23:59:59Z", self.now)

    def policy(self, **values):
        policy = {"schema": "spectralforge.chimera.beta-policy.v1", "version": "1.3.1-beta.2",
                  "expires_at_utc": None, "expiry_approved": False,
                  "warning_lead_seconds": 1209600, "new_expired_instance": "muted-output-state-preserved",
                  "active_instance_grace": True, **values}
        path = self.root / beta_lifecycle.POLICY_PATH
        path.parent.mkdir(exist_ok=True)
        path.write_text(json.dumps(policy), encoding="utf-8")

    def test_pending_deadline_is_unset_and_packaging_is_blocked(self):
        self.policy()
        value = beta_lifecycle.configure_metadata(self.root, SHA, now=self.now)
        self.assertEqual(value["mode"], "pending-approval")
        self.assertEqual(value["expires_at_unix"], 0)
        with self.assertRaisesRegex(ValueError, "unset"):
            beta_lifecycle.validate_metadata(value, self.root, SHA, now=self.now)

    def test_qualification_can_be_staged_but_never_published(self):
        self.policy()
        value = beta_lifecycle.configure_metadata(self.root, SHA, True, self.now)
        self.assertEqual(value["mode"], "qualification-only")
        self.assertFalse(value["expiry_approved"])
        beta_lifecycle.validate_metadata(value, self.root, SHA, now=self.now)
        with self.assertRaisesRegex(ValueError, "may not be published"):
            beta_lifecycle.validate_metadata(value, self.root, SHA, publication=True, now=self.now)
        for field, replacement in (("source_sha", "f"*40), ("build_id", "f"*10),
                                   ("version", "1.3.1-beta.3"), ("expires_at_unix", 1),
                                   ("mode", "approved"), ("expiry_approved", True)):
            changed = {**value, field: replacement}
            with self.subTest(field=field), self.assertRaises(ValueError):
                beta_lifecycle.validate_metadata(changed, self.root, SHA, now=self.now)

    def test_explicit_approved_deadline_is_checked_at_packaging_and_publication(self):
        self.policy(expires_at_utc="2027-01-31T23:59:59Z", expiry_approved=True)
        value = beta_lifecycle.configure_metadata(self.root, SHA, True, self.now)
        self.assertEqual(value["mode"], "approved")
        beta_lifecycle.validate_metadata(value, self.root, SHA, publication=True, now=self.now)
        with self.assertRaisesRegex(ValueError, "later"):
            beta_lifecycle.validate_metadata(value, self.root, SHA, publication=True,
                now=beta_lifecycle.parse_utc("2027-02-01T00:00:00Z"))
        text = beta_lifecycle.notice(value)
        for language in ("EN:", "DE:", "KO:"):
            self.assertIn(language, text)
        self.assertIn(value["expires_at_utc"], text)
        self.assertIn(SHA, text)

    def test_policy_cannot_infer_or_silently_approve_a_deadline(self):
        for changes in ({"expiry_approved": True}, {"expiry_approved": "true"},
                        {"expires_at_utc": "2027-01-31T23:59:59Z"},
                        {"version": "1.3.1-beta.9"}, {"active_instance_grace": False}):
            self.policy(**changes)
            with self.subTest(changes=changes), self.assertRaises(ValueError):
                beta_lifecycle.configure_metadata(self.root, SHA, now=self.now)

    def test_timezone_and_rebuild_do_not_restart_expiry(self):
        from datetime import timedelta
        self.policy(expires_at_utc="2027-01-31T23:59:59Z", expiry_approved=True)
        first = beta_lifecycle.configure_metadata(self.root, SHA, now=self.now)
        for current in (self.now.astimezone(timezone(timedelta(hours=9))), self.now+timedelta(days=20)):
            self.assertEqual(first, beta_lifecycle.configure_metadata(self.root, SHA, now=current))

    def test_current_approved_date_and_kst_rollover_match_header_and_notice(self):
        for utc, kst in (("2027-02-11T14:59:00Z", "2027-02-11 23:59:00"),
                         ("2027-02-11T15:00:00Z", "2027-02-12 00:00:00"),
                         ("2028-02-29T23:59:59Z", "2028-03-01 08:59:59"),
                         ("2026-12-31T23:59:59Z", "2027-01-01 08:59:59")):
            with self.subTest(utc=utc):
                self.policy(expires_at_utc=utc, expiry_approved=True)
                value = beta_lifecycle.configure_metadata(self.root, SHA, now=self.now)
                self.assertEqual(beta_lifecycle.expiry_kst(value), kst)
                header = self.root / "generated/BetaLifecycleBuild.h"
                beta_lifecycle.write_header(header, value)
                self.assertIn(f'#define CHIMERA_BETA_EXPIRY_KST "{kst}"', header.read_text())
                self.assertIn(f"Beta expiry (KST / UTC+09:00): {kst}", beta_lifecycle.notice(value))
        self.policy()
        pending = beta_lifecycle.configure_metadata(self.root, SHA, now=self.now)
        self.assertEqual(beta_lifecycle.expiry_kst(pending), "")
        self.assertIn("KST / UTC+09:00): NOT SET", beta_lifecycle.notice(pending))

    def test_next_version_cannot_inherit_current_approved_expiry(self):
        # Dates here are fixtures only. A source version change invalidates the
        # old policy even if qualification mode is requested.
        self.policy(expires_at_utc="2027-02-11T14:59:00Z", expiry_approved=True)
        (self.root / "RELEASE_CHANNEL").write_text("beta.3\n", encoding="utf-8")
        for qualification in (False, True):
            with self.subTest(qualification=qualification), self.assertRaisesRegex(ValueError, "version differs"):
                beta_lifecycle.configure_metadata(self.root, SHA, qualification, self.now)
        self.policy(version="1.3.1-beta.3")
        pending = beta_lifecycle.configure_metadata(self.root, SHA, now=self.now)
        self.assertEqual(pending["mode"], "pending-approval")
        self.assertEqual(pending["expires_at_unix"], 0)
        with self.assertRaisesRegex(ValueError, "unset"):
            beta_lifecycle.validate_metadata(pending, self.root, SHA, publication=True, now=self.now)

    def test_committed_beta2_approval_is_distinct_from_published_beta1(self):
        source = Path(__file__).resolve().parents[1]
        policy = json.loads((source / beta_lifecycle.POLICY_PATH).read_text())
        self.assertEqual((source / "VERSION").read_text().strip(), "1.3.2")
        self.assertEqual((source / "RELEASE_CHANNEL").read_text().strip(), "beta.2")
        self.assertEqual(policy["version"], "1.3.2-beta.2")
        self.assertEqual(policy["expires_at_utc"], "2027-03-31T14:59:00Z")
        self.assertIs(policy["expiry_approved"], True)
        metadata = beta_lifecycle.configure_metadata(source, SHA, now=self.now)
        self.assertEqual(metadata["expires_at_unix"], 1806505140)
        self.assertEqual(beta_lifecycle.expiry_kst(metadata), "2027-03-31 23:59:00")
        self.assertIn("2027-03-31T14:59:00Z", beta_lifecycle.notice(metadata))
        # An immutable beta.1 policy cannot configure the beta.2 source.
        (self.root / "VERSION").write_text("1.3.2\n")
        self.policy(version="1.3.2-beta.1", expires_at_utc="2027-02-11T14:59:00Z", expiry_approved=True)
        with self.assertRaisesRegex(ValueError, "version differs"):
            beta_lifecycle.configure_metadata(self.root, SHA, now=self.now)

    def test_non_beta_build_is_isolated(self):
        (self.root / "RELEASE_CHANNEL").write_text("stable\n", encoding="utf-8")
        value = beta_lifecycle.configure_metadata(self.root, SHA, now=self.now)
        self.assertEqual(value["mode"], "not-beta")
        beta_lifecycle.validate_metadata(value, self.root, SHA, publication=True, now=self.now)

    def test_header_and_json_carry_the_same_identity(self):
        result = beta_lifecycle.build_metadata(
            self.root, SHA, "2027-01-31T23:59:59Z", self.now)
        header = self.root / "generated/BetaLifecycleBuild.h"
        beta_lifecycle.write_header(header, result)
        text = header.read_text(encoding="utf-8")
        for value in (result["version"], result["source_sha"],
                      result["expires_at_utc"], str(result["expires_at_unix"])):
            self.assertIn(str(value), text)
        encoded = json.dumps(result, sort_keys=True)
        self.assertIn('"network_required": false', encoded)


if __name__ == "__main__":
    unittest.main()
