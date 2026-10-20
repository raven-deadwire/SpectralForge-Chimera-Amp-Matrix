#!/usr/bin/env python3
"""Offline safety regressions for preset maintenance; never contacts GitHub."""
from __future__ import annotations

import copy
from contextlib import redirect_stdout
import io
import json
from pathlib import Path
import struct
import subprocess
import tempfile
import unittest
from unittest.mock import Mock, patch
import zipfile

import preset_maintenance as contract
import publish_preset_maintenance as publisher

REAL_ROOT = contract.ROOT
SOURCE = "a" * 40


def fixture_state(output_value="0", ordinal=0):
    output = "" if output_value is None else f'<PARAM id="output" value="{output_value}"/>'
    xml = (f'<PARAMS schemaVersion="11">{output}'
           f'<PARAM id="fixture" value="{ordinal}"/><USER_IRS/><IR_ASSETS/><MIDI_MAP/></PARAMS>').encode()
    return struct.pack("<II", 0x21324356, len(xml)) + xml + b"\0"


def fixture_pack(source=SOURCE, platform="linux", *, output_value="0"):
    rows, files = [], {}
    for ordinal in range(48):
        name = f"{ordinal + 1:02d}-Factory-{ordinal}.chimera"
        data = fixture_state(output_value, ordinal)
        files[name] = data
        rows.append(dict(ordinal=ordinal, name=f"Factory {ordinal}", category="Fixture", file=name,
            size=len(data), sha256=contract.digest_bytes(data), parameter_count=2,
            parameter_sha256=f"{ordinal:064x}", restore_max_normalized_error=0,
            output_db=0, fresh_restore_passed=True, metadata_restore_passed=True,
            tone_occupied=3, final_occupied=3, normalized_parameters=[ordinal / 47, .5]))
    return dict(schema="spectralforge.chimera.factory-preset-pack.v1", product_version=contract.BASE_VERSION,
        source_sha=source, platform=platform, beta_mode="approved", beta_expires_at_utc=contract.BASE_EXPIRY,
        preset_count=48, state_schema_version=11, fresh_processor_restore_passed=True, fresh_gui_metadata_restore_passed=True,
        personal_ir_included=False, personal_midi_mapping_included=False, actual_daw_acceptance=False,
        listening_acceptance=False, presets=rows, parameter_ids=["fixture", "output"]), files


class SourceScope(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.git("init", "--quiet")
        self.git("config", "user.name", "Preset maintenance fixture")
        self.git("config", "user.email", "fixture@example.invalid")
        self.git("config", "commit.gpgsign", "false")
        self.git("config", "core.autocrlf", "false")
        for name in ("Validation/release-policy.json", "Validation/ci-check-map.json", "Validation/waivers.json"):
            self.write(name, (REAL_ROOT / name).read_bytes())
        self.write(contract.OLD_APPROVAL_PATH, b'{"not_yet_approved":true}\n')
        self.write("VERSION", b"1.3.2\n")
        self.write("RELEASE_CHANNEL", b"beta.1\n")
        self.write("CMakeLists.txt", b"# Original product configuration\n")
        self.write("Source/Engine.h", b"// Original engine bytes\n")
        self.write("Validation/beta-lifecycle-policy.json", (REAL_ROOT / "Validation/beta-lifecycle-policy.json").read_bytes())
        self.write(".github/workflows/build.yml", b"# Original seven-workflow contract\n")
        for name in contract.WRAPPERS:
            self.write(name, ("// Original " + name + "\n").encode())
        self.base_source = self.commit("published product fixture")
        receipt = contract.read_json(REAL_ROOT / contract.OLD_APPROVAL_PATH)
        receipt["approved_product_source_sha"] = self.base_source
        self.write_json(contract.OLD_APPROVAL_PATH, receipt)
        self.base_controller = self.commit("original product acceptance")
        for key, value in (("BASE_SOURCE", self.base_source), ("BASE_CONTROLLER", self.base_controller)):
            patcher = patch.object(contract, key, value)
            patcher.start()
            self.addCleanup(patcher.stop)
        self.policy = contract.read_json(REAL_ROOT / contract.POLICY_PATH)
        self.policy.update(base_product_source_sha=self.base_source,
                           base_publication_controller_sha=self.base_controller,
                           approved_product_source_sha=None)
        for name in contract.ADDITIONS:
            self.write(name, b"# New maintenance support\n")
        self.write(contract.RECIPE_PATH, b"// Reviewed preset recipe\n")
        self.policy["preset_recipe_sha256"] = contract.sha256(self.root / contract.RECIPE_PATH)
        self.write_json(contract.POLICY_PATH, self.policy)
        self.write("CMakeLists.txt", (self.root / "CMakeLists.txt").read_bytes() + contract.CMAKE_BLOCK)
        for name, hook in contract.WRAPPERS.items():
            self.write(name, b'#include "FactoryPresetFinishing.h"\n'
                       + (self.root / name).read_bytes() + hook.encode())
        self.source = self.commit("tested presets and exporter")

    def git(self, *args):
        return subprocess.check_output(["git", *args], cwd=self.root, stderr=subprocess.STDOUT, text=True).strip()

    def write(self, name, data):
        path = self.root / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)

    def write_json(self, name, value):
        self.write(name, (json.dumps(value, ensure_ascii=False, indent=2) + "\n").encode())

    def commit(self, message):
        self.git("add", "--all")
        self.git("commit", "--quiet", "-m", message)
        return self.git("rev-parse", "HEAD")

    def test_only_preset_recipe_hooks_and_isolated_export_target_are_accepted(self):
        proof = contract.verify_source_scope(self.source, self.policy, root=self.root)
        self.assertTrue(proof["engine_source_bytes_unchanged_except_preset_finishing"])
        self.assertEqual(proof["product_source_sha"], self.source)

    def test_reviewed_preset_dependent_ui_fixture_headers_are_accepted(self):
        for name in ("Tests/SignalPathUITests.h", "Tests/CommonDynamicsUITests.h"):
            with self.subTest(name=name):
                self.git("reset", "--hard", self.source)
                self.write(name, b"// Reviewed preset-dependent UI fixture correction\n")
                changed = self.commit("reviewed UI fixture")
                proof = contract.verify_source_scope(changed, self.policy, root=self.root)
                self.assertIn(name, proof["changed_paths"])
                self.assertTrue(proof["engine_source_bytes_unchanged_except_preset_finishing"])

    def test_nearby_unreviewed_ui_headers_still_fail_closed(self):
        for name in ("Tests/GateUITests.h", "Tests/EQPendingInputTests.h",
                     "Tests/unreviewed/SignalPathUITests.h", "Tests/CommonDynamicsUITestsExtra.h"):
            with self.subTest(name=name):
                self.git("reset", "--hard", self.source)
                self.write(name, b"// Unreviewed UI fixture change\n")
                changed = self.commit("unreviewed UI fixture")
                with self.assertRaisesRegex(RuntimeError, "outside preset-only maintenance"):
                    contract.verify_source_scope(changed, self.policy, root=self.root)

    def test_only_exact_reviewed_tuner_diagnostics_are_accepted(self):
        reviewed = (REAL_ROOT / contract.TUNER_DIAGNOSTICS_PATH).read_bytes()
        self.assertEqual(contract.digest_bytes(reviewed), contract.TUNER_DIAGNOSTICS_SHA256)
        self.write(contract.TUNER_DIAGNOSTICS_PATH, reviewed)
        changed = self.commit("reviewed deferred tuner diagnostics")
        contract.verify_source_scope(changed, self.policy, root=self.root)
        # An altered gate, missing failure evidence, or extra timing adjustment
        # must not become a generic exception to preset-only source validation.
        for old, new in ((b"b.acquire<290", b"b.acquire<400"),
                         (b"std::abs(b.cents)<.1", b"std::abs(b.cents)<1"),
                         (b'"ROUTE FAIL rate="', b'"route omitted="'),
                         (b"const double elapsed=", b"const double elapsed=0*")):
            with self.subTest(mutation=old):
                self.assertIn(old, reviewed)
                self.write(contract.TUNER_DIAGNOSTICS_PATH, reviewed.replace(old, new, 1))
                mutated = self.commit("unapproved tuner change")
                with self.assertRaisesRegex(RuntimeError, "reviewed diagnostic-only"):
                    contract.verify_source_scope(mutated, self.policy, root=self.root)

    def test_engine_version_expiry_publisher_and_existing_workflow_changes_fail_closed(self):
        for name in ("Source/Engine.h", "VERSION", "RELEASE_CHANNEL", "Validation/beta-lifecycle-policy.json",
                     "Tools/publish_release.py", ".github/workflows/build.yml", "Validation/waivers.json",
                     "docs/GRAPHICAL_EQ_1_3_1_VALIDATION.md"):
            with self.subTest(name=name):
                self.git("reset", "--hard", self.source)
                self.write(name, b"unapproved modification\n")
                changed = self.commit("out-of-scope fixture")
                with self.assertRaises((RuntimeError, ValueError, KeyError)):
                    contract.verify_source_scope(changed, self.policy, root=self.root)

    def test_changing_wrapper_beyond_one_hook_or_product_cmake_is_rejected(self):
        for name in (next(iter(contract.WRAPPERS)), "CMakeLists.txt"):
            with self.subTest(name=name):
                self.git("reset", "--hard", self.source)
                self.write(name, (self.root / name).read_bytes() + b"// extra runtime change\n")
                changed = self.commit("unexpected runtime fixture")
                with self.assertRaises(RuntimeError):
                    contract.verify_source_scope(changed, self.policy, root=self.root)

    def test_edited_recipe_needs_fresh_reviewed_content_hash(self):
        self.write(contract.RECIPE_PATH, b"// Different recipe\n")
        changed = self.commit("recipe changed after review")
        with self.assertRaisesRegex(RuntimeError, "reviewed content digest"):
            contract.verify_source_scope(changed, self.policy, root=self.root)

    def test_exact_source_controller_can_change_only_approved_sha(self):
        approved = {**self.policy, "approved_product_source_sha": self.source}
        self.write_json(contract.POLICY_PATH, approved)
        controller = self.commit("exact tested-source maintenance approval")
        self.assertEqual(contract.verify_controller(self.source, controller, approved, root=self.root)
                         ["controller_changed_paths"], [contract.POLICY_PATH])
        for mutation in ("source", "scope", "engine"):
            with self.subTest(mutation=mutation):
                self.git("reset", "--hard", controller)
                changed_policy = copy.deepcopy(approved)
                if mutation == "source":
                    changed_policy["approved_product_source_sha"] = "f" * 40
                elif mutation == "scope":
                    changed_policy["owner_authorization"]["context"] += " Additional acceptance."
                else:
                    self.write("Source/Engine.h", b"different engine\n")
                self.write_json(contract.POLICY_PATH, changed_policy)
                changed = self.commit("invalid controller fixture")
                with self.assertRaises(RuntimeError):
                    contract.verify_controller(self.source, changed, changed_policy, root=self.root)


class PackContracts(unittest.TestCase):
    def test_output_zero_and_values_inside_native_float32_limit_are_accepted(self):
        # The final pair is the adjacent float32 value below native 1.e-6f.
        for value in ("0", "-0.0", "-8.046627044677734e-7", "8.046627044677734e-7",
                      "9.99999883788405e-7", "-9.99999883788405e-7"):
            with self.subTest(value=value):
                self.assertEqual(contract.validate_state(fixture_state(value))["output"], float(value))

    def test_actual_macos_output_rounding_passes_full_pack_validation(self):
        # Actual 4de53 macOS XML: unity is normalized 0.75 and restores exactly.
        value = "-8.046627044677734e-7"
        manifest, files = fixture_pack(platform="macos", output_value=value)
        for row in manifest["presets"]:
            row["normalized_parameters"][1] = .75
        self.assertIs(contract.validate_pack(manifest, files, SOURCE, "macos"), manifest)
        # The manifest retains the authored exact-zero reference, not raw XML rounding.
        manifest["presets"][0]["output_db"] = float(value)
        with self.assertRaisesRegex(RuntimeError, "Preset fresh restore failed"):
            contract.validate_pack(manifest, files, SOURCE, "macos")

    def test_output_at_or_beyond_native_float32_limit_is_rejected(self):
        # Native 1.e-6f itself is less than Python 1e-6, but must still fail.
        for magnitude in ("9.999999974752427e-7", "1.0000001111620804e-6", "1e-6", ".01"):
            for sign in ("", "-"):
                value = sign + magnitude
                with self.subTest(value=value), self.assertRaisesRegex(RuntimeError, "unity output reference"):
                    contract.validate_state(fixture_state(value))

    def test_missing_output_is_rejected(self):
        with self.assertRaisesRegex(RuntimeError, "unity output reference"):
            contract.validate_state(fixture_state(None))

    def test_nonfinite_output_is_rejected(self):
        for value in ("nan", "NaN", "inf", "-inf", "1e309", "-1e309"):
            with self.subTest(value=value), self.assertRaisesRegex(RuntimeError, "Non-finite factory parameter"):
                contract.validate_state(fixture_state(value))

    def test_all_48_valid_states_and_cross_platform_parameter_bank(self):
        with tempfile.TemporaryDirectory() as temporary:
            manifests, directories = {}, {}
            for platform in contract.PLATFORMS:
                manifest, files = fixture_pack(platform=platform)
                manifests[platform] = contract.validate_pack(manifest, files, SOURCE, platform)
                directories[platform] = Path(temporary) / platform
                directories[platform].mkdir()
                for name, data in files.items(): (directories[platform] / name).write_bytes(data)
            self.assertTrue(contract.valid_sha(contract.compare_platform_packs(manifests), 64))
            manifests["macos"]["presets"][0]["normalized_parameters"][0] = 5e-7
            contract.compare_platform_packs(manifests)
            manifests["macos"]["presets"][0]["normalized_parameters"][0] = 2e-6
            with self.assertRaisesRegex(RuntimeError, "between platforms"):
                contract.compare_platform_packs(manifests)

    def test_qualification_only_and_changed_expiry_cannot_be_exported_as_public(self):
        for key, value in (("beta_mode", "qualification-only"), ("beta_expires_at_utc", "2099-01-01T00:00:00Z"),
                           ("source_sha", "f" * 40), ("actual_daw_acceptance", True)):
            with self.subTest(key=key):
                manifest, files = fixture_pack()
                manifest[key] = value
                with self.assertRaises(RuntimeError):
                    contract.validate_pack(manifest, files, SOURCE, "linux")

    def test_missing_extra_tampered_and_failed_restore_states_are_rejected(self):
        for mutation in ("missing", "extra", "bytes", "restore", "ordinal", "unsafe"):
            with self.subTest(mutation=mutation):
                manifest, files = fixture_pack()
                first = manifest["presets"][0]
                if mutation == "missing": files.pop(first["file"])
                elif mutation == "extra": files["extra.chimera"] = b"extra"
                elif mutation == "bytes": files[first["file"]] += b"modified"
                elif mutation == "restore": first["restore_max_normalized_error"] = .01
                elif mutation == "ordinal": first["ordinal"] = 1
                else: first["file"] = "../private.chimera"
                with self.assertRaises(RuntimeError):
                    contract.validate_pack(manifest, files, SOURCE, "linux")

    def test_private_ir_midi_or_path_at_any_snapshot_depth_is_rejected(self):
        for extra in ('<COMPARISONS><PARAMS><USER_IRS><IR asset="private"/></USER_IRS></PARAMS></COMPARISONS>',
                      '<IR_ASSETS><ASSET data="private"/></IR_ASSETS>', '<MIDI_MAP><CC id="output" cc="7"/></MIDI_MAP>',
                      '<PARAMS token="credential"/>', '<IR path="/private/example.wav"/>'):
            with self.subTest(extra=extra):
                xml = ('<PARAMS schemaVersion="11"><PARAM id="output" value="0"/>' + extra + '</PARAMS>').encode()
                data = struct.pack("<II", 0x21324356, len(xml)) + xml + b"\0"
                with self.assertRaises(RuntimeError):
                    contract.validate_state(data)

    def test_export_must_be_in_complete_successful_ctest_inventory(self):
        manifest, files = fixture_pack()
        for mutation in ("pass", "skipped", "missing", "extra_file", "unsafe"):
            with self.subTest(mutation=mutation), tempfile.TemporaryDirectory() as temporary:
                root = Path(temporary)
                archive = root / "integration.zip"
                with zipfile.ZipFile(archive, "w") as bundle:
                    prefix = "validation/preset-maintenance/"
                    bundle.writestr(prefix + contract.PACK_MANIFEST, json.dumps(manifest))
                    for name, data in files.items(): bundle.writestr(prefix + name, data)
                    name = contract.EXPORT_TEST if mutation != "missing" else "OtherTest"
                    child = '<skipped/>' if mutation == "skipped" else ''
                    bundle.writestr("ctest-results.xml", f'<testsuite><testcase name="{name}" status="run">{child}</testcase></testsuite>')
                    bundle.writestr("ctest-inventory.json", json.dumps({"tests": [{"name": contract.EXPORT_TEST}]}))
                    if mutation == "extra_file": bundle.writestr(prefix + "unreviewed.txt", "extra")
                    if mutation == "unsafe": bundle.writestr("../escape", "unsafe")
                if mutation == "pass":
                    self.assertEqual(contract.read_integration_archive(archive, SOURCE, "linux", root / "out")["preset_count"], 48)
                else:
                    with self.assertRaises(RuntimeError):
                        contract.read_integration_archive(archive, SOURCE, "linux", root / "out")


class ApprovalAndTransport(unittest.TestCase):
    def test_package_labels_change_without_changing_any_verified_package_bytes(self):
        policy = contract.read_json(REAL_ROOT / contract.POLICY_PATH)
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            inputs, originals = {}, {}
            for suffix in publisher.SUFFIXES:
                name = f"SpectralForge-Chimera-{contract.BASE_VERSION}-{suffix}"
                path = root / name
                path.write_bytes(("verified CI package " + suffix).encode())
                inputs[name] = path
                originals[suffix] = path.read_bytes()
            for name in ("update-beta.json", "SHA256SUMS.txt", "candidate-source.json"):
                path = root / name
                path.write_text("original candidate metadata\n")
                inputs[name] = path
            pack = root / "factory-presets.zip"
            pack.write_bytes(b"verified factory pack")
            assets, binaries = publisher.maintenance_assets(inputs, pack, policy, root)
            self.assertEqual(len(binaries), 5)
            self.assertNotIn("update-beta.json", assets)
            self.assertIn("candidate-source.json", assets)
            for suffix, name in zip(publisher.SUFFIXES, binaries):
                self.assertIn("presets-" + policy["preset_revision"], name)
                self.assertEqual(assets[name].read_bytes(), originals[suffix])
                self.assertEqual(assets[name + ".sha256.txt"].read_text(), contract.sha256(assets[name]) + "  " + name + "\n")
            self.assertEqual(len(assets["SHA256SUMS.txt"].read_text().splitlines()), 6)

    def test_unapproved_and_broader_approval_cannot_start_network_publication(self):
        policy = contract.read_json(REAL_ROOT / contract.POLICY_PATH)
        policy["approved_product_source_sha"] = None
        with patch.object(publisher.transport, "GitHub") as api:
            with self.assertRaisesRegex(RuntimeError, "Exact tested product source"):
                publisher.publish(policy, 0)
        api.assert_not_called()
        for key in ("new_validation_gaps_accepted", "beta2_or_unfinished_p0_approved", "expiry_change_approved"):
            changed = copy.deepcopy(policy)
            changed["owner_authorization"][key] = True
            with self.assertRaises(RuntimeError): contract.validate_policy(changed)

    def test_receipt_read_does_not_mutate_or_contact_github(self):
        policy = contract.read_json(REAL_ROOT / contract.POLICY_PATH)
        policy["approved_product_source_sha"] = None
        with patch.object(publisher, "read_json", return_value=policy), \
             patch.object(publisher.transport, "GitHub") as api, redirect_stdout(io.StringIO()):
            self.assertEqual(publisher.main(["--approval-state"]), {"ready": False, "published": False})
        api.assert_not_called()

    def test_existing_release_mismatch_stops_before_any_write(self):
        policy = contract.read_json(REAL_ROOT / contract.POLICY_PATH)
        policy["approved_product_source_sha"] = SOURCE
        api = Mock()
        api.pages.return_value = [dict(tag_name=policy["release_tag"], name="Different release")]
        with patch.object(publisher.transport, "verify_tag"), self.assertRaisesRegex(RuntimeError, "no replacement"):
            publisher.publish_immutable(api, policy, SOURCE, "Reviewed notes",
                                        {"preset-maintenance-source.json": Path("unused")}, Mock())
        api.json.assert_not_called()

    def test_strict_gate_gaps_remain_blocked_and_new_failures_are_rejected(self):
        receipt = contract.read_json(REAL_ROOT / contract.OLD_APPROVAL_PATH)
        rows = [dict(id=row["id"], computed_status="PASS") for row in receipt["required_ci_checks"]]
        rows += [dict(id=row["id"], computed_status="BLOCKED", reason_code="NOT_EXECUTED")
                 for row in receipt["known_validation_gaps"]]
        verdict = dict(checks=rows, counts={"checks": {"total": 128, "pass": 21, "blocked": 107, "not_applicable": 0}},
                       hard_gates={"total": 63, "pass": 21, "blocked": 42}, verdict="BLOCKED", ready=False)
        result = contract.verify_retained_strict_gate(verdict, receipt)
        self.assertFalse(result["ready"])
        self.assertFalse(result["new_validation_gaps_accepted"])
        verdict["checks"][0]["computed_status"] = "BLOCKED"
        with self.assertRaises(RuntimeError): contract.verify_retained_strict_gate(verdict, receipt)
        verdict["checks"][0]["computed_status"] = "PASS"
        verdict["checks"][-1]["computed_status"] = "PASS"
        with self.assertRaises(RuntimeError): contract.verify_retained_strict_gate(verdict, receipt)


if __name__ == "__main__":
    unittest.main()
