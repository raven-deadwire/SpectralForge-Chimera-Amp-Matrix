#!/usr/bin/env python3
"""Read-only exact-source artifact audit; never calls a publication entry point."""
import sys
sys.dont_write_bytecode = True
import hashlib, json, os, re, stat, subprocess, time, traceback, zipfile
from pathlib import Path, PurePosixPath

SOURCE_SHA = "6033236cb81820a43679cbbd34be020804f9658d"
SOURCE_TREE = "7b23b503b807db36ec4f51491d6aa5f8c29e7b8c"
PRODUCT_RUN = 38350264755
ATTEMPT = 1
ROOT = Path.cwd().resolve()
OUTPUT = Path(os.environ["CHIMERA_AUDIT_OUTPUT"]).resolve()
if OUTPUT == ROOT or ROOT in OUTPUT.parents:
    raise RuntimeError("Audit output must be outside the product checkout")
OUTPUT.mkdir(parents=True, exist_ok=False)
sys.path.insert(0, str(ROOT / "Tools"))
import publish_release as release_tools

result = {
    "schema": "chimera.read-only-source-artifact-audit.v1",
    "source_sha": SOURCE_SHA, "tree_sha": SOURCE_TREE,
    "product_run_id": PRODUCT_RUN, "product_run_attempt": ATTEMPT,
    "published": False, "publication_entry_point_called": False,
    "candidate_binary_archive_downloaded": False,
    "result": "INCOMPLETE", "verified_artifacts": [], "qualification_failures": []
}

def save(name, value):
    p = OUTPUT / name
    p.parent.mkdir(parents=True, exist_ok=True)
    p.write_text(json.dumps(value, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    return p

def digest(data):
    return hashlib.sha256(data).hexdigest()

def source_identity():
    def git(*args):
        return subprocess.check_output(["git", *args], cwd=ROOT, text=True).strip()
    value = {
        "sha": git("rev-parse", "HEAD"),
        "tree": git("rev-parse", "HEAD^{tree}"),
        "tracked_status": git("status", "--porcelain", "--untracked-files=no"),
        "tracked_diff": git("diff", "--binary", "HEAD", "--"),
    }
    if value != {"sha": SOURCE_SHA, "tree": SOURCE_TREE, "tracked_status": "", "tracked_diff": ""}:
        raise RuntimeError("Product checkout identity or tracked bytes changed")
    return value

def safe_member(name):
    p = PurePosixPath(name)
    if (not p.parts or "\x00" in name or p.is_absolute() or ".." in p.parts
            or "\\" in name or ":" in name or name.rstrip("/") != p.as_posix()):
        raise RuntimeError("Unsafe archive entry: " + name)
    return p

def audit_zip(path, artifact, run_id):
    """Byte-integrity and evidence provenance checks, without executing extracted content."""
    release_tools.check_archive(path, artifact)
    folder = OUTPUT / "artifacts" / str(artifact["id"])
    folder.mkdir(parents=True, exist_ok=False)
    record = {
        "id": artifact["id"], "name": artifact["name"],
        "run_id": run_id, "source_sha": SOURCE_SHA,
        "size": path.stat().st_size, "sha256": digest(path.read_bytes()),
        "api_digest": artifact["digest"], "zip_crc_valid": False,
        "internal_hashes_checked": 0, "extracted_regular_evidence_files": [],
    }
    with zipfile.ZipFile(path) as z:
        entries = z.infolist()
        names = [e.filename for e in entries]
        if len(entries) > 2500 or len(names) != len(set(names)):
            raise RuntimeError("Unexpected archive inventory")
        if sum(e.file_size for e in entries) > 128 * 1024 * 1024:
            raise RuntimeError("Evidence archive exceeds expanded-size bound")
        for e in entries:
            safe_member(e.filename)
            mode = e.external_attr >> 16
            filetype = stat.S_IFMT(mode)
            if filetype and filetype not in (stat.S_IFREG, stat.S_IFDIR):
                raise RuntimeError("Non-regular evidence ZIP member")
        if z.testzip() is not None:
            raise RuntimeError("Evidence ZIP CRC failure")
        record["zip_crc_valid"] = True
        if "SHA256SUMS.json" in names:
            index = release_tools.parse_json(z.read("SHA256SUMS.json").decode("utf-8-sig"))
            if not isinstance(index, list):
                raise RuntimeError("Malformed CAB hash index")
            for item in index:
                safe_member(item["path"])
                raw = z.read(item["path"])
                if len(raw) != item["bytes"] or digest(raw) != item["sha256"]:
                    raise RuntimeError("CAB indexed file integrity failure")
            record["internal_hashes_checked"] += len(index)
        if "evidence.json" in names:
            manifest = release_tools.parse_json(z.read("evidence.json").decode("utf-8-sig"))
            expected = {"commit_sha": SOURCE_SHA, "run_id": str(run_id), "run_attempt": str(ATTEMPT)}
            if manifest.get("revision") != expected:
                raise RuntimeError("Platform evidence source/run/attempt mismatch")
            for item in manifest["files"].values():
                safe_member(item["path"])
                if digest(z.read(item["path"])) != item["sha256"]:
                    raise RuntimeError("Platform evidence internal hash mismatch")
                record["internal_hashes_checked"] += 1
            record["platform_outcomes"] = manifest.get("outcomes")
        if "manifest.json" in names and artifact["name"].startswith("CAB-realtime-"):
            manifest = release_tools.parse_json(z.read("manifest.json").decode("utf-8-sig"))
            runner = manifest["environment"]["runner"]
            if manifest["candidate_sha"] != SOURCE_SHA or runner["GITHUB_RUN_ID"] != str(run_id) or runner["GITHUB_RUN_ATTEMPT"] != str(ATTEMPT):
                raise RuntimeError("CAB candidate/run identity mismatch")
            candidate = manifest["candidate"]
            for label in ("source", "source_after"):
                s = candidate[label]
                if s["sha"] != SOURCE_SHA or s["tree"] != SOURCE_TREE or s["clean"] is not True:
                    raise RuntimeError("CAB checkout identity mismatch")
            routes = candidate["timing"]
            record["cab"] = {
                "reported_result": manifest["result"],
                "ctest_returncode": candidate["ctest_returncode"],
                "tests": candidate["tests"],
                "passing_p99_routes": sum(r["p99_within_deadline"] for r in routes),
                "total_p99_routes": len(routes),
                "total_callbacks": sum(int(r["misses"].split("/")[1]) for r in routes),
                "callback_deadline_misses": sum(int(r["misses"].split("/")[0]) for r in routes),
                "failed_routes": [r for r in routes if not r["p99_within_deadline"]],
                "former_failure_route": [r for r in routes if (r["engine"], r["sr"], r["block"], r["channels"]) == ("expanded-v2", "96000", "32", "2")],
                "qualification": "Existing candidate CTest/p99 gates; PASS does not imply zero callback overruns.",
            }
        allowed = {".json", ".log", ".txt", ".md", ".csv", ".xml", ".cmake", ".yml", ".yaml"}
        for e in entries:
            if e.is_dir() or PurePosixPath(e.filename).suffix.lower() not in allowed:
                continue
            p = folder.joinpath(*safe_member(e.filename).parts)
            p.parent.mkdir(parents=True, exist_ok=True)
            raw = z.read(e.filename)
            p.write_bytes(raw)
            record["extracted_regular_evidence_files"].append({"path": e.filename, "bytes": len(raw), "sha256": digest(raw)})
            if (PurePosixPath(e.filename).suffix.lower() == ".json"
                    and any(token in PurePosixPath(e.filename).name.lower()
                            for token in ("upgrade", "verification", "document", "scope"))):
                obj = release_tools.parse_json(raw.decode("utf-8-sig"))
                record.setdefault("selected_reports", []).append({"path": e.filename, "report": obj})
    save("artifacts/" + str(artifact["id"]) + "/audit.json", record)
    result["verified_artifacts"].append(record)
    bounded = {k: v for k, v in record.items() if k not in ("extracted_regular_evidence_files", "selected_reports")}
    print(json.dumps({"artifact_audit": bounded}, ensure_ascii=False), flush=True)
    for report in record.get("selected_reports", []):
        text = json.dumps(report, ensure_ascii=False)
        print("ORIGINAL_REPORT " + text, flush=True)
    return record

def download_evidence(api, metadata, name, run_id):
    artifact = release_tools.verified_artifact(metadata, name, SOURCE_SHA, run_id)
    if artifact["size_in_bytes"] > 16 * 1024 * 1024:
        raise RuntimeError("Refusing a large/non-evidence artifact")
    archive = OUTPUT / "archives" / (str(artifact["id"]) + ".zip")
    archive.parent.mkdir(parents=True, exist_ok=True)
    api.download("/actions/artifacts/" + str(artifact["id"]) + "/zip", archive)
    audit_zip(archive, artifact, run_id)
    return archive


def diagnose_gate(gate_archive, artifact, policy, release):
    """Read/hash original evidence and call unchanged sparse producers; no qualification override."""
    import release_evidence as evidence
    folder = OUTPUT / "artifacts" / str(artifact["id"])
    matches = list(folder.rglob("release-gate.json"))
    if len(matches) != 1:
        raise RuntimeError("Diagnostic requires exactly one original gate")
    gate_path = matches[0]
    original = release_tools.read_json(gate_path)
    result["original_gate"] = {k: original.get(k) for k in ("verdict", "ready", "counts", "hard_gates", "producer", "revision")}
    print("ORIGINAL_GATE " + json.dumps(result["original_gate"], ensure_ascii=False), flush=True)
    mapping = release_tools.read_json(ROOT / "Validation/ci-check-map.json")["checks"]
    revision = {"commit_sha": SOURCE_SHA, "run_id": str(PRODUCT_RUN), "run_attempt": str(ATTEMPT)}
    inputs = {v["kind"]: v["manifest_sha256"] for v in original["producer"]["inputs"]}
    regenerated = {}
    for kind in evidence.KINDS:
        target = gate_path.parent / "evidence" / kind
        if digest((target / "evidence.json").read_bytes()) != inputs[kind]:
            raise RuntimeError("Diagnostic raw evidence manifest hash mismatch")
        doc, paths = evidence.read_bundle(target, kind, revision, policy, profile=release.profile)
        reports = evidence.reports_for_bundle(doc, paths, policy, mapping)
        regenerated[kind] = reports
        if kind == "windows":
            cases, matrix_ok = evidence.test_results(paths["ctest"], paths["inventory"])
            log = paths["dsp_log"].read_text(encoding="utf-8-sig")
            spec = mapping["E1.STATE_UI"]
            sections = re.split(r"^\d+/\d+ Testing: ", log, flags=re.MULTILINE)[1:]
            matched = [v for v in sections if v.split("\n", 1)[0] == "ChimeraUITests"]
            actual = matched[0] if len(matched) == 1 else ""
            markers = [line for line in actual.splitlines() if line.startswith("PASS native UI:") or line.startswith("PASS: six native amp contexts")]
            detail = {
                "source_sha": SOURCE_SHA, "run_id": PRODUCT_RUN, "run_attempt": ATTEMPT,
                "ctest_outcome": doc["outcomes"].get("ctest"),
                "ctest_passed_count": sum(value is True for value in cases.values()),
                "ctest_total_count": len(cases), "ctest_matrix_ok": matrix_ok,
                "ChimeraUITests_passed": cases.get("ChimeraUITests"),
                "scoped_test_section_count": len(matched),
                "actual_original_markers": markers,
                "required_exact_patterns": evidence.contract_tests(spec, kind),
                "pattern_matches": [{"pattern": pattern, "matched": any(re.fullmatch(pattern, line) for line in actual.splitlines())}
                                    for pattern in evidence.contract_tests(spec, kind)["ChimeraUITests"]],
                "unchanged_contract_passed": evidence.contract_passed(spec, kind, cases, log),
                "regenerated_E1.STATE_UI": reports["E1.STATE_UI"],
                "original_full_log_sha256": digest(paths["dsp_log"].read_bytes()),
                "original_full_log_bytes": paths["dsp_log"].stat().st_size,
                "qualification": "Diagnostic only; original contract and strict gate acceptance unchanged."
            }
            save("diagnostic-E1.STATE_UI.json", detail)
            print("E1_STATE_UI_DIAGNOSTIC " + json.dumps(detail, ensure_ascii=False), flush=True)
    save("diagnostic-regenerated-automatic-reports.json", regenerated)
    print("REGENERATED_AUTOMATIC_REPORTS " + json.dumps(regenerated, ensure_ascii=False), flush=True)
    return original


def main():
    result["source_before"] = source_identity()
    save("source-before.json", result["source_before"])
    policy_paths = ["Validation/release-policy.json", "Validation/waivers.json",
                    "Validation/weekly-publication-policy.json", "Validation/ci-check-map.json"]
    result["policy_hashes_before"] = {p: digest((ROOT / p).read_bytes()) for p in policy_paths}
    release = release_tools.Release("1.3.1-beta.2", SOURCE_SHA)
    policy = release_tools.release_policy(release)
    save("source-release-policy.json", policy)
    api = release_tools.transport.GitHub()
    deadline = time.monotonic() + 1200
    while True:
        run = api.json("/actions/runs/" + str(PRODUCT_RUN))
        if run.get("head_sha") != SOURCE_SHA or run.get("run_attempt") != ATTEMPT or run.get("repository", {}).get("full_name") != release_tools.transport.REPO:
            raise RuntimeError("Product run identity changed")
        if run.get("status") == "completed":
            save("original-product-run.json", run)
            if run.get("conclusion") != "success":
                raise RuntimeError("Product run ended without success: " + str(run.get("conclusion")))
            break
        if time.monotonic() >= deadline:
            raise RuntimeError("Product run remains pending after bounded read-only wait")
        print("Waiting for original Product run 38350264755; no CI rerun or publication requested", flush=True)
        time.sleep(30)
    # This is the unchanged source-specific verifier. Do not patch its required workflows.
    ci = release_tools.verify_ci(api, release)
    if ci["runs"]["build.yml"]["id"] != PRODUCT_RUN or ci["runs"]["build.yml"]["run_attempt"] != ATTEMPT:
        raise RuntimeError("A different Product run superseded the pinned audit target")
    save("original-verify-ci.json", ci)
    result["verify_ci"] = {"result": "PASS", "workflows": {n: {k: r[k] for k in ("id", "run_attempt", "head_sha", "status", "conclusion")} for n, r in ci["runs"].items()}}
    print(json.dumps({"unchanged_verify_ci": result["verify_ci"]}), flush=True)
    gate_metadata = ci["gate"]
    gate_archive = download_evidence(api, [gate_metadata], release_tools.GATE_ARTIFACT_NAME, PRODUCT_RUN)
    original_gate = diagnose_gate(gate_archive, gate_metadata, policy, release)
    verdict = None
    try:
        verdict, decision = release_tools.verify_release_gate_archive(gate_archive, release, PRODUCT_RUN, ATTEMPT, policy, receipt=None)
        if decision is not None:
            raise RuntimeError("A publication decision was unexpectedly evaluated")
        save("verified-release-gate.json", verdict)
        result["gate"] = {k: verdict[k] for k in ("verdict", "ready", "counts", "hard_gates", "producer", "revision")}
        print(json.dumps({"unchanged_gate_verifier": result["gate"]}, ensure_ascii=False), flush=True)
    except Exception as exc:
        failure = {"stage": "unchanged_gate_verifier", "type": type(exc).__name__, "message": str(exc)}
        result["qualification_failures"].append(failure)
        save("strict-gate-error.json", failure)
        (OUTPUT / "strict-gate-error.log").write_text(traceback.format_exc(), encoding="utf-8")
        print("UNCHANGED_GATE_REJECTED " + json.dumps(failure), flush=True)
    # Run the unchanged CLI in offline dry-run mode; remove credentials from its environment.
    command = [sys.executable, str(ROOT / "Tools/publish_release.py"),
               "--dry-run", "--target-version", release.version, "--source-sha", SOURCE_SHA,
               "--gate-archive", str(gate_archive), "--run-id", str(PRODUCT_RUN), "--run-attempt", str(ATTEMPT)]
    env = dict(os.environ)
    env.pop("GH_TOKEN", None)
    env.pop("GITHUB_TOKEN", None)
    env["PYTHONDONTWRITEBYTECODE"] = "1"
    completed = subprocess.run(command, cwd=ROOT, env=env, text=True, capture_output=True)
    (OUTPUT / "unchanged-dry-run.stdout.log").write_text(completed.stdout, encoding="utf-8")
    (OUTPUT / "unchanged-dry-run.stderr.log").write_text(completed.stderr, encoding="utf-8")
    result["dry_run"] = {"returncode": completed.returncode, "stdout": completed.stdout, "stderr": completed.stderr}
    print("UNCHANGED_DRY_RUN " + json.dumps(result["dry_run"], ensure_ascii=False), flush=True)
    if completed.returncode != 0:
        result["qualification_failures"].append({"stage": "unchanged_offline_dry_run", "returncode": completed.returncode,
                                               "message": "Original offline dry-run rejected the evidence; failure retained."})
    else:
        dry = release_tools.parse_json(completed.stdout)
        if dry.get("published") is not False or dry.get("publication_ready") is not False or dry.get("consolidated_acceptance") != original_gate["verdict"]:
            raise RuntimeError("Unexpected dry-run publication or gate claim")
        save("unchanged-dry-run.json", dry)
    groups = [
        (38350264747, ["Chimera-document-upgrade-ubuntu-24.04", "Chimera-document-upgrade-windows-inno-fixtures", "Chimera-document-upgrade-macos-15"]),
        (38350264864, ["Chimera-candidate-installer-verification"]),
        (PRODUCT_RUN, ["Chimera-Windows-installer-verification", "Chimera-release-evidence-windows", "Chimera-release-evidence-linux", "Chimera-release-evidence-macos", "Chimera-release-evidence-assembly"]),
        (38350264721, ["CAB-realtime-" + p + "-" + SOURCE_SHA for p in ("Windows", "Linux", "macOS")]),
    ]
    for run_id, names in groups:
        metadata = api.pages("/actions/runs/" + str(run_id) + "/artifacts", "artifacts")
        save("original-artifact-metadata-" + str(run_id) + ".json", metadata)
        for name in names:
            download_evidence(api, metadata, name, run_id)
    # Recheck the original CI identities without accepting replacement evidence during downloads.
    if release_tools.ci_identity(release_tools.verify_ci(api, release)) != release_tools.ci_identity(ci):
        raise RuntimeError("Required run/attempt or candidate/gate artifact identity changed during audit")
    result["source_after"] = source_identity()
    save("source-after.json", result["source_after"])
    result["policy_hashes_after"] = {p: digest((ROOT / p).read_bytes()) for p in policy_paths}
    if result["policy_hashes_after"] != result["policy_hashes_before"]:
        raise RuntimeError("Policy bytes changed")
    result["result"] = "AUDIT_FAILED" if result["qualification_failures"] else "AUDIT_COMPLETED"
    result["release_qualification"] = "Strict gate and unchanged dry-run results are preserved verbatim; this audit grants no approval and publishes nothing."
    result["installation_scope"] = "Archive verification is not an additional installation or DAW test. Distinguish actual previous-release upgrade reports from synthetic document fixtures."
    return 1 if result["qualification_failures"] else 0

exit_code = 1
try:
    exit_code = main()
except Exception as exc:
    result["result"] = "AUDIT_FAILED"
    result["error"] = {"type": type(exc).__name__, "message": str(exc)}
    (OUTPUT / "audit-error.log").write_text(traceback.format_exc(), encoding="utf-8")
    print(json.dumps({"audit_error": result["error"]}), flush=True)
finally:
    try:
        result["source_final"] = source_identity()
    except Exception as exc:
        result["source_final_error"] = str(exc)
        result["result"] = "AUDIT_FAILED"
        exit_code = 1
    policy_paths = ["Validation/release-policy.json", "Validation/waivers.json",
                    "Validation/weekly-publication-policy.json", "Validation/ci-check-map.json"]
    result["policy_hashes_final"] = {p: digest((ROOT / p).read_bytes()) for p in policy_paths}
    if result.get("policy_hashes_before") != result["policy_hashes_final"]:
        result["result"] = "AUDIT_FAILED"
        exit_code = 1
    print("SOURCE_AND_POLICY_FINAL " + json.dumps({key: result.get(key) for key in
          ("source_before", "source_after", "source_final", "source_final_error", "policy_hashes_before", "policy_hashes_after", "policy_hashes_final")}), flush=True)
    save("audit-result.json", result)
    index = [{"path": str(p.relative_to(OUTPUT)), "bytes": p.stat().st_size,
              "sha256": digest(p.read_bytes())}
             for p in sorted(OUTPUT.rglob("*")) if p.is_file() and p.name != "audit-file-index.json"]
    save("audit-file-index.json", index)
    print(json.dumps({"audit_result": result["result"], "source": SOURCE_SHA,
                      "gate": result.get("gate"), "verified_artifact_count": len(result["verified_artifacts"]),
                      "published": False}, ensure_ascii=False), flush=True)
raise SystemExit(exit_code)
