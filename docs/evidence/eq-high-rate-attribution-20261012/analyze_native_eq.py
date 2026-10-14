#!/usr/bin/env python3
"""Read-only audit of one Chimera EQ attribution output directory.

The JSON validates evidence structure and accounting, never real-time or DAW
performance. It does not rewrite input CSVs. Python standard library only.
"""
from __future__ import annotations

import argparse
import csv
import json
import math
import re
import sys
from collections import Counter, defaultdict
from decimal import Decimal, InvalidOperation
from pathlib import Path


TEXT_FIELDS = {
    "group", "stage", "pacing", "signal", "consumer", "source_status",
    "sample_flow_status", "performance_acceptance",
}
INT_FIELDS = {
    "case_id", "repeat", "rate", "block", "fft_size", "requested_update_ms",
    "callbacks", "produced_samples", "expected_samples_by_wall",
    "within_5pct_nominal_rate", "callback_execution_overruns",
    "source_starts_over_one_period_late", "deadline_rebases", "catch_up_blocks",
    "max_consecutive_catch_up", "initial_pending", "capture_expected_samples",
    "consumed_samples", "discarded_samples", "rejected_samples", "pending_samples",
    "sample_accounting_error", "observed_fifo_high_water", "fft_windows",
    "analysis_updates", "unobserved_updates", "panel_paint_calls", "dispatch_calls",
    "callback_records", "latency_samples", "callback", "rebased", "catch_up",
    "update_sequence", "pending_snapshot",
}
LEGACY = "legacy_sleep_rebase"
DUE = "due_samples_catch_up"
DIRECT = "headless_direct"
NO_PAINT = "native_panel_paint_suppressed"
PAINT = "native_paint"
FULL = "full_processor"
NOOP = "producer_noop"
EQPAIR = "eq_pair"
CALCULATED = "per_sample_sin_fmod"
CACHED = "precomputed_same_tone"


class Problems:
    def __init__(self):
        self.errors = []
        self.warnings = []
        self.error_count = 0
        self.warning_count = 0

    def error(self, code, **details):
        self.error_count += 1
        if len(self.errors) < 200:
            self.errors.append({"code": code, **details})

    def warning(self, code, **details):
        self.warning_count += 1
        if len(self.warnings) < 200:
            self.warnings.append({"code": code, **details})


def div(numerator, denominator):
    if numerator is None or denominator is None or denominator == 0:
        return None
    value = numerator / denominator
    return value if math.isfinite(value) else None


def pct(value):
    return None if value is None else 100.0 * value


def delta(new, old):
    return None if new is None or old is None else new - old


def quantile(values, fraction):
    """The fixture's lower-order-statistic percentile, not interpolation."""
    if not values:
        return None
    ordered = sorted(values)
    return ordered[min(len(ordered) - 1, int(fraction * (len(ordered) - 1)))]


def distribution(values):
    finite = [x for x in values if x is not None and math.isfinite(x)]
    return {
        "count": len(finite),
        "mean": div(math.fsum(finite), len(finite)),
        "p50": quantile(finite, .5),
        "p95": quantile(finite, .95),
        "p99": quantile(finite, .99),
        "max": max(finite) if finite else None,
    }


def parse_number(value, field, file, row, problems):
    if value is None or value.strip() == "":
        problems.error("MISSING_NUMERIC_VALUE", file=file, row=row, field=field)
        return None
    try:
        number = Decimal(value)
    except InvalidOperation:
        problems.error("INVALID_NUMERIC_VALUE", file=file, row=row, field=field, value=value)
        return None
    if not number.is_finite():
        problems.error("NONFINITE_NUMERIC_VALUE", file=file, row=row, field=field, value=value)
        return None
    if field in INT_FIELDS:
        if number != number.to_integral_value():
            problems.error("NONINTEGER_COUNT", file=file, row=row, field=field, value=value)
            return None
        return int(number)
    result = float(number)
    if not math.isfinite(result):
        problems.error("NUMERIC_OVERFLOW", file=file, row=row, field=field, value=value)
        return None
    return result


def rows(path, required, problems):
    if not path.is_file():
        problems.error("MISSING_FILE", file=path.name)
        return
    try:
        with path.open(encoding="utf-8-sig", newline="") as handle:
            reader = csv.DictReader(handle)
            headers = reader.fieldnames or []
            if len(headers) != len(set(headers)):
                problems.error("DUPLICATE_CSV_COLUMNS", file=path.name)
            missing = sorted(set(required) - set(headers))
            if missing:
                problems.error("MISSING_CSV_COLUMNS", file=path.name, columns=missing)
            for lineno, raw in enumerate(reader, 2):
                if None in raw:
                    problems.error("EXTRA_CSV_FIELDS", file=path.name, row=lineno)
                parsed = {}
                for field, value in raw.items():
                    if field is None:
                        continue
                    if field in TEXT_FIELDS:
                        parsed[field] = value
                    else:
                        parsed[field] = parse_number(value, field, path.name, lineno, problems)
                yield parsed
    except (OSError, UnicodeError, csv.Error) as exc:
        problems.error("CSV_READ_ERROR", file=path.name, detail=str(exc))


def require_equal(actual, expected, code, problems, **details):
    if actual != expected:
        problems.error(code, actual=actual, expected=expected, **details)
        return False
    return True


def approximately_equal(actual, expected, *, abs_tol=1.e-4, rel_tol=1.e-8):
    return actual is not None and expected is not None and math.isclose(actual, expected, rel_tol=rel_tol, abs_tol=abs_tol)


def expected_cases(headless, scale):
    result = []

    def add(group, rate, fft, interval, stage, pacing, consumer, duration, signal=CALCULATED, repeat=0):
        result.append({
            "case_id": len(result) + 1, "group": group, "repeat": repeat,
            "rate": rate, "block": 128, "stage": stage, "pacing": pacing,
            "signal": signal, "consumer": consumer, "fft_size": fft,
            "requested_update_ms": interval,
            "requested_duration_s": None if scale is None else duration * scale,
        })

    for stage in [NOOP, EQPAIR, FULL]:
        for pacing in [LEGACY, DUE]:
            add("stage_clock_control", 192000, 0, 40, stage, pacing, DIRECT, 2.)
    for rate in [44100, 48000, 96000]:
        for pacing in [LEGACY, DUE]:
            add("source_rate_control", rate, 0, 40, FULL, pacing, DIRECT, 1.2)
    for stage in [NOOP, FULL]:
        for pacing in [LEGACY, DUE]:
            add("generator_control", 192000, 0, 40, stage, pacing, DIRECT, 2., CACHED)
    for fft in [2048, 4096, 8192]:
        for pacing in [LEGACY, DUE]:
            add("consumer_control", 192000, fft, 40, FULL, pacing, DIRECT, 2.)
    if headless:
        return result
    for fft in [0, 2048, 4096, 8192]:
        for pacing in [LEGACY, DUE]:
            for consumer in [NO_PAINT, PAINT]:
                add("consumer_control", 192000, fft, 40, FULL, pacing, consumer, 2.)
    for rate in [44100, 48000, 96000, 192000]:
        for fft in [0, 2048, 4096, 8192]:
            for interval in [20, 40, 80]:
                add("rate_fft_cadence", rate, fft, interval, FULL, DUE, PAINT, 1.2)
    for repeat in [1, 2]:
        for pacing in [LEGACY, DUE]:
            add("native_off_repeat", 192000, 0, 40, FULL, pacing, PAINT, 2., repeat=repeat)
    return result


def callback_aggregate():
    return {
        "count": 0, "next_index": 0, "index_error_count": 0,
        "rebases": 0, "rebase_us": 0., "catch_up": 0,
        "generation_us": [], "callback_us": [], "entry_gap_us": [],
        "positive_requested_us": [], "positive_actual_us": [],
        "positive_wake_late_us": [], "positive_actual_minus_requested_us": [],
        "positive_overshoot_us": [], "positive_requests_over_50us_wake_late_us": [],
        "zero_request_count": 0, "zero_request_wake_max_us": None,
        "first_entry_us": None, "last_entry_us": None, "last_original_clock_late_us": None,
        "original_clock_late_max_us": None, "source_starts_late": 0,
    }


def compact_case(row, callbacks, updates):
    case_id = row.get("case_id")
    callback = callbacks.get(case_id, {})
    update = updates.get(case_id, {})
    gaps = update.get("gaps", [])
    wall_us = (row.get("observed_wall_s") or 0.) * 1.e6
    generation = callback.get("generation_us", [])
    processing = callback.get("callback_us", [])
    return {
        "case_id": case_id, "group": row.get("group"), "repeat": row.get("repeat"),
        "rate": row.get("rate"), "fft": row.get("fft_size"),
        "update_ms": row.get("requested_update_ms"), "stage": row.get("stage"),
        "pacing": row.get("pacing"), "signal": row.get("signal"), "consumer": row.get("consumer"),
        "source_status": row.get("source_status"),
        "production_percent": pct(row.get("production_ratio")),
        "nominal_shortfall_percentage_points": None if row.get("production_ratio") is None else 100. * (1. - row["production_ratio"]),
        "observed_seconds": row.get("observed_wall_s"),
        "callbacks": row.get("callbacks"),
        "produced_samples": row.get("produced_samples"),
        "process_cpu_percent": row.get("process_cpu_one_core_percent"),
        "producer_cpu_percent": row.get("producer_thread_cpu_one_core_percent"),
        "message_cpu_percent": row.get("message_thread_cpu_one_core_percent"),
        "generation_wall_us": distribution(generation),
        "generation_wall_equivalent_percent": pct(div(math.fsum(generation), wall_us)),
        "callback_wall_us": distribution(processing),
        "callback_wall_equivalent_percent": pct(div(math.fsum(processing), wall_us)),
        "callback_budget_us": row.get("callback_budget_us"),
        "callback_p99_us": row.get("callback_p99_us"), "callback_max_us": row.get("callback_max_us"),
        "callback_execution_overruns": row.get("callback_execution_overruns"),
        "late_source_starts": row.get("source_starts_over_one_period_late"),
        "rebases": row.get("deadline_rebases"),
        "rebase_lost_ms": None if row.get("rebased_source_time_us") is None else row["rebased_source_time_us"] / 1000.,
        "catch_up_blocks": row.get("catch_up_blocks"),
        "max_consecutive_catch_up": row.get("max_consecutive_catch_up"),
        "source_clock_late_max_us": row.get("source_clock_late_max_us"),
        "fifo_flow": row.get("sample_flow_status"),
        "rejected": row.get("rejected_samples"), "discarded": row.get("discarded_samples"),
        "pending": row.get("pending_samples"), "observed_fifo_high_water": row.get("observed_fifo_high_water"),
        "fft_windows": row.get("fft_windows"), "analysis_updates": row.get("analysis_updates"),
        "fft_wall_equivalent_percent": pct(div(row.get("fft_total_us"), wall_us)),
        "analysis_wall_equivalent_percent": pct(div(row.get("analysis_total_us"), (row.get("observed_wall_s") or 0.) * 1.e6)),
        "analysis_p99_ms": None if row.get("analysis_p99_us") is None else row["analysis_p99_us"] / 1000.,
        "analysis_max_ms": None if row.get("analysis_max_us") is None else row["analysis_max_us"] / 1000.,
        "path_wall_equivalent_percent": pct(div(row.get("path_total_us"), (row.get("observed_wall_s") or 0.) * 1.e6)),
        "paint_wall_equivalent_percent": pct(div(row.get("panel_paint_total_us"), (row.get("observed_wall_s") or 0.) * 1.e6)),
        "first_consumer_gap_ms": gaps[0] if gaps else None,
        "consumer_gap_all_ms": distribution(gaps),
        "consumer_gap_excluding_first_ms": distribution(gaps[1:]),
        "heartbeat_gap_p99_ms": row.get("heartbeat_gap_p99_ms"),
        "heartbeat_gap_max_ms": row.get("heartbeat_gap_max_ms"),
        "dispatch_max_ms": None if row.get("dispatch_max_us") is None else row["dispatch_max_us"] / 1000.,
        "cf_dispatch_max_ms": None if row.get("cf_dispatch_max_us") is None else row["cf_dispatch_max_us"] / 1000.,
        "appkit_poll_max_ms": None if row.get("appkit_poll_max_us") is None else row["appkit_poll_max_us"] / 1000.,
        "appkit_send_max_ms": None if row.get("appkit_send_max_us") is None else row["appkit_send_max_us"] / 1000.,
        "positive_request_wake_lateness_us": distribution(callbacks.get(case_id, {}).get("positive_wake_late_us", [])),
        "performance_acceptance": row.get("performance_acceptance"),
    }


def comparison(old, new, kind):
    return {
        "kind": kind, "old_case": old.get("case_id"), "new_case": new.get("case_id"),
        "old_production_percent": pct(old.get("production_ratio")),
        "new_production_percent": pct(new.get("production_ratio")),
        "production_delta_percentage_points": pct(delta(new.get("production_ratio"), old.get("production_ratio"))),
        "process_cpu_delta_percentage_points": delta(new.get("process_cpu_one_core_percent"), old.get("process_cpu_one_core_percent")),
        "producer_cpu_delta_percentage_points": delta(new.get("producer_thread_cpu_one_core_percent"), old.get("producer_thread_cpu_one_core_percent")),
        "message_cpu_delta_percentage_points": delta(new.get("message_thread_cpu_one_core_percent"), old.get("message_thread_cpu_one_core_percent")),
        "old_callback_p99_us": old.get("callback_p99_us"), "new_callback_p99_us": new.get("callback_p99_us"),
        "old_rebases": old.get("deadline_rebases"), "new_rebases": new.get("deadline_rebases"),
        "old_rejections": old.get("rejected_samples"), "new_rejections": new.get("rejected_samples"),
        "performance_acceptance": "NOT_ASSESSED",
    }


def assess_cpu_counters(compact, directory):
    """Keep counter interpretation separate from structural/FIFO integrity."""
    provenance = directory.parent / "eq-attribution-provenance.json"
    platform = "UNKNOWN"
    if provenance.is_file():
        try:
            platform = json.loads(provenance.read_text()).get("runner", {}).get("RUNNER_OS", "UNKNOWN")
        except (OSError, UnicodeError, json.JSONDecodeError):
            pass
    deltas = []
    for row in compact:
        wall = row.get("observed_seconds")
        if wall is not None:
            deltas.extend(row[key] * .01 * wall for key in ["process_cpu_percent", "producer_cpu_percent", "message_cpu_percent"]
                          if row.get(key) is not None and row[key] > 0.)
    quantum = None
    if platform == "Windows" and deltas:
        candidate = min(deltas)
        if candidate >= .001 and all(math.isclose(value / candidate, round(value / candidate), abs_tol=1.e-6) for value in deltas):
            quantum = round(candidate, 9)
    warnings, inconsistent, limited = [], [], []
    for row in compact:
        residual = delta(delta(row.get("process_cpu_percent"), row.get("producer_cpu_percent")), row.get("message_cpu_percent"))
        wall = row.get("observed_seconds")
        residual_seconds = None if residual is None or wall is None else residual * .01 * wall
        row["process_minus_producer_message_cpu_percentage_points"] = residual
        row["process_minus_producer_message_cpu_seconds"] = residual_seconds
        row["cpu_counter_status"] = "NO_NEGATIVE_RESIDUAL_OBSERVED" if residual is not None else "NOT_AVAILABLE"
        row["process_cpu_inference"] = "REPORTED_COUNTER_ONLY_NOT_ABSOLUTE_TRUTH"
        if quantum is not None:
            row["observed_cpu_quantum_seconds"] = quantum
            row["process_cpu_inference"] = "RESOLUTION_LIMITED_REPORTED_COUNTER"
        if residual is not None and residual < 0.:
            if platform == "Windows" and quantum is not None and residual_seconds is not None and abs(residual_seconds) <= 2. * quantum:
                status = "CPU_COUNTER_QUANTIZATION_LIMIT"
                explanation = "Small published process/thread disagreement is within the observed Windows CPU-accounting quanta. Decimal serialization may also contribute. Absolute counter snapshots are unavailable; no correction or exact decomposition is justified."
                limited.append(row["case_id"])
            else:
                status = "CPU_COUNTER_INCONSISTENT"
                explanation = "Reported process CPU is less than producer plus message CPU for the nominal capture window. Exclude the process aggregate and residual from performance inference. Without absolute counter snapshots, the source of disagreement is not established; thread counters are not certified as absolute truth either."
                inconsistent.append(row["case_id"])
                row["process_cpu_inference"] = "EXCLUDE_UNRECONCILED_PROCESS_AGGREGATE"
            row["cpu_counter_status"] = status
            warnings.append({
                "code": status, "case_id": row["case_id"],
                "process_cpu_percent": row.get("process_cpu_percent"),
                "producer_cpu_percent": row.get("producer_cpu_percent"),
                "message_cpu_percent": row.get("message_cpu_percent"),
                "process_minus_producer_message_percentage_points": residual,
                "process_minus_producer_message_CPU_seconds": residual_seconds,
                "explanation": explanation,
            })
    return {
        "scope": "Independent process/thread counter interpretation; separate from structural and FIFO checks",
        "platform": platform, "observed_cpu_quantum_seconds": quantum,
        "warning_count": len(warnings), "warnings": warnings,
        "inconsistent_case_ids": inconsistent, "quantization_limited_negative_case_ids": limited,
        "interpretation": "No negative residual observed is only a consistency observation, never certification of process or thread CPU accuracy. CPU zeros may reflect measurement resolution. Raw values are retained. Steady-clock source production/callback wall times and exact FIFO counters are separate measures; paired FFT CPU uses its own measurement path.",
    }


def audit(directory):
    problems = Problems()
    method_path = directory / "eq-attribution-method.txt"
    try:
        method = method_path.read_text(encoding="utf-8")
    except (OSError, UnicodeError) as exc:
        method = ""
        problems.error("METHOD_MANIFEST_UNREADABLE", detail=str(exc))
    headless = "headless_direct_22_cases" in method
    complete = "complete_90_case_synthetic_matrix" in method
    if headless == complete:
        problems.error("SCOPE_NOT_EXACTLY_ONE_OF_22_OR_90")
    scope = "headless_22" if headless else "complete_90" if complete else "UNKNOWN"
    scale_match = re.search(r"Duration scale=([-+]?(?:\d+(?:\.\d*)?|\.\d+)(?:[eE][-+]?\d+)?)", method)
    scale = parse_number(scale_match.group(1), "duration_scale", method_path.name, 0, problems) if scale_match else None
    if scale_match is None:
        problems.error("DURATION_SCALE_MISSING_FROM_MANIFEST")
    expected = expected_cases(headless, scale) if (headless or complete) else []
    required_summary = set(expected[0]) if expected else {"case_id", "group", "rate", "fft_size"}
    required_summary |= {"callbacks", "callback_records", "produced_samples", "initial_pending", "capture_expected_samples", "consumed_samples", "discarded_samples", "rejected_samples", "pending_samples", "sample_accounting_error", "performance_acceptance", "observed_wall_s", "production_ratio", "deadline_rebases", "rebased_source_time_us"}
    summaries = list(rows(directory / "eq-attribution-summary.csv", required_summary, problems))
    by_id = {}
    for row in summaries:
        case_id = row.get("case_id")
        if case_id in by_id:
            problems.error("DUPLICATE_CASE_ID", case_id=case_id)
        by_id[case_id] = row
    require_equal(len(summaries), len(expected), "CASE_COUNT_MISMATCH", problems)
    for index, wanted in enumerate(expected):
        if index >= len(summaries):
            break
        actual = summaries[index]
        for field, value in wanted.items():
            if field == "requested_duration_s":
                if value is not None and not approximately_equal(actual.get(field), value, abs_tol=1.e-9):
                    problems.error("CASE_DURATION_MISMATCH", case_id=wanted["case_id"], actual=actual.get(field), expected=value)
            else:
                require_equal(actual.get(field), value, "CASE_SHAPE_MISMATCH", problems, case_id=wanted["case_id"], field=field)

    callback_data = defaultdict(callback_aggregate)
    callback_required = {"case_id", "callback", "entry_offset_us", "entry_gap_us", "generation_us", "process_us", "schedule_late_us", "original_source_clock_late_us", "sleep_requested_us", "sleep_actual_us", "wake_late_us", "rebased", "rebase_lost_us", "catch_up"}
    for row in rows(directory / "eq-attribution-callbacks.csv", callback_required, problems):
        case_id = row.get("case_id")
        if case_id not in by_id:
            problems.error("CALLBACK_WITHOUT_SUMMARY", case_id=case_id)
        agg = callback_data[case_id]
        if row.get("callback") != agg["next_index"]:
            agg["index_error_count"] += 1
        agg["next_index"] = (row.get("callback") or 0) + 1
        agg["count"] += 1
        for raw, target in [("generation_us", "generation_us"), ("process_us", "callback_us")]:
            if row.get(raw) is not None:
                agg[target].append(row[raw])
        if row.get("callback") and row.get("entry_gap_us") is not None:
            agg["entry_gap_us"].append(row["entry_gap_us"])
        agg["rebases"] += row.get("rebased") or 0
        agg["rebase_us"] += row.get("rebase_lost_us") or 0.
        agg["catch_up"] += row.get("catch_up") or 0
        if agg["first_entry_us"] is None:
            agg["first_entry_us"] = row.get("entry_offset_us")
        agg["last_entry_us"] = row.get("entry_offset_us")
        agg["last_original_clock_late_us"] = row.get("original_source_clock_late_us")
        debt = row.get("original_source_clock_late_us")
        if debt is not None:
            agg["original_clock_late_max_us"] = max(agg["original_clock_late_max_us"] or 0., debt)
        summary = by_id.get(case_id, {})
        period = div((summary.get("block") or 0) * 1.e6, summary.get("rate"))
        if row.get("schedule_late_us") is not None and period is not None:
            agg["source_starts_late"] += row["schedule_late_us"] > period
        requested, actual, wake = row.get("sleep_requested_us"), row.get("sleep_actual_us"), row.get("wake_late_us")
        if requested is not None and requested > 0 and actual is not None and wake is not None:
            agg["positive_requested_us"].append(requested)
            agg["positive_actual_us"].append(actual)
            agg["positive_wake_late_us"].append(wake)
            agg["positive_actual_minus_requested_us"].append(actual - requested)
            agg["positive_overshoot_us"].append(max(0., actual - requested))
            if requested >= 50.:
                agg["positive_requests_over_50us_wake_late_us"].append(wake)
        elif requested == 0:
            agg["zero_request_count"] += 1
            if wake is not None:
                agg["zero_request_wake_max_us"] = max(agg["zero_request_wake_max_us"] or 0., wake)

    update_data = defaultdict(lambda: {"count": 0, "consumed": 0, "windows": 0, "gaps": [], "fft_us": [], "update_us": [], "path_us": [], "timer_us": [], "last_sequence": None, "skipped_sequences": 0})
    update_required = {"case_id", "elapsed_ms", "update_sequence", "pending_snapshot", "consumed_samples", "fft_windows", "consumer_gap_ms", "fft_us", "update_us", "path_us", "timer_us"}
    for row in rows(directory / "eq-attribution-updates.csv", update_required, problems):
        case_id = row.get("case_id")
        if case_id not in by_id:
            problems.error("UPDATE_WITHOUT_SUMMARY", case_id=case_id)
        agg = update_data[case_id]
        agg["count"] += 1
        agg["consumed"] += row.get("consumed_samples") or 0
        agg["windows"] += row.get("fft_windows") or 0
        for raw, target in [("consumer_gap_ms", "gaps"), ("fft_us", "fft_us"), ("update_us", "update_us"), ("path_us", "path_us"), ("timer_us", "timer_us")]:
            if row.get(raw) is not None:
                agg[target].append(row[raw])
        sequence = row.get("update_sequence")
        if sequence is not None and agg["last_sequence"] is not None:
            if sequence <= agg["last_sequence"]:
                problems.error("NONMONOTONIC_UPDATE_SEQUENCE", case_id=case_id, previous=agg["last_sequence"], current=sequence)
            else:
                agg["skipped_sequences"] += sequence - agg["last_sequence"] - 1
        agg["last_sequence"] = sequence

    accounting_results, legacy_debt, sleep_results = [], [], []
    for row in summaries:
        case_id = row.get("case_id")
        cb, up = callback_data.get(case_id, callback_aggregate()), update_data.get(case_id, {})
        require_equal(cb["count"], row.get("callbacks"), "CALLBACK_ROW_COUNT_MISMATCH", problems, case_id=case_id)
        require_equal(cb["count"], row.get("callback_records"), "CALLBACK_RECORD_COUNT_MISMATCH", problems, case_id=case_id)
        require_equal(cb["index_error_count"], 0, "CALLBACK_INDEX_SEQUENCE_MISMATCH", problems, case_id=case_id)
        require_equal(cb["rebases"], row.get("deadline_rebases"), "REBASE_COUNT_MISMATCH", problems, case_id=case_id)
        require_equal(cb["catch_up"], row.get("catch_up_blocks"), "CATCHUP_COUNT_MISMATCH", problems, case_id=case_id)
        if not approximately_equal(cb["rebase_us"], row.get("rebased_source_time_us")):
            problems.error("RAW_REBASE_TIME_MISMATCH", case_id=case_id, raw=cb["rebase_us"], summary=row.get("rebased_source_time_us"))
        require_equal(up.get("count", 0), row.get("analysis_updates"), "UPDATE_ROW_COUNT_MISMATCH", problems, case_id=case_id)
        require_equal(up.get("windows", 0), row.get("fft_windows"), "FFT_WINDOW_SUM_MISMATCH", problems, case_id=case_id)
        require_equal(row.get("performance_acceptance"), "NOT_ASSESSED", "INVALID_PERFORMANCE_ACCEPTANCE_CLAIM", problems, case_id=case_id)
        needed = ["callbacks", "block", "produced_samples", "initial_pending", "capture_expected_samples", "consumed_samples", "discarded_samples", "rejected_samples", "pending_samples", "sample_accounting_error"]
        if all(row.get(field) is not None for field in needed):
            for field in needed:
                if field != "sample_accounting_error" and row[field] < 0:
                    problems.error("NEGATIVE_SAMPLE_COUNT", case_id=case_id, field=field, value=row[field])
            require_equal(row["produced_samples"], row["callbacks"] * row["block"], "PRODUCED_SAMPLE_COUNT_MISMATCH", problems, case_id=case_id)
            capture = row.get("fft_size") != 0 and row.get("stage") != NOOP
            require_equal(row["capture_expected_samples"], row["produced_samples"] if capture else 0, "CAPTURE_EXPECTATION_MISMATCH", problems, case_id=case_id)
            computed = row["initial_pending"] + row["capture_expected_samples"] - row["consumed_samples"] - row["discarded_samples"] - row["rejected_samples"] - row["pending_samples"]
            require_equal(computed, row["sample_accounting_error"], "REPORTED_FIFO_ERROR_MISMATCH", problems, case_id=case_id)
            require_equal(computed, 0, "FIFO_SAMPLE_ACCOUNTING_ERROR", problems, case_id=case_id)
            expected_flow = ("ACCOUNTING_ERROR" if computed else "CAPTURE_OFF" if not capture
                             else "FIFO_REJECTION" if row["rejected_samples"] else "DISCONTINUITY"
                             if row["discarded_samples"] else "NO_LOSS" if row.get("fft_windows") else "NO_FFT_WINDOW")
            require_equal(row.get("sample_flow_status"), expected_flow, "FIFO_FLOW_LABEL_MISMATCH", problems, case_id=case_id)
            accounting_results.append({"case_id": case_id, "computed_error": computed, "flow": row.get("sample_flow_status"), "rejected": row["rejected_samples"], "discarded": row["discarded_samples"], "pending": row["pending_samples"]})
            # Last-consumed telemetry can be reset during a reported recovery.
            # Full cumulative accounting remains the authority in that case.
            if up.get("consumed", 0) != row["consumed_samples"]:
                if row["discarded_samples"] or row["rejected_samples"]:
                    problems.warning("UPDATE_CONSUMED_SUM_DIFFERS_DURING_RECOVERY", case_id=case_id, raw=up.get("consumed", 0), cumulative=row["consumed_samples"])
                else:
                    problems.error("UPDATE_CONSUMED_SUM_MISMATCH", case_id=case_id, raw=up.get("consumed", 0), cumulative=row["consumed_samples"])
        ratio = div(row.get("produced_samples"), (row.get("observed_wall_s") or 0) * (row.get("rate") or 0))
        if not approximately_equal(ratio, row.get("production_ratio"), abs_tol=1.e-9):
            problems.error("PRODUCTION_RATIO_MISMATCH", case_id=case_id, computed=ratio, reported=row.get("production_ratio"))
        nominal = None if ratio is None else int(.95 <= ratio <= 1.05)
        require_equal(row.get("within_5pct_nominal_rate"), nominal, "NOMINAL_RATE_FLAG_MISMATCH", problems, case_id=case_id)
        if ratio is not None and row.get("source_status") != "PRODUCER_ERROR":
            expected_source = "UNDERPRODUCED" if ratio < .95 else "OVERPRODUCED" if ratio > 1.05 else "NEAR_NOMINAL_DIAGNOSTIC"
            require_equal(row.get("source_status"), expected_source, "SOURCE_STATUS_LABEL_MISMATCH", problems, case_id=case_id)
        if row.get("unobserved_updates"):
            problems.warning("UNOBSERVED_UPDATES_RETAINED", case_id=case_id, count=row["unobserved_updates"])
        if row.get("source_status") == "PRODUCER_ERROR":
            problems.warning("PRODUCER_ERROR_RETAINED", case_id=case_id)
        wall_us = None if row.get("observed_wall_s") is None else row["observed_wall_s"] * 1.e6
        produced_duration_us = div((row.get("produced_samples") or 0) * 1.e6, row.get("rate"))
        debt_us = delta(wall_us, produced_duration_us)
        rebase_us = cb["rebase_us"]
        residual_us = delta(debt_us, rebase_us)
        if row.get("pacing") == LEGACY:
            legacy_debt.append({
                "case_id": case_id, "group": row.get("group"), "rate": row.get("rate"),
                "stage": row.get("stage"), "consumer": row.get("consumer"), "fft": row.get("fft_size"),
                "production_percent": pct(ratio), "wall_source_shortfall_signed_us": debt_us,
                "legacy_rebased_source_time_us": rebase_us,
                "rebase_share_of_positive_shortfall_unclamped": div(rebase_us, debt_us) if debt_us is not None and debt_us > 0 else None,
                "residual_unexplained_source_debt_signed_us": residual_us,
                "residual_in_nominal_block_periods": div(residual_us, div((row.get("block") or 0) * 1.e6, row.get("rate"))),
                "observed_shortfall_percentage_points": None if ratio is None else 100. * (1. - ratio),
                "rebase_shortfall_percentage_points": pct(div(rebase_us, wall_us)),
                "residual_shortfall_percentage_points": pct(div(residual_us, wall_us)),
                "first_source_entry_us": cb["first_entry_us"],
                "last_source_entry_us": cb["last_entry_us"],
                "last_original_clock_lateness_us": cb["last_original_clock_late_us"],
                "interpretation": "Time-accounting attribution to deadline rebasing; it does not establish which stage originally missed a deadline. Residual includes capture boundaries and final sleep/wakeup/bookkeeping.",
            })
        sleep_results.append({
            "case_id": case_id, "pacing": row.get("pacing"), "rate": row.get("rate"),
            "stage": row.get("stage"), "consumer": row.get("consumer"), "fft": row.get("fft_size"),
            "positive_request_rows": len(cb["positive_requested_us"]),
            "excluded_zero_request_rows": cb["zero_request_count"],
            "excluded_zero_request_wake_max_us": cb["zero_request_wake_max_us"],
            "positive_request_wake_lateness_us": distribution(cb["positive_wake_late_us"]),
            "positive_request_sleep_overshoot_us": distribution(cb["positive_overshoot_us"]),
            "actual_minus_requested_signed_us": distribution(cb["positive_actual_minus_requested_us"]),
            "requests_at_least_50us_wake_lateness_us": distribution(cb["positive_requests_over_50us_wake_late_us"]),
            "interpretation": "Only sleep_requested_us > 0 rows contribute. Wake lateness is relative to the requested deadline. Actual-minus-requested also contains small timestamp/bookkeeping differences; the 50us subset checks that boundary.",
        })

    compact = [compact_case(row, callback_data, update_data) for row in summaries]
    cpu_quality = assess_cpu_counters(compact, directory)
    high_rows = [row for row in summaries if row.get("rate") == 192000]
    compact_by_id = {row["case_id"]: row for row in compact}
    pacing_pairs, stage_pairs, generator_pairs, consumer_pairs = [], [], [], []
    grouping = defaultdict(dict)
    pair_fields = ("group", "repeat", "rate", "block", "stage", "signal", "consumer", "fft_size", "requested_update_ms")
    for row in high_rows:
        grouping[tuple(row.get(field) for field in pair_fields)][row.get("pacing")] = row
    for pair in grouping.values():
        if LEGACY in pair and DUE in pair:
            pacing_pairs.append(comparison(pair[LEGACY], pair[DUE], "legacy_to_due_catch_up"))

    def select(**terms):
        matches = [row for row in high_rows if all(row.get(key) == value for key, value in terms.items())]
        return matches[0] if len(matches) == 1 else None

    for pacing in [LEGACY, DUE]:
        noop = select(group="stage_clock_control", stage=NOOP, pacing=pacing)
        full = select(group="stage_clock_control", stage=FULL, pacing=pacing)
        eq = select(group="stage_clock_control", stage=EQPAIR, pacing=pacing)
        if noop and full:
            stage_pairs.append(comparison(noop, full, "noop_to_full_processor"))
        if noop and eq:
            stage_pairs.append(comparison(noop, eq, "noop_to_eq_pair"))
        for stage in [NOOP, FULL]:
            calculated = select(group="stage_clock_control", stage=stage, pacing=pacing)
            cached = select(group="generator_control", stage=stage, pacing=pacing)
            if calculated and cached:
                generator_pairs.append(comparison(calculated, cached, "calculated_to_cached_same_tone"))
        for fft in [0, 2048, 4096, 8192]:
            direct = select(group="stage_clock_control" if fft == 0 else "consumer_control", stage=FULL, pacing=pacing, consumer=DIRECT, fft_size=fft)
            suppressed = select(group="consumer_control", pacing=pacing, consumer=NO_PAINT, fft_size=fft)
            paint = select(group="consumer_control", pacing=pacing, consumer=PAINT, fft_size=fft)
            for old, new, kind in [(direct, suppressed, "direct_to_native_panel_paint_suppressed"), (suppressed, paint, "native_panel_paint_suppressed_to_native_paint"), (direct, paint, "direct_to_native_paint")]:
                if old and new:
                    consumer_pairs.append(comparison(old, new, kind))

    grid = [row for row in compact if row.get("group") == "rate_fft_cadence"]
    grid_by_rate = []
    for rate in [44100, 48000, 96000, 192000]:
        selected = [row for row in grid if row.get("rate") == rate]
        if not selected:
            continue
        grid_by_rate.append({
            "rate": rate, "count": len(selected),
            "production_percent": distribution([row["production_percent"] for row in selected]),
            "producer_cpu_percent": distribution([row["producer_cpu_percent"] for row in selected]),
            "process_cpu_percent": distribution([row["process_cpu_percent"] for row in selected]),
            "underproduced_cases": [row["case_id"] for row in selected if row["source_status"] == "UNDERPRODUCED"],
            "fifo_loss_cases": [row["case_id"] for row in selected if (row["rejected"] or 0) or (row["discarded"] or 0)],
            "rows": selected,
        })

    return {
        "schema": "chimera.eq-attribution-evidence-audit.v1",
        "input_directory": str(directory.resolve()), "scope": scope, "duration_scale": scale,
        "evidence_integrity": {
            "status": "CONSISTENT" if not problems.error_count else "INCONSISTENT",
            "expected_case_count": len(expected), "recorded_case_count": len(summaries),
            "callback_row_count": sum(agg["count"] for agg in callback_data.values()),
            "update_row_count": sum(agg["count"] for agg in update_data.values()),
            "error_count": problems.error_count, "warning_count": problems.warning_count,
            "errors": problems.errors, "warnings": problems.warnings,
        },
        "performance_acceptance": "NOT_ASSESSED",
        "cpu_measurement_quality": cpu_quality,
        "interpretation": [
            "Evidence consistency and zero FIFO-accounting error do not qualify real-time audio or DAW stability.",
            "Due-sample pacing may restore aggregate production by issuing late catch-up bursts. It is not a hardware audio clock.",
            "A legacy rebase explains lost source time arithmetically, not whether scheduling, generation, audio DSP or another thread caused the original missed deadline.",
            "CPU percentages use actual process/thread CPU-time deltas and can exceed 100 for the whole process. FFT/update/path/paint/CF/AppKit costs are inclusive wall durations and must not be added blindly.",
            "Process/thread CPU disagreement is separately flagged, without rewriting raw values or claiming either counter is absolute truth. Structural/FIFO consistency does not validate CPU counters.",
            "Suppressed native painting covers the EQ panel only; other editor activity remains. Direct mode has no native dispatch or paint.",
            "The first consumer gap can include setup. Both all-gap and first-excluded distributions are retained.",
            "Underproduction and FIFO loss rows are preserved rather than filtered out. Pending samples at capture end are not losses.",
            "The fixture's UNDERPRODUCED label means below 95%, not any production below 100%. Numeric shortfalls and all below-nominal rows are separately retained; NEAR_NOMINAL_DIAGNOSTIC is never a performance pass.",
            "Short CI captures identify measurement bottlenecks; no physical audio interface, actual DAW session or long-session acceptance is assessed.",
        ],
        "coverage": {
            "groups": dict(Counter(row.get("group") for row in summaries)),
            "sample_rates": sorted({row["rate"] for row in summaries if row.get("rate") is not None}),
            "fft_sizes": sorted({row["fft_size"] for row in summaries if row.get("fft_size") is not None}),
            "latency_sample_values": sorted({row["latency_samples"] for row in summaries if row.get("latency_samples") is not None}),
            "native_ui": "NOT_RUN" if headless else "SYNTHETIC_OBSERVED" if complete else "UNKNOWN",
            "grid_by_rate": grid_by_rate,
        },
        "high_rate_192k": {
            "stage_controls": [compact_by_id[row["case_id"]] for row in high_rows if row.get("group") == "stage_clock_control"],
            "generator_controls": [compact_by_id[row["case_id"]] for row in high_rows if row.get("group") == "generator_control"],
            "consumer_controls": [compact_by_id[row["case_id"]] for row in high_rows if row.get("group") == "consumer_control"],
            "native_off_repeats": [compact_by_id[row["case_id"]] for row in high_rows if row.get("group") == "native_off_repeat"],
            "legacy_due_pairs": pacing_pairs, "noop_stage_pairs": stage_pairs,
            "generator_cache_pairs": generator_pairs, "consumer_pairs": consumer_pairs,
        },
        "legacy_source_clock_reconciliation": legacy_debt,
        "positive_sleep_request_diagnostics": sleep_results,
        "fifo_accounting": accounting_results,
        "underproduced_rows": [row for row in compact if row.get("source_status") == "UNDERPRODUCED"],
        "below_nominal_rows": [row for row in compact if row.get("production_percent") is not None and row["production_percent"] < 100.],
        "fifo_loss_rows": [row for row in compact if (row.get("rejected") or 0) or (row.get("discarded") or 0)],
        "all_cases": compact,
    }


def markdown(result):
    integrity = result["evidence_integrity"]
    cpu = result.get("cpu_measurement_quality", {})
    lines = [f"Scope: **{result['scope']}**; structural/FIFO evidence: **{integrity['status']}**; cases: {integrity['recorded_case_count']}/{integrity['expected_case_count']}; structural errors: {integrity['error_count']}; separate CPU warnings: {cpu.get('warning_count', 0)}. DAW: **NOT_ASSESSED**.", "", "| Case | 192k control | Pacing | Production | Producer CPU | Process CPU | Rebase loss |", "|---:|---|---|---:|---:|---:|---:|"]
    def fmt(value, suffix="", digits=2):
        return "n/a" if value is None else f"{value:.{digits}f}{suffix}"
    for row in result["high_rate_192k"]["stage_controls"] + result["high_rate_192k"]["generator_controls"]:
        label = row["stage"] + (" cached" if row["signal"] == CACHED else "")
        lines.append(f"| {row['case_id']} | {label} | {'legacy' if row['pacing'] == LEGACY else 'due'} | {fmt(row['production_percent'], '%')} | {fmt(row['producer_cpu_percent'], '%')} | {fmt(row['process_cpu_percent'], '%')} | {fmt(row['rebase_lost_ms'], ' ms')} |")
    lines += ["", "| Rate | Grid cases | Production min–max | Underproduced | FIFO loss |", "|---:|---:|---:|---:|---:|"]
    for group in result["coverage"]["grid_by_rate"]:
        rates = [row["production_percent"] for row in group["rows"] if row["production_percent"] is not None]
        bounds = f"{min(rates):.2f}–{max(rates):.2f}%" if rates else "n/a"
        lines.append(f"| {group['rate']} | {group['count']} | {bounds} | {len(group['underproduced_cases'])} | {len(group['fifo_loss_cases'])} |")
    return "\n".join(lines) + "\n"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("directory", type=Path)
    parser.add_argument("--output", type=Path, help="Save JSON here; inputs are never modified.")
    parser.add_argument("--table", action="store_true", help="Emit a compact Markdown table to stderr.")
    args = parser.parse_args()
    result = audit(args.directory)
    encoded = json.dumps(result, ensure_ascii=False, indent=2, allow_nan=False) + "\n"
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(encoded, encoding="utf-8")
    else:
        sys.stdout.write(encoded)
    if args.table:
        sys.stderr.write(markdown(result))
    return 0 if result["evidence_integrity"]["status"] == "CONSISTENT" else 2


if __name__ == "__main__":
    raise SystemExit(main())
