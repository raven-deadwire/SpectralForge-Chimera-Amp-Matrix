#!/usr/bin/env python3
"""Read existing EQ cost/parity artifacts; never runs a build or benchmark.

Usage:
  python eq-performance-report.py ARTIFACT_DIR_OR_ZIP --platform Windows \
      --expected-source ddb0880 --output-prefix /absolute/scratch/windows-eq

Only standard-library Python is required. Input receipt bytes are never changed.
"""
from __future__ import annotations

import argparse
import csv
import hashlib
import io
import json
import math
from pathlib import Path, PurePosixPath
import statistics
import sys
import xml.etree.ElementTree as ET
import zipfile
from collections import defaultdict

FILES = {
    "summary": "eq-analyzer-performance-summary.csv",
    "paired": "eq-analyzer-performance-paired.csv",
    "parity": "eq-analyzer-performance-parity.csv",
}
RATES = (44100, 48000, 96000, 192000)
SIZES = (2048, 4096, 8192)
CADENCES = (20, 40, 80)
ORDER = ("baseline_beb72f", "candidate", "candidate", "baseline_beb72f",
         "candidate", "baseline_beb72f", "baseline_beb72f", "candidate")
PAIRS = ((0, 1), (3, 2), (5, 4), (6, 7))


class Receipts:
    def __init__(self, path: Path):
        self.path = path.resolve()
        self.archive = zipfile.ZipFile(self.path) if self.path.is_file() else None
        self.names = (sorted(n for n in self.archive.namelist() if not n.endswith("/"))
                      if self.archive else
                      sorted(p.relative_to(self.path).as_posix() for p in self.path.rglob("*") if p.is_file()))

    def data(self, name: str) -> bytes:
        return self.archive.read(name) if self.archive else (self.path / name).read_bytes()

    def rows(self, name: str) -> list[dict[str, str]]:
        return list(csv.DictReader(io.StringIO(self.data(name).decode("utf-8-sig"))))


def number(row, field):
    value = float(row[field])
    if not math.isfinite(value):
        raise ValueError(f"Nonfinite {field}: {row[field]!r}")
    return value


def integer(row, field):
    value = number(row, field)
    if value != int(value):
        raise ValueError(f"Noninteger {field}: {row[field]!r}")
    return int(value)


def key(row):
    interval = None if not row["ui_interval_ms"].strip() else integer(row, "ui_interval_ms")
    return integer(row, "rate"), integer(row, "fft_size"), row["fixture"], interval


def reduction(baseline, candidate):
    return None if baseline <= 0 else 100.0 * (1.0 - candidate / baseline)


def median(values):
    values = [v for v in values if v is not None]
    return statistics.median(values) if values else None


def same(a, b):
    return math.isclose(a, b, rel_tol=1e-8, abs_tol=1e-6)


def evaluate(receipts, parent, platform, expected_source):
    names = {kind: (str(PurePosixPath(parent) / filename) if parent else filename)
             for kind, filename in FILES.items()}
    result = {"platform": platform, "dataset": parent or ".", "input_artifact": str(receipts.path),
              "performance_acceptance": "NOT_ASSESSED", "issues": [], "notes": [], "cells": []}
    issues = result["issues"]

    def require(condition, message):
        if not condition:
            issues.append(message)

    missing = [name for name in names.values() if name not in receipts.names]
    if missing:
        result.update(csv_gate="INCOMPLETE", missing_files=missing)
        issues.append("Required CSV receipt missing")
        return result

    rows = {kind: receipts.rows(name) for kind, name in names.items()}
    result["receipts_sha256"] = {name: hashlib.sha256(receipts.data(name)).hexdigest() for name in names.values()}
    result["row_counts"] = {kind: len(value) for kind, value in rows.items()}
    for kind, count in (("summary", 48), ("paired", 384), ("parity", 108)):
        require(len(rows[kind]) == count, f"{kind}: expected {count} rows, found {len(rows[kind])}")

    provenance_name = str(PurePosixPath(parent) / "eq-attribution-provenance.json") if parent else "eq-attribution-provenance.json"
    if provenance_name in receipts.names:
        provenance = json.loads(receipts.data(provenance_name))
        result["source_commit"] = provenance.get("source_commit")
        result["runner"] = provenance.get("runner")
        result["source_sha256"] = {name: digest for name, digest in provenance.get("source_sha256", {}).items()
                                   if name in ("Source/EQSpectrumAnalyzer.h", "Tests/EQAnalyzerPerformanceTests.cpp",
                                               "Tests/reference/EQSpectrumAnalyzerBeb72f.h")}
        if expected_source:
            require(str(result["source_commit"]).startswith(expected_source),
                    f"Source mismatch: expected {expected_source}, found {result['source_commit']}")
        require(provenance.get("performance_acceptance") == "NOT_ASSESSED", "Provenance upgrades untested native acceptance")
    elif expected_source:
        require(False, "Expected-source verification requested, but source provenance receipt is absent")
    else:
        result["notes"].append("Source provenance was not supplied; verify CI/head identity separately.")

    groups = defaultdict(list)
    summaries = {}
    for row in rows["paired"]:
        groups[key(row)].append(row)
    for row in rows["summary"]:
        k = key(row)
        require(k not in summaries, f"Duplicate summary cell {k}")
        summaries[k] = row
    expected = {(rate, size, "nominal_cadence", interval) for rate in RATES for size in SIZES for interval in CADENCES}
    expected |= {(rate, size, "full_capacity_catchup", None) for rate in RATES for size in SIZES}
    require(set(groups) == expected, f"Raw cost grid incomplete/unexpected: missing={sorted(map(str, expected-set(groups)))}, extra={sorted(map(str, set(groups)-expected))}")
    require(set(summaries) == expected, "Summary cost grid incomplete or unexpected")
    raw_zero_trials = 0
    raw_cpu_zero = 0
    raw_losses = 0

    for k in sorted(groups, key=lambda item: (item[0], item[1], item[2], item[3] or 0)):
        rate, size, fixture, interval = k
        trial_rows = groups[k]
        by_sequence = {integer(row, "sequence"): row for row in trial_rows}
        require(len(trial_rows) == 8 and set(by_sequence) == set(range(8)), f"ABBA BAAB sequence incomplete {k}")
        samples = 65536 if fixture == "full_capacity_catchup" else 2 * rate
        batch = 65536 if fixture == "full_capacity_catchup" else int(rate * interval / 1000)
        hop = size // 4
        windows = samples // hop
        calls = math.ceil(samples / batch)
        expected_zero_calls = sum(end // hop == start // hop
                                  for start in range(0, samples, batch)
                                  for end in [min(samples, start + batch)])
        trial_metrics = defaultdict(list)
        for seq, row in sorted(by_sequence.items()):
            implementation = row["implementation"]
            require(seq < 8 and implementation == ORDER[seq], f"Wrong paired order {k}, sequence {seq}")
            require(integer(row, "produced_samples") == samples, f"Wrong sample total {k}, sequence {seq}")
            require(integer(row, "fft_windows") == windows, f"Window-count mismatch {k}, sequence {seq}")
            require(integer(row, "consumer_calls") == calls, f"Consumer-call mismatch {k}, sequence {seq}")
            lost = integer(row, "dropped_samples")
            raw_losses += lost
            require(lost == 0, f"FIFO loss {k}, sequence {seq}: {lost}")
            raw_zero_trials += integer(row, "fft_windows") == 0
            update = number(row, "analysis_update_wall_ms")
            fft = number(row, "fft_operation_wall_ms")
            wall = number(row, "feed_and_analysis_wall_ms")
            cpu = number(row, "feed_and_analysis_thread_cpu_ms")
            require(update > 0 and cpu >= 0, f"Invalid timing value {k}, sequence {seq}")
            require(fft <= update + 1e-6 and update <= wall + 1e-6, f"Nested wall timing inconsistent {k}, sequence {seq}")
            raw_cpu_zero += cpu == 0
            trial_metrics[(implementation, "update")].append(update)
            trial_metrics[(implementation, "cpu")].append(cpu)
            trial_metrics[(implementation, "max_update_us")].append(number(row, "max_update_us"))
        a = median(trial_metrics[("baseline_beb72f", "update")])
        b = median(trial_metrics[("candidate", "update")])
        ca = median(trial_metrics[("baseline_beb72f", "cpu")])
        cb = median(trial_metrics[("candidate", "cpu")])
        if None in (a, b, ca, cb):
            issues.append(f"Missing implementation trials {k}")
            continue
        paired_gains = [reduction(number(by_sequence[x], "analysis_update_wall_ms"),
                                  number(by_sequence[y], "analysis_update_wall_ms"))
                        for x, y in PAIRS if x in by_sequence and y in by_sequence]
        cell = {"rate": rate, "fft_size": size, "fixture": fixture, "ui_interval_ms": interval,
                "samples_per_trial": samples, "fft_windows_per_trial": windows, "consumer_calls_per_trial": calls,
                "expected_zero_fft_calls_from_fixture": expected_zero_calls,
                "zero_fft_calls_observed_in_cost_csv": "NOT_RECORDED_PER_UPDATE",
                "baseline_update_median_ms": a, "candidate_update_median_ms": b,
                "update_cost_reduction_percent": reduction(a, b),
                "baseline_cpu_median_ms": ca, "candidate_cpu_median_ms": cb,
                "cpu_cost_reduction_percent": reduction(ca, cb) if ca > 0 and cb > 0 else None,
                "cpu_reduction_status": "COMPARABLE_OBSERVATION" if ca > 0 and cb > 0 else "ZERO_MEDIAN_TIMER_RESOLUTION_LIMIT",
                "adjacent_pair_update_reductions_percent": paired_gains,
                "adjacent_pair_reduction_median_percent": median(paired_gains),
                "baseline_update_trials_ms": trial_metrics[("baseline_beb72f", "update")],
                "candidate_update_trials_ms": trial_metrics[("candidate", "update")],
                "baseline_cpu_trials_ms": trial_metrics[("baseline_beb72f", "cpu")],
                "candidate_cpu_trials_ms": trial_metrics[("candidate", "cpu")],
                "baseline_max_update_us_median": median(trial_metrics[("baseline_beb72f", "max_update_us")]),
                "candidate_max_update_us_median": median(trial_metrics[("candidate", "max_update_us")])}
        result["cells"].append(cell)
        summary = summaries.get(k)
        if summary:
            for field, expected_value in (("baseline_update_median_ms", a), ("candidate_update_median_ms", b),
                                          ("baseline_feed_analysis_cpu_median_ms", ca), ("candidate_feed_analysis_cpu_median_ms", cb),
                                          ("update_cost_reduction_percent", reduction(a, b))):
                require(same(number(summary, field), expected_value), f"Summary/raw mismatch {k}: {field}")
            require(integer(summary, "pairs") == 4 and integer(summary, "fft_windows_per_trial") == windows,
                    f"Summary pair/window metadata mismatch {k}")
            require(summary["daw_performance_status"] == "NOT_ASSESSED", f"Native acceptance incorrectly promoted {k}")

    pkeys = set()
    pmax = {field: 0.0 for field in ("max_raw_error_db", "max_envelope_error_db", "max_display_error_db",
                                    "max_raw_power_relative_error", "max_smoothed_power_relative_error")}
    parity_losses = parity_discontinuities = 0
    for row in rows["parity"]:
        rate, size, interval, mode = (integer(row, field) for field in ("rate", "fft_size", "ui_interval_ms", "smoothing"))
        pk = rate, size, interval, mode
        require(pk not in pkeys, f"Duplicate parity case {pk}")
        pkeys.add(pk)
        consumed = integer(row, "consumed_samples")
        expected_consumed = int(rate * .48) + 2 * size
        hop = size // 4
        require(consumed == expected_consumed, f"Parity fixture sample mismatch {pk}")
        require(integer(row, "fft_windows") == 1 + (consumed - size) // hop, f"Parity window mismatch {pk}")
        require(integer(row, "remaining_hop_samples") == (consumed - size) % hop, f"Parity remaining-hop mismatch {pk}")
        require(integer(row, "checked_updates") == math.ceil(consumed / int(rate * interval / 1000)), f"Parity update-count mismatch {pk}")
        require(integer(row, "parity_pass") == 1, f"Parity failure {pk}")
        for field in pmax:
            value = number(row, field)
            pmax[field] = max(pmax[field], value)
            require(value >= 0, f"Negative error magnitude {pk}: {field}")
            if field.endswith("_db"):
                require(value <= .002, f"Spectral tolerance exceeded {pk}: {field}={value}")
        parity_losses += integer(row, "dropped_samples")
        parity_discontinuities += integer(row, "discontinuities")
    expected_parity = {(r, n, t, m) for r in RATES for n in SIZES for t in CADENCES for m in range(3)}
    require(pkeys == expected_parity, "Parity 108-cell grid incomplete or unexpected")
    require(parity_losses == 0 and parity_discontinuities == 0, "Parity FIFO continuity failure")
    result["parity"] = dict(cases=len(pkeys), max_errors=pmax, dropped_samples=parity_losses,
                            discontinuities=parity_discontinuities, tolerance_db=.002)
    result["cost_accounting"] = dict(dropped_samples=raw_losses, entire_trials_with_zero_windows=raw_zero_trials,
                                     raw_thread_cpu_zero_rows=raw_cpu_zero,
                                     per_update_zero_window_observation="Not available in these cost CSVs; expected fixture counts only.")
    cells = result["cells"]
    result["aggregate"] = dict(cells=len(cells), lower_candidate_update_cells=sum(c["update_cost_reduction_percent"] > 0 for c in cells),
                               median_update_reduction_percent=median(c["update_cost_reduction_percent"] for c in cells),
                               median_cpu_reduction_percent=median(c["cpu_cost_reduction_percent"] for c in cells),
                               cpu_comparable_cells=sum(c["cpu_cost_reduction_percent"] is not None for c in cells))
    result["by_fft"] = {str(n): {"median_update_reduction_percent": median(c["update_cost_reduction_percent"] for c in cells if c["fft_size"] == n),
                                    "median_cpu_reduction_percent": median(c["cpu_cost_reduction_percent"] for c in cells if c["fft_size"] == n)}
                        for n in SIZES}
    result["192khz_cells"] = [c for c in cells if c["rate"] == 192000]
    result["ctest_result"] = "NOT_PROVIDED"
    junit_files = [name for name in receipts.names if PurePosixPath(name).name == "eq-results.xml"]
    for name in junit_files:
        for test in ET.fromstring(receipts.data(name)).iter("testcase"):
            if test.attrib.get("name") == "ChimeraEQAnalyzerPerformanceTests":
                status = ("FAILED" if test.find("failure") is not None or test.find("error") is not None else
                          "SKIPPED" if test.find("skipped") is not None else "PASSED")
                result["ctest_result"] = status
                require(status == "PASSED", f"CTest receipt reports performance test {status}")
    zero_name = str(PurePosixPath(parent) / "eq-analyzer-cpu.csv") if parent else "eq-analyzer-cpu.csv"
    if zero_name in receipts.names:
        result["separate_observed_zero_fft_updates"] = [{field: row.get(field) for field in
            ("rate", "fft_size", "ui_hz", "measured_update_calls", "zero_fft_update_calls", "fft_windows", "fifo_dropped_samples")}
            for row in receipts.rows(zero_name)]
    if platform.lower().startswith("win") or raw_cpu_zero:
        result["notes"].append("Short Windows GetThreadTimes observations may be coarse/quantized. A zero baseline or candidate CPU median yields no CPU percentage; inspect raw CPU trials and wall timings.")
    if platform.lower().startswith("win"):
        cpu_values = [number(row, "feed_and_analysis_thread_cpu_ms") for row in rows["paired"]]
        ticks = [round(value * 10000) for value in cpu_values if value > 0]
        step_ms = math.gcd(*ticks) / 10000.0 if ticks else None
        distribution = {str(value): cpu_values.count(value) for value in sorted(set(cpu_values))}
        result["cpu_timer_assessment"] = {
            "timer": "GetThreadTimes(GetCurrentThread), user + kernel",
            "status": "QUANTIZATION_LIMITED",
            "observed_nonzero_gcd_step_ms": step_ms,
            "step_interpretation": "Spacing observed in this receipt, not an API-wide guaranteed resolution.",
            "raw_cpu_value_counts_ms": distribution,
            "zero_cpu_rows": [{field: row[field] for field in
                ("rate", "fft_size", "fixture", "ui_interval_ms", "sequence", "implementation",
                 "analysis_update_wall_ms", "feed_and_analysis_wall_ms", "feed_and_analysis_thread_cpu_ms")}
                for row in rows["paired"] if number(row, "feed_and_analysis_thread_cpu_ms") == 0],
            "cells_with_lower_observed_cpu_median": sum(c["cpu_cost_reduction_percent"] is not None and c["cpu_cost_reduction_percent"] > 0 for c in cells),
            "cells_with_equal_observed_cpu_median": sum(c["cpu_cost_reduction_percent"] == 0 for c in cells),
            "cells_with_higher_observed_cpu_median": sum(c["cpu_cost_reduction_percent"] is not None and c["cpu_cost_reduction_percent"] < 0 for c in cells),
            "interpretation": "Retain CPU values and arithmetic ratios, but do not claim precise CPU savings, zero CPU cost, or equal cost from quantized ties. Four-trial medians can have half-step values merely from averaging two observations."
        }
        for c in cells:
            if c["cpu_cost_reduction_percent"] is not None:
                c["cpu_reduction_status"] = "QUANTIZATION_LIMITED_NONZERO_MEDIANS"
        result["aggregate"]["cpu_comparable_cells_definition"] = "Arithmetic nonzero-median ratios only; precision is limited by observed GetThreadTimes quantization."
        result["aggregate"]["cpu_savings_acceptance"] = "NOT_ESTABLISHED_AT_OBSERVED_RESOLUTION"
        result["notes"].append("GetThreadTimes CPU observations are QUANTIZATION_LIMITED. A numerical 0% median reduction or a 0 ms individual observation is not evidence of equal cost or free execution.")
    result["notes"].extend([
        "CPU milliseconds include FIFO feeding and analysis. FFT-only wall measurements have different magnitude-pass scope across variants; use complete-update wall cost.",
        "Timing reductions are observations, with no performance threshold or statistical significance gate. Shared-runner variation remains visible in raw trials.",
        "FIFO pending/discarded values are asserted by the C++ test but absent from cost CSV columns; do not invent separate observed values.",
        "Zero-FFT consumer updates are legitimate when a batch is shorter than one hop. Consumer totals include them; no zero-update timing row is discarded.",
        "CSV consistency PASS is separate from the CTest process result and from untested DAW/audio-interface real-time acceptance."])
    result["csv_gate"] = "PASS" if not issues else "FAIL"
    return result


def fmt(value, digits=3):
    return "N/A" if value is None else f"{value:.{digits}f}"


def markdown(report):
    text = [f"# {report['platform']} EQ paired-artifact analysis", "",
            f"Artifact: `{report['artifact']}`", "", "Native/DAW performance acceptance: **NOT_ASSESSED**.", ""]
    for data in report["datasets"]:
        text += [f"## Dataset `{data['dataset']}`", "",
                 f"CSV checks: **{data['csv_gate']}**. CTest receipt: **{data.get('ctest_result', 'NOT_PROVIDED')}**.", ""]
        if data.get("source_commit"):
            text += [f"Source: `{data['source_commit']}`.", ""]
        if "aggregate" not in data:
            text += ["Issues: " + "; ".join(data["issues"]), ""]
            continue
        agg = data["aggregate"]
        quant = data.get("cpu_timer_assessment")
        cpu_summary = ("CPU reduction: **QUANTIZATION_LIMITED**; raw observations and arithmetic ratios are preserved, but no precise CPU-saving percentage is established."
                       if quant else f"Median CPU-cost reduction: **{fmt(agg['median_cpu_reduction_percent'])}%**, from {agg['cpu_comparable_cells']} comparable cells.")
        text += [f"Rows: {data['row_counts']}. Lower candidate update medians: **{agg['lower_candidate_update_cells']} / {agg['cells']}**.", "",
                 f"Median update reduction: **{fmt(agg['median_update_reduction_percent'])}%**. {cpu_summary}", ""]
        if quant:
            text += ["### Actual thread CPU counter limitation", "",
                     f"Observed raw CPU step: **{fmt(quant['observed_nonzero_gcd_step_ms'])} ms**; zero-valued raw observations: **{len(quant['zero_cpu_rows'])}**. CPU medians decrease / tie / increase in {quant['cells_with_lower_observed_cpu_median']} / {quant['cells_with_equal_observed_cpu_median']} / {quant['cells_with_higher_observed_cpu_median']} cells. Non-lower measured CPU medians include both ties and increases; these are quantization-limited counter observations, not precision CPU conclusions.", "",
                     "Four-value medians may have half-step values because the middle two counter observations are averaged. That does not improve the timer's observed resolution.", "",
                     "| Raw CPU observation, ms | Count |", "|---:|---:|"]
            text += [f"| {value} | {count} |" for value, count in quant["raw_cpu_value_counts_ms"].items()]
            text += [""]
            for row in quant["zero_cpu_rows"]:
                text += [f"Zero CPU example: {row['rate']} Hz, FFT {row['fft_size']}, {row['fixture']}, sequence {row['sequence']}, {row['implementation']}: update wall **{row['analysis_update_wall_ms']} ms**, feed + analysis wall **{row['feed_and_analysis_wall_ms']} ms**, observed CPU **0 ms**. This is not zero-cost execution.", ""]
        text += [
                 "### 192 kHz: 2-second cadence trials and one full-capacity catch-up", "",
                 "| FFT | Fixture / cadence | FFT windows | Update baseline → candidate, ms | Wall reduction | CPU baseline → candidate, ms | CPU reduction |",
                 "|---:|---|---:|---:|---:|---:|---:|"]
        for cell in sorted(data["192khz_cells"], key=lambda c: (c["fft_size"], c["ui_interval_ms"] or 999)):
            label = "full-capacity catch-up" if cell["ui_interval_ms"] is None else f"{cell['ui_interval_ms']} ms cadence"
            cpu_reduction = "Resolution limited" if quant else f"{fmt(cell['cpu_cost_reduction_percent'])}%"
            text.append(f"| {cell['fft_size']} | {label} | {cell['fft_windows_per_trial']} | {fmt(cell['baseline_update_median_ms'])} → {fmt(cell['candidate_update_median_ms'])} | {fmt(cell['update_cost_reduction_percent'])}% | {fmt(cell['baseline_cpu_median_ms'])} → {fmt(cell['candidate_cpu_median_ms'])} | {cpu_reduction} |")
        parity = data["parity"]
        text += ["", "### Preservation and accounting", "",
                 f"Parity: {parity['cases']} cases; maximum raw/envelope/display error = " +
                 " / ".join(f"{parity['max_errors'][k]:.12g}" for k in ("max_raw_error_db", "max_envelope_error_db", "max_display_error_db")) + " dB.", "",
                 "The 108 CSV rows are the four-rate, three-FFT-size, three-cadence, three-smoothing A/B spectral parity grid. The separate C++ checkFiniteRecovery() check covers nonfinite/overflow containment and reset-then-fresh-spectrum recovery at 192 kHz with three FFT sizes; it is not included in these 108 rows.", "",
                 f"Cost FIFO drops: {data['cost_accounting']['dropped_samples']}; parity FIFO drops/discontinuities: {parity['dropped_samples']} / {parity['discontinuities']}; entire cost trials with zero windows: {data['cost_accounting']['entire_trials_with_zero_windows']}.", ""]
        if data["issues"]:
            text += ["### Issues", ""] + [f"- {issue}" for issue in data["issues"]] + [""]
        text += ["### Interpretation", ""] + [f"- {note}" for note in data["notes"]] + [""]
    return "\n".join(text)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("artifact", type=Path)
    parser.add_argument("--platform", default="unspecified")
    parser.add_argument("--expected-source")
    parser.add_argument("--dataset-match", help="Optional substring to select one dataset inside a combined archive")
    parser.add_argument("--output-prefix", type=Path, help="Write PREFIX.json and PREFIX.md in scratch")
    args = parser.parse_args()
    receipts = Receipts(args.artifact)
    parents = sorted({str(PurePosixPath(name).parent) if str(PurePosixPath(name).parent) != "." else ""
                      for name in receipts.names if PurePosixPath(name).name in FILES.values()})
    if args.dataset_match:
        parents = [p for p in parents if args.dataset_match in p]
    if not parents:
        raise SystemExit("No EQ performance CSV dataset found in the supplied artifact")
    results = []
    for parent in parents:
        try:
            results.append(evaluate(receipts, parent, args.platform, args.expected_source))
        except (KeyError, ValueError, TypeError, ZeroDivisionError, ET.ParseError) as error:
            results.append(dict(platform=args.platform, dataset=parent or ".", csv_gate="MALFORMED", issues=[str(error)],
                                performance_acceptance="NOT_ASSESSED"))
    report = dict(platform=args.platform, artifact=str(args.artifact.resolve()), datasets=results)
    rendered = markdown(report)
    if args.output_prefix:
        args.output_prefix.parent.mkdir(parents=True, exist_ok=True)
        Path(str(args.output_prefix) + ".json").write_text(json.dumps(report, indent=2, allow_nan=False) + "\n", encoding="utf-8")
        Path(str(args.output_prefix) + ".md").write_text(rendered, encoding="utf-8")
    print(rendered)
    return 0 if all(item["csv_gate"] == "PASS" for item in results) else 1


if __name__ == "__main__":
    sys.exit(main())
