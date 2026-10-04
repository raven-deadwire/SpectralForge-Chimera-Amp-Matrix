#!/usr/bin/env python3
"""Diagnose callback timing from interleaved baseline/candidate UI probes.

This deliberately never grants a release PASS.  It separates machine-wide
drift, represented by the closed-editor audio control, from UI-scene deltas so
that a controlled follow-up run can distinguish a likely editor regression
from shared-runner noise.
"""
import argparse
import json
import math
from pathlib import Path
from statistics import median


CONDITION_KEYS = ("os", "cpu", "juce", "sample_rate", "block_size", "fixture", "scope")
ROW_KEYS = ("pair", "scene", "scale", "instances")


def _audio_rows(report):
    rows = {}
    for row in report["rows"]:
        if "callback_p99_us" not in row:
            continue
        key = tuple(row[name] for name in ROW_KEYS)
        if key in rows:
            raise ValueError(f"Duplicate callback row: {key}")
        if row.get("callback_count", 0) <= 0:
            raise ValueError(f"Missing callback samples: {key}")
        rows[key] = row
    return rows


def _percentile(values, fraction):
    ordered = sorted(values)
    if not ordered:
        raise ValueError("Cannot take a percentile of an empty sequence")
    return ordered[math.ceil(fraction * len(ordered)) - 1]


def analyze(baseline, candidate, minimum_pairs=5):
    for key in CONDITION_KEYS:
        if baseline.get(key) != candidate.get(key):
            raise ValueError(f"Mismatched benchmark condition: {key}")

    before = _audio_rows(baseline)
    after = _audio_rows(candidate)
    if before.keys() != after.keys():
        raise ValueError("Baseline/candidate callback row coverage differs")
    if not before:
        raise ValueError("No callback rows found")

    hashes = {
        row.get("audio_output_fnv64")
        for row in list(before.values()) + list(after.values())
    }
    if None in hashes or len(hashes) != 1:
        raise ValueError("Synthetic audio output is not identical")

    grouped = {}
    for key, base in before.items():
        pair, scene, scale, instances = key
        cand = after[key]
        if base["callback_count"] != cand["callback_count"]:
            raise ValueError(f"Callback count differs: {key}")
        grouped.setdefault((scene, scale, instances), {})[pair] = {
            "raw_p99_ratio": cand["callback_p99_us"] / base["callback_p99_us"],
            "mean_ratio": cand["callback_mean_us"] / base["callback_mean_us"],
            "miss_rate_delta_per_1000": 1000.0 * (
                cand["callback_deadline_misses"] - base["callback_deadline_misses"]
            ) / base["callback_count"],
        }

    control_key = next((key for key in grouped if key[0] == "closed_audio"), None)
    if control_key is None:
        raise ValueError("Missing closed_audio control")
    control = grouped[control_key]
    if len(control) < minimum_pairs:
        raise ValueError(f"Need at least {minimum_pairs} interleaved pairs")

    scenes = []
    diagnosis = "LIKELY_NONREGRESSION"
    for key, pairs in sorted(grouped.items()):
        if pairs.keys() != control.keys():
            raise ValueError(f"Pair coverage differs from closed_audio: {key}")
        normalized = []
        adjusted_misses = []
        for pair, values in sorted(pairs.items()):
            control_values = control[pair]
            values["control_p99_ratio"] = control_values["raw_p99_ratio"]
            values["normalized_p99_ratio"] = (
                values["raw_p99_ratio"] / control_values["raw_p99_ratio"]
            )
            values["control_adjusted_miss_rate_delta_per_1000"] = (
                values["miss_rate_delta_per_1000"]
                - control_values["miss_rate_delta_per_1000"]
            )
            normalized.append(values["normalized_p99_ratio"])
            adjusted_misses.append(values["control_adjusted_miss_rate_delta_per_1000"])

        worse = sum(value > 1.10 for value in normalized)
        miss_worse = sum(value > 0.0 for value in adjusted_misses)
        row = {
            "scene": key[0],
            "scale": key[1],
            "instances": key[2],
            "pairs": len(pairs),
            "median_raw_p99_ratio": median(value["raw_p99_ratio"] for value in pairs.values()),
            "median_control_normalized_p99_ratio": median(normalized),
            "p80_control_normalized_p99_ratio": _percentile(normalized, 0.80),
            "candidate_over_10_percent_worse_pairs": worse,
            "candidate_control_adjusted_miss_worse_pairs": miss_worse,
            "median_control_adjusted_miss_rate_delta_per_1000": median(adjusted_misses),
        }
        scenes.append(row)
        if key != control_key:
            if worse >= math.ceil(0.8 * len(pairs)) and row["median_control_normalized_p99_ratio"] > 1.10:
                diagnosis = "LIKELY_REGRESSION"
            elif (
                miss_worse >= math.ceil(0.8 * len(pairs))
                and row["median_control_adjusted_miss_rate_delta_per_1000"] > 0.5
            ):
                diagnosis = "LIKELY_REGRESSION"
            elif row["p80_control_normalized_p99_ratio"] > 1.10 and diagnosis != "LIKELY_REGRESSION":
                diagnosis = "INCONCLUSIVE"

    return {
        "schema": "spectralforge.chimera.callback-paired-diagnosis",
        "schema_version": 1,
        "baseline_revision": baseline["source_revision"],
        "candidate_revision": candidate["source_revision"],
        "pair_count": len(control),
        "synthetic_audio_identity": "PASS",
        "probe_diagnosis": diagnosis,
        "release_gate_status": "BLOCKED",
        "release_gate_reason": (
            "A paired synthetic probe can diagnose likely timing direction but cannot certify "
            "commercial-DAW callback timing, driver scheduling, or a controlled workstation soak."
        ),
        "control_policy": (
            "Each scene's candidate/baseline p99 ratio and miss-rate delta are normalized by "
            "the same pair's closed_audio result."
        ),
        "scenes": scenes,
    }


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("baseline", type=Path)
    parser.add_argument("candidate", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--minimum-pairs", type=int, default=5)
    args = parser.parse_args()
    result = analyze(
        json.loads(args.baseline.read_text(encoding="utf-8")),
        json.loads(args.candidate.read_text(encoding="utf-8")),
        args.minimum_pairs,
    )
    args.output.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
