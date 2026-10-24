#!/usr/bin/env python3
"""Bounded beta.1 preset maintenance contracts; this module never publishes.

The shipped product policies remain intact. Engine source bytes, version,
expiry, packaging and the seven required workflows must match the published
beta.1. A new preset recipe, its tests and documentation are the allowed scope.
"""
from __future__ import annotations

import hashlib
import json
import math
from pathlib import Path, PurePosixPath
import re
import stat
import struct
import subprocess
import xml.etree.ElementTree as ET
import zipfile

import check_public_names
import produce_ctest_validation
import weekly_publication
from publish_beta131 import read_json, parse_json
from publish_open_beta import require, sha256

ROOT = Path(__file__).resolve().parents[1]
BASE_SOURCE = "3bec6c70a50bbb4021dcc6133f37e0c2fb3e9e58"
BASE_CONTROLLER = "ec6fd639e2958d34605bfb04bb81ca02c1cfb38f"
BASE_VERSION = "1.3.2-beta.1"
BASE_EXPIRY = "2027-02-11T14:59:00Z"
BASE_RELEASE_ID = 415938432
BASE_DECISION_SHA256 = "621ad9f51d34326130b2cd527cbf3ae26dc720b07ac006014cca07e1c3141d52"
POLICY_PATH = "Validation/preset-maintenance-policy.json"
WORKFLOW_PATH = ".github/workflows/preset-maintenance.yml"
RECIPE_PATH = "Source/FactoryPresetFinishing.h"
OLD_APPROVAL_PATH = "Validation/weekly-publication-policy.json"
EXPORT_TEST = "ChimeraFactoryPresetExport"
PACK_MANIFEST = "factory-preset-pack.json"
# Match the exporter's strict abs(rawOutput) < 1.e-6f, including float32 rounding.
FACTORY_OUTPUT_TOLERANCE_DB = struct.unpack("<f", struct.pack("<f", 1e-6))[0]
PLATFORMS = {"windows": "Windows", "linux": "Linux", "macos": "macOS"}
WRAPPERS = {
    "Source/FactoryNativeVoicing.h": "    voiceFactoryFinishing(index,get,set);\n",
    "Source/GuitarSignaturePresets.h": "    voiceFactoryFinishing(factoryPresetCount+song,get,set);\n",
    "Source/NiflheimrPresets.h": "    voiceFactoryFinishing(niflheimrPresetStart+preset,get,set);\n",
    "Source/OriginalPresets.h": "    voiceFactoryFinishing(originalPresetStart+preset,get,set);\n",
}
ALLOWED_TESTS = frozenset({"Tests/GateProcessorTests.cpp", "Tests/GraphicalEQEditingTests.cpp",
    "Tests/GraphicalEQTests.cpp", "Tests/GraphicalEQUITests.cpp", "Tests/IntegratedProcessorTests.cpp",
    "Tests/SignalPathUITests.h", "Tests/CommonDynamicsUITests.h"})
# The owner requested the PR108 macOS timing investigation. Permit only this
# reviewed diagnostic-I/O change; do not allow arbitrary tuner fixture/gate edits.
# Digest updated for the audio-time route gate (docs/TUNER_TIMING_ATTRIBUTION.md);
# owner review of that fixture change is still required.
TUNER_DIAGNOSTICS_PATH = "Tests/PrecisionTunerIntegrationTests.cpp"
TUNER_DIAGNOSTICS_SHA256 = "1371469c87ec73b0c2edd24d3daebc5342d026d2bdbdcf456be28581bc1ecd60"
ALLOWED_DOCS = frozenset({"docs/PRESETS.md", "docs/MANUAL.html", "docs/COMMON_DYNAMICS_1_3_2.md",
    "docs/GRAPHICAL_EQ_1_3_1.md", "docs/SIGNAL_EQ_INTEGRATION_1_3_1.md", "docs/PRESET_DYNAMICS_EQ.md"})
ADDITIONS = frozenset({RECIPE_PATH, POLICY_PATH, WORKFLOW_PATH,
    "Tools/ExportFactoryPresetPack.cpp", "Tools/preset_maintenance.py",
    "Tools/publish_preset_maintenance.py", "Tools/test_preset_maintenance.py"})
CMAKE_BLOCK = """
# BEGIN CHIMERA PRESET MAINTENANCE EXPORTER
# The existing Product build and full CTest matrix produce and verify this pack.
# build.yml already retains build/validation on Windows, Linux and macOS.
juce_add_console_app(ChimeraFactoryPresetExport PRODUCT_NAME "Chimera Factory Preset Export")
target_sources(ChimeraFactoryPresetExport PRIVATE Tools/ExportFactoryPresetPack.cpp)
target_include_directories(ChimeraFactoryPresetExport PRIVATE Source)
target_link_libraries(ChimeraFactoryPresetExport PRIVATE ChimeraAmpMatrix ChimeraArtwork ChimeraManual juce::juce_cryptography juce::juce_audio_utils juce::juce_dsp)
add_test(NAME ChimeraFactoryPresetExport COMMAND ChimeraFactoryPresetExport "${CMAKE_BINARY_DIR}/validation/preset-maintenance")
set_tests_properties(ChimeraFactoryPresetExport PROPERTIES TIMEOUT 300 RUN_SERIAL TRUE)
# END CHIMERA PRESET MAINTENANCE EXPORTER
""".encode()


def git(*args, root=ROOT, binary=False):
    result = subprocess.check_output(["git", *args], cwd=root)
    return result if binary else result.decode("utf-8").strip()


def blob(revision, path, root=ROOT):
    return git("show", f"{revision}:{path}", root=root, binary=True)


def tree(revision, root=ROOT):
    result = {}
    for entry in git("ls-tree", "-r", "-z", revision, root=root, binary=True).split(b"\0"):
        if not entry:
            continue
        info, name = entry.split(b"\t", 1)
        mode, kind, digest = info.decode().split()
        result[name.decode("utf-8")] = (mode, kind, digest)
    return result


def digest_bytes(value):
    return hashlib.sha256(value).hexdigest()


def valid_sha(value, length=40):
    return isinstance(value, str) and re.fullmatch(r"[0-9a-f]{" + str(length) + "}", value) is not None


def validate_policy(policy, *, source=None, publication=False):
    require(isinstance(policy, dict) and policy.get("schema") == "spectralforge.chimera.preset-maintenance-policy.v1",
            "Unexpected preset maintenance policy")
    require(set(policy) == {"schema", "preset_revision", "release_tag", "base_product_version",
        "base_product_source_sha", "base_publication_controller_sha", "base_release_id",
        "base_publication_decision_sha256", "retained_beta_expiry_utc", "preset_recipe_sha256",
        "approved_product_source_sha", "owner_authorization", "requirements"},
        "Unexpected maintenance approval or policy field")
    require(policy.get("base_product_version") == BASE_VERSION
            and policy.get("base_product_source_sha") == BASE_SOURCE
            and policy.get("base_publication_controller_sha") == BASE_CONTROLLER
            and policy.get("base_release_id") == BASE_RELEASE_ID
            and policy.get("base_publication_decision_sha256") == BASE_DECISION_SHA256
            and policy.get("retained_beta_expiry_utc") == BASE_EXPIRY,
            "Preset maintenance must retain the published beta.1 identity and expiry")
    revision = policy.get("preset_revision")
    require(isinstance(revision, str) and re.fullmatch(r"2026\.10\.19\.[1-9][0-9]*", revision)
            and policy.get("release_tag") == "presets-" + revision,
            "Maintenance requires its own immutable preset revision/tag")
    require(valid_sha(policy.get("preset_recipe_sha256"), 64), "Preset recipe digest is missing")
    approval = policy.get("owner_authorization", {})
    require(isinstance(approval, dict) and set(approval) == {"instruction", "instruction_date",
                "publication_authorized", "scope", "new_validation_gaps_accepted",
                "beta2_or_unfinished_p0_approved", "expiry_change_approved", "context"}
            and approval.get("publication_authorized") is True
            and approval.get("instruction") == "프리셋은 업데이트하면 바로 배포판에 반영"
            and approval.get("scope") == "preset-only-beta1-maintenance-installers-and-preset-pack"
            and approval.get("new_validation_gaps_accepted") is False
            and approval.get("beta2_or_unfinished_p0_approved") is False
            and approval.get("expiry_change_approved") is False,
            "Explicit maintenance authorization cannot grant broader product acceptance")
    required = {"unchanged_engine_source_bytes", "unchanged_product_policies_and_seven_workflows",
                "same_source_all_seven_workflows_success", "same_build_five_verified_packages",
                "three_platform_48_preset_export_and_fresh_restore", "immutable_assets_and_tag",
                "preserve_strict_gate_and_each_existing_gap", "no_update_feed_or_latest_change"}
    require(policy.get("requirements") == {name: True for name in sorted(required)},
            "A preset maintenance verification requirement is missing or changed")
    approved_source = policy.get("approved_product_source_sha")
    require(approved_source is None or valid_sha(approved_source), "Invalid approved product source SHA")
    if publication:
        require(valid_sha(source) and approved_source == source,
                "Exact tested product source approval is required before publication")
    return policy


def verify_source_scope(source, policy, *, root=ROOT):
    """Compare Git blobs, never a worktree relabelled with an arbitrary source SHA."""
    require(valid_sha(source), "Full tested source SHA required")
    validate_policy(policy)
    base, published, current = tree(BASE_CONTROLLER, root), tree(BASE_SOURCE, root), tree(source, root)
    base_delta = {name for name in base.keys() | published.keys() if base.get(name) != published.get(name)}
    require(base_delta == {OLD_APPROVAL_PATH}, "The selected beta.1 controller changed released engine bytes")
    receipt = parse_json(blob(BASE_CONTROLLER, OLD_APPROVAL_PATH, root).decode("utf-8"))
    strict = parse_json(blob(source, "Validation/release-policy.json", root).decode("utf-8"))
    # Validate the historical acceptance only against its original source. It is
    # provenance of the unchanged engine, never a new-source product approval.
    weekly_publication.validate_approval(receipt, strict, BASE_SOURCE, version=BASE_VERSION,
        profile="beta_1_3", waivers=parse_json(blob(source, "Validation/waivers.json", root).decode()),
        ci_check_map=parse_json(blob(source, "Validation/ci-check-map.json", root).decode()))
    changed = {name for name in base.keys() | current.keys() if base.get(name) != current.get(name)}
    require(set(WRAPPERS) | ADDITIONS | {"CMakeLists.txt"} <= changed,
            "Preset maintenance source or exporter is incomplete")
    for name in sorted(changed):
        require(current.get(name, ())[:2] == ("100644", "blob"),
                "Deleted, symlinked or executable maintenance source: " + name)
        if name in WRAPPERS:
            value = blob(source, name, root)
            for addition in (b'#include "FactoryPresetFinishing.h"\n', WRAPPERS[name].encode()):
                require(value.count(addition) == 1, "Unexpected factory recipe hook: " + name)
                value = value.replace(addition, b"", 1)
            require(value == blob(BASE_CONTROLLER, name, root),
                    "Factory wrapper changed beyond its finishing call: " + name)
        elif name == "CMakeLists.txt":
            require(blob(source, name, root) == blob(BASE_CONTROLLER, name, root) + CMAKE_BLOCK,
                    "Product build configuration changed beyond the isolated exporter target")
        elif name in ADDITIONS:
            require(name not in base, "Maintenance support must be added without replacing existing tooling")
        elif name == TUNER_DIAGNOSTICS_PATH:
            require(digest_bytes(blob(source, name, root)) == TUNER_DIAGNOSTICS_SHA256,
                    "Tuner fixture differs from the reviewed diagnostic-only change")
        elif name in ALLOWED_TESTS | ALLOWED_DOCS:
            pass
        elif name.startswith("evidence/preset-dynamics-eq-20261019/"):
            require(name not in base and PurePosixPath(name).suffix in (".json", ".csv", ".txt", ".md", ".png", ".xml"),
                    "Unexpected preset measurement artifact")
        else:
            raise RuntimeError("Change is outside preset-only maintenance: " + name)
    require(digest_bytes(blob(source, RECIPE_PATH, root)) == policy["preset_recipe_sha256"],
            "Preset recipe differs from the reviewed content digest")
    require(blob(source, "VERSION", root).strip() == b"1.3.2"
            and blob(source, "RELEASE_CHANNEL", root).strip() == b"beta.1",
            "Preset maintenance cannot bump the product version/channel")
    engine_rows = [name + "\0" + published[name][2] for name in sorted(published) if name.startswith("Source/")]
    return {"base_product_source_sha": BASE_SOURCE, "product_source_sha": source,
            "base_engine_git_blobs_sha256": digest_bytes("\n".join(engine_rows).encode()),
            "engine_source_bytes_unchanged_except_preset_finishing": True,
            "runtime_source_files_compared": len(engine_rows), "changed_paths": sorted(changed),
            "preset_recipe_sha256": policy["preset_recipe_sha256"],
            "product_version": BASE_VERSION, "beta_expires_at_utc": BASE_EXPIRY}


def verify_controller(source, controller, policy, *, root=ROOT):
    require(valid_sha(source) and valid_sha(controller), "Full source/controller SHAs required")
    require(git("rev-parse", "HEAD", root=root) == controller, "Publication controller checkout differs")
    subprocess.run(["git", "merge-base", "--is-ancestor", source, controller], cwd=root, check=True)
    require(not git("status", "--porcelain", "--untracked-files=no", root=root), "Controller has local changes")
    before, after = tree(source, root), tree(controller, root)
    changed = sorted(name for name in before.keys() | after.keys() if before.get(name) != after.get(name))
    require(changed == [POLICY_PATH] and after[POLICY_PATH][:2] == ("100644", "blob"),
            "Only a separate exact-source approval receipt may follow the tested source")
    source_policy = parse_json(blob(source, POLICY_PATH, root).decode())
    validate_policy(source_policy)
    require(source_policy.get("approved_product_source_sha") is None,
            "Tested source already contains an unrelated or stale source approval")
    require({**source_policy, "approved_product_source_sha": source} == policy
            and parse_json(blob(controller, POLICY_PATH, root).decode()) == policy,
            "Controller changed approval scope or untested publisher/product bytes")
    validate_policy(policy, source=source, publication=True)
    return {"product_source_sha": source, "publication_controller_sha": controller,
            "controller_changed_paths": changed, "policy_sha256": weekly_publication.digest(policy)}


def verify_retained_strict_gate(verdict, old_receipt):
    """Reject any new failure, waiver, omitted check or unperformed PASS claim."""
    rows = verdict.get("checks", [])
    require(isinstance(rows, list) and all(isinstance(row, dict) for row in rows), "Missing strict check inventory")
    statuses = {row.get("id"): row for row in rows}
    automatic = {row["id"] for row in old_receipt["required_ci_checks"]}
    gaps = {row["id"] for row in old_receipt["known_validation_gaps"]}
    require(len(statuses) == len(rows) and set(statuses) == automatic | gaps
            and all(statuses[cid].get("computed_status") == "PASS" for cid in automatic)
            and all(statuses[cid].get("computed_status") == "BLOCKED"
                    and statuses[cid].get("reason_code") == "NOT_EXECUTED" for cid in gaps)
            and verdict.get("counts", {}).get("checks") == {"total": 128, "pass": 21, "blocked": 107, "not_applicable": 0}
            and verdict.get("hard_gates") == {"total": 63, "pass": 21, "blocked": 42}
            and verdict.get("verdict") == "BLOCKED" and verdict.get("ready") is False,
            "Strict report differs from the retained beta.1 limitation inventory")
    return {"verdict": verdict["verdict"], "ready": False, "counts": verdict["counts"],
            "hard_gates": verdict["hard_gates"], "new_validation_gaps_accepted": False,
            "known_validation_gaps": old_receipt["known_validation_gaps"]}


def validate_state(data):
    require(9 < len(data) <= 16 * 1024 * 1024, "Unexpected factory state size")
    magic, size = struct.unpack("<II", data[:8])
    require(magic == 0x21324356 and size == len(data) - 9 and data[-1] == 0,
            "Factory file is not exact JUCE binary XML state")
    xml = data[8:-1]
    require(b"<!DOCTYPE" not in xml.upper() and b"<!ENTITY" not in xml.upper(), "Unexpected state XML declarations")
    root = ET.fromstring(xml)
    require(root.tag == "PARAMS" and root.attrib.get("schemaVersion") == "11", "Factory state schema differs")
    for item in root.iter():
        if item.tag in ("USER_IRS", "IR_ASSETS", "MIDI_MAP"):
            require(not list(item) and not item.attrib, "Factory state contains private IR or MIDI data")
        require(not {key.lower() for key in item.attrib} & {"path", "filename", "filepath", "token", "authorization"},
                "Factory state contains local path or credential metadata")
    values = {}
    for item in root.findall("PARAM"):
        name = item.attrib.get("id")
        require(isinstance(name, str) and name and name not in values, "Invalid/duplicate factory parameter ID")
        number = float(item.attrib["value"])
        require(math.isfinite(number), "Non-finite factory parameter")
        values[name] = number
    output = values.get("output")
    require(output is not None and abs(output) < FACTORY_OUTPUT_TOLERANCE_DB,
            "Factory state lost its unity output reference")
    return values


def validate_pack(manifest, files, source, platform):
    require(manifest.get("schema") == "spectralforge.chimera.factory-preset-pack.v1"
            and manifest.get("product_version") == BASE_VERSION and manifest.get("source_sha") == source
            and manifest.get("platform") == platform and platform in PLATFORMS
            and manifest.get("beta_mode") == "approved" and manifest.get("beta_expires_at_utc") == BASE_EXPIRY,
            "Factory export source/platform/version or approved beta expiry differs")
    require(manifest.get("preset_count") == 48 and manifest.get("state_schema_version") == 11
            and manifest.get("fresh_processor_restore_passed") is True
            and manifest.get("fresh_gui_metadata_restore_passed") is True
            and manifest.get("personal_ir_included") is False
            and manifest.get("personal_midi_mapping_included") is False
            and manifest.get("actual_daw_acceptance") is False
            and manifest.get("listening_acceptance") is False,
            "Factory export verification scope differs")
    rows = manifest.get("presets")
    require(isinstance(rows, list) and len(rows) == 48 and all(isinstance(row, dict) for row in rows),
            "Expected 48 factory presets")
    parameter_ids = manifest.get("parameter_ids")
    require(isinstance(parameter_ids, list) and parameter_ids
            and all(isinstance(name, str) and name for name in parameter_ids)
            and parameter_ids == sorted(set(parameter_ids)), "Missing/duplicate canonical parameter IDs")
    names = [row.get("file") for row in rows]
    require(all(isinstance(name, str) and re.fullmatch(r"[0-9]{2}-[A-Za-z0-9-]+\.chimera", name) for name in names)
            and len(set(names)) == 48 and set(names) == set(files), "Missing, extra or unsafe preset file")
    for ordinal, row in enumerate(rows):
        data = files[row["file"]]
        require(type(row.get("ordinal")) is int and row["ordinal"] == ordinal
                and row["file"].startswith(f"{ordinal + 1:02d}-")
                and isinstance(row.get("name"), str) and row["name"]
                and isinstance(row.get("category"), str) and row["category"], "Factory preset identity/order differs")
        require(type(row.get("size")) is int and row["size"] == len(data)
                and row.get("sha256") == digest_bytes(data), "Preset file digest/size mismatch")
        error = row.get("restore_max_normalized_error")
        require(type(error) in (int, float) and math.isfinite(error) and 0 <= error < 1e-6
                and row.get("fresh_restore_passed") is True and row.get("output_db") == 0
                and row.get("metadata_restore_passed") is True
                and all(type(row.get(key)) is int and 0 <= row[key] < 4096 for key in ("tone_occupied", "final_occupied"))
                and valid_sha(row.get("parameter_sha256"), 64), "Preset fresh restore failed")
        values = validate_state(data)
        normalized = row.get("normalized_parameters")
        require(type(row.get("parameter_count")) is int and len(values) == row["parameter_count"]
                and set(values) == set(parameter_ids)
                and isinstance(normalized, list) and len(normalized) == len(parameter_ids)
                and all(type(value) in (int, float) and math.isfinite(value) and 0 <= value <= 1 for value in normalized),
                "Exported state parameter inventory differs")
    return manifest


def read_integration_archive(archive, source, platform, output):
    """Extract only verified preset bytes and reuse the ordinary full CTest gate."""
    output.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(archive) as bundle:
        entries = bundle.infolist()
        names = [item.filename for item in entries]
        require(len(set(names)) == len(names) and sum(item.file_size for item in entries) <= 2 * 1024 ** 3,
                "Duplicate or oversized integration artifact")
        require(all(not PurePosixPath(name).is_absolute() and ".." not in PurePosixPath(name).parts
                    and "\\" not in name and not stat.S_ISLNK(item.external_attr >> 16)
                    for name, item in zip(names, entries)), "Unsafe integration archive entry")
        require(bundle.testzip() is None, "Integration artifact CRC mismatch")
        def one(basename):
            matches = [name for name in names if PurePosixPath(name).name == basename]
            require(len(matches) == 1, "Missing/duplicate integration file: " + basename)
            return matches[0]
        manifest_name = one(PACK_MANIFEST)
        parent = PurePosixPath(manifest_name).parent
        require(parent.name == "preset-maintenance", "Factory export is outside its configured output path")
        manifest = parse_json(bundle.read(manifest_name).decode("utf-8-sig"))
        children = [name for name in names if PurePosixPath(name).parent == parent and not name.endswith("/")]
        require(len(children) == 49, "Factory export directory contains missing or unreviewed files")
        files = {PurePosixPath(name).name: bundle.read(name) for name in children if name != manifest_name}
        validate_pack(manifest, files, source, platform)
        for basename in ("ctest-results.xml", "ctest-inventory.json"):
            (output / basename).write_bytes(bundle.read(one(basename)))
        cases, matrix_ok = produce_ctest_validation.test_results(output / "ctest-results.xml", output / "ctest-inventory.json")
        require(matrix_ok and cases.get(EXPORT_TEST) is True, "Exporter must execute and pass in the complete Product CTest matrix")
        (output / PACK_MANIFEST).write_bytes(bundle.read(manifest_name))
        for name, data in files.items():
            (output / name).write_bytes(data)
    return manifest


def compare_platform_packs(manifests):
    require(set(manifests) == set(PLATFORMS), "All three platform exports are required")
    columns = ("ordinal", "name", "category", "file", "parameter_count", "output_db",
               "tone_occupied", "final_occupied")
    def identity(manifest):
        return [{key: row[key] for key in columns} for row in manifest["presets"]]
    reference = identity(manifests["linux"])
    parameter_ids = manifests["linux"]["parameter_ids"]
    require(all(identity(value) == reference and value["parameter_ids"] == parameter_ids for value in manifests.values()),
            "Factory parameter bank differs between platforms")
    # The same JUCE NormalisableRange may round a floating-point conversion by
    # a few ULPs across libm implementations. Preserve every file's exact digest
    # while comparing the processor's actual normalized values at the same
    # 1e-6 tolerance used by the product's fresh-state restore regression.
    for ordinal, row in enumerate(manifests["linux"]["presets"]):
        baseline = row["normalized_parameters"]
        for other in manifests.values():
            values = other["presets"][ordinal]["normalized_parameters"]
            require(len(values) == len(baseline) == len(parameter_ids)
                    and all(math.isfinite(a) and math.isfinite(b) and abs(a-b) <= 1e-6 for a, b in zip(baseline, values)),
                    "Normalized factory parameter values differ between platforms: " + row["file"])
    return weekly_publication.digest({"parameter_ids": parameter_ids,
        "reference": [{**row, "parameter_sha256": authored["parameter_sha256"]}
                      for row, authored in zip(reference, manifests["linux"]["presets"])]})


def create_pack(source_dir, destination, policy, source, documentation):
    manifest = read_json(source_dir / PACK_MANIFEST)
    files = {row["file"]: (source_dir / row["file"]).read_bytes() for row in manifest["presets"]}
    validate_pack(manifest, files, source, "linux")
    readme = (f"SpectralForge Chimera factory presets {policy['preset_revision']}\n\n"
        f"Product: {BASE_VERSION}\nSource: {source}\nFactory presets: 48\n\n"
        "The maintenance installers already contain these factory sounds. Existing saved projects and sounds "
        "keep their stored settings. Extract this optional ZIP pack, then use OPEN in Chimera and choose "
        "one .chimera file. Save your current sound before opening another file.\n\n"
        "These files use the unchanged beta.1 state serializer and contain no personal IRs or MIDI mappings. "
        "The installed beta engine retains its approved expiry of 2027-02-11 14:59 UTC. Importing a preset "
        "does not extend that expiry. The 48 fresh-state restores and automated signal tests are technical "
        "checks; actual DAW and listening acceptance is not claimed.\n")
    presentation = check_public_names.load_policy(ROOT)
    check_public_names.validate_text(readme, "factory preset pack README", policy=presentation, exceptions=[])
    check_public_names.validate_text(documentation.read_text(encoding="utf-8"), "PRESET_DYNAMICS_EQ.md",
                                     policy=presentation, exceptions=[])
    check_public_names.validate_text("\n".join(row["name"] + " / " + row["category"] for row in manifest["presets"]),
                                     "factory preset pack display names", policy=presentation, exceptions=[])
    contents = {**files, PACK_MANIFEST: (source_dir / PACK_MANIFEST).read_bytes(),
                "README.txt": readme.encode(), "PRESET_DYNAMICS_EQ.md": documentation.read_bytes()}
    # Stable ZIP metadata makes a rerun idempotent; existing public assets must
    # match their original bytes and are never replaced to make a rerun succeed.
    with zipfile.ZipFile(destination, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=9) as bundle:
        for name, data in sorted(contents.items()):
            entry = zipfile.ZipInfo(name, (2026, 10, 19, 0, 0, 0))
            entry.compress_type = zipfile.ZIP_DEFLATED
            entry.external_attr = (stat.S_IFREG | 0o644) << 16
            bundle.writestr(entry, data)
    return destination
