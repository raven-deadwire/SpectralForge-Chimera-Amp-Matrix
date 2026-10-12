#!/usr/bin/env python3
"""Run the real Windows VST3 host and reject incomplete shutdown evidence.

This is an ABI regression, not Cakewalk/Cubase acceptance. Synthetic inputs in
the companion unit tests only test this verifier's rejection behavior.
"""
from __future__ import annotations

import argparse
from collections import Counter
from datetime import datetime, timezone
import hashlib
import json
import math
from pathlib import Path
import re
import subprocess


SCENARIOS = (
    "headless", "editor-pumping", "editor-pending", "multiple-instances", "reopen-project",
    "live-remove-controller-first", "live-remove-component-first", "headless-component-first",
    "editor-closed-before-project-controller-first", "editor-closed-before-project-component-first",
    "remove-last-controller-first", "remove-last-component-first",
)
MODULE_PHASES = ("module.load_library", "module.init", "factory.get", "factory.release",
                 "module.exit", "module.free_library", "module.unmapped_check", "host.after_unload_pump")
INSTANCE_PHASES = ("instance.create", "component.deactivate", "component.terminate",
                   "component.release", "controller.terminate", "controller.release")
EDITOR_PHASES = ("editor.create", "editor.attach", "view.removed", "view.release", "host_window.destroy")
# Existing JUCE String(double, 0) traces may use decimal/scientific notation.
# Accept those observations as well as the precise integral format now emitted.
TRACE_LINE = re.compile(r"^(\d+(?:\.\d+)?(?:[eE][+-]?\d+)?) us thread=([0-9a-f]+) instance=([0-9a-f]+) ([\w.]+)(.*)$")


class EvidenceError(ValueError):
    pass


def require(condition, message):
    if not condition:
        raise EvidenceError(message)


def unique_object(pairs):
    result = {}
    for key, value in pairs:
        require(key not in result, f"Duplicate JSON field: {key}")
        result[key] = value
    return result


def parse_json(text):
    return json.loads(text, object_pairs_hook=unique_object)


def raw_file(directory, name):
    require(isinstance(name, str) and name and Path(name).name == name
            and name not in (".", "..") and "/" not in name and "\\" not in name,
            f"Unsafe evidence filename: {name!r}")
    path = directory / name
    require(path.is_file() and not path.is_symlink(), f"Missing evidence file: {name}")
    require(path.stat().st_size > 0, f"Empty evidence file: {name}")
    return path


def digest(path):
    value = hashlib.sha256()
    with path.open("rb") as handle:
        for block in iter(lambda: handle.read(1024 * 1024), b""):
            value.update(block)
    return {"bytes": path.stat().st_size, "sha256": value.hexdigest()}


def verify_events(events, scenario, pid):
    """Check actual returned operations, not a PASS string or a destructor begin."""
    require(events, f"{scenario}: no child events")
    pending, completed = {}, []
    last_seq = 0
    for event in events:
        require(event.get("schema") == 1 and event.get("scenario") == scenario
                and event.get("pid") == pid, f"{scenario}: event identity mismatch")
        seq = event.get("seq")
        require(type(seq) is int and seq > last_seq, f"{scenario}: duplicate/reordered event sequence")
        last_seq = seq
        require(type(event.get("tid")) is int and event["tid"] > 0, f"{scenario}: missing native TID")
        require(type(event.get("monotonic_us")) is int and event["monotonic_us"] >= 0,
                f"{scenario}: invalid event clock")
        edge, op = event.get("edge"), event.get("operation_id")
        if edge == "begin":
            require(type(op) is int and op > 0 and op not in pending,
                    f"{scenario}: invalid/duplicate operation begin")
            require(not any(old[0]["operation_id"] == op for old in completed),
                    f"{scenario}: reused operation ID")
            pending[op] = event
        elif edge == "return":
            require(op in pending, f"{scenario}: return without begin")
            start = pending.pop(op)
            for key in ("phase", "instance", "round", "tid"):
                require(event.get(key) == start.get(key), f"{scenario}: mismatched {key} at return")
            require(event["monotonic_us"] >= start["monotonic_us"]
                    and type(event.get("elapsed_us")) is int and event["elapsed_us"] >= 0,
                    f"{scenario}: invalid return duration")
            completed.append((start, event))
        else:
            require(edge == "point" and op == 0, f"{scenario}: unknown event edge")
    require(not pending, f"{scenario}: pending operations: {[x['phase'] for x in pending.values()]}")
    returns = [end for _, end in completed]
    rounds = 3 if scenario == "reopen-project" else 1
    seen_instances = set()

    def operations(phase, number, instance=None):
        return sorted((pair for pair in completed if pair[0]["round"] == number
                       and pair[0]["phase"] == phase
                       and (instance is None or pair[0]["instance"] == instance)),
                      key=lambda pair: pair[0]["seq"])

    def one(phase, number, instance=None):
        found = operations(phase, number, instance)
        require(len(found) == 1, f"{scenario}: expected one round {number} instance {instance} {phase}")
        return found[0]

    def before(first, second):
        require(first[1]["seq"] < second[0]["seq"],
                f"{scenario}: {first[0]['phase']} did not return before {second[0]['phase']} began")

    def within(inner, outer):
        require(outer[0]["seq"] < inner[0]["seq"] < inner[1]["seq"] < outer[1]["seq"],
                f"{scenario}: {inner[0]['phase']} outside {outer[0]['phase']}")

    for start, _ in completed:
        if start["phase"] in MODULE_PHASES + INSTANCE_PHASES + EDITOR_PHASES:
            require(type(start.get("round")) is int and 1 <= start["round"] <= rounds,
                    f"{scenario}: lifetime operation outside expected rounds")
    for number in range(1, rounds + 1):
        by_phase = {phase: [e for e in returns if e["round"] == number and e["phase"] == phase]
                    for phase in MODULE_PHASES + INSTANCE_PHASES + EDITOR_PHASES}
        for phase in MODULE_PHASES:
            require(len(by_phase[phase]) == 1, f"{scenario}: missing/duplicate round {number} {phase}")
        require([by_phase[p][0]["seq"] for p in MODULE_PHASES]
                == sorted(by_phase[p][0]["seq"] for p in MODULE_PHASES),
                f"{scenario}: DLL unload boundaries out of order")
        module = {phase: one(phase, number) for phase in MODULE_PHASES}
        for first, second in zip(MODULE_PHASES, MODULE_PHASES[1:]):
            before(module[first], module[second])
        project_close = one("project.close", number)
        before(project_close, module["factory.release"])
        for phase in INSTANCE_PHASES:
            require(by_phase[phase], f"{scenario}: missing returned {phase}")
        created = Counter(e["instance"] for e in by_phase["instance.create"])
        expected_instances = (6 if scenario.startswith("live-remove") else
                              3 if scenario == "multiple-instances" else
                              2 if scenario.startswith("remove-last") else 1)
        require(len(created) == expected_instances and all(count == 1 for count in created.values())
                and all(type(identity) is int and identity > 0 and identity not in seen_instances for identity in created),
                f"{scenario}: wrong instance count or reused identity in round {number}")
        seen_instances.update(created)
        for phase in INSTANCE_PHASES[1:]:
            require(Counter(e["instance"] for e in by_phase[phase]) == created,
                    f"{scenario}: incomplete instance lifetime at {phase}")
        for phase in EDITOR_PHASES:
            if scenario.startswith("headless"):
                require(not by_phase[phase], f"{scenario}: headless control created an editor")
            else:
                require(by_phase[phase], f"{scenario}: missing returned {phase}")
        if not scenario.startswith("headless"):
            opened = Counter(e["instance"] for e in by_phase["editor.create"])
            expected_editors = 12 if scenario.startswith("live-remove") else expected_instances
            require(set(opened) == set(created) and sum(opened.values()) == expected_editors,
                    f"{scenario}: wrong editor coverage in round {number}")
            for phase in EDITOR_PHASES[1:]:
                require(Counter(e["instance"] for e in by_phase[phase]) == opened,
                        f"{scenario}: incomplete editor lifetime at {phase}")
        removals = operations("plugin.remove", number)
        recreations = operations("instance.recreate", number)
        empty_idle = operations("project.empty_idle", number)
        early_idle = operations("project.idle_after_editor_close", number)
        expected_removals = 3 if scenario.startswith("live-remove") else 2 if scenario.startswith("remove-last") else 0
        expected_recreations = 3 if scenario.startswith("live-remove") else 1 if scenario.startswith("remove-last") else 0
        require(len(removals) == expected_removals and len(recreations) == expected_recreations,
                f"{scenario}: wrong remove/recreate coverage")
        require(len(empty_idle) == (2 if scenario.startswith("remove-last") else 0)
                and len(early_idle) == (1 if scenario.startswith("editor-closed-before-project") else 0),
                f"{scenario}: wrong idle-stage coverage")
        removed_ids = [pair[0]["instance"] for pair in removals]
        require(len(set(removed_ids)) == len(removed_ids) and set(removed_ids) <= set(created),
                f"{scenario}: removal did not target unique existing instances")
        for identity in created:
            lifetime = {phase: one(phase, number, identity) for phase in INSTANCE_PHASES}
            before(module["factory.get"], lifetime["instance.create"])
            before(lifetime["instance.create"], lifetime["component.deactivate"])
            for kind in ("component", "controller"):
                before(lifetime["component.deactivate"], lifetime[f"{kind}.terminate"])
                before(lifetime[f"{kind}.terminate"], lifetime[f"{kind}.release"])
                before(lifetime[f"{kind}.release"], module["factory.release"])
            # A removed victim belongs to its removal operation. Retained
            # instances must stay alive until the actual project-close call.
            destruction = next((pair for pair in removals if pair[0]["instance"] == identity), project_close)
            for phase in INSTANCE_PHASES[1:]:
                within(lifetime[phase], destruction)
            # The live-removal scenarios deliberately mix the final survivors'
            # order. Their named order applies to the removed victim only.
            if not scenario.startswith("live-remove") or identity in removed_ids:
                if scenario.endswith("component-first"):
                    before(lifetime["component.release"], lifetime["controller.terminate"])
                elif scenario.endswith("controller-first"):
                    before(lifetime["controller.release"], lifetime["component.terminate"])
            creator_thread = lifetime["instance.create"][0]["tid"]
            for phase in INSTANCE_PHASES + EDITOR_PHASES:
                for operation in operations(phase, number, identity):
                    require(operation[0]["tid"] == creator_thread,
                            f"{scenario}: {phase} moved off the creating UI thread")
            joins = operations("audio.worker_join", number, identity)
            require(joins, f"{scenario}: instance {identity} never completed audio work")
            for joined in joins:
                before(lifetime["instance.create"], joined)
                before(joined, lifetime["component.deactivate"])
            editor_cycles = [operations(phase, number, identity) for phase in EDITOR_PHASES]
            previous = lifetime["instance.create"]
            for cycle in zip(*editor_cycles):
                for operation in cycle:
                    before(previous, operation)
                    previous = operation
                before(previous, lifetime["component.deactivate"])
        for recreation in recreations:
            replacements = [operation for operation in operations("instance.create", number)
                            if recreation[0]["seq"] < operation[0]["seq"] < recreation[1]["seq"]]
            require(len(replacements) == 1, f"{scenario}: recreation did not construct exactly one instance")
            within(replacements[0], recreation)
        if scenario.startswith("remove-last"):
            require(set(removed_ids) == set(created), f"{scenario}: project did not become empty")
            chain = (removals[0], empty_idle[0], recreations[0], removals[1], empty_idle[1], project_close)
            for first, second in zip(chain, chain[1:]):
                before(first, second)
        elif scenario.startswith("live-remove"):
            for index, (removal, recreation) in enumerate(zip(removals, recreations)):
                before(removal, recreation)
                before(recreation, removals[index + 1] if index + 1 < len(removals) else project_close)
        elif early_idle:
            for closed_view in operations("host_window.destroy", number):
                before(closed_view, early_idle[0])
            before(early_idle[0], project_close)
        for end in returns:
            if end["round"] == number and end["phase"] in INSTANCE_PHASES + EDITOR_PHASES:
                require(end["seq"] < by_phase["module.exit"][0]["seq"],
                        f"{scenario}: object lifetime extends past module exit")
    require(any(e["phase"] == "host.return_ready" and e["edge"] == "point" for e in events),
            f"{scenario}: host return boundary missing")
    require(any(e["phase"] == "host.com_uninitialize" for e in returns),
            f"{scenario}: COM teardown did not return")
    if scenario.startswith("editor-closed-before-project"):
        needed = ("view.release", "project.idle_after_editor_close", "project.close")
    elif scenario.startswith("remove-last"):
        needed = ("plugin.remove", "project.empty_idle", "instance.recreate", "project.close")
    elif scenario.startswith("live-remove"):
        needed = ("plugin.remove", "project.close")
    else:
        needed = ("project.close",)
    for phase in needed:
        require(any(e["phase"] == phase for e in returns), f"{scenario}: missing returned {phase}")
    require([next(e["seq"] for e in returns if e["phase"] == phase) for phase in needed]
            == sorted(next(e["seq"] for e in returns if e["phase"] == phase) for phase in needed),
            f"{scenario}: scenario boundaries out of order")
    require(any(e["phase"] == "audio.worker_join" for e in returns), f"{scenario}: no audio worker completion")
    return {"events": len(events), "completed_operations": len(completed), "rounds": rounds,
            "instances": sum(e["phase"] == "instance.create" for e in returns),
            "editors": sum(e["phase"] == "editor.create" for e in returns)}


def verify_plugin_trace(path, scenario, pid, expected_instances=None, expected_editors=None):
    """Pair object/worker scopes and preserve the body/members/base distinction."""
    pending, processors, editors, owners, workers = {}, {}, {}, {}, set()
    counts = Counter()
    for line in path.read_text(encoding="utf-8-sig").splitlines():
        match = TRACE_LINE.fullmatch(line)
        require(match is not None, f"{scenario}: malformed plugin trace")
        timestamp, thread, instance, name, suffix = match.groups()
        timestamp = float(timestamp)
        require(math.isfinite(timestamp) and timestamp >= 0, f"{scenario}: invalid plugin trace clock")
        fields = dict(item.split("=", 1) for item in suffix.split())
        require(fields.get("pid") == str(pid), f"{scenario}: plugin trace PID mismatch")
        require(fields.get("tid", "").isdigit() and int(fields["tid"]) > 0,
                f"{scenario}: missing plugin native TID")
        counts[name] += 1
        if name.endswith(".begin"):
            key = (name[:-6], instance, thread)
            require(key not in pending, f"{scenario}: duplicate trace begin {name}")
            pending[key] = timestamp
        elif name.endswith(".end") and not name.endswith(".members.end"):
            key = (name[:-4], instance, thread)
            require(key in pending and timestamp >= pending.pop(key),
                    f"{scenario}: unpaired trace end {name}")
        if name == "processor.create.body.begin":
            require(instance not in processors, f"{scenario}: processor address reused before destruction")
            processors[instance] = "creating"
        elif name == "processor.create.body.end":
            require(processors.get(instance) == "creating", f"{scenario}: processor construction missing")
            processors[instance] = "alive"
        elif name in ("ir.owner", "tuner.owner"):
            owner = fields.get("related")
            require(owner in processors, f"{scenario}: worker owner missing")
            owners[(name.split(".")[0], instance)] = owner
        elif name in ("ir.worker.begin", "tuner.worker.begin"):
            worker = (name.split(".")[0], instance)
            require(worker in owners and worker not in workers, f"{scenario}: unowned/duplicate worker")
            workers.add(worker)
        elif name in ("ir.worker.end", "tuner.worker.end"):
            worker = (name.split(".")[0], instance)
            require(worker in workers, f"{scenario}: worker exit without entry")
            workers.remove(worker)
        elif name in ("ir.stop.end", "tuner.stop.end"):
            require((name.split(".")[0], instance) not in workers,
                    f"{scenario}: stop returned with worker still active")
        elif name == "editor.create.body.begin":
            require(not scenario.startswith("headless"), f"{scenario}: plugin unexpectedly created editor")
            require(instance not in editors and fields.get("related") in processors,
                    f"{scenario}: editor owner/lifetime missing")
            editors[instance] = {"owner": fields["related"], "body_ended": False}
        elif name == "editor.destroy.body.end":
            require(instance in editors, f"{scenario}: editor destruction without creation")
            editors[instance]["body_ended"] = True
        elif name == "editor.destroy.members.end":
            require(instance in editors and editors[instance]["body_ended"],
                    f"{scenario}: editor member boundary missing body end")
            del editors[instance]
        elif name == "processor.destroy.body.end":
            require(processors.get(instance) == "alive", f"{scenario}: processor destruction without creation")
            processors[instance] = "body_ended"
        elif name == "processor.destroy.members.end":
            require(processors.get(instance) == "body_ended", f"{scenario}: processor member boundary missing body end")
            require(not any(e["owner"] == instance for e in editors.values()), f"{scenario}: live editor at processor destruction")
            require(not any(owners[w] == instance for w in workers), f"{scenario}: live worker at processor destruction")
            del processors[instance]
        if name.startswith(("processor.", "editor.")):
            require("message_thread" in fields, f"{scenario}: missing message-thread context")
    require(not pending and not processors and not editors and not workers,
            f"{scenario}: unfinished plugin lifetime scopes")
    for name in ("processor.create.body.end", "processor.destroy.members.end", "ir.stop.end", "tuner.stop.end",
                 "ir.worker.end", "tuner.worker.end"):
        require(counts[name] > 0, f"{scenario}: missing plugin event {name}")
    if not scenario.startswith("headless"):
        require(counts["editor.destroy.members.end"] > 0, f"{scenario}: no editor member destruction")
    if expected_instances is not None:
        require(counts["processor.create.body.end"] == expected_instances
                and counts["processor.destroy.members.end"] == expected_instances,
                f"{scenario}: plugin/host instance counts differ")
    if expected_editors is not None:
        require(counts["editor.create.body.end"] == expected_editors
                and counts["editor.destroy.members.end"] == expected_editors,
                f"{scenario}: plugin/host editor counts differ")
    return dict(sorted(counts.items()))


def verify(directory, expected_parent_pid=None):
    summary = parse_json(raw_file(directory, "summary.json").read_text(encoding="utf-8-sig"))
    require(summary.get("schema") == 1, "Unsupported summary schema")
    parent = summary.get("parent_pid")
    require(type(parent) is int and parent > 0, "Missing parent PID")
    if expected_parent_pid is not None:
        require(parent == expected_parent_pid, "Stale evidence: parent PID mismatch")
    rows = summary.get("scenarios", [])
    require(isinstance(rows, list) and len(rows) == len(SCENARIOS)
            and Counter(r.get("scenario") for r in rows) == Counter(SCENARIOS),
            "Missing, duplicate or unknown scenario; single-scenario runs are diagnostic only")
    require(summary.get("full_suite") is True and summary.get("total") == len(SCENARIOS)
            and summary.get("passed") == len(SCENARIOS),
            "Native host did not report the full passing matrix")
    all_events = [parse_json(line) for line in raw_file(directory, "events.jsonl").read_text(encoding="utf-8-sig").splitlines()]
    results, files = [], {"summary.json", "events.jsonl"}
    for row in rows:
        name, pid = row["scenario"], row.get("pid")
        require(type(pid) is int and pid > 0 and pid != parent, f"{name}: invalid child PID")
        require(type(row.get("exit_code")) is int and row["exit_code"] == 0
                and row.get("timed_out") is False and row.get("force_terminated") is False
                and row.get("process_exit_confirmed") is True and row.get("return_ready_seen") is True
                and row.get("capture_error") is False,
                f"{name}: failure, watchdog or unconfirmed OS process exit")
        require(row.get("watchdog_ms") == 45000 and type(row.get("elapsed_ms")) is int
                and row["elapsed_ms"] >= 0, f"{name}: invalid watchdog/timing record")
        require(row.get("pending_phases") == [], f"{name}: pending operations at exit")
        log = raw_file(directory, row.get("log_file"))
        trace = raw_file(directory, row.get("plugin_trace_file"))
        files.update((log.name, trace.name))
        events = [e for e in all_events if e.get("pid") == pid and e.get("scenario") == name]
        # Aggregate events must be the actual child stream, not a separately
        # manufactured successful summary. Parent observations are independent.
        from_log = [parse_json(line[len("LIFECYCLE_EVENT "):]) for line in log.read_text(encoding="utf-8-sig").splitlines()
                    if line.startswith("LIFECYCLE_EVENT ")]
        require(events == from_log, f"{name}: aggregate events differ from raw child log")
        exit_events = [e for e in all_events if e.get("pid") == parent and e.get("scenario") == name]
        require(len(exit_events) == 1 and exit_events[0].get("phase") == "host.process_exit"
                and exit_events[0].get("edge") == "point" and exit_events[0].get("child_pid") == pid
                and exit_events[0].get("process_exit_confirmed") is True
                and exit_events[0].get("exit_code") == 0 and exit_events[0].get("timed_out") is False,
                f"{name}: independent parent process-exit evidence missing")
        host_result = verify_events(events, name, pid)
        results.append({"scenario": name, "pid": pid, **host_result,
                        "plugin_events": verify_plugin_trace(trace, name, pid, host_result["instances"], host_result["editors"])})
    identities = {(row["scenario"], row["pid"]) for row in rows} | {(name, parent) for name in SCENARIOS}
    require(all((e.get("scenario"), e.get("pid")) in identities for e in all_events), "Unexpected process/scenario in event aggregate")
    return {"scenarios": results, "raw_files": [{"path": name, **digest(directory / name)} for name in sorted(files)]}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--host", type=Path, required=True)
    parser.add_argument("--plugin", type=Path, required=True)
    parser.add_argument("--evidence-dir", type=Path, required=True)
    parser.add_argument("--build-metadata", type=Path, required=True)
    parser.add_argument("--source-sha", required=True)
    args = parser.parse_args()
    directory = args.evidence_dir.resolve()
    directory.mkdir(parents=True, exist_ok=True)
    report = {"schema": 1, "status": "FAIL", "scope": "Windows VST3 ABI lifecycle regression",
              "commercial_daw_acceptance": {"Cakewalk Sonar": "NOT_RUN", "Cubase 10.5": "NOT_RUN"},
              "issue_3_resolved": False, "started_utc": datetime.now(timezone.utc).isoformat()}
    try:
        metadata = parse_json(args.build_metadata.read_text(encoding="utf-8-sig"))
        require(re.fullmatch(r"[0-9a-f]{40}", args.source_sha) is not None
                and metadata.get("source_sha") == args.source_sha, "Build metadata/source SHA mismatch")
        report.update(build=metadata, build_metadata=digest(args.build_metadata),
                      host_binary=digest(args.host), plugin_binary=digest(args.plugin))
        output = directory / "host-output.log"
        with output.open("w", encoding="utf-8") as handle:
            process = subprocess.Popen([str(args.host.resolve()), str(args.plugin.resolve()),
                                        "--evidence-dir", str(directory)], stdout=handle, stderr=subprocess.STDOUT)
            report["parent_pid"] = process.pid
            code = process.wait()  # Native child watchdog + outer CTest timeout bound execution.
        report["exit_code"] = code
        require(code == 0, f"Native lifecycle host exited {code}; inspect host-output.log and pending phases")
        report.update(verify(directory, process.pid))
        require(report["host_binary"] == digest(args.host) and report["plugin_binary"] == digest(args.plugin),
                "Binary changed during lifecycle execution")
        report["raw_files"].append({"path": output.name, **digest(output)})
        report["status"] = "PASS"
    except (EvidenceError, OSError, ValueError, KeyError, TypeError) as error:
        report["error"] = str(error)
    report["finished_utc"] = datetime.now(timezone.utc).isoformat()
    (directory / "verification.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(f"{report['status']}: VST3 lifecycle evidence; Cakewalk/Cubase NOT_RUN", flush=True)
    if report["status"] != "PASS":
        print(report.get("error", "unknown failure"), flush=True)
        output = directory / "host-output.log"
        if output.is_file():
            print(output.read_text(encoding="utf-8", errors="replace")[-8000:], flush=True)
    raise SystemExit(0 if report["status"] == "PASS" else 1)


if __name__ == "__main__":
    main()
