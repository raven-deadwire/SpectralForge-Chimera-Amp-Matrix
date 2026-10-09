#!/usr/bin/env python3
"""Negative publication contracts and offline candidate review; no network writes."""
import copy
import io
import json
from pathlib import Path
import tempfile
import unittest
import zipfile
from contextlib import redirect_stdout
from unittest.mock import patch

import publish_beta13 as publisher

HEAD = "a" * 40
RUN = 123
REAL_ROOT = publisher.ROOT


class PublicationInputs(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        self.candidate = self.root / "candidate"
        self.candidate.mkdir()
        self.policy = publisher.read_json(REAL_ROOT / "Validation/release-policy.json")
        self.policy["profiles"][publisher.PROFILE] = copy.deepcopy(self.policy["profiles"]["beta_1_2"])
        self.policy["release_scope"].update(version=publisher.VERSION, profile=publisher.PROFILE,
            channel="beta.1", preparation_authorized=True, publication_authorized=False,
            release_approved=False, status="PREPARATION_ONLY")
        (self.root / "Validation").mkdir()
        self.write_policy()
        self.write_json(self.root / "Validation/waivers.json", {"waivers": []})
        (self.root / "VERSION").write_text("1.3.0\n", encoding="utf-8")
        (self.root / "RELEASE_CHANNEL").write_text("beta.1\n", encoding="utf-8")
        patcher = patch.object(publisher, "ROOT", self.root)
        patcher.start()
        self.addCleanup(patcher.stop)
        (self.root / "docs").mkdir()
        for name in publisher.SOURCE_DOCUMENTS:
            (self.root / "docs" / name).write_text("Fixture — 한국어 검증\n", encoding="utf-8")
        notes = f"# SpectralForge Chimera {publisher.VERSION}\n\nCAB · Niflheimr\nStudio One acceptance remains pending.\n한국어 문서 — 오리지널 캐비넷\n"
        (self.root / "docs/OPEN_BETA_RELEASE_NOTES.md").write_text(notes, encoding="utf-8")
        (self.candidate / "OPEN_BETA_RELEASE_NOTES.md").write_text(notes, encoding="utf-8")
        (self.root / "Tools").mkdir()
        (self.root / "Tools/Trace-Chimera-Session.ps1").write_text("# fixture\n", encoding="utf-8")
        (self.root / "COPYRIGHT.txt").write_text("Fixture copyright\n", encoding="utf-8")
        for name in publisher.DOCUMENTS:
            (self.candidate / name).write_text("fixture\n", encoding="utf-8")
        self.source = dict(version=publisher.VERSION, tag=publisher.TAG, revision=HEAD,
            runId=str(RUN), published=False, publisherSigned=False, macOSNotarized=False)
        self.write_json(self.candidate / "candidate-source.json", self.source)
        self.prefix = f"SpectralForge-Chimera-{publisher.VERSION}-"
        for suffix in publisher.SUFFIXES:
            (self.candidate / (self.prefix + suffix)).write_bytes(b"fixture-package")
        with zipfile.ZipFile(self.candidate / (self.prefix + "win64.zip"), "w") as archive:
            archive.writestr("payload-manifest.json", json.dumps({"version": publisher.VERSION, "source_sha": HEAD}))
            archive.writestr("Standalone/fixture.exe", b"fixture")
        self.manifest = dict(schema=1, version=publisher.VERSION, channel="beta",
                             releaseUrl=publisher.RELEASE_URL, assets=[])
        for platform, arch, suffix in [("windows", "x86_64", "win64-Setup.exe"),
                ("macos", "universal", "macos-universal.pkg"), ("linux", "x86_64", "linux-x86_64.deb")]:
            name = self.prefix + suffix
            file = self.candidate / name
            self.manifest["assets"].append(dict(platform=platform, arch=arch, name=name,
                url=publisher.DOWNLOAD_URL + name, size=file.stat().st_size, sha256=publisher.sha256(file)))
        self.write_json(self.candidate / "update-beta.json", self.manifest)
        self.refresh_sums()

    def write_json(self, path, value):
        path.write_text(json.dumps(value), encoding="utf-8")

    def write_policy(self):
        self.write_json(self.root / "Validation/release-policy.json", self.policy)

    def refresh_sums(self):
        lines = []
        for suffix in publisher.SUFFIXES:
            name = self.prefix + suffix
            line = publisher.sha256(self.candidate / name) + "  " + name + "\n"
            lines.append(line)
            (self.candidate / (name + ".sha256.txt")).write_text(line, encoding="utf-8")
        (self.candidate / "SHA256SUMS.txt").write_text("".join(lines), encoding="utf-8")

    def test_complete_source_bound_five_package_review(self):
        notes, assets = publisher.prepare_assets(self.candidate, HEAD, RUN)
        self.assertIn("한국어", notes)
        self.assertEqual(sum(name.startswith(self.prefix) for name in assets), 5)
        self.assertIn("CAB_LAYOUT_V3.md", assets)

    def test_offline_review_never_constructs_api_or_publishes(self):
        with patch.object(publisher.transport, "GitHub") as api, \
             patch.object(publisher.transport, "publish") as publish, redirect_stdout(io.StringIO()):
            result = publisher.main(["--dry-run", "--source-sha", HEAD,
                                     "--candidate-dir", str(self.candidate), "--run-id", str(RUN)])
        api.assert_not_called()
        publish.assert_not_called()
        self.assertEqual(result["candidate"], "LOCAL_PACKAGE_CHECKS_PASS")
        self.assertFalse(result["publication_ready"])
        self.assertFalse(result["published"])
        self.assertIn("PUBLICATION_NOT_AUTHORIZED", result["blockers"])

    def test_default_mode_is_offline_and_does_not_claim_missing_files(self):
        with patch.object(publisher.transport, "GitHub") as api, redirect_stdout(io.StringIO()):
            result = publisher.main(["--source-sha", HEAD])
        api.assert_not_called()
        self.assertEqual(result["candidate"], "NOT_SUPPLIED")
        self.assertEqual(result["consolidated_acceptance"], "NOT_SUPPLIED")

    def test_preparation_scope_blocks_live_before_any_request(self):
        with patch.object(publisher.transport, "GitHub") as api, \
             patch.object(publisher.transport, "publish") as publish:
            with self.assertRaisesRegex(RuntimeError, "preparation scope"):
                publisher.main(["--publish", "--source-sha", HEAD, "--confirm", "publish-" + publisher.TAG])
        api.assert_not_called()
        publish.assert_not_called()

    def test_each_publication_flag_and_review_marker_are_independent(self):
        for authorized, accepted in ((False, True), (True, False), ("true", True), (True, "true")):
            with self.subTest(authorized=authorized, accepted=accepted):
                self.policy["release_scope"].update(publication_authorized=authorized, release_approved=accepted)
                self.write_policy()
                with patch.object(publisher.transport, "GitHub") as api, self.assertRaisesRegex(RuntimeError, "preparation scope"):
                    publisher.main(["--publish", "--source-sha", HEAD])
                api.assert_not_called()
        self.policy["release_scope"].update(publication_authorized=True, release_approved=True)
        (self.root / "RELEASE_REVIEW_PENDING").write_text("Pending\n", encoding="utf-8")
        self.assertIn("RELEASE_REVIEW_PENDING", publisher.publication_blockers(self.policy))

    def test_approval_cannot_bypass_manual_workflow_identity(self):
        self.policy["release_scope"].update(publication_authorized=True, release_approved=True)
        self.write_policy()
        with patch.dict(publisher.os.environ, {"GITHUB_EVENT_NAME": "push"}), \
             patch.object(publisher.transport, "GitHub") as api, self.assertRaisesRegex(RuntimeError, "manually confirmed"):
            publisher.main(["--publish", "--source-sha", HEAD, "--confirm", "publish-" + publisher.TAG])
        api.assert_not_called()

    def test_version_preview_and_scope_mismatch_are_rejected(self):
        for version, channel in (("1.2.0", "beta.1"), ("1.3.0", "preview"), ("1.3.0", "beta.2")):
            with self.subTest(version=version, channel=channel):
                (self.root / "VERSION").write_text(version, encoding="utf-8")
                (self.root / "RELEASE_CHANNEL").write_text(channel, encoding="utf-8")
                with self.assertRaisesRegex(RuntimeError, "pinned"):
                    publisher.release_policy(HEAD)

    def test_existing_manual_and_host_gates_cannot_be_dropped(self):
        self.policy["profiles"][publisher.PROFILE]["required_stages"].remove("B12")
        self.write_policy()
        with self.assertRaisesRegex(RuntimeError, "preserve existing acceptance"):
            publisher.release_policy(HEAD)

    def test_rejects_stale_source_run_version_or_published_candidate(self):
        for key, value in (("revision", "b" * 40), ("runId", "124"), ("version", "1.2.0-beta.1"),
                           ("published", True), ("publisherSigned", True)):
            with self.subTest(key=key):
                self.write_json(self.candidate / "candidate-source.json", {**self.source, key: value})
                with self.assertRaisesRegex(RuntimeError, "source identity"):
                    publisher.prepare_assets(self.candidate, HEAD, RUN)

    def test_changed_binary_and_sidecar_cannot_pass(self):
        setup = self.candidate / (self.prefix + "win64-Setup.exe")
        setup.write_bytes(b"changed")
        with self.assertRaisesRegex(RuntimeError, "checksum mismatch"):
            publisher.prepare_assets(self.candidate, HEAD, RUN)
        self.refresh_sums()
        (self.candidate / (self.prefix + "win64.zip.sha256.txt")).write_text("invalid\n", encoding="utf-8")
        with self.assertRaisesRegex(RuntimeError, "Sidecar"):
            publisher.prepare_assets(self.candidate, HEAD, RUN)

    def test_missing_platform_or_external_update_url_are_rejected(self):
        self.manifest["assets"][0]["url"] = "https://example.invalid/setup.exe"
        self.write_json(self.candidate / "update-beta.json", self.manifest)
        with self.assertRaisesRegex(RuntimeError, "Update asset"):
            publisher.prepare_assets(self.candidate, HEAD, RUN)
        self.manifest["assets"].pop()
        self.write_json(self.candidate / "update-beta.json", self.manifest)
        with self.assertRaisesRegex(RuntimeError, "three platforms"):
            publisher.prepare_assets(self.candidate, HEAD, RUN)

    def test_private_capture_rejected_even_with_refreshed_hashes(self):
        with zipfile.ZipFile(self.candidate / (self.prefix + "win64.zip"), "a") as archive:
            archive.writestr("capture.nam", "private")
        self.refresh_sums()
        with self.assertRaisesRegex(RuntimeError, "Private capture"):
            publisher.prepare_assets(self.candidate, HEAD, RUN)

    def test_packaged_notes_must_match_reviewed_source(self):
        (self.candidate / "OPEN_BETA_RELEASE_NOTES.md").write_text("Older release notes", encoding="utf-8")
        with self.assertRaisesRegex(RuntimeError, "notes differ"):
            publisher.prepare_assets(self.candidate, HEAD, RUN)


class CIContracts(unittest.TestCase):
    def good_run(self, run_id=RUN):
        return dict(id=run_id, run_attempt=1, head_sha=HEAD, event="pull_request",
                    status="completed", conclusion="success", repository={"full_name": publisher.transport.REPO})

    def test_stale_missing_failed_pending_or_newer_failed_runs_are_rejected(self):
        good = self.good_run()
        self.assertEqual(publisher.verified_run([good], HEAD)["id"], RUN)
        for changes in ({"head_sha": "b" * 40}, {"event": "push"}, {"status": "in_progress"},
                        {"conclusion": "failure"}, {"conclusion": "cancelled"}, {"run_attempt": None}):
            with self.subTest(changes=changes), self.assertRaises(RuntimeError):
                publisher.verified_run([{**good, **changes}], HEAD)
        with self.assertRaisesRegex(RuntimeError, "missing"):
            publisher.verified_run([], HEAD)
        with self.assertRaisesRegex(RuntimeError, "Latest"):
            publisher.verified_run([good, {**good, "id": RUN + 1, "conclusion": "failure"}], HEAD)

    def test_every_cab_platform_and_consolidator_must_succeed(self):
        for workflow in ("build.yml", "cab-panel.yml"):
            expected = publisher.WORKFLOWS[workflow]
            good = [dict(name=name, status="completed", conclusion="success") for name in expected]
            publisher.verified_jobs(good, expected)
            for index in range(len(good)):
                with self.subTest(workflow=workflow, missing=good[index]["name"]), self.assertRaises(RuntimeError):
                    publisher.verified_jobs(good[:index] + good[index + 1:], expected)
            with self.assertRaises(RuntimeError):
                publisher.verified_jobs(good + [good[0]], expected)

    def test_seed_artifact_does_not_replace_missing_consolidator(self):
        expected = publisher.WORKFLOWS["build.yml"]
        good = [dict(name=name, status="completed", conclusion="success") for name in expected
                if name != "consolidate-release-gate"]
        with self.assertRaisesRegex(RuntimeError, "consolidate-release-gate"):
            publisher.verified_jobs(good, expected)

    def test_missing_duplicate_expired_or_wrong_source_artifact_is_rejected(self):
        good = dict(id=1, name=publisher.ARTIFACT_NAME, expired=False, size_in_bytes=123,
                    workflow_run={"id": RUN, "head_sha": HEAD}, digest="sha256:" + "a" * 64)
        publisher.verified_artifact([good], publisher.ARTIFACT_NAME, HEAD, RUN)
        for values in ([], [good, good], [{**good, "expired": True}],
                       [{**good, "workflow_run": {"id": RUN, "head_sha": "b" * 40}}],
                       [{**good, "digest": ""}]):
            with self.subTest(values=values), self.assertRaises(RuntimeError):
                publisher.verified_artifact(values, publisher.ARTIFACT_NAME, HEAD, RUN)

    def test_duplicate_and_nonfinite_json_are_rejected(self):
        for text in ('{"ready":false,"ready":true}', '{"value":NaN}'):
            with self.subTest(text=text), self.assertRaises((RuntimeError, ValueError)):
                publisher.parse_json(text)

    def test_workflow_has_manual_default_review_and_no_push_trigger(self):
        workflow = (REAL_ROOT / ".github/workflows/publish-beta13.yml").read_text(encoding="utf-8")
        self.assertIn("workflow_dispatch:", workflow)
        self.assertNotIn("\n  push:", workflow)
        self.assertIn("default: false", workflow)
        self.assertIn("--dry-run", workflow)
        self.assertIn("release/open-beta-1.3.0", workflow)
        self.assertIn("publish-v1.3.0-beta.1", workflow)


if __name__ == "__main__":
    unittest.main()
