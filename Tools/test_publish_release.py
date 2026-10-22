#!/usr/bin/env python3
"""Weekly publisher regression tests: real Git source binding, no network writes."""
from __future__ import annotations

import copy
from datetime import date
from contextlib import redirect_stdout
import io
import json
from pathlib import Path
import subprocess
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import Mock, patch
import zipfile

import publish_release as publisher
import package_release as packaging
import release_evidence as evidence
import test_release_consolidator as fixtures

REAL_ROOT = publisher.ROOT
RUN, ATTEMPT = 123, 2


class SourceFixture(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.work = Path(self.temporary.name)
        self.root = self.work / "repo"
        self.root.mkdir()
        self.candidate = self.work / "candidate"
        self.candidate.mkdir()
        self.git("init", "--quiet")
        self.git("config", "user.name", "Publisher regression fixture")
        self.git("config", "user.email", "fixture@example.invalid")
        self.git("config", "commit.gpgsign", "false")
        self.git("config", "core.autocrlf", "false")
        for name in ("Validation/release-policy.json", "Validation/waivers.json",
                     "Validation/ci-check-map.json", publisher.APPROVAL_PATH):
            fixture = ("Tools/fixtures/weekly-publication-policy-unapproved.json"
                       if name == publisher.APPROVAL_PATH else name)
            self.write(name, (REAL_ROOT / fixture).read_text(encoding="utf-8"))
        for name in publisher.SOURCE_DOCUMENTS:
            self.write("docs/" + name, "Fixture source document\n")
        self.write("Tools/Trace-Chimera-Session.ps1", "# Fixture\n")
        self.write("Source/PluginProcessor.cpp", "// Tested product\n")
        self.write("Tools/publish_release.py", "# Tested publisher\n")
        self.write("COPYRIGHT.txt", "Fixture copyright\n")
        patcher = patch.object(publisher, "ROOT", self.root)
        patcher.start()
        self.addCleanup(patcher.stop)
        clock = patch.object(publisher.check_release_calendar, 'kst_today', return_value=date(2026, 10, 16))
        clock.start()
        self.addCleanup(clock.stop)
        self.select_version("1.3.1-beta.2")

    def git(self, *args):
        return subprocess.check_output(["git", *args], cwd=self.root,
            text=True, stderr=subprocess.STDOUT).strip()

    def write(self, name, value):
        path = self.root / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(value, encoding="utf-8")

    def write_json(self, path, value):
        path.write_text(json.dumps(value), encoding="utf-8")

    def commit(self, message):
        self.git("add", "--all")
        self.git("commit", "--quiet", "--allow-empty", "-m", message)
        return self.git("rev-parse", "HEAD")

    def select_version(self, version):
        product, channel = publisher.chimera_version.parse_release_version(version)
        self.write("VERSION", product + "\n")
        self.write("RELEASE_CHANNEL", channel + "\n")
        self.policy = publisher.read_json(self.root / "Validation/release-policy.json")
        profile = "beta_1_3" if channel.startswith("beta.") else "full_release"
        self.policy["release_scope"].update(version=version, product_version=product,
            channel=channel, profile=profile, preparation_authorized=True,
            publication_authorized=False, release_approved=False,
            deferred_checks=["I2.PITCH_LIVE", "I2.PITCH_DI"] if profile == "beta_1_3" else [])
        self.write_json(self.root / "Validation/release-policy.json", self.policy)
        self.notes = f"# Chimera {version}\n\nCAB update. Studio One acceptance is source-specific.\n"
        self.write("docs/OPEN_BETA_RELEASE_NOTES.md", self.notes)
        calendar = json.loads((REAL_ROOT / "Validation/release-calendar.json").read_text(encoding="utf-8"))
        # Synthetic fixture history predates beta.2; it does not alter real history.
        original_calendar = copy.deepcopy(calendar)
        calendar['updated_at'] = '2026-10-15'
        calendar['published_releases'] = [r for r in calendar['published_releases']
                                        if r['version'] not in ('1.3.1-beta.2', '1.3.2-beta.1')]
        calendar.pop('completed_reservations', None)
        calendar.pop('next_release', None)
        calendar['source_baseline'].update(release='v1.3.1-beta.1', product_source=calendar['published_releases'][0]['product_source'])
        row = calendar['chimera_weekly'][0]
        row.update(row.pop('missed_target'))
        row.update(version=version, tag='v'+version, status='blocked', scope='Synthetic conditional reservation.')
        row.pop('defer_reason', None)
        row.pop('cancellation_reason', None)
        calendar['development_candidates'][0].update(version=version, tag='v'+version,
            publication_target=row['publication_target'], reservation_status='blocked')
        self.write_json(self.root / 'Validation/release-calendar.json', calendar)
        text = (REAL_ROOT / 'docs/RELEASE_CALENDAR_2026_2027.md').read_text(encoding='utf-8')
        old_row = original_calendar['chimera_weekly'][0]
        old_line = f"| TBD | TBD | TBD | `{old_row['version']}` | {old_row['scope']} |"
        new_line = f"| {row['test_build_date']} | {row['candidate_date']} | {row['publication_target']} | `{version}` | {row['scope']} |"
        text = text.replace(old_line, new_line)
        self.write('docs/RELEASE_CALENDAR_2026_2027.md', text)
        self.write('docs/DEVELOPMENT_ROADMAP_2026_2027.md', publisher.check_release_calendar.roadmap_status_table(calendar))
        catalog = json.loads((REAL_ROOT / 'Validation/catalog-count-contract.json').read_text(encoding='utf-8'))
        catalog['roadmap']['delivered_baseline']['version'] = '1.3.1-beta.1'
        catalog['roadmap']['quality_followup']['weekly_versions'] = publisher.check_release_calendar.weekly_mirror(calendar)
        self.write_json(self.root / 'Validation/catalog-count-contract.json', catalog)
        self.source = self.commit("tested " + version)
        self.release = publisher.Release(version, self.source)
        return self.release

    def make_candidate(self):
        for path in self.candidate.iterdir():
            path.unlink()
        release = self.release
        for name in publisher.DOCUMENTS:
            (self.candidate / name).write_text("fixture\n", encoding="utf-8")
        (self.candidate / "OPEN_BETA_RELEASE_NOTES.md").write_text(self.notes, encoding="utf-8")
        self.source_manifest = dict(version=release.version, tag=release.tag, revision=release.source_sha,
            runId=str(RUN), runAttempt=str(ATTEMPT), published=False, publisherSigned=False, macOSNotarized=False)
        self.write_json(self.candidate / "candidate-source.json", self.source_manifest)
        self.prefix = f"SpectralForge-Chimera-{release.version}-"
        for suffix in publisher.SUFFIXES:
            (self.candidate / (self.prefix + suffix)).write_bytes(b"fixture-package")
        self.payload = publisher.chimera_version.identity(self.root, release.source_sha)
        self.write_portable()
        self.refresh_sums()
        manifest = dict(schema=1, version=release.version, source_sha=release.source_sha,
                        releaseUrl=release.release_url, assets=[])
        for platform, arch, suffix in (("windows", "x86_64", "win64-Setup.exe"),
                ("macos", "universal", "macos-universal.pkg"), ("linux", "x86_64", "linux-x86_64.deb")):
            name = self.prefix + suffix
            path = self.candidate / name
            manifest["assets"].append(dict(platform=platform, arch=arch, name=name,
                url=release.download_url + name, size=path.stat().st_size, sha256=publisher.sha256(path)))
        for name in release.manifest_names:
            self.write_json(self.candidate / name,
                            {**manifest, "channel": name.removeprefix("update-").removesuffix(".json")})

    def write_portable(self):
        with zipfile.ZipFile(self.candidate / (self.prefix + "win64.zip"), "w") as archive:
            archive.writestr("payload-manifest.json", json.dumps(self.payload))
            archive.writestr("Standalone/fixture.exe", b"fixture")

    def refresh_sums(self):
        lines = []
        for suffix in publisher.SUFFIXES:
            name = self.prefix + suffix
            line = publisher.sha256(self.candidate / name) + "  " + name + "\n"
            lines.append(line)
            (self.candidate / (name + ".sha256.txt")).write_text(line, encoding="utf-8")
        (self.candidate / "SHA256SUMS.txt").write_text("".join(lines), encoding="utf-8")


class SourceContracts(SourceFixture):
    def test_issue97_unimplemented_scope_stops_before_network_or_staging(self):
        release = self.select_version("1.3.2-beta.2")
        with patch.object(publisher.transport, "GitHub") as api, \
             patch.object(publisher.transport, "publish") as publish, \
             patch.object(publisher, "prepare_assets") as stage:
            for mode in ("--dry-run", "--publish"):
                with self.subTest(mode=mode), self.assertRaisesRegex(
                        RuntimeError, "ISSUE97_P0_GATE_UNIMPLEMENTED"):
                    publisher.main([mode, "--target-version", release.version,
                                    "--source-sha", release.source_sha,
                                    "--confirm", release.confirm])
        api.assert_not_called()
        publish.assert_not_called()
        stage.assert_not_called()

    def test_every_target_requires_both_full_source_and_strict_version(self):
        for version in ("1.3.1-beta.2", "1.3.1-beta.10", "1.3.2-beta.1", "1.3.1-rc.1", "1.3.1"):
            with self.subTest(version=version):
                release = self.select_version(version)
                self.assertEqual(publisher.release_policy(release)["release_scope"]["version"], version)
                self.assertEqual(release.confirm, "publish-v" + version + "@" + self.source)
                self.assertEqual(release.artifact_name, f"SpectralForge-Chimera-{version}-Release-Candidate")
        for version in ("v1.3.1-beta.2", "1.3.1-beta.0", "1.3.1-beta.02", "1.3.1-preview.abcdef1234",
                        "1.3.1+meta", "01.3.1", "1.3.1-beta.2\n", "$(echo bad)"):
            with self.subTest(version=version), self.assertRaises((RuntimeError, ValueError)):
                publisher.Release(version, self.source)
        for source in (None, "", self.source[:10], "A" * 40, "g" * 40, "a" * 40 + "\n"):
            with self.subTest(source=source), self.assertRaises(RuntimeError):
                publisher.Release("1.3.1-beta.2", source)

    def test_default_review_and_invalid_inputs_never_construct_api(self):
        with patch.object(publisher.transport, "GitHub") as api, \
             patch.object(publisher.transport, "publish") as publish, redirect_stdout(io.StringIO()):
            result = publisher.main(["--target-version", self.release.version, "--source-sha", self.source])
            self.assertFalse(result["published"])
            self.assertFalse(result["publication_ready"])
            self.assertEqual(result["candidate"], "NOT_SUPPLIED")
            self.assertTrue(any("APPROVAL_REQUIRED" in item for item in result["blockers"]))
            with self.assertRaises(RuntimeError):
                publisher.main(["--target-version", "1.3.1-beta.10", "--source-sha", self.source])
            with self.assertRaises(ValueError):
                publisher.main(["--target-version", "1.3.1-beta.02", "--source-sha", self.source])
        api.assert_not_called()
        publish.assert_not_called()

    def test_source_files_are_read_from_selected_commit_not_relabelled(self):
        old_source = self.source
        self.select_version("1.3.1-beta.10")
        with self.assertRaisesRegex(RuntimeError, "selected source"):
            publisher.release_policy(publisher.Release("1.3.1-beta.10", old_source))

    def test_only_approval_receipt_can_follow_the_tested_source(self):
        self.assertEqual(publisher.CONTROL_PATHS, frozenset({"Validation/weekly-publication-policy.json"}))
        self.write(publisher.APPROVAL_PATH, '{"still_unapproved": true}\n')
        controller = self.commit("approval metadata only")
        proof = publisher.verify_controller_source(self.source, controller)
        self.assertEqual(proof["controller_changed_paths"], [publisher.APPROVAL_PATH])
        self.assertEqual(publisher.release_policy(self.release), self.policy)
        for path in ("Source/PluginProcessor.cpp", "VERSION", "RELEASE_CHANNEL", "Tools/publish_release.py",
                     "Validation/release-policy.json", ".github/workflows/build.yml", "docs/OPEN_BETA_RELEASE_NOTES.md"):
            with self.subTest(path=path):
                self.git("reset", "--hard", controller)
                self.write(path, "untested change\n")
                changed = self.commit("untested product/controller")
                with self.assertRaisesRegex(RuntimeError, "Untested"):
                    publisher.verify_controller_source(self.source, changed)

    def test_dirty_or_unrelated_controller_is_rejected(self):
        self.write(publisher.APPROVAL_PATH, "uncommitted change")
        with self.assertRaisesRegex(RuntimeError, "local changes"):
            publisher.verify_controller_source(self.source, self.source)
        self.git("reset", "--hard", self.source)
        self.write(publisher.APPROVAL_PATH, '{"metadata":1}')
        controller = self.commit("controller")
        self.git("checkout", "--quiet", "--detach", self.source)
        self.write(publisher.APPROVAL_PATH, '{"metadata":2}')
        sibling = self.commit("sibling")
        self.git("checkout", "--quiet", "--detach", controller)
        with self.assertRaises(subprocess.CalledProcessError):
            publisher.verify_controller_source(sibling, controller)

    def test_approval_deletion_and_symlink_cannot_enter_controller(self):
        (self.root / publisher.APPROVAL_PATH).unlink()
        deleted = self.commit("delete approval")
        with self.assertRaisesRegex(RuntimeError, "Untested"):
            publisher.verify_controller_source(self.source, deleted)
        self.git("reset", "--hard", self.source)
        self.git("config", "core.symlinks", "false")
        self.write(publisher.APPROVAL_PATH, "../Source/PluginProcessor.cpp")
        blob = self.git("hash-object", "-w", publisher.APPROVAL_PATH)
        self.git("update-index", "--cacheinfo", "120000," + blob + "," + publisher.APPROVAL_PATH)
        self.git("commit", "--quiet", "-m", "symlink approval")
        with self.assertRaisesRegex(RuntimeError, "Untested|file mode"):
            publisher.verify_controller_source(self.source, self.git("rev-parse", "HEAD"))

    def test_historical_approval_and_confirmation_never_publish_new_source(self):
        for receipt in (publisher.read_json(REAL_ROOT / "Validation/beta-publication-policy-1.3.1.json"),
                        publisher.read_json(REAL_ROOT / publisher.APPROVAL_PATH)):
            self.write_json(self.root / publisher.APPROVAL_PATH, receipt)
            controller = self.commit("unapproved receipt")
            environment = dict(GITHUB_SHA=controller, GITHUB_REPOSITORY=publisher.transport.REPO,
                GITHUB_EVENT_NAME="workflow_dispatch", GITHUB_REF="refs/heads/main", GITHUB_RUN_ID="900")
            with patch.dict(publisher.os.environ, environment), patch.object(publisher.transport, "GitHub") as api:
                with self.assertRaises((RuntimeError, ValueError)):
                    publisher.main(["--publish", "--target-version", self.release.version,
                        "--source-sha", self.source, "--confirm", self.release.confirm])
                with self.assertRaisesRegex(RuntimeError, "manual confirmation"):
                    publisher.main(["--publish", "--target-version", self.release.version,
                        "--source-sha", self.source, "--confirm", "publish-v1.3.1-beta.1"])
            api.assert_not_called()


class PackageContracts(SourceFixture):
    def test_public_attachments_exclude_research_reference_documents(self):
        self.make_candidate()
        _, assets = publisher.prepare_assets(self.candidate, self.release, RUN, ATTEMPT)
        self.assertTrue(set(publisher.PUBLIC_DOCUMENTS) <= set(assets))
        for name in ("AMP_NATIVE_DSP.md", "POST_NATIVE_DSP.md", "PEDAL_BOARD_DSP.md",
                     "MODELS_AND_REFERENCE.md", "NAM_REFERENCE_RESULTS.md", "ARTWORK_PROMPTS.json"):
            self.assertNotIn(name, assets)

    def test_original_name_in_source_public_document_blocks_preparation(self):
        self.make_candidate()
        self.write("docs/PRESETS.md", "New preset uses Fender Twin Reverb.\n")
        with self.assertRaisesRegex(RuntimeError, "Original amp/effect name"):
            publisher.prepare_assets(self.candidate, self.release, RUN, ATTEMPT)

    def test_original_name_in_candidate_manual_blocks_preparation(self):
        self.make_candidate()
        (self.candidate / "MANUAL.html").write_text('<button title="BOSS SD-1">Yellow Asym</button>', encoding="utf-8")
        with self.assertRaisesRegex(RuntimeError, "Original amp/effect name"):
            publisher.prepare_assets(self.candidate, self.release, RUN, ATTEMPT)

    def test_five_packages_and_all_channel_feeds_are_verified(self):
        for version in ("1.3.1-beta.2", "1.3.1-beta.10", "1.3.2-beta.1", "1.3.1-rc.1", "1.3.1"):
            with self.subTest(version=version):
                self.select_version(version)
                self.make_candidate()
                notes, assets = publisher.prepare_assets(self.candidate, self.release, RUN, ATTEMPT)
                self.assertIn(version, notes)
                self.assertEqual(sum(name.startswith(self.prefix) for name in assets), 5)
                self.assertTrue(set(self.release.manifest_names) <= set(assets))

    def test_candidate_mismatched_version_source_run_and_attempt_are_rejected(self):
        self.make_candidate()
        for key, value in (("version", "1.3.1-beta.1"), ("revision", "a" * 40), ("runId", "124"),
                           ("runAttempt", "1"), ("runAttempt", None), ("tag", "v1.3.1-beta.1"),
                           ("published", True), ("publisherSigned", True), ("macOSNotarized", True)):
            with self.subTest(key=key, value=value):
                self.write_json(self.candidate / "candidate-source.json", {**self.source_manifest, key: value})
                with self.assertRaisesRegex(RuntimeError, "Candidate source/version/run"):
                    publisher.prepare_assets(self.candidate, self.release, RUN, ATTEMPT)

    def test_tampered_package_checksum_feed_and_payload_are_rejected(self):
        self.make_candidate()
        binary = self.candidate / (self.prefix + "linux-x86_64.deb")
        binary.write_bytes(b"changed")
        with self.assertRaisesRegex(RuntimeError, "Binary checksum"):
            publisher.prepare_assets(self.candidate, self.release, RUN, ATTEMPT)
        self.make_candidate()
        feed_path = self.candidate / "update-beta.json"
        original = publisher.read_json(feed_path)
        for field, value in (("version", "1.3.1-beta.1"), ("source_sha", "b" * 40), ("channel", "stable"),
                              ("releaseUrl", "https://example.invalid/"), ("schema", True)):
            with self.subTest(field=field):
                self.write_json(feed_path, {**original, field: value})
                with self.assertRaisesRegex(RuntimeError, "manifest identity"):
                    publisher.prepare_assets(self.candidate, self.release, RUN, ATTEMPT)
        self.write_json(feed_path, original)
        self.payload["source_sha"] = "b" * 40
        self.write_portable()
        self.refresh_sums()
        with self.assertRaisesRegex(ValueError, "Payload source_sha"):
            publisher.prepare_assets(self.candidate, self.release, RUN, ATTEMPT)

    def test_stable_legacy_feed_cannot_point_to_different_bits(self):
        self.select_version("1.3.1")
        self.make_candidate()
        path = self.candidate / "update-beta.json"
        feed = publisher.read_json(path)
        feed["assets"][0]["url"] = self.release.download_url.replace("v1.3.1/", "v1.3.1-beta.1/") + feed["assets"][0]["name"]
        self.write_json(path, feed)
        with self.assertRaisesRegex(RuntimeError, "Update asset mismatch"):
            publisher.prepare_assets(self.candidate, self.release, RUN, ATTEMPT)

    def test_offline_candidate_review_never_creates_release(self):
        self.make_candidate()
        with patch.object(publisher.transport, "GitHub") as api, \
             patch.object(publisher.transport, "publish") as publish, redirect_stdout(io.StringIO()):
            result = publisher.main(["--target-version", self.release.version, "--source-sha", self.source,
                "--candidate-dir", str(self.candidate), "--run-id", str(RUN), "--run-attempt", str(ATTEMPT)])
        api.assert_not_called()
        publish.assert_not_called()
        self.assertEqual(result["candidate"], "LOCAL_PACKAGE_CHECKS_PASS")
        self.assertFalse(result["publication_ready"])


class BetaLifecyclePackageContracts(SourceFixture):
    def beta_candidate(self, approved=False, include=True, corrupt_notice=False):
        lifecycle = publisher.beta_lifecycle
        self.write_json(self.root / lifecycle.POLICY_PATH, {
            "schema": "spectralforge.chimera.beta-policy.v1", "version": "1.3.1-beta.3",
            "expires_at_utc": "2098-01-01T00:00:00Z" if approved else None,
            "expiry_approved": approved, "warning_lead_seconds": 1209600,
            "new_expired_instance": "muted-output-state-preserved", "active_instance_grace": True})
        self.select_version("1.3.1-beta.3")
        self.make_candidate()
        if include:
            metadata = lifecycle.configure_metadata(self.root, self.source, qualification_only=True)
            build, stage = self.work / "beta-build", self.work / "beta-stage"
            build.mkdir(exist_ok=True)
            stage.mkdir(exist_ok=True)
            self.write_json(build / "chimera-beta-lifecycle.json", metadata)
            self.write_json(build / "chimera-build-version.json", {
                "beta_lifecycle_sha256": packaging.sha256(build / "chimera-beta-lifecycle.json")})
            with patch.object(packaging, "ROOT", self.root):
                packaging.lifecycle_documents(stage, build, version={"source_sha": self.source})
            # Exercise the real stager on Windows too: writestr(text) bypasses
            # Windows newline translation and used to conceal this mismatch.
            self.assertEqual((stage / "BETA_LIFECYCLE.txt").read_bytes(), lifecycle.notice(metadata).encode("utf-8"))
            if corrupt_notice:
                (stage / "BETA_LIFECYCLE.txt").write_bytes(b"wrong deadline")
            with zipfile.ZipFile(self.candidate / (self.prefix + "win64.zip"), "a") as archive:
                for name in ("chimera-beta-lifecycle.json", "BETA_LIFECYCLE.txt"):
                    archive.write(stage / name, name)
        self.refresh_sums()

    def test_real_publisher_rejects_qualification_metadata(self):
        self.beta_candidate()
        with self.assertRaisesRegex(ValueError, "may not be published"):
            publisher.prepare_assets(self.candidate, self.release, RUN, ATTEMPT)

    def test_real_publisher_rejects_missing_or_inconsistent_notice(self):
        self.beta_candidate(include=False)
        with self.assertRaisesRegex(RuntimeError, "metadata is missing"):
            publisher.prepare_assets(self.candidate, self.release, RUN, ATTEMPT)
        self.beta_candidate(approved=True, corrupt_notice=True)
        with self.assertRaisesRegex(RuntimeError, "notice differs"):
            publisher.prepare_assets(self.candidate, self.release, RUN, ATTEMPT)

    def test_approved_fixture_exports_exact_notice_without_authorizing_publication(self):
        self.beta_candidate(approved=True)
        _, assets = publisher.prepare_assets(self.candidate, self.release, RUN, ATTEMPT)
        self.assertIn("BETA_LIFECYCLE.txt", assets)
        self.assertIn("chimera-beta-lifecycle.json", assets)
        self.assertIn("2098-01-01T00:00:00Z", assets["BETA_LIFECYCLE.txt"].read_text(encoding="utf-8"))
        self.assertFalse(self.policy["release_scope"]["release_approved"])


class CIContracts(unittest.TestCase):
    def setUp(self):
        self.release = publisher.Release("1.3.1-beta.2", "a" * 40)
        self.runs, self.jobs = {}, {}
        for index, (workflow, expected) in enumerate(publisher.WORKFLOWS.items()):
            run_id = RUN + index
            self.runs[workflow] = [dict(id=run_id, run_attempt=ATTEMPT, head_sha=self.release.source_sha,
                event="pull_request", status="completed", conclusion="success",
                repository={"full_name": publisher.transport.REPO},
                head_repository={"full_name": publisher.transport.REPO})]
            self.jobs[run_id] = [dict(name=name, status="completed", conclusion="success") for name in expected]
        self.artifacts = [dict(id=index + 1, name=name, expired=False, size_in_bytes=123,
            workflow_run={"id": RUN, "head_sha": self.release.source_sha}, digest="sha256:" + "b" * 64)
            for index, name in enumerate((self.release.artifact_name, publisher.GATE_ARTIFACT_NAME))]
        self.api = Mock()
        self.api.pages.side_effect = self.pages

    def pages(self, path, key):
        if key == "workflow_runs":
            return self.runs[path.split("/")[3]]
        if key == "jobs":
            return self.jobs[int(path.split("/")[3])]
        if key == "artifacts":
            return self.artifacts
        raise AssertionError(path)

    def test_exact_workflows_and_three_platform_jobs_remain_required(self):
        self.assertEqual(set(publisher.WORKFLOWS), {"build.yml", "update-candidate.yml", "cab-panel.yml",
            "cab-realtime.yml", "amp-head-controls.yml", "signal-path.yml", "graphical-eq.yml", "release-publisher.yml"})
        self.assertEqual(publisher.verify_ci(self.api, self.release)["candidate"]["name"], self.release.artifact_name)
        for workflow, jobs in self.jobs.items():
            for index, job in enumerate(jobs):
                with self.subTest(workflow=workflow, job=job["name"]):
                    self.jobs[workflow] = jobs[:index] + jobs[index + 1:]
                    with self.assertRaises(RuntimeError):
                        publisher.verify_ci(self.api, self.release)
            self.jobs[workflow] = jobs

    def test_new_failed_pending_cancelled_run_never_falls_back_to_success(self):
        good = self.runs["build.yml"][0]
        for status, conclusion, error in (("completed", "failure", RuntimeError),
                ("completed", "cancelled", RuntimeError), ("in_progress", None, publisher.CIPending)):
            with self.subTest(status=status, conclusion=conclusion):
                self.runs["build.yml"] = [good, {**good, "id": 999, "status": status, "conclusion": conclusion}]
                with self.assertRaises(error):
                    publisher.verify_ci(self.api, self.release)

    def test_wrong_head_repository_and_missing_attempt_are_rejected(self):
        good = self.runs["build.yml"][0]
        for change in ({"head_repository": {"full_name": "fork/repo"}}, {"run_attempt": None},
                       {"head_sha": "b" * 40}):
            with self.subTest(change=change):
                self.runs["build.yml"] = [{**good, **change}]
                with self.assertRaises(RuntimeError):
                    publisher.verify_ci(self.api, self.release)

    def test_artifact_wrong_version_run_digest_expiry_and_duplicates_are_rejected(self):
        good = copy.deepcopy(self.artifacts)
        for change in ({"name": "SpectralForge-Chimera-1.3.1-beta.1-Release-Candidate"},
                       {"workflow_run": {"id": RUN + 1, "head_sha": self.release.source_sha}},
                       {"digest": ""}, {"size_in_bytes": 0}, {"expired": True}):
            with self.subTest(change=change):
                self.artifacts = [{**good[0], **change}, good[1]]
                with self.assertRaises(RuntimeError):
                    publisher.verify_ci(self.api, self.release)
        self.artifacts = good + [good[0]]
        with self.assertRaises(RuntimeError):
            publisher.verify_ci(self.api, self.release)


class InvocationContracts(SourceFixture):
    def setUp(self):
        super().setUp()
        self.receipt = publisher.read_json(self.root / publisher.APPROVAL_PATH)
        self.receipt.update(version=self.release.version, product_version=self.release.product_version,
            profile=self.release.profile, approved_product_source_sha=self.source,
            strict_policy_sha256=publisher.weekly_publication.digest(self.policy),
            ci_check_map_sha256=publisher.weekly_publication.digest(
                publisher.read_json(self.root / "Validation/ci-check-map.json")),
            release_approved=True, publication_authorized=True)
        self.receipt["owner_acceptance"].update(approved=True, publication_authorized=True,
            validation_gaps_accepted=False, instruction="Synthetic fixture approval only",
            context="Unit test; this is not real product approval", scope=self.release.version + " / " + self.source)
        self.write_json(self.root / publisher.APPROVAL_PATH, self.receipt)
        self.controller = self.commit("synthetic approval receipt")
        self.proof = publisher.verify_controller_source(self.source, self.controller)
        self.environment = dict(GITHUB_SHA=self.controller, GITHUB_REPOSITORY=publisher.transport.REPO,
            GITHUB_EVENT_NAME="workflow_dispatch", GITHUB_REF="refs/heads/trunk", GITHUB_RUN_ID="900")
        self.api = Mock()
        self.api.json.side_effect = self.response
        self.api.pages.return_value = []
        self.args = SimpleNamespace(confirm=self.release.confirm, source_sha=self.source,
            candidate_dir=None, candidate_zip=None, artifact_json=None, gate_archive=None,
            run_id=None, run_attempt=None, wait_ci_seconds=0)

    def response(self, path, **kwargs):
        self.assertNotIn("method", kwargs, "Invocation preflight must not mutate GitHub")
        if path == "":
            return dict(default_branch="trunk", full_name=publisher.transport.REPO)
        if path == "/actions/runs/900":
            return dict(head_sha=self.controller, head_branch="trunk", event="workflow_dispatch",
                        path=publisher.WORKFLOW_PATH)
        if path == "/git/ref/heads/trunk":
            return {"object": {"sha": self.controller}}
        raise AssertionError(path)

    def test_default_branch_is_resolved_from_repository_not_a_release_name(self):
        with patch.dict(publisher.os.environ, self.environment), \
             patch.object(publisher.transport, "GitHub", return_value=self.api), \
             patch.object(publisher, "verify_ci", side_effect=RuntimeError("missing exact CI")) as ci, \
             patch.object(publisher.transport, "publish") as publish:
            with self.assertRaisesRegex(RuntimeError, "missing exact CI"):
                publisher.publish(self.args, self.release, self.policy, self.proof)
        ci.assert_called_once_with(self.api, self.release)
        publish.assert_not_called()

    def test_consumed_reservation_stops_before_ci_downloads_or_publication(self):
        self.api.pages.return_value = [{"ref": "refs/tags/" + self.release.tag}]
        def inventory(path):
            return [{"ref": "refs/tags/" + self.release.tag}] if path == "/git/refs/tags" else []
        self.api.pages.side_effect = inventory
        with patch.dict(publisher.os.environ, self.environment), \
             patch.object(publisher.transport, "GitHub", return_value=self.api), \
             patch.object(publisher, "verify_ci") as ci, patch.object(publisher.transport, "publish") as publish:
            with self.assertRaisesRegex(ValueError, "already consumed"):
                publisher.publish(self.args, self.release, self.policy, self.proof)
        ci.assert_not_called()
        self.api.download.assert_not_called()
        publish.assert_not_called()

    def test_feature_branch_and_wrong_workflow_cannot_reach_ci_or_mutation(self):
        for bad in ({"GITHUB_REF": "refs/heads/release/open-beta-1.3.1"},
                    {"GITHUB_REF": "refs/heads/arbitrary"}, {"GITHUB_EVENT_NAME": "push"},
                    {"GITHUB_SHA": "b" * 40}, {"GITHUB_REPOSITORY": "fork/repo"}):
            with self.subTest(bad=bad), patch.dict(publisher.os.environ, {**self.environment, **bad}), \
                 patch.object(publisher.transport, "GitHub", return_value=self.api), \
                 patch.object(publisher, "verify_ci") as ci, patch.object(publisher.transport, "publish") as publish:
                with self.assertRaises(RuntimeError):
                    publisher.publish(self.args, self.release, self.policy, self.proof)
                ci.assert_not_called()
                publish.assert_not_called()
        def wrong_workflow(path, **kwargs):
            answer = self.response(path, **kwargs)
            return {**answer, "path": ".github/workflows/build.yml"} if path == "/actions/runs/900" else answer
        self.api.json.side_effect = wrong_workflow
        with patch.dict(publisher.os.environ, self.environment), \
             patch.object(publisher.transport, "GitHub", return_value=self.api), \
             patch.object(publisher, "verify_ci") as ci:
            with self.assertRaisesRegex(RuntimeError, "workflow identity"):
                publisher.publish(self.args, self.release, self.policy, self.proof)
        ci.assert_not_called()

    def test_local_evidence_override_is_never_a_publication_input(self):
        for field in ("candidate_dir", "candidate_zip", "artifact_json", "gate_archive", "run_id", "run_attempt"):
            with self.subTest(field=field):
                setattr(self.args, field, 1)
                with patch.object(publisher.transport, "GitHub") as api, self.assertRaisesRegex(RuntimeError, "local override"):
                    publisher.publish(self.args, self.release, self.policy, self.proof)
                api.assert_not_called()
                setattr(self.args, field, None)

    def test_ci_rerun_after_staging_blocks_final_publication_callback(self):
        ci = dict(runs={"build.yml": dict(id=RUN, run_attempt=ATTEMPT, head_sha=self.source)},
            candidate=dict(id=1, name=self.release.artifact_name, digest="sha256:" + "a" * 64, size_in_bytes=10),
            gate=dict(id=2, name=publisher.GATE_ARTIFACT_NAME, digest="sha256:" + "b" * 64, size_in_bytes=10))
        changed = copy.deepcopy(ci)
        changed["runs"]["build.yml"]["run_attempt"] += 1
        outcome = []
        def staged(api, notes, assets, **kwargs):
            outcome.append("staged")
            kwargs["before_publish"]()
            outcome.append("public")
            return {"html_url": self.release.release_url}
        with patch.dict(publisher.os.environ, self.environment), \
             patch.object(publisher.transport, "GitHub", return_value=self.api), \
             patch.object(publisher, "wait_for_ci", return_value=ci), \
             patch.object(publisher, "verify_ci", side_effect=[ci, changed]), \
             patch.object(publisher, "check_archive"), \
             patch.object(publisher, "verify_release_gate_archive", return_value=({}, {"publication_allowed": True})), \
             patch.object(publisher.transport, "extract_candidate"), \
             patch.object(publisher, "prepare_assets", return_value=(self.notes, {})), \
             patch.object(publisher.weekly_publication, "render_markdown", return_value="fixture disclosure"), \
             patch.object(publisher.transport, "publish", side_effect=staged):
            with self.assertRaisesRegex(RuntimeError, "runs or artifacts changed"):
                publisher.publish(self.args, self.release, self.policy, self.proof)
        self.assertEqual(outcome, ["staged"])


    def assert_calendar_change_after_staging_blocks_publication(self, change, message):
        ci = dict(runs={"build.yml": dict(id=RUN, run_attempt=ATTEMPT, head_sha=self.source)},
            candidate=dict(id=1, name=self.release.artifact_name, digest="sha256:" + "a" * 64, size_in_bytes=10),
            gate=dict(id=2, name=publisher.GATE_ARTIFACT_NAME, digest="sha256:" + "b" * 64, size_in_bytes=10))
        outcome = []
        def staged(api, notes, assets, **kwargs):
            outcome.append("staged")
            change()
            kwargs["before_publish"]()
            outcome.append("public")
            return {"html_url": self.release.release_url}
        with patch.dict(publisher.os.environ, self.environment), \
             patch.object(publisher.transport, "GitHub", return_value=self.api), \
             patch.object(publisher, "wait_for_ci", return_value=ci), \
             patch.object(publisher, "verify_ci", return_value=ci), \
             patch.object(publisher, "check_archive"), \
             patch.object(publisher, "verify_release_gate_archive", return_value=({}, {"publication_allowed": True})), \
             patch.object(publisher.transport, "extract_candidate"), \
             patch.object(publisher, "prepare_assets", return_value=(self.notes, {})), \
             patch.object(publisher.weekly_publication, "render_markdown", return_value="fixture disclosure"), \
             patch.object(publisher.transport, "publish", side_effect=staged):
            with self.assertRaisesRegex(ValueError, message):
                publisher.publish(self.args, self.release, self.policy, self.proof)
        self.assertEqual(outcome, ["staged"])

    def test_consumed_tag_after_staging_blocks_final_publication_callback(self):
        def change():
            self.api.pages.side_effect = lambda path: ([{"ref": "refs/tags/" + self.release.tag}]
                if path == "/git/refs/tags" else [])
        self.assert_calendar_change_after_staging_blocks_publication(change, 'already consumed')

    def test_higher_release_after_staging_blocks_final_publication_callback(self):
        def change():
            # Public release only: a racing tag inventory can omit its tag.
            self.api.pages.side_effect = lambda path: ([{'tag_name': 'v1.3.2-beta.1', 'draft': False}]
                if path == '/releases' else [])
        self.assert_calendar_change_after_staging_blocks_publication(change, 'does not advance')

    def test_kst_midnight_after_staging_blocks_final_publication_callback(self):
        def change():
            publisher.check_release_calendar.kst_today.return_value = date(2026, 10, 17)
        self.assert_calendar_change_after_staging_blocks_publication(change, 'Missed publication target')

    def test_elapsed_target_blocks_before_ci_or_remote_staging(self):
        with patch.dict(publisher.os.environ, self.environment), \
             patch.object(publisher.transport, 'GitHub', return_value=self.api), \
             patch.object(publisher.check_release_calendar, 'kst_today', return_value=date(2026, 10, 17)), \
             patch.object(publisher, 'wait_for_ci') as ci, \
             patch.object(publisher.transport, 'publish') as publish:
            with self.assertRaisesRegex(ValueError, 'Missed publication target'):
                publisher.publish(self.args, self.release, self.policy, self.proof)
        ci.assert_not_called()
        publish.assert_not_called()


class GateContracts(SourceFixture):
    def setUp(self):
        super().setUp()
        self.source = fixtures.HEAD
        self.release = publisher.Release(self.release.version, self.source)
        mapping = publisher.read_json(self.root / "Validation/ci-check-map.json")["checks"]
        self.fixture = fixtures.Consolidation("test_actual_automatic_receipts_pass_only_owned_checks")
        with patch.object(evidence, "configuration", return_value=(self.policy, mapping)):
            self.fixture.setUp()
            self.addCleanup(self.fixture.doCleanups)
            self.verdict, self.output = self.fixture.run_gate()
        self.checks = publisher.gate.read_checks(self.output / "checks")
        self.archive = self.work / "gate.zip"

    def write_archive(self):
        with zipfile.ZipFile(self.archive, "w") as archive:
            archive.writestr("release-gate.json", json.dumps(self.verdict))
            for cid, check in self.checks.items():
                archive.writestr("checks/" + cid + ".json", json.dumps(check))
            for path in (self.output / "evidence").rglob("*"):
                if path.is_file():
                    archive.write(path, path.relative_to(self.output).as_posix())

    def verify(self):
        self.write_archive()
        return publisher.verify_release_gate_archive(self.archive, self.release, RUN, ATTEMPT, self.policy)

    def test_complete_strict_archive_is_recomputed_without_granting_publication(self):
        verdict, decision = self.verify()
        self.assertEqual(verdict["verdict"], "BLOCKED")
        self.assertEqual(verdict["counts"]["checks"]["pass"], 21)
        self.assertIsNone(decision)

    def test_summary_only_missing_check_and_invented_pass_are_rejected(self):
        self.checks.pop(next(iter(self.checks)))
        with self.assertRaisesRegex(RuntimeError, "inventory"):
            self.verify()

    def test_failed_underlying_check_cannot_be_hidden_by_pass_summary(self):
        self.checks["A12.01"]["assertion"]["passed"] = False
        with self.assertRaisesRegex(RuntimeError, "summary differs"):
            self.verify()

    def test_stale_run_attempt_wrong_source_and_platform_gaps_are_rejected(self):
        original = copy.deepcopy(self.verdict)
        for mutation in (lambda doc: doc["revision"].update(run_attempt="1"),
                         lambda doc: doc["revision"].update(commit_sha="b" * 40),
                         lambda doc: doc["producer"]["inputs"].pop(),
                         lambda doc: doc["producer"].update(errors=["missing platform"])):
            self.verdict = copy.deepcopy(original)
            mutation(self.verdict)
            with self.assertRaises(RuntimeError):
                self.verify()


class TransportContracts(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.release = publisher.Release("1.3.1-beta.2", "a" * 40)
        transport = publisher.transport
        values = {name: getattr(transport, name) for name in
            ("VERSION", "TAG", "HEAD", "TITLE", "RELEASE_URL", "DOWNLOAD_URL")}
        patcher = patch.multiple(transport, **values)
        patcher.start()
        self.addCleanup(patcher.stop)
        publisher.configure_transport(self.release)
        self.path = self.root / "update-beta.json"
        self.path.write_text('{}', encoding="utf-8")
        self.assets = {self.path.name: self.path}
        self.remote_assets, self.mutations = [], []
        self.api = Mock()
        self.api.optional.return_value = None
        self.api.pages.side_effect = lambda path, **kwargs: [] if path == "/releases" else self.remote_assets
        self.api.json.side_effect = self.mutate

    def mutate(self, path, **kwargs):
        self.mutations.append((path, kwargs.get("method"), kwargs.get("payload")))
        if path == "/releases":
            return dict(id=7, draft=True, upload_url=f"https://uploads.github.com/repos/{publisher.transport.REPO}/releases/7/assets{{?name}}")
        if path.startswith("https://uploads.github.com/"):
            asset = dict(id=8, name=self.path.name, size=self.path.stat().st_size,
                         state="uploaded", digest="sha256:" + publisher.sha256(self.path))
            self.remote_assets.append(asset)
            return asset
        if kwargs.get("method") == "PATCH":
            return dict(id=7)
        raise AssertionError(path)

    def test_changed_approval_or_ci_after_upload_never_executes_public_patch(self):
        def revoked():
            self.assertTrue(self.remote_assets, "Approval must be rechecked after uploads")
            raise RuntimeError("Verification or source approval revoked")
        with patch.object(publisher.transport, "verify_tag"), \
             patch.object(publisher.transport, "public_check") as public, \
             redirect_stdout(io.StringIO()), self.assertRaisesRegex(RuntimeError, "revoked"):
            publisher.transport.publish(self.api, "notes", self.assets, before_publish=revoked)
        self.assertTrue(self.remote_assets)
        self.assertFalse(any(method == "PATCH" for _, method, _ in self.mutations))
        public.assert_not_called()

    def test_successful_callback_precedes_public_patch_and_stable_is_not_prerelease(self):
        order = []
        def mutation(path, **kwargs):
            if kwargs.get("method") == "PATCH":
                order.append("public")
            return self.mutate(path, **kwargs)
        self.api.json.side_effect = mutation
        with patch.object(publisher.transport, "verify_tag"), \
             patch.object(publisher.transport, "public_check", return_value={"id": 7}) as public, \
             redirect_stdout(io.StringIO()):
            publisher.transport.publish(self.api, "notes", self.assets, prerelease=False,
                before_publish=lambda: order.append("revalidated"))
        self.assertEqual(order, ["revalidated", "public"])
        self.assertTrue(all(payload["prerelease"] is False
            for _, method, payload in self.mutations if method == "PATCH" or payload))
        self.assertFalse(public.call_args.kwargs["prerelease"])

    def test_existing_tag_for_different_source_is_never_moved_or_overwritten(self):
        self.api.optional.return_value = {"object": {"type": "commit", "sha": "b" * 40}}
        with self.assertRaisesRegex(RuntimeError, "refusing to move"):
            publisher.transport.publish(self.api, "notes", self.assets)
        self.api.json.assert_not_called()

    def test_tag_changed_during_final_ci_recheck_never_becomes_public(self):
        def move_tag():
            self.api.optional.return_value = {"object": {"type": "commit", "sha": "b" * 40}}
        with patch.object(publisher.transport, "public_check") as public, \
             redirect_stdout(io.StringIO()), self.assertRaisesRegex(RuntimeError, "refusing to move"):
            publisher.transport.publish(self.api, "notes", self.assets, before_publish=move_tag)
        self.assertTrue(self.remote_assets)
        self.assertFalse(any(method == "PATCH" for _, method, _ in self.mutations))
        public.assert_not_called()

    def test_existing_public_metadata_cannot_be_replaced(self):
        self.api.pages.side_effect = lambda path, **kwargs: [dict(id=7, tag_name=self.release.tag,
            name="different title", body="notes", prerelease=True, draft=False)]
        with self.assertRaisesRegex(RuntimeError, "refusing to overwrite"):
            publisher.transport.publish(self.api, "notes", self.assets)
        self.api.json.assert_not_called()


if __name__ == "__main__":
    unittest.main()
