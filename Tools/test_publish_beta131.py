#!/usr/bin/env python3
"""Negative publication contracts and offline candidate review; no network writes."""
import copy
import io
import json
from pathlib import Path
import subprocess
import tempfile
import unittest
import zipfile
from contextlib import redirect_stdout
from unittest.mock import Mock, call, patch

import publish_beta131 as publisher

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
        (self.root / "VERSION").write_text("1.3.1\n", encoding="utf-8")
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
        for version, channel in (("1.2.0", "beta.1"), ("1.3.0", "beta.1"),
                                 ("1.3.1", "preview"), ("1.3.1", "beta.2")):
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
        with self.assertRaisesRegex(RuntimeError, "Private model capture"):
            publisher.prepare_assets(self.candidate, HEAD, RUN)

    def test_reference_audio_folder_cannot_bypass_public_payload_policy(self):
        with zipfile.ZipFile(self.candidate / (self.prefix + "win64.zip"), "a") as archive:
            archive.writestr("reference-audio/third-party.wav", b"private audio")
        self.refresh_sums()
        with self.assertRaises(RuntimeError):
            publisher.prepare_assets(self.candidate, HEAD, RUN)

    def test_renamed_wave_cannot_publish_even_with_current_checksums(self):
        with zipfile.ZipFile(self.candidate / (self.prefix + "win64.zip"), "a") as archive:
            archive.writestr("Resources/capture.dat", b"RIFF\x24\x00\x00\x00WAVEdata")
        self.refresh_sums()
        with self.assertRaises(RuntimeError):
            publisher.prepare_assets(self.candidate, HEAD, RUN)

    def test_packaged_notes_must_match_reviewed_source(self):
        (self.candidate / "OPEN_BETA_RELEASE_NOTES.md").write_text("Older release notes", encoding="utf-8")
        with self.assertRaisesRegex(RuntimeError, "notes differ"):
            publisher.prepare_assets(self.candidate, HEAD, RUN)


class ControllerSourceBinding(unittest.TestCase):
    """Exercise real Git trees/index/worktrees, not mocked diff output."""

    CONTROL_FILES = (
        ".github/workflows/publish-beta131.yml", "Tools/publish_beta131.py",
        "Tools/test_publish_beta131.py", "Tools/beta_publication131.py",
        "Tools/test_beta_publication131.py", "Validation/beta-publication-policy-1.3.1.json",
        "docs/RELEASE_1_3_1_PIPELINE.md",
    )
    PRODUCT_FILES = (
        "Source/PluginProcessor.cpp", "Assets/Artwork/Cab/cab-guitar-412.png",
        "VERSION", "RELEASE_CHANNEL", "Validation/release-policy.json",
        "Validation/waivers.json", "Validation/ci-check-map.json",
        ".github/workflows/build.yml", ".github/workflows/update-candidate.yml",
        ".github/workflows/cab-panel.yml", "docs/MANUAL.html",
        "docs/OPEN_BETA_RELEASE_NOTES.md", "Tools/publish_open_beta.py",
    )

    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        self.git("init", "--quiet")
        self.git("config", "user.name", "Publication binding fixture")
        self.git("config", "user.email", "publication-fixture@example.invalid")
        self.git("config", "commit.gpgsign", "false")
        self.git("config", "core.autocrlf", "false")
        for name in (*self.PRODUCT_FILES, "Tools/publish_beta131.py",
                     "docs/RELEASE_1_3_1_PIPELINE.md"):
            self.write(name, "reviewed product bytes\n")
        self.product = self.commit("tested product")
        self.write("Tools/publish_beta131.py", "publication controller only\n")
        self.controller = self.commit("publication-only descendant")
        patcher = patch.object(publisher, "ROOT", self.root)
        patcher.start()
        self.addCleanup(patcher.stop)

    def git(self, *args):
        return subprocess.check_output(["git", *args], cwd=self.root,
                                       text=True, stderr=subprocess.STDOUT).strip()

    def write(self, name, text):
        target = self.root / name
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_text(text, encoding="utf-8")

    def commit(self, message):
        self.git("add", "--all")
        self.git("commit", "--quiet", "-m", message)
        return self.git("rev-parse", "HEAD")

    def test_accepts_only_the_seven_publication_paths_in_a_real_descendant(self):
        # Keep this independent inventory literal: accidentally widening the
        # implementation's allowlist must not silently widen the contract.
        self.assertEqual(publisher.CONTROL_PATHS, frozenset(self.CONTROL_FILES))
        for name in self.CONTROL_FILES:
            self.write(name, "reviewed publication-only change: " + name + "\n")
        controller = self.commit("all seven publication-only files")
        proof = publisher.verify_controller_source(self.product, controller)
        self.assertEqual(proof, {
            "product_source_sha": self.product,
            "publication_controller_sha": controller,
            "controller_changed_paths": sorted(self.CONTROL_FILES),
        })
        self.assertNotEqual(self.product, controller)

    def test_rejects_product_policy_build_workflow_and_packaged_document_changes(self):
        for name in self.PRODUCT_FILES:
            with self.subTest(path=name):
                self.git("reset", "--hard", self.controller)
                self.write(name, "unverified product change\n")
                changed = self.commit("forbidden change: " + name)
                with self.assertRaisesRegex(RuntimeError, "Untested product change"):
                    publisher.verify_controller_source(self.product, changed)

    def test_rejects_a_checkout_other_than_the_requested_controller(self):
        self.git("checkout", "--quiet", "--detach", self.product)
        with self.assertRaisesRegex(RuntimeError, "Checked-out publication controller differs"):
            publisher.verify_controller_source(self.product, self.controller)

    def test_rejects_a_source_from_a_sibling_history(self):
        self.git("checkout", "--quiet", "--detach", self.product)
        self.write("Tools/publish_beta131.py", "different publication sibling\n")
        sibling = self.commit("sibling source is not an ancestor")
        self.git("checkout", "--quiet", "--detach", self.controller)
        with self.assertRaises(subprocess.CalledProcessError):
            publisher.verify_controller_source(sibling, self.controller)

    def test_rejects_staged_and_unstaged_controller_mutations(self):
        for staged in (False, True):
            with self.subTest(staged=staged):
                self.git("reset", "--hard", self.controller)
                self.write("Tools/publish_beta131.py", "uncommitted controller mutation\n")
                if staged:
                    self.git("add", "Tools/publish_beta131.py")
                with self.assertRaisesRegex(RuntimeError, "local changes"):
                    publisher.verify_controller_source(self.product, self.controller)

    def test_rejects_deleting_or_renaming_even_an_allowed_file(self):
        for rename in (False, True):
            with self.subTest(rename=rename):
                self.git("reset", "--hard", self.controller)
                source = self.root / "Tools/publish_beta131.py"
                if rename:
                    source.rename(self.root / "Tools/beta_publication131.py")
                else:
                    source.unlink()
                changed = self.commit("removed an allowed controller file")
                with self.assertRaisesRegex(RuntimeError, "Untested product change"):
                    publisher.verify_controller_source(self.product, changed)

    def test_rejects_a_committed_symlink_mode_on_an_allowed_path(self):
        # core.symlinks=false represents a symlink as a normal worktree file,
        # so this checks the actual 120000 Git tree mode on Windows too, without
        # requiring OS permission to create symlinks or mocking ls-tree.
        self.git("config", "core.symlinks", "false")
        name = "Tools/beta_publication131.py"
        self.write(name, "../Source/PluginProcessor.cpp")
        blob = self.git("hash-object", "-w", name)
        self.git("update-index", "--add", "--cacheinfo", "120000," + blob + "," + name)
        self.git("commit", "--quiet", "-m", "symlink-mode controller")
        controller = self.git("rev-parse", "HEAD")
        self.assertEqual(self.git("status", "--porcelain", "--untracked-files=no"), "")
        with self.assertRaisesRegex(RuntimeError, "Unexpected publication controller file mode"):
            publisher.verify_controller_source(self.product, controller)


class CIContracts(unittest.TestCase):
    def good_run(self, run_id=RUN):
        return dict(id=run_id, run_attempt=1, head_sha=HEAD, event="pull_request",
                    status="completed", conclusion="success", repository={"full_name": publisher.transport.REPO})

    def test_stale_missing_failed_pending_or_newer_failed_runs_are_rejected(self):
        good = self.good_run()
        self.assertEqual(publisher.verified_run([good], HEAD)["id"], RUN)
        for status in ("queued", "in_progress", "waiting", "pending", "requested"):
            with self.subTest(pending=status), self.assertRaises(publisher.CIPending):
                publisher.verified_run([good, {**good, "id": RUN + 1, "status": status,
                                              "conclusion": None}], HEAD)
        for changes in ({"head_sha": "b" * 40}, {"event": "push"},
                        {"conclusion": "failure"}, {"conclusion": "cancelled"}, {"run_attempt": None}):
            with self.subTest(changes=changes), self.assertRaises(RuntimeError) as caught:
                publisher.verified_run([{**good, **changes}], HEAD)
            self.assertNotIsInstance(caught.exception, publisher.CIPending)
        with self.assertRaisesRegex(RuntimeError, "missing"):
            publisher.verified_run([], HEAD)
        with self.assertRaisesRegex(RuntimeError, "Latest"):
            publisher.verified_run([good, {**good, "id": RUN + 1, "conclusion": "failure"}], HEAD)

    def test_pending_build_does_not_hide_a_later_failed_workflow(self):
        api = Mock()
        pending = {**self.good_run(), "status": "in_progress", "conclusion": None}
        failed = {**self.good_run(RUN + 1), "conclusion": "failure"}
        api.pages.side_effect = [[pending], [failed]]
        with self.assertRaisesRegex(RuntimeError, "has not passed") as caught:
            publisher.verify_ci(api, HEAD)
        self.assertNotIsInstance(caught.exception, publisher.CIPending)
        requests = [call(f"/actions/workflows/{name}/runs?head_sha={HEAD}", "workflow_runs")
                    for name in publisher.WORKFLOWS]
        self.assertEqual(api.pages.call_args_list, requests[:2])

        api.reset_mock()
        api.pages.side_effect = [[{**pending, "id": RUN + index}] for index in range(len(publisher.WORKFLOWS))]
        with self.assertRaises(publisher.CIPending) as caught:
            publisher.verify_ci(api, HEAD)
        self.assertEqual(api.pages.call_args_list, requests)
        for name in publisher.WORKFLOWS:
            self.assertIn(name, str(caught.exception))

    def test_every_cab_platform_and_consolidator_must_succeed(self):
        for workflow in publisher.WORKFLOWS:
            expected = publisher.WORKFLOWS[workflow]
            good = [dict(name=name, status="completed", conclusion="success") for name in expected]
            publisher.verified_jobs(good, expected)
            for index in range(len(good)):
                with self.subTest(workflow=workflow, missing=good[index]["name"]), self.assertRaises(RuntimeError):
                    publisher.verified_jobs(good[:index] + good[index + 1:], expected)
            with self.assertRaises(RuntimeError):
                publisher.verified_jobs(good + [good[0]], expected)

    def test_131_release_requires_all_new_feature_and_shipped_cab_workflows(self):
        # Independent release contract: dropping a feature workflow or a
        # platform must fail even if all remaining jobs happen to pass.
        self.assertEqual(set(publisher.WORKFLOWS), {
            "build.yml", "update-candidate.yml", "cab-panel.yml",
            "cab-realtime.yml", "amp-head-controls.yml", "signal-path.yml",
            "graphical-eq.yml",
        })
        for name in ("amp-head-controls.yml", "signal-path.yml"):
            self.assertEqual(set(publisher.WORKFLOWS[name]), {
                "native-navigation (windows-latest)",
                "native-navigation (ubuntu-latest)",
                "native-navigation (macos-latest)",
            })
        for name, prefix in (("cab-panel.yml", "cab-panel"),
                             ("cab-realtime.yml", "realtime"),
                             ("graphical-eq.yml", "graphical-eq")):
            self.assertEqual(set(publisher.WORKFLOWS[name]), {
                prefix + " (windows-latest)", prefix + " (ubuntu-22.04)",
                prefix + " (macos-15)",
            })

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
        workflow = (REAL_ROOT / ".github/workflows/publish-beta131.yml").read_text(encoding="utf-8")
        self.assertIn("workflow_dispatch:", workflow)
        self.assertNotIn("\n  push:", workflow)
        self.assertIn("default: false", workflow)
        self.assertIn("--dry-run", workflow)
        self.assertIn("release/open-beta-1.3.1", workflow)
        self.assertIn("publish-v1.3.1-beta.1", workflow)


class CIWaitContracts(unittest.TestCase):
    def test_pending_then_success_rechecks_branch_before_each_verification(self):
        api, result = object(), {"verified": "exact source"}
        calls = Mock()
        calls.verify.side_effect = [publisher.CIPending("active run"), result]
        with patch.object(publisher, "verify_ci", calls.verify), \
             patch.object(publisher.time, "monotonic", return_value=100), \
             patch.object(publisher.time, "sleep") as sleep, redirect_stdout(io.StringIO()):
            self.assertIs(publisher.wait_for_ci(api, HEAD, 60, calls.branch), result)
        self.assertEqual(calls.mock_calls, [call.branch(), call.verify(api, HEAD),
                                           call.branch(), call.verify(api, HEAD)])
        sleep.assert_called_once_with(30)

    def test_terminal_failure_or_missing_evidence_is_never_retried(self):
        for message in ("Latest exact-source verification run has not passed",
                        "Exact-source verification run is missing",
                        "Expected one exact-source artifact: release gate"):
            with self.subTest(error=message):
                error, branch, api = RuntimeError(message), Mock(), object()
                with patch.object(publisher, "verify_ci", side_effect=error) as verify, \
                     patch.object(publisher.time, "monotonic", return_value=100), \
                     patch.object(publisher.time, "sleep") as sleep, \
                     self.assertRaises(RuntimeError) as caught:
                    publisher.wait_for_ci(api, HEAD, 60, branch)
                self.assertIs(caught.exception, error)
                verify.assert_called_once_with(api, HEAD)
                branch.assert_called_once_with()
                sleep.assert_not_called()

    def test_zero_and_expired_waits_stop_without_exceeding_the_bound(self):
        for seconds, clock, attempts, sleeps in ((0, [100, 100], 1, []),
                                                 (5, [100, 100, 105], 2, [call(5)])):
            with self.subTest(seconds=seconds):
                branch = Mock()
                with patch.object(publisher, "verify_ci", side_effect=publisher.CIPending("active")) as verify, \
                     patch.object(publisher.time, "monotonic", side_effect=clock), \
                     patch.object(publisher.time, "sleep") as sleep, \
                     redirect_stdout(io.StringIO()), self.assertRaises(publisher.CIPending):
                    publisher.wait_for_ci(object(), HEAD, seconds, branch)
                self.assertEqual(verify.call_count, attempts)
                self.assertEqual(branch.call_count, attempts)
                self.assertEqual(sleep.call_args_list, sleeps)
        for seconds in (-1, 5401, True, 1.5):
            with self.subTest(invalid_seconds=seconds), \
                 patch.object(publisher, "verify_ci") as verify, \
                 patch.object(publisher.time, "monotonic") as clock, \
                 patch.object(publisher.time, "sleep") as sleep:
                branch = Mock()
                with self.assertRaisesRegex(RuntimeError, "Invalid CI wait duration"):
                    publisher.wait_for_ci(object(), HEAD, seconds, branch)
                verify.assert_not_called()
                branch.assert_not_called()
                clock.assert_not_called()
                sleep.assert_not_called()

    def test_superseded_branch_stops_before_the_next_ci_request(self):
        error = RuntimeError("A newer publication controller supersedes this invocation")
        branch = Mock(side_effect=[None, error])
        api = object()
        with patch.object(publisher, "verify_ci", side_effect=publisher.CIPending("active")) as verify, \
             patch.object(publisher.time, "monotonic", return_value=100), \
             patch.object(publisher.time, "sleep") as sleep, \
             redirect_stdout(io.StringIO()), self.assertRaises(RuntimeError) as caught:
            publisher.wait_for_ci(api, HEAD, 10, branch)
        self.assertIs(caught.exception, error)
        self.assertEqual(branch.call_count, 2)
        verify.assert_called_once_with(api, HEAD)
        sleep.assert_called_once_with(10)


if __name__ == "__main__":
    unittest.main()
