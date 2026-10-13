#!/usr/bin/env python3
"""Independent integrity verifier for CAB callback attribution evidence.

Only the standard library is used. No probe/controller summarizer is imported.
Exit 0 means complete, internally consistent diagnostic evidence; it is never
product acceptance, a causal performance finding or actual audio-host validation.
Exit 2 means invalid/inconsistent evidence. Exit 3 means missing/incomplete
evidence. The verifier never edits inputs or imputes missing observations.
"""
from __future__ import annotations

import argparse
from collections import Counter, defaultdict
import csv
import hashlib
import json
import math
from pathlib import Path
import re
from typing import Any

SCHEMA_VERSION = 1
CALLBACK_COUNT = 1600
WORKER_CALLBACKS = 1200
SAMPLE_RATE = 96000
BLOCK_SIZE = 64
NO_CALLBACK = (1 << 64) - 1
LIMITATIONS = [
    "Complete means evidence integrity only. Original uninstrumented acceptance remains separate.",
    "Wall and CPU reads have different clock envelopes. CPU may exceed wall; their difference is not scheduler time.",
    "Inclusive stages overlap their children. Parent remainders subtract direct children only and retain clock-boundary cost.",
    "Worker/event overlap is descriptive and does not identify causation, preemption, cache misses or a hypervisor event.",
    "Logical byte counts describe authored operations, not unique memory, cache traffic or measured hardware bandwidth.",
    "Resource-query, stage, compiled-out and synthetic populations are kept separate; none are subtracted from another.",
    "Missing or unavailable observations are not measured zeros. Console evidence does not validate AU/VST3/CoreAudio/DAW playback.",
    "Route source/compiler fields are producer receipts. Authenticate source/build provenance with the separately preserved source manifest and binary receipts.",
]


class InvalidEvidence(ValueError):
    pass


def require(condition: bool, message: str) -> None:
    if not condition:
        raise InvalidEvidence(message)


def load_json(path: Path) -> dict[str, Any]:
    def unique(pairs):
        result = {}
        for key, value in pairs:
            require(key not in result, f"duplicate JSON key {key!r}")
            result[key] = value
        return result

    def nonfinite(value):
        raise InvalidEvidence(f"non-finite JSON constant {value}")

    value = json.loads(path.read_text(encoding="utf-8"),
                       object_pairs_hook=unique, parse_constant=nonfinite)
    require(isinstance(value, dict), "metadata must be a JSON object")
    return value


def rows_csv(path: Path, fields: tuple[str, ...] | list[str]) -> list[dict[str, str]]:
    with path.open(newline="", encoding="utf-8") as handle:
        reader = csv.DictReader(handle)
        require(reader.fieldnames is not None, f"{path.name}: missing CSV header")
        require(len(reader.fieldnames) == len(set(reader.fieldnames)),
                f"{path.name}: duplicate CSV columns")
        require(set(fields).issubset(reader.fieldnames),
                f"{path.name}: missing columns {sorted(set(fields) - set(reader.fieldnames))}")
        result = []
        for index, row in enumerate(reader):
            require(None not in row and all(value is not None for value in row.values()),
                    f"{path.name}: malformed CSV row {index}")
            result.append(row)
        return result


def integer(value: Any, label: str, minimum: int | None = None) -> int:
    require(not isinstance(value, bool), f"{label}: boolean is not an integer observation")
    if isinstance(value, int):
        result = value
    else:
        require(isinstance(value, str) and re.fullmatch(r"-?\d+", value) is not None,
                f"{label}: invalid integer {value!r}")
        result = int(value)
    require(minimum is None or result >= minimum, f"{label}: below minimum {minimum}")
    return result


def boolean(value: Any, label: str) -> bool:
    require(isinstance(value, (bool, int, str)) and value in (True, False, 0, 1, "0", "1"),
            f"{label}: expected explicit boolean/0/1")
    return value is True or value == 1 or value == "1"


def number(value: Any, label: str) -> float:
    require(not isinstance(value, bool), f"{label}: boolean is not a numeric observation")
    try:
        result = float(value)
    except (TypeError, ValueError) as error:
        raise InvalidEvidence(f"{label}: invalid number {value!r}") from error
    require(math.isfinite(result), f"{label}: non-finite number")
    return result


def approximately(observed: float, expected: float, label: str) -> None:
    # Raw timestamps are integral nanoseconds; displayed microseconds allow
    # roundoff only, never tolerance proportional to a callback deadline.
    require(math.isclose(observed, expected, rel_tol=2e-12, abs_tol=1e-6),
            f"{label}: {observed!r} does not match recomputed {expected!r}")


def require_fields(value: dict[str, Any], fields, label: str) -> None:
    require(all(key in value for key in fields),
            f"{label}: missing fields {sorted(set(fields) - set(value))}")


def quantiles(values: list[float]) -> dict[str, Any]:
    ordered = sorted(values)
    if not ordered:
        return {"samples": 0, "p50_us": None, "p99_us": None, "maximum_us": None}
    return {"samples": len(ordered), "p50_us": ordered[int(len(ordered) * .5)],
            "p99_us": ordered[int(len(ordered) * .99)], "maximum_us": ordered[-1]}


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for block in iter(lambda: handle.read(1 << 20), b""):
            digest.update(block)
    return digest.hexdigest()


def expected_values(value, numeric: bool = False):
    if value is None:
        return None
    if isinstance(value, str):
        value = [part.strip() for part in value.split(",") if part.strip()]
    elif isinstance(value, int):
        value = [value]
    else:
        value = list(value)
    require(bool(value), "expected route set must not be empty")
    return {integer(item, "expected channel", 1) if numeric else str(item) for item in value}


EVENT_FIELDS = (
    "schema_version,event_id,parent_id,depth,callback_index,lane,mic,stage,role,model_key,generation,logical_bytes,"
    "wall_start_ns,wall_end_ns,cpu_start_ns,cpu_end_ns,wall_valid,cpu_valid,wall_ns,cpu_ns,"
    "wall_start_error,wall_end_error,cpu_start_error,cpu_end_error,complete,worker_sequence_start,worker_sequence_end,"
    "wall_start_valid,wall_end_valid,cpu_start_valid,cpu_end_valid"
).split(",")
CALLBACK_FIELDS = (
    "schema_version,mode,engine,channels,sample_rate,block_size,callback_index,phase,phase_index,"
    "automation_requested,publication_requested,wall_start_ns,wall_end_ns,wall_ns,cpu_start_ns,cpu_end_ns,cpu_ns,"
    "cpu_valid,wall_valid,wall_start_error,wall_end_error,cpu_start_error,cpu_end_error,deadline_ns,deadline_miss,"
    "entry_wall_ns,exit_wall_ns,entry_cpu_ns,exit_cpu_ns,envelope_wall_ns,envelope_cpu_ns,"
    "worker_seq_before,worker_seq_after,worker_state_before,worker_state_after,worker_lane_before,worker_lane_after,"
    "worker_mic_before,worker_mic_after,trace_event_first,trace_event_count,resource_scope,resource_valid,"
    "minor_faults_delta,major_faults_delta,voluntary_switches_delta,involuntary_switches_delta,"
    "task_faults_delta,task_pageins_delta,thread_state_valid,thread_run_state_before,thread_run_state_after,"
    "thread_suspend_count_before,thread_suspend_count_after,resource_before_error,resource_after_error,"
    "thread_state_before_error,thread_state_after_error,resource_before_start_wall_ns,resource_before_end_wall_ns,"
    "resource_after_start_wall_ns,resource_after_end_wall_ns,resource_before_query_wall_ns,resource_after_query_wall_ns,"
    "allocations,deletions,checksum,wall_start_valid,wall_end_valid,cpu_start_valid,cpu_end_valid,"
    "entry_wall_valid,exit_wall_valid,entry_cpu_valid,exit_cpu_valid,entry_wall_error,exit_wall_error,entry_cpu_error,exit_cpu_error,"
    "worker_snapshot_before_valid,worker_snapshot_after_valid,resource_before_valid,resource_after_valid"
).split(",")
RESOURCE_COUNTERS = ("minor_faults", "major_faults", "voluntary_switches", "involuntary_switches",
                     "task_faults", "task_pageins")
for _counter in RESOURCE_COUNTERS:
    CALLBACK_FIELDS += [_counter + "_before", _counter + "_after"]
for _point in ("before", "after"):
    for _end in ("start", "end"):
        CALLBACK_FIELDS += [f"resource_{_point}_{_end}_cpu_ns",
                            f"resource_{_point}_{_end}_wall_valid", f"resource_{_point}_{_end}_wall_error",
                            f"resource_{_point}_{_end}_cpu_valid", f"resource_{_point}_{_end}_cpu_error"]
SLOT_FIELDS = (
    "schema_version,callback_index,point,lane,mic,requested_model_key,active_model_key,active_generation,"
    "fading_model_key,fading_generation,pending_present,pending_identity_available,fade_remaining,fade_length,rejected_models,"
    "active_present,fading_present"
).split(",")
STAGES = set((
    "callback,cab_process,parameters,kernel_swap,kernel_accept,kernel_reject,kernel_retire,filters,copy_dry,copy_second,"
    "copy_fade,mic_wake,reset,active_convolution,fading_convolution,convolution,fft_forward,fft_inverse,spectral_mac,"
    "input_pack,spectrum_pack,spectrum_unpack,history_clear,spectrum_copy,output_mix,overlap_add,overlap_copy,fade_mix,"
    "mic_post,delay_history,mic_gain,mic_blend,fractional_delay_mix,dry_mix,worker_build,response_generate,spectra_prepare,"
    "kernel_prepare,worker_publish,worker_collect,worker_wait,worker_run,worker_exit,worker_cancel,worker_error,"
    "kernel_destroy,scope_calibration,arithmetic_control,memory_control"
).split(","))
ROLES = {"none", "active", "fading", "pending", "rejected", "retired", "worker"}
ENGINES = {"original-v1", "expanded-v2", "array-v3-6x10"}
MODES = {"cab", "arithmetic", "memory", "clock"}
TRACE_LEVELS = {"off", "stages", "resources"}


def clock_pair(row: dict[str, Any], kind: str, label: str,
               incomplete: list[str]) -> tuple[int, int, int, int]:
    start = integer(row[f"{kind}_start_ns"], label + f" {kind} start", 0)
    end = integer(row[f"{kind}_end_ns"], label + f" {kind} end", 0)
    start_valid = boolean(row[f"{kind}_start_valid"], label + f" {kind} start valid")
    end_valid = boolean(row[f"{kind}_end_valid"], label + f" {kind} end valid")
    start_error = integer(row[f"{kind}_start_error"], label + f" {kind} start error")
    end_error = integer(row[f"{kind}_end_error"], label + f" {kind} end error")
    require(not start_valid or start_error == 0, label + f": valid {kind} start has error")
    require(not end_valid or end_error == 0, label + f": valid {kind} end has error")
    require(start_valid or start_error != 0, label + f": unavailable {kind} start has no error")
    require(end_valid or end_error != 0, label + f": unavailable {kind} end has no error")
    valid = start_valid and end_valid and end >= start
    require(boolean(row[f"{kind}_valid"], label + f" {kind} valid") == valid,
            label + f": inconsistent {kind} validity")
    delta = integer(row[f"{kind}_ns"], label + f" {kind} delta", -1)
    require(delta == (end - start if valid else -1), label + f": inconsistent {kind} delta")
    if not valid:
        incomplete.append(label + f": {kind} clock unavailable or non-monotonic")
    return start, end, delta, int(not start_valid) + int(not end_valid)


def event_stream(path: Path, stream: str, callbacks: list[dict[str, Any]],
                 incomplete: list[str]) -> tuple[list[dict[str, Any]], dict[str, Any]]:
    records = rows_csv(path, EVENT_FIELDS)
    parsed = []
    direct_children: dict[int, list[int]] = defaultdict(list)
    roots = []
    stack: list[dict[str, Any]] = []
    endpoint_errors = 0
    incomplete_events = 0
    for offset, record in enumerate(records):
        label = f"{path.name} event {offset + 1}"
        event = dict(record)
        for key in ("schema_version", "event_id", "parent_id", "depth", "callback_index", "model_key", "generation",
                    "logical_bytes", "worker_sequence_start", "worker_sequence_end"):
            event[key] = integer(record[key], label + " " + key, 0)
        for key in ("lane", "mic"):
            event[key] = integer(record[key], label + " " + key, -1)
        require(event["schema_version"] == SCHEMA_VERSION, label + ": wrong schema")
        require(event["event_id"] == offset + 1, label + ": event IDs must be complete one-based preorder")
        require(event["stage"] in STAGES and event["role"] in ROLES, label + ": unknown stage/role")
        require(event["lane"] in (-1, 0, 1, 2) and event["mic"] in (-1, 0, 1), label + ": invalid lane/mic")
        require((event["lane"] == -1) == (event["mic"] == -1), label + ": partial lane/mic identity")
        require(event["worker_sequence_start"] <= event["worker_sequence_end"], label + ": reversed worker sequence")
        if not boolean(record["complete"], label + " complete"):
            incomplete_events += 1
            incomplete.append(label + ": unfinished scope")
        for kind in ("wall", "cpu"):
            start, end, delta, errors = clock_pair(record, kind, label, incomplete)
            event[f"{kind}_start_ns"], event[f"{kind}_end_ns"], event[f"{kind}_ns"] = start, end, delta
            endpoint_errors += errors
        depth, parent = event["depth"], event["parent_id"]
        require(depth <= len(stack), label + ": skipped tree depth")
        stack = stack[:depth]
        require(parent == (stack[-1]["event_id"] if stack else 0), label + ": parent does not match preorder/depth")
        require(parent < event["event_id"], label + ": parent must precede child")
        if stack:
            outer = stack[-1]
            require(event["callback_index"] == outer["callback_index"], label + ": child crosses callback identity")
            for kind in ("wall", "cpu"):
                if event[f"{kind}_ns"] >= 0 and outer[f"{kind}_ns"] >= 0:
                    require(outer[f"{kind}_start_ns"] <= event[f"{kind}_start_ns"]
                            <= event[f"{kind}_end_ns"] <= outer[f"{kind}_end_ns"],
                            label + f": child outside parent {kind} envelope")
            require(outer["worker_sequence_start"] <= event["worker_sequence_start"]
                    <= event["worker_sequence_end"] <= outer["worker_sequence_end"],
                    label + ": child outside parent worker-sequence window")
        else:
            roots.append(offset)
        siblings = direct_children[parent]
        if siblings:
            previous = parsed[siblings[-1]]
            for kind in ("wall", "cpu"):
                if event[f"{kind}_ns"] >= 0 and previous[f"{kind}_ns"] >= 0:
                    require(previous[f"{kind}_end_ns"] <= event[f"{kind}_start_ns"],
                            label + f": overlapping sequential sibling {kind} scopes")
        siblings.append(offset)
        if stream == "stages":
            index = event["callback_index"]
            require(index < len(callbacks), label + ": missing callback association")
            callback = callbacks[index]
            for kind in ("wall", "cpu"):
                if event[f"{kind}_ns"] >= 0 and callback[f"{kind}_ns"] >= 0:
                    require(callback[f"{kind}_start_ns"] <= event[f"{kind}_start_ns"]
                            <= event[f"{kind}_end_ns"] <= callback[f"{kind}_end_ns"],
                            label + f": outside associated callback {kind} interval")
            if parent == 0:
                require(event["stage"] == "callback", label + ": callback root has wrong stage")
                require(event["worker_sequence_start"] <= callback["worker_seq_before"]
                        <= callback["worker_seq_after"] <= event["worker_sequence_end"],
                        label + ": callback snapshots outside root worker-sequence window")
            else:
                require(event["stage"] != "callback", label + ": nested callback root")
        else:
            require(event["callback_index"] == NO_CALLBACK, label + ": background/caller event invents callback ID")
            require(event["stage"] != "callback", label + ": non-callback stream has callback root")
        parsed.append(event)
        stack.append(event)

    summaries: dict[str, dict[str, Any]] = {}
    for offset, event in enumerate(parsed):
        group = summaries.setdefault(event["stage"], {
            "stage": event["stage"], "calls": 0, "logical_bytes": 0,
            "inclusive_wall_us": [], "inclusive_thread_cpu_us": [],
            "parent_clock_boundary_residual_wall_us": [],
        })
        group["calls"] += 1
        group["logical_bytes"] += event["logical_bytes"]
        if event["wall_ns"] >= 0:
            group["inclusive_wall_us"].append(event["wall_ns"] / 1000)
        if event["cpu_ns"] >= 0:
            group["inclusive_thread_cpu_us"].append(event["cpu_ns"] / 1000)
        children = [parsed[index] for index in direct_children[event["event_id"]]]
        if event["wall_ns"] >= 0 and all(child["wall_ns"] >= 0 for child in children):
            residual = event["wall_ns"] - sum(child["wall_ns"] for child in children)
            require(residual >= 0, f"{path.name} event {offset + 1}: negative direct-child wall residual")
            group["parent_clock_boundary_residual_wall_us"].append(residual / 1000)
    for group in summaries.values():
        for key in ("inclusive_wall_us", "inclusive_thread_cpu_us", "parent_clock_boundary_residual_wall_us"):
            group[key] = quantiles(group[key])
    return parsed, {"stream": stream, "event_count": len(parsed), "root_count": len(roots),
                    "clock_endpoint_errors": endpoint_errors, "incomplete_events": incomplete_events,
                    "stages": list(summaries.values()), "logical_bytes_are_hardware_traffic": False,
                    "inclusive_stage_totals_are_additive": False,
                    "residual_definition": "parent wall minus DIRECT child wall intervals; includes clock boundaries, not scheduler time"}


def resource_record(row: dict[str, Any], metadata: dict[str, Any], label: str,
                    incomplete: list[str]) -> tuple[bool, bool]:
    enabled = metadata["trace_level"] == "resources"
    scope = row["resource_scope"]
    require(scope == metadata["resource_scope"], label + ": resource scope changed within route")
    require(scope in {"disabled", "unavailable", "thread_rusage", "process_task_events"},
            label + ": unknown resource scope")
    require((scope != "disabled") == enabled, label + ": resource sampling mode/scope mismatch")
    valid = {point: boolean(row[f"resource_{point}_valid"], label + f" {point} resource valid")
             for point in ("before", "after")}
    require(boolean(row["resource_valid"], label + " resource valid") == all(valid.values()),
            label + ": aggregate resource validity mismatch")
    clock_failure = False
    for point in ("before", "after"):
        error = integer(row[f"resource_{point}_error"], label + f" {point} resource error")
        require(not valid[point] or error == 0, label + ": valid native resource query has error")
        if not enabled:
            require(not valid[point] and error == 0, label + ": disabled resource query claims measurement")
        elif not valid[point]:
            incomplete.append(label + f": {point} native resource query unavailable or failed")
        query_values = {}
        for kind in ("wall", "cpu"):
            endpoints = {}
            endpoint_validity = {}
            for endpoint in ("start", "end"):
                prefix = f"resource_{point}_{endpoint}_{kind}"
                stamp = integer(row[prefix + "_ns"], label + " " + prefix, 0)
                available = boolean(row[prefix + "_valid"], label + " " + prefix + " valid")
                error = integer(row[prefix + "_error"], label + " " + prefix + " error")
                require(not available or error == 0, label + ": resource clock valid with error")
                if enabled:
                    require(available or error != 0, label + ": enabled resource clock unavailable without error")
                else:
                    require(not available and stamp == 0 and error == 0,
                            label + ": disabled resource clock must explicitly be unavailable")
                endpoints[endpoint], endpoint_validity[endpoint] = stamp, available
            pair_valid = all(endpoint_validity.values()) and endpoints["end"] >= endpoints["start"]
            delta = endpoints["end"] - endpoints["start"] if pair_valid else -1
            if kind == "wall":
                require(integer(row[f"resource_{point}_query_wall_ns"], label + " query wall", -1) == delta,
                        label + ": inconsistent native query wall delta")
            query_values[kind] = endpoints
            if enabled and not pair_valid:
                clock_failure = True
                incomplete.append(label + f": {point} resource {kind} clock unavailable/non-monotonic")
            if enabled and pair_valid and row[f"{kind}_ns"] >= 0:
                if point == "before":
                    require(row[f"entry_{kind}_ns"] <= endpoints["start"] <= endpoints["end"]
                            <= row[f"{kind}_start_ns"], label + ": before-query outside observer envelope")
                else:
                    require(row[f"{kind}_end_ns"] <= endpoints["start"] <= endpoints["end"]
                            <= row[f"exit_{kind}_ns"], label + ": after-query outside observer envelope")
    for counter in RESOURCE_COUNTERS:
        before = integer(row[counter + "_before"], label + " " + counter + " before", -1)
        after = integer(row[counter + "_after"], label + " " + counter + " after", -1)
        expected = after - before if before >= 0 and after >= before else -1
        require(integer(row[counter + "_delta"], label + " " + counter + " delta", -1) == expected,
                label + ": inconsistent " + counter + " delta")
        supported = enabled and ((scope == "thread_rusage" and counter not in {"task_faults", "task_pageins"})
                                 or (scope == "process_task_events" and counter in {"task_faults", "task_pageins"}))
        if supported and all(valid.values()):
            require(before >= 0 and after >= before, label + ": invalid supported " + counter + " endpoints")
        if not supported:
            require(before == after == expected == -1, label + ": unsupported counter was invented as a measurement")
    state_valid = boolean(row["thread_state_valid"], label + " thread state valid")
    require(not state_valid or (enabled and scope == "process_task_events"),
            label + ": unsupported native thread-state claim")
    for point in ("before", "after"):
        state = integer(row[f"thread_run_state_{point}"], label + " thread run state", -1)
        suspended = integer(row[f"thread_suspend_count_{point}"], label + " thread suspend count", -1)
        error = integer(row[f"thread_state_{point}_error"], label + " thread state error")
        if state_valid:
            require(state >= 0 and suspended >= 0 and error == 0, label + ": inconsistent thread state")
        elif not enabled or scope != "process_task_events":
            require(state == suspended == -1 and error == 0, label + ": unsupported thread-state observation")
        else:
            incomplete.append(label + ": native thread-state observations incomplete")
    return (all(valid.values()) if enabled else True), clock_failure


def callback_records(path: Path, metadata: dict[str, Any],
                     incomplete: list[str]) -> tuple[list[dict[str, Any]], dict[str, Any]]:
    records = rows_csv(path, CALLBACK_FIELDS)
    parsed = []
    clock_error_rows = 0
    resource_clock_error_rows = 0
    resources_valid = True
    checksum = 0
    missed = []
    traced = metadata["hooks_compiled"] and metadata["trace_level"] != "off"
    is_cab = metadata["mode"] == "cab"
    previous_sequence = 0
    for index, raw in enumerate(records):
        label = f"{path.name} callback {index}"
        row = dict(raw)
        for key in ("schema_version", "channels", "sample_rate", "block_size", "callback_index", "phase_index",
                    "trace_event_first", "trace_event_count", "worker_seq_before", "worker_seq_after",
                    "allocations", "deletions", "checksum"):
            row[key] = integer(raw[key], label + " " + key, 0)
        require(row["schema_version"] == SCHEMA_VERSION, label + ": wrong schema")
        require(row["callback_index"] == index and index < CALLBACK_COUNT,
                label + ": missing, duplicate or out-of-order callback index")
        for key in ("mode", "engine", "channels", "sample_rate", "block_size"):
            require(row[key] == metadata[key], label + ": inconsistent route " + key)
        phase = ("worker_and_automation" if is_cab else "paced_control") if index < WORKER_CALLBACKS else (
            "forced_six_slot_publication" if is_cab else "burst_control")
        require(row["phase"] == phase and row["phase_index"] == (index if index < WORKER_CALLBACKS else index - WORKER_CALLBACKS),
                label + ": phase or phase-index mismatch")
        require(boolean(row["automation_requested"], label + " automation") == (is_cab and index < 80),
                label + ": automation schedule changed")
        require(boolean(row["publication_requested"], label + " publication") == (is_cab and index == WORKER_CALLBACKS),
                label + ": publication schedule changed")
        valid_clocks = True
        for kind in ("wall", "cpu"):
            start, end, delta, _ = clock_pair(raw, kind, label, incomplete)
            row[f"{kind}_start_ns"], row[f"{kind}_end_ns"], row[f"{kind}_ns"] = start, end, delta
            envelope = {}
            for point in ("entry", "exit"):
                stamp = integer(raw[f"{point}_{kind}_ns"], label + f" {point} {kind}", 0)
                available = boolean(raw[f"{point}_{kind}_valid"], label + f" {point} {kind} valid")
                error = integer(raw[f"{point}_{kind}_error"], label + f" {point} {kind} error")
                require(not available or error == 0, label + ": valid outer clock has error")
                require(available or error != 0, label + ": outer clock unavailable without error")
                row[f"{point}_{kind}_ns"] = stamp
                envelope[point] = (stamp, available)
            envelope_valid = envelope["entry"][1] and envelope["exit"][1] and envelope["exit"][0] >= envelope["entry"][0]
            observed = integer(raw[f"envelope_{kind}_ns"], label + " outer " + kind, -1)
            require(observed == (envelope["exit"][0] - envelope["entry"][0] if envelope_valid else -1),
                    label + ": inconsistent observer envelope delta")
            valid_clocks = valid_clocks and delta >= 0 and envelope_valid
            if envelope_valid and delta >= 0:
                require(envelope["entry"][0] <= start <= end <= envelope["exit"][0],
                        label + ": body outside observer envelope")
            if not envelope_valid:
                incomplete.append(label + ": outer clock unavailable/non-monotonic")
            if parsed and envelope_valid:
                require(parsed[-1][f"exit_{kind}_ns"] <= row[f"entry_{kind}_ns"],
                        label + ": caller clock goes backwards between callbacks")
        clock_error_rows += not valid_clocks
        approximately(number(raw["deadline_ns"], label + " deadline"), BLOCK_SIZE * 1e9 / SAMPLE_RATE,
                      label + " deadline")
        miss = row["wall_ns"] >= 0 and row["wall_ns"] * SAMPLE_RATE > BLOCK_SIZE * 1_000_000_000
        require(boolean(raw["deadline_miss"], label + " deadline miss") == miss,
                label + ": false or suppressed deadline-miss flag")
        if miss:
            missed.append(dict(raw))
        row["deadline_miss"] = miss
        require(previous_sequence <= row["worker_seq_before"] <= row["worker_seq_after"],
                label + ": worker progress sequence reversed")
        previous_sequence = row["worker_seq_after"]
        for point in ("before", "after"):
            valid = boolean(raw[f"worker_snapshot_{point}_valid"], label + " worker snapshot valid")
            state = integer(raw[f"worker_state_{point}"], label + " worker state", 0)
            lane = integer(raw[f"worker_lane_{point}"], label + " worker lane", -1)
            mic = integer(raw[f"worker_mic_{point}"], label + " worker mic", -1)
            require(state <= 10 and lane in (-1, 0, 1, 2) and mic in (-1, 0, 1),
                    label + ": invalid worker snapshot tuple")
            require((lane == -1) == (mic == -1), label + ": partial worker lane/mic identity")
            require(valid == (is_cab and traced), label + ": worker snapshot availability inconsistent with mode")
            if not valid:
                require(row[f"worker_seq_{point}"] == 0 and state == 0 and lane == mic == -1,
                        label + ": unavailable worker snapshot invents state")
            row[f"worker_state_{point}"], row[f"worker_lane_{point}"], row[f"worker_mic_{point}"] = state, lane, mic
        resource_valid, query_clock_failed = resource_record(row, metadata, label, incomplete)
        resources_valid = resources_valid and resource_valid
        resource_clock_error_rows += query_clock_failed
        checksum ^= row["checksum"]
        parsed.append(row)
    return parsed, {
        "wall": quantiles([row["wall_ns"] / 1000 for row in parsed if row["wall_ns"] >= 0]),
        "thread_cpu": quantiles([row["cpu_ns"] / 1000 for row in parsed if row["cpu_ns"] >= 0]),
        "deadline_misses": len(missed), "miss_records": missed,
        "cpu_greater_than_wall_samples": sum(row["cpu_ns"] > row["wall_ns"] >= 0 for row in parsed),
        "wall_misses_with_cpu_over_budget": sum(row["deadline_miss"] and row["cpu_ns"] * SAMPLE_RATE > BLOCK_SIZE * 1_000_000_000
                                               for row in parsed if row["cpu_ns"] >= 0),
        "clock_error_rows": clock_error_rows, "resource_clock_error_rows": resource_clock_error_rows,
        "resource_capture_valid": resources_valid if metadata["trace_level"] == "resources" else False,
        "checksum": checksum,
        "observed_callback_allocations": sum(row["allocations"] for row in parsed),
        "observed_callback_deletions": sum(row["deletions"] for row in parsed),
        "phase_results": [{
            "phase": phase, "callbacks": len(selected := [row for row in parsed if row["phase"] == phase]),
            "wall": quantiles([row["wall_ns"] / 1000 for row in selected if row["wall_ns"] >= 0]),
            "thread_cpu": quantiles([row["cpu_ns"] / 1000 for row in selected if row["cpu_ns"] >= 0]),
            "deadline_misses": sum(row["deadline_miss"] for row in selected),
        } for phase in dict.fromkeys(row["phase"] for row in parsed)],
    }


def slot_records(path: Path, metadata: dict[str, Any], callbacks: list[dict[str, Any]],
                 stages: list[dict[str, Any]]) -> int:
    records = rows_csv(path, SLOT_FIELDS)
    expected = metadata["mode"] == "cab" and metadata["hooks_compiled"] and metadata["trace_level"] != "off"
    require(len(records) == (len(callbacks) * 12 if expected else 0),
            path.name + ": missing/extra before-after six-slot snapshots")
    by_key = {}
    for row_index, raw in enumerate(records):
        label = f"{path.name} row {row_index}"
        row = dict(raw)
        for key in ("schema_version", "callback_index", "lane", "mic", "requested_model_key", "active_model_key",
                    "active_generation", "fading_model_key", "fading_generation", "fade_remaining", "fade_length", "rejected_models"):
            row[key] = integer(raw[key], label + " " + key, 0)
        require(row["schema_version"] == SCHEMA_VERSION and row["callback_index"] < len(callbacks),
                label + ": invalid schema/callback association")
        require(row["point"] in {"before", "after"} and row["lane"] in (0, 1, 2) and row["mic"] in (0, 1),
                label + ": invalid slot identity")
        key = (row["callback_index"], row["point"], row["lane"], row["mic"])
        require(key not in by_key, label + ": duplicate slot snapshot")
        require(not boolean(raw["pending_identity_available"], label + " pending identity"),
                label + ": unsafe pending identity must remain unavailable")
        for field in ("pending_present", "active_present", "fading_present"):
            row[field] = boolean(raw[field], label + " " + field)
        require(row["active_present"] or (row["active_model_key"] == row["active_generation"] == 0),
                label + ": absent active kernel claims identity")
        require(row["fading_present"] or (row["fading_model_key"] == row["fading_generation"] == 0),
                label + ": absent fading kernel claims identity")
        require(row["active_present"] and row["active_model_key"] != 0, label + ": modeled route lacks prepared active kernel")
        require(row["fade_length"] == 4800 and row["fade_remaining"] <= 4800, label + ": original 50 ms fade changed")
        require(not row["fading_present"] or row["fade_remaining"] > 0, label + ": fading kernel without an active fade")
        by_key[key] = row
    events_by_callback: dict[int, list[dict[str, Any]]] = defaultdict(list)
    for event in stages:
        events_by_callback[event["callback_index"]].append(event)
    for index in range(len(callbacks)) if expected else ():
        events = events_by_callback[index]
        for lane in range(3):
            for point in ("before", "after"):
                require(sum(by_key[(index, point, lane, mic)]["fade_remaining"] > 0 for mic in range(2)) <= 1,
                        f"{path.name} callback {index}: serialized A/B fade bound violated")
            for mic in range(2):
                before, after = by_key[(index, "before", lane, mic)], by_key[(index, "after", lane, mic)]
                require(before["requested_model_key"] == after["requested_model_key"],
                        f"{path.name} callback {index}: request changes inside callback")
                accepts = [event for event in events if event["stage"] == "kernel_accept" and event["lane"] == lane and event["mic"] == mic]
                require(len(accepts) <= 1, f"{path.name} callback {index}: multiple accepts per slot")
                changed = (before["active_model_key"], before["active_generation"]) != (after["active_model_key"], after["active_generation"])
                if changed:
                    require(bool(accepts), f"{path.name} callback {index}: active identity changed without accept event")
                if accepts:
                    require((accepts[0]["model_key"], accepts[0]["generation"]) ==
                            (after["active_model_key"], after["active_generation"]),
                            f"{path.name} callback {index}: accept identity mismatches resulting active kernel")
                rejects = sum(event["stage"] == "kernel_reject" and event["lane"] == lane and event["mic"] == mic for event in events)
                require(after["rejected_models"] - before["rejected_models"] == rejects,
                        f"{path.name} callback {index}: rejection counter/event mismatch")
    return len(records)


def worker_overlap(callbacks: list[dict[str, Any]], events: list[dict[str, Any]]) -> list[dict[str, Any]]:
    """Wall-only intersections by named worker scope, retaining separate groups."""
    groups: dict[str, list[tuple[int, int]]] = defaultdict(list)
    for event in events:
        if event["wall_ns"] >= 0:
            groups[event["stage"]].append((event["wall_start_ns"], event["wall_end_ns"]))
    result = []
    for stage, intervals in sorted(groups.items()):
        merged: list[list[int]] = []
        for start, end in sorted(intervals):
            if merged and start <= merged[-1][1]:
                merged[-1][1] = max(merged[-1][1], end)
            else:
                merged.append([start, end])
        count = misses = duration = cursor = 0
        for callback in callbacks:
            if callback["wall_ns"] < 0:
                continue
            start, end = callback["wall_start_ns"], callback["wall_end_ns"]
            while cursor < len(merged) and merged[cursor][1] <= start:
                cursor += 1
            interval_index = cursor
            overlap = 0
            while interval_index < len(merged) and merged[interval_index][0] < end:
                interval = merged[interval_index]
                overlap += max(0, min(end, interval[1]) - max(start, interval[0]))
                interval_index += 1
            if overlap:
                count += 1
                misses += callback["deadline_miss"]
                duration += overlap
        result.append({"worker_stage": stage, "callbacks_with_wall_overlap": count,
                       "wall_misses_with_overlap": misses, "intersection_wall_us": duration / 1000,
                       "interpretation": "descriptive overlap; worker groups can overlap and do not establish cause"})
    return result


METADATA_FIELDS = (
    "schema_version,mode,engine,channels,sample_rate,block_size,trace_level,complete,callback_count,stage_count,"
    "worker_event_count,caller_event_count,slot_count,dropped,clock_errors,nesting_errors,audio_host_validation,"
    "product_acceptance,product_performance_change,worker_trace_enabled,hooks_compiled,source_commit,compiler,system,"
    "wall_clock,thread_cpu_clock,clock_read_order,wall_minus_cpu_is_exclusive_scheduling_time,resource_scope,"
    "resource_capture_requested,resource_capture_valid,macos_minor_major_faults_supported,"
    "macos_resource_counters_include_worker,macos_thread_state_is_continuous_scheduler_trace,"
    "hardware_cache_counters_available,logical_bytes_are_hardware_traffic,pending_identity_available_in_snapshot,"
    "pending_identity_source,semantic_valid,worker_scheduling_valid,worker_start_succeeded,worker_entered_run,"
    "worker_observed_priority,worker_cancelled_builds,qos_setter_called,worker_priority_changed,callback_allocations,"
    "callback_deletions,allocation_guard,callbacks_worker_phase,callbacks_forced_phase,automation_callbacks,"
    "paced_sleep_every,paced_sleep_ms,event_capacity,worker_event_capacity,caller_event_capacity,event_size_bytes,"
    "allocated_event_bytes,control_memory_bytes,arithmetic_iterations,memory_touches_per_slot,"
    "memory_logical_bytes_per_callback,calibration_empty_scopes,kernel_residual,peak,max_step,deadline_misses,"
    "deadline_misses_are_product_gate,checksum,recording_on_reference_oracle,caller_stream_work,parameter_refresh_per_callback"
).split(",")


def verify_route(metadata_path: Path) -> dict[str, Any]:
    metadata = load_json(metadata_path)
    require_fields(metadata, METADATA_FIELDS, metadata_path.name)
    prefix = metadata_path.name.removesuffix(".metadata.json")
    files = {suffix: metadata_path.with_name(prefix + "." + suffix + ".csv")
             for suffix in ("callbacks", "stages", "worker", "caller", "slots")}
    missing = [path.name for path in files.values() if not path.is_file()]
    if missing:
        return {"metadata": str(metadata_path), "complete": False, "invalid": False,
                "incomplete_reasons": ["missing required evidence files: " + ", ".join(missing)],
                "mode": metadata.get("mode"), "trace_level": metadata.get("trace_level"), "engine": metadata.get("engine"),
                "channels": metadata.get("channels")}
    incomplete: list[str] = []
    for key in ("schema_version", "channels", "sample_rate", "block_size", "callback_count", "stage_count",
                "worker_event_count", "caller_event_count", "slot_count", "dropped", "clock_errors", "nesting_errors",
                "callback_allocations", "callback_deletions", "event_capacity", "worker_event_capacity",
                "caller_event_capacity", "event_size_bytes", "allocated_event_bytes", "deadline_misses", "checksum"):
        metadata[key] = integer(metadata[key], metadata_path.name + " " + key, 0)
    for key in ("hooks_compiled", "complete", "worker_trace_enabled", "resource_capture_requested", "resource_capture_valid",
                "semantic_valid", "worker_scheduling_valid", "worker_start_succeeded", "worker_entered_run",
                "recording_on_reference_oracle"):
        metadata[key] = boolean(metadata[key], metadata_path.name + " " + key)
    require(metadata["schema_version"] == SCHEMA_VERSION, "unknown metadata schema")
    require(metadata["mode"] in MODES and metadata["engine"] in ENGINES and metadata["trace_level"] in TRACE_LEVELS,
            "unknown mode/engine/trace-level tuple")
    require(metadata["sample_rate"] == SAMPLE_RATE and metadata["block_size"] == BLOCK_SIZE
            and metadata["channels"] in (1, 2), "attribution protocol must retain 96 kHz/64 mono or stereo")
    require(prefix == f'{metadata["mode"]}-{metadata["engine"]}-96000-64-{metadata["channels"]}-{metadata["trace_level"]}',
            "route filename does not match metadata identity")
    require(re.fullmatch(r"[0-9a-f]{40}", metadata["source_commit"]) is not None, "source commit is not an immutable full SHA")
    for key in ("compiler", "system", "wall_clock", "thread_cpu_clock"):
        require(isinstance(metadata[key], str) and bool(metadata[key]), "missing native/build receipt " + key)
    require(metadata["clock_read_order"] == "wall_then_cpu_at_each_endpoint", "unknown clock envelope/read order")
    require(metadata["audio_host_validation"] == "not_executed", "console evidence cannot claim real audio-host validation")
    for key in ("product_acceptance", "product_performance_change", "wall_minus_cpu_is_exclusive_scheduling_time",
                "macos_minor_major_faults_supported", "macos_thread_state_is_continuous_scheduler_trace",
                "hardware_cache_counters_available", "logical_bytes_are_hardware_traffic",
                "pending_identity_available_in_snapshot", "qos_setter_called", "worker_priority_changed",
                "deadline_misses_are_product_gate", "parameter_refresh_per_callback"):
        require(not boolean(metadata[key], key), "unsupported attribution/acceptance claim: " + key)
    require(metadata["pending_identity_source"] == "publication_swap_rejection_events",
            "pending identity source must be separately owned publication/swap/rejection evidence")
    require(boolean(metadata["macos_resource_counters_include_worker"], "macOS resource counter scope"),
            "task-level macOS resource counts cannot exclude concurrent worker contributions")
    is_cab = metadata["mode"] == "cab"
    traced = metadata["hooks_compiled"] and metadata["trace_level"] != "off"
    require(metadata["hooks_compiled"] or metadata["trace_level"] == "off", "compiled-out target claims enabled attribution")
    require(metadata["worker_trace_enabled"] == (is_cab and traced), "worker trace availability contradicts mode")
    require(metadata["recording_on_reference_oracle"] == (is_cab and traced), "reference-oracle recording receipt mismatch")
    require(metadata["resource_capture_requested"] == (metadata["trace_level"] == "resources"),
            "resource capture request contradicts trace level")
    for key, expected in (("callbacks_worker_phase", 1200), ("callbacks_forced_phase", 400),
                          ("automation_callbacks", 80 if is_cab else 0), ("paced_sleep_every", 8), ("paced_sleep_ms", 1),
                          ("arithmetic_iterations", 256), ("memory_touches_per_slot", 256),
                          ("memory_logical_bytes_per_callback", 12288 if metadata["mode"] == "memory" else 0),
                          ("calibration_empty_scopes", 64 if metadata["mode"] == "clock" and traced else 0)):
        require(integer(metadata[key], key, 0) == expected, "protocol changed: " + key)
    require(metadata["allocated_event_bytes"] ==
            (metadata["event_capacity"] + metadata["worker_event_capacity"] + metadata["caller_event_capacity"])
            * metadata["event_size_bytes"], "bounded-storage byte accounting mismatch")
    if not metadata["complete"]:
        incomplete.append("producer explicitly retained an incomplete route")
    if not metadata["semantic_valid"]:
        incomplete.append("producer semantic checks did not complete successfully")
    for key in ("dropped", "clock_errors", "nesting_errors", "callback_allocations", "callback_deletions"):
        if metadata[key] != 0:
            incomplete.append(f"recorded {key}={metadata[key]}; no complete attribution claim")
    if is_cab:
        if not all(metadata[key] for key in ("worker_scheduling_valid", "worker_start_succeeded", "worker_entered_run")):
            incomplete.append("IR worker scheduling/start evidence incomplete")
        require(integer(metadata["worker_observed_priority"], "worker priority") == -1,
                "IR worker priority no longer matches the unchanged low-priority contract")
        for key, threshold in (("kernel_residual", 3e-6), ("peak", 2), ("max_step", .3)):
            observed = number(metadata[key], key)
            require(observed >= 0, "negative semantic observation " + key)
            if observed >= threshold:
                incomplete.append("original diagnostic semantic criterion not met: " + key)
    callbacks, callback_summary = callback_records(files["callbacks"], metadata, incomplete)
    require(len(callbacks) == metadata["callback_count"], "metadata callback count disagrees with raw rows")
    if len(callbacks) != CALLBACK_COUNT:
        incomplete.append(f"callback population incomplete: {len(callbacks)}/{CALLBACK_COUNT}")
    stages, stage_summary = event_stream(files["stages"], "stages", callbacks, incomplete)
    worker, worker_summary = event_stream(files["worker"], "worker", callbacks, incomplete)
    caller, caller_summary = event_stream(files["caller"], "caller", callbacks, incomplete)
    for key, events, capacity in (("stage_count", stages, "event_capacity"),
                                  ("worker_event_count", worker, "worker_event_capacity"),
                                  ("caller_event_count", caller, "caller_event_capacity")):
        require(len(events) == metadata[key] <= metadata[capacity], "raw event count/capacity mismatch: " + key)
    require(len(stages) == sum(row["trace_event_count"] for row in callbacks), "callback event totals do not cover stage rows")
    cursor = 0
    for index, callback in enumerate(callbacks):
        require(callback["trace_event_first"] == cursor, f"callback {index}: stage event range skipped/overlapped")
        count = callback["trace_event_count"]
        group = stages[cursor:cursor + count]
        if traced:
            require(group and group[0]["parent_id"] == 0 and group[0]["stage"] == "callback",
                    f"callback {index}: attribution lacks a root")
            require(sum(event["parent_id"] == 0 for event in group) == 1,
                    f"callback {index}: duplicate callback roots")
            require(all(event["callback_index"] == index for event in group),
                    f"callback {index}: stage range crosses callback IDs")
            if is_cab:
                processes = [(event["lane"], event["mic"]) for event in group if event["stage"] == "cab_process"]
                require(sorted(processes) == [(lane, mic) for lane in range(3) for mic in range(2)],
                        f"callback {index}: did not preserve all three CAB lanes/two microphone paths")
                require(sum(event["stage"] == "active_convolution" for event in group) == 6,
                        f"callback {index}: missing active-convolution attribution")
                require(sum(event["stage"] == "fading_convolution" for event in group) <= 3,
                        f"callback {index}: fading-path bound exceeded")
            else:
                expected_leaf, expected_count = {
                    "arithmetic": ("arithmetic_control", 1), "memory": ("memory_control", 1),
                    "clock": ("scope_calibration", 64),
                }[metadata["mode"]]
                require(len(group) == expected_count + 1 and all(event["stage"] == expected_leaf for event in group[1:]),
                        f"callback {index}: synthetic control scope population changed")
                require(sum(event["logical_bytes"] for event in group) ==
                        (12288 if metadata["mode"] == "memory" else 0),
                        f"callback {index}: synthetic logical-byte accounting mismatch")
        else:
            require(count == 0, f"callback {index}: hooks-off control has stage events")
        cursor += count
    if traced and is_cab:
        roots = [event for event in worker if event["parent_id"] == 0]
        require(len(roots) == 1 and roots[0]["stage"] == "worker_run",
                "worker stream must contain one final measured run, not mixed reset epochs")
        root = roots[0]
        require(bool(caller), "missing separately attributed caller reference/publications")
        for event in caller:
            require(event["worker_sequence_start"] == event["worker_sequence_end"] == 0,
                    "caller-main reference/publication sequence must not impersonate worker progress")
        publications = [event for event in caller if event["stage"] == "worker_publish"]
        require(sorted((event["lane"], event["mic"]) for event in publications) ==
                [(lane, mic) for lane in range(3) for mic in range(2)],
                "caller stream must preserve six separately owned forced publications")
        require(len({event["model_key"] for event in publications}) == 1
                and all(event["generation"] == 100 and event["model_key"] != 0 for event in publications),
                "forced publications must retain six instances of the common replacement response")
        for callback in callbacks[WORKER_CALLBACKS:]:
            require(callback["wall_start_ns"] >= root["wall_end_ns"], "worker stream overlaps a worker-stopped callback")
            require(callback["worker_seq_before"] == callback["worker_seq_after"] == root["worker_sequence_end"],
                    "stopped-worker callback reports unexpected progress")
            require(callback["worker_state_before"] == callback["worker_state_after"] == 10,
                    "stopped-worker callback does not observe exit state")
        if len(callbacks) > WORKER_CALLBACKS:
            for event in caller:
                require(callbacks[WORKER_CALLBACKS - 1]["wall_end_ns"] <= event["wall_start_ns"]
                        <= event["wall_end_ns"] <= callbacks[WORKER_CALLBACKS]["wall_start_ns"],
                        "caller reference/publication work must remain in the unmeasured phase boundary")
                require(root["wall_end_ns"] <= event["wall_start_ns"], "caller forced work occurred before worker joined")
            require(max(event["cpu_end_ns"] for event in caller) <= callbacks[WORKER_CALLBACKS]["cpu_start_ns"],
                    "caller stream CPU endpoints cannot be associated with the worker thread")
    else:
        require(not worker and not caller, "non-CAB or hooks-off control invents worker/caller attribution")
    slot_count = slot_records(files["slots"], metadata, callbacks, stages)
    require(slot_count == metadata["slot_count"], "metadata slot count disagrees with raw snapshots")
    require(callback_summary["deadline_misses"] == metadata["deadline_misses"], "reported miss total contradicts raw callbacks")
    require(callback_summary["checksum"] == metadata["checksum"], "reported checksum differs from XOR of callback receipts")
    require(callback_summary["resource_capture_valid"] == metadata["resource_capture_valid"],
            "reported resource capture validity contradicts raw callbacks")
    observed_clock_errors = callback_summary["clock_error_rows"] + callback_summary["resource_clock_error_rows"] + sum(
        summary["clock_endpoint_errors"] for summary in (stage_summary, worker_summary, caller_summary))
    require(observed_clock_errors == metadata["clock_errors"], "reported clock-error count contradicts raw endpoints")
    # The C++ guard also runs in unmeasured numerical oracles. Its route totals
    # may exceed measured callback row totals; zero remains mandatory for a
    # complete receipt, and neither means universal malloc/lock tracing.
    require(metadata["callback_allocations"] >= callback_summary["observed_callback_allocations"]
            and metadata["callback_deletions"] >= callback_summary["observed_callback_deletions"],
            "route allocation guard suppresses measured callback allocations/deletions")
    if metadata["complete"]:
        require(not incomplete, "producer claimed complete despite incomplete/error evidence: " + "; ".join(incomplete[:3]))
    return {
        "metadata": str(metadata_path), "complete": not incomplete, "invalid": False,
        "incomplete_reasons": list(dict.fromkeys(incomplete)),
        "mode": metadata["mode"], "trace_level": metadata["trace_level"], "engine": metadata["engine"],
        "channels": metadata["channels"], "sample_rate": SAMPLE_RATE, "block_size": BLOCK_SIZE,
        "hooks_compiled": metadata["hooks_compiled"], "attribution_available": traced,
        "source_commit": metadata["source_commit"], "system": metadata["system"], "compiler": metadata["compiler"],
        "callback_count": len(callbacks), "callbacks": callback_summary,
        "stage_trace": stage_summary, "worker_trace": worker_summary, "caller_trace": caller_summary,
        "slot_count": slot_count, "worker_overlap_groups": worker_overlap(callbacks, worker),
        "input_sha256": {key: sha256(path) for key, path in {"metadata": metadata_path, **files}.items()},
        "product_fix": False, "product_acceptance": False, "audio_host_validation": "not_executed",
        "semantic_valid_is_producer_receipt_not_independent_audio_reanalysis": True,
    }


def verify_directory(input: str | Path, expected_mode=None, expected_trace_level=None,
                     expected_engines=None, expected_channels=None) -> dict[str, Any]:
    root = Path(input)
    errors: list[str] = []
    incomplete: list[str] = []
    routes: list[dict[str, Any]] = []
    expected = {}
    try:
        if expected_mode is not None:
            require(expected_mode in MODES, "unknown expected mode")
        if expected_trace_level is not None:
            require(expected_trace_level in TRACE_LEVELS, "unknown expected trace level")
        engines = expected_values(expected_engines)
        channels = expected_values(expected_channels, numeric=True)
        require(engines is None or engines <= ENGINES, "unknown expected engines")
        require(channels is None or channels <= {1, 2}, "unknown expected channels")
        expected = {"mode": expected_mode, "trace_level": expected_trace_level,
                    "engines": sorted(engines) if engines is not None else None,
                    "channels": sorted(channels) if channels is not None else None}
    except (InvalidEvidence, TypeError) as error:
        return {"schema_version": SCHEMA_VERSION, "status": "invalid", "complete": False, "exit_code": 2,
                "errors": [str(error)], "incomplete_reasons": [], "routes": [], "route_count": 0,
                "product_fix": False, "product_acceptance": False, "audio_host_validation": "not_executed"}
    if not root.is_dir():
        incomplete.append("input directory does not exist")
        metadata_paths = []
    else:
        metadata_paths = sorted(root.rglob("*.metadata.json"))
        if not metadata_paths:
            incomplete.append("no route metadata files; empty evidence is not complete")
    seen = set()
    covered = set()
    for path in metadata_paths:
        try:
            result = verify_route(path)
            for key, value in (("mode", expected_mode), ("trace_level", expected_trace_level)):
                require(value is None or result[key] == value, f"{path.name}: unexpected {key}")
            require(engines is None or result["engine"] in engines, path.name + ": unexpected engine")
            require(channels is None or result["channels"] in channels, path.name + ": unexpected channels")
            identity = (result["mode"], result["trace_level"], result["engine"], result["channels"])
            require(identity not in seen, f"{path.name}: duplicate route tuple; verify separate trials independently")
            seen.add(identity)
            covered.add((result["engine"], result["channels"]))
            routes.append(result)
            incomplete.extend(str(path.relative_to(root)) + ": " + reason
                              for reason in result["incomplete_reasons"])
        except (InvalidEvidence, OSError, UnicodeError, csv.Error, json.JSONDecodeError, KeyError, TypeError) as error:
            errors.append(str(path.relative_to(root)) + ": " + str(error))
    if engines is not None and channels is not None:
        missing = {(engine, channel) for engine in engines for channel in channels} - covered
        if missing:
            incomplete.append("expected route grid missing: " + ", ".join(
                f"{engine}/{channel}" for engine, channel in sorted(missing)))
    # Reject stray CSV artifacts: a metadata file deleted to hide a failed route
    # must not turn the remainder of a directory into apparently complete data.
    if root.is_dir():
        registered = {path.with_name(path.name.removesuffix(".metadata.json") + "." + suffix + ".csv")
                      for path in metadata_paths for suffix in ("callbacks", "stages", "worker", "caller", "slots")}
        for suffix in ("callbacks", "stages", "worker", "caller", "slots"):
            for path in root.rglob("*." + suffix + ".csv"):
                if path not in registered:
                    incomplete.append(str(path.relative_to(root)) + ": orphan raw CSV has no route metadata")
    status = "invalid" if errors else ("incomplete" if incomplete else "verified_complete")
    return {
        "schema_version": SCHEMA_VERSION, "input": str(root), "status": status,
        "complete": status == "verified_complete", "exit_code": 2 if errors else (3 if incomplete else 0),
        "expected": expected, "route_count": len(routes), "errors": errors,
        "incomplete_reasons": list(dict.fromkeys(incomplete)), "routes": routes,
        "populations_are_not_pooled": True, "product_fix": False, "product_acceptance": False,
        "audio_host_validation": "not_executed", "limitations": LIMITATIONS,
    }


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", required=True, type=Path, help="One trial/condition directory, recursively")
    parser.add_argument("--output", required=True, type=Path, help="Independent verification JSON file")
    parser.add_argument("--expected-mode", choices=sorted(MODES))
    parser.add_argument("--expected-trace-level", choices=sorted(TRACE_LEVELS))
    parser.add_argument("--expected-engines", help="Comma-separated route engines")
    parser.add_argument("--expected-channels", help="Comma-separated 1,2")
    arguments = parser.parse_args(argv)
    report = verify_directory(arguments.input, expected_mode=arguments.expected_mode,
                              expected_trace_level=arguments.expected_trace_level,
                              expected_engines=arguments.expected_engines, expected_channels=arguments.expected_channels)
    arguments.output.parent.mkdir(parents=True, exist_ok=True)
    arguments.output.write_text(json.dumps(report, ensure_ascii=False, indent=2, allow_nan=False) + "\n", encoding="utf-8")
    print(json.dumps({"status": report["status"], "complete": report["complete"], "exit_code": report["exit_code"],
                      "routes": report["route_count"], "errors": len(report["errors"]),
                      "product_fix": False, "audio_host_validation": "not_executed"}, allow_nan=False))
    return report["exit_code"]


if __name__ == "__main__":
    raise SystemExit(main())
