#!/usr/bin/env python3
"""Read-only CPU/dispatch/rebase supplement to analyze_native_eq.py JSON."""
from __future__ import annotations

import argparse
import collections
import json
from pathlib import Path

import analyze_native_eq as base


def supplement(directory, audit_file):
    audit = json.loads(audit_file.read_text())
    problems = base.Problems()
    if Path(audit["input_directory"]).resolve() != directory.resolve():
        problems.error("AUDIT_INPUT_DIRECTORY_MISMATCH")
    summaries = {r["case_id"]: r for r in base.rows(
        directory / "eq-attribution-summary.csv",
        {"case_id", "callback_budget_us", "observed_wall_s"}, problems)}
    events, previous = collections.defaultdict(list), {}
    callback_fields = {"case_id", "callback", "entry_offset_us", "schedule_late_us",
                       "generation_us", "process_us", "rebased", "rebase_lost_us",
                       "sleep_requested_us", "sleep_actual_us", "wake_late_us"}
    for raw in base.rows(directory / "eq-attribution-callbacks.csv", callback_fields, problems):
        case = raw.get("case_id")
        summary = summaries.get(case, {})
        prior = previous.get(case)
        if raw.get("rebased"):
            needed = [raw.get(k) for k in ["schedule_late_us", "generation_us", "process_us", "rebase_lost_us"]]
            period = summary.get("callback_budget_us")
            if None in needed or period is None:
                problems.error("REBASE_RECORD_MISSING_INTERVAL", case_id=case)
            else:
                entry, generation, processing, lost = needed
                threshold = 3. * period
                if entry > threshold:
                    where = "already_above_three_periods_at_entry"
                elif entry + generation > threshold:
                    where = "threshold_crossed_during_generation_wall"
                elif entry + generation + processing > threshold:
                    where = "threshold_crossed_during_process_wall"
                else:
                    where = "threshold_rounding_boundary"
                events[case].append({
                    "callback": raw.get("callback"), "entry_offset_us": raw.get("entry_offset_us"),
                    "schedule_late_at_entry_us": entry, "generation_wall_us": generation,
                    "process_wall_us": processing, "rebase_lost_us": lost,
                    "nominal_block_period_us": period,
                    "nominal_clock_decomposition_residual_us": lost - (entry + generation + processing - period),
                    "threshold_crossing_interval": where,
                    "entry_over_one_period_late": entry > period,
                    "process_over_one_period": processing > period,
                    "previous_positive_sleep_request": None if prior is None else (prior.get("sleep_requested_us") or 0.) > 0.,
                    "previous_sleep_requested_us": None if prior is None else prior.get("sleep_requested_us"),
                    "previous_sleep_actual_us": None if prior is None else prior.get("sleep_actual_us"),
                    "previous_wake_late_us": None if prior is None else prior.get("wake_late_us"),
                })
        previous[case] = raw

    compact_fields = ["case_id", "group", "rate", "fft", "stage", "pacing", "consumer",
                      "production_percent", "process_cpu_percent", "producer_cpu_percent",
                      "message_cpu_percent", "callbacks", "callback_execution_overruns",
                      "late_source_starts", "catch_up_blocks", "max_consecutive_catch_up",
                      "source_clock_late_max_us", "callback_max_us", "analysis_wall_equivalent_percent",
                      "paint_wall_equivalent_percent", "path_wall_equivalent_percent",
                      "consumer_gap_excluding_first_ms", "cpu_counter_status", "process_cpu_inference"]
    cases = []
    for compact in audit["all_cases"]:
        case = compact["case_id"]
        summary = summaries.get(case, {})
        groups = collections.defaultdict(lambda: {
            "events": 0, "rebased_source_us": 0.,
            "previous_positive_sleep_events": 0, "entry_within_one_period_events": 0})
        for event in events[case]:
            group = groups[event["threshold_crossing_interval"]]
            group["events"] += 1
            group["rebased_source_us"] += event["rebase_lost_us"]
            group["previous_positive_sleep_events"] += bool(event["previous_positive_sleep_request"])
            group["entry_within_one_period_events"] += not event["entry_over_one_period_late"]
        residual = base.delta(base.delta(compact.get("process_cpu_percent"), compact.get("producer_cpu_percent")), compact.get("message_cpu_percent"))
        wall = compact.get("observed_seconds")
        out = {key: compact.get(key) for key in compact_fields}
        out.update({
            "process_minus_producer_message_CPU_percentage_points": residual,
            "observed_seconds": wall, "rebase_trigger_groups": dict(groups),
            "rebase_events": events[case],
        })
        for kind in ["process", "producer", "message"]:
            value = compact.get(f"{kind}_cpu_percent")
            out[f"{kind}_CPU_seconds_in_capture"] = None if value is None or wall is None else value * .01 * wall
        out["unattributed_CPU_seconds_in_capture"] = None if residual is None or wall is None else residual * .01 * wall
        for target, field in [
            ("analysis_updates_per_second", "analysis_updates"),
            ("panel_paints_per_second", "panel_paint_calls"),
            ("fft_windows_per_second", "fft_windows"),
            ("dispatch_calls_per_second", "dispatch_calls"),
        ]:
            out[target] = base.div(summary.get(field), wall)
        out["callback_execution_overrun_percent"] = base.pct(base.div(summary.get("callback_execution_overruns"), summary.get("callbacks")))
        for label, field in [
            ("dispatch", "dispatch_total_us"), ("cf", "cf_dispatch_inclusive_us"),
            ("appkit_poll", "appkit_poll_inclusive_us"), ("appkit_send", "appkit_send_inclusive_us"),
        ]:
            out[f"{label}_inclusive_wall_percent"] = base.div(summary.get(field), None if wall is None else wall * 1.e4)
        for target, field in [
            ("dispatch_max_us", "dispatch_max_us"), ("cf_max_us", "cf_dispatch_max_us"),
            ("appkit_poll_max_us", "appkit_poll_max_us"), ("appkit_send_max_us", "appkit_send_max_us"),
            ("heartbeat_gap_p99_ms", "heartbeat_gap_p99_ms"), ("heartbeat_gap_max_ms", "heartbeat_gap_max_ms"),
        ]:
            out[target] = summary.get(field)
        cases.append(out)
    provenance_path = directory.parent / "eq-attribution-provenance.json"
    provenance = json.loads(provenance_path.read_text()) if provenance_path.is_file() else {}
    return {
        "schema": "chimera.eq-attribution-interval-supplement.v1",
        "input_directory": str(directory.resolve()),
        "source_commit": provenance.get("source_commit"),
        "run": provenance.get("runner", {}).get("GITHUB_RUN_ID"),
        "runner": provenance.get("runner"),
        "performance_acceptance": "NOT_ASSESSED",
        "evidence_integrity": audit["evidence_integrity"],
        "cpu_measurement_quality": audit.get("cpu_measurement_quality"),
        "supplement_parse_errors": problems.errors,
        "rebase_interpretation": "Entry lateness + generator wall + process wall > three nominal periods triggers a rebase. Wall spans can contain descheduling. Classification locates threshold crossing, not an exclusive compute/scheduler cause. Period rounding remains visible as a numeric residual.",
        "CPU_interpretation": "Process minus producer minus message uses independently sampled platform CPU counters. Negative residuals cannot identify a physical cause without absolute snapshots. Flagged process aggregates are excluded from performance inference; positive residuals and thread CPU are not certified as absolute truth. Windows small disagreements may be limited by observed CPU quanta/serialization. Cgroup CPU has a wider process scope.",
        "inclusive_wall_interpretation": "FFT, analysis, path, timer, paint and CF/AppKit/dispatch intervals may contain or call one another; do not sum inclusive wall metrics as CPU.",
        "case_CPU_and_events": cases,
        "negative_CPU_residual_cases": [
            {"case_id": r["case_id"], "CPU_percentage_points": r["process_minus_producer_message_CPU_percentage_points"]}
            for r in cases if r["process_minus_producer_message_CPU_percentage_points"] is not None
            and r["process_minus_producer_message_CPU_percentage_points"] < 0.],
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("directory", type=Path)
    parser.add_argument("--audit", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    result = supplement(args.directory, args.audit)
    args.output.write_text(json.dumps(result, ensure_ascii=False, indent=2, allow_nan=False) + "\n")
    return 2 if result["supplement_parse_errors"] else 0


if __name__ == "__main__":
    raise SystemExit(main())
