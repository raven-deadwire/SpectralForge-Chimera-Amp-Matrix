#!/usr/bin/env python3
from __future__ import annotations

from datetime import datetime, timezone
import json
from pathlib import Path
import tempfile
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
