#!/usr/bin/env python3
"""Diagnose whether a scalar output gain could solve the archived A2 failures.

This is post-selection algebra on an immutable report, not a model calibration.
It never loads audio, trains a model, changes scores, or approves a release.
The oracle is deliberately allowed to know the evaluation statistics: its only
purpose is to prove a limit on that fixed prediction. Its gain must not be used
as a fitted candidate or described as independent-test performance.
"""

import argparse
import hashlib
import json
import math
from pathlib import Path


SOURCE_COMMIT = "859fea9bc49c4b9981c926a314655e485c51027c"
REPORT_SHA256 = "89c19c30d87159d1c5091f76d8c8ee7b90caab534513073e23e6128710c37f93"
NAMES = ("Fenrir", "Surtr", "Nidhoggr", "Fimbulvetr", "Ragnarok")
FROZEN_POLICY = {
    "id": "nastrond-tone3000-fidelity-v1",
    "authority": "project acceptance policy; not a TONE3000 certification",
    "window_samples": 4096,
    "skip_samples": 6347,
    "active_reference_rms_min_dbfs": -60.0,
    "full_esr_max_exclusive": 0.01,
    "esr_guidance_source": "https://www.tone3000.com/guides/capture-your-gear-with-nam-dry-wet",
    "active_window_esr_median_max": 0.005,
    "active_window_esr_p95_max": 0.01,
    "active_window_esr_worst_max": 0.02,
    "quiet_residual_rms_max_dbfs": -70.0,
    "quiet_residual_peak_max_dbfs": -60.0,
    "rms_error_max_db": 0.5,
    "peak_error_max_db": 1.0,
    "steady_silence_rms_max_dbfs": -80.0,
    "engine_parity_residual_max_dbfs": -80.0,
    "level_anchor": "delivered v0.2 reference_output_scale, frozen per channel",
    "source_output_scale": {
        "Fenrir": 0.09195752759643712,
        "Surtr": 0.09034985741800312,
        "Nidhoggr": 0.0860012970451488,
        "Fimbulvetr": 0.09721294970620417,
        "Ragnarok": 0.05162876564505051,
    },
}


def finite_number(value, name):
    if type(value) not in (int, float) or not math.isfinite(value):
        raise ValueError(f"Expected finite numeric {name}")
    return float(value)


def scalar_diagnostic(metrics, rms_limit=0.5, peak_limit=1.0):
    """Exact scalar-only bound from same-window ESR and RMS/peak ratios.

    For q = E[p^2]/E[r^2], c = E[p*r]/E[r^2], and E = ESR(p,r):
        c = (q + 1 - E)/2
        ESR(a*p,r) = q*a^2 - 2*c*a + 1
        minimum over all real a = 1 - c^2/q.
    This grants more freedom than a physically positive output gain. Failure
    even under this oracle therefore rules out every positive scalar gain.
    No DC intercept, EQ, alignment change, or output dynamics are included.
    """
    esr = finite_number(metrics["full_esr"], "full_esr")
    rms = finite_number(metrics["rms_error_db"], "rms_error_db")
    peak = finite_number(metrics["peak_error_db"], "peak_error_db")
    rms_limit = finite_number(rms_limit, "rms_limit")
    peak_limit = finite_number(peak_limit, "peak_limit")
    if esr < 0 or min(rms_limit, peak_limit) < 0:
        raise ValueError("ESR and level limits must be nonnegative")
    try:
        q = 10.0 ** (rms / 10.0)
    except OverflowError as exc:
        raise ValueError("Invalid power ratio") from exc
    if not math.isfinite(q) or q <= 0:
        raise ValueError("Expected finite nonzero prediction power ratio")
    c = (q + 1.0 - esr) / 2.0
    explained = c * c / q
    if not math.isfinite(explained) or explained > 1.0 + 1e-10:
        raise ValueError("ESR and RMS statistics violate Cauchy-Schwarz")
    # Clamp roundoff at a mathematical zero, never an acceptance threshold.
    minimum_esr = max(0.0, 1.0 - explained)
    oracle_gain = c / q
    rms_interval = [-rms_limit - rms, rms_limit - rms]
    peak_interval = [-peak_limit - peak, peak_limit - peak]
    lower = max(rms_interval[0], peak_interval[0])
    upper = min(rms_interval[1], peak_interval[1])
    return {
        "archived_full_esr": esr,
        "archived_rms_error_db": rms,
        "archived_peak_error_db": peak,
        "prediction_to_reference_power_ratio": q,
        "normalized_cross_moment": c,
        "oracle_scalar_linear": oracle_gain,
        "oracle_scalar_db_if_positive": (
            20.0 * math.log10(oracle_gain) if oracle_gain > 0 else None
        ),
        "oracle_minimum_full_esr": minimum_esr,
        "rms_allowed_gain_db": rms_interval,
        "peak_allowed_gain_db": peak_interval,
        "simultaneous_level_feasible": lower <= upper,
        "simultaneous_allowed_gain_db": [lower, upper] if lower <= upper else None,
        "level_interval_signed_overlap_db": upper - lower,
        "crest_factor_error_db": peak - rms,
    }


def audit_report(report):
    if report["quality_profile"] != FROZEN_POLICY:
        raise ValueError("Fidelity policy differs from the pinned report")
    if set(report["channels"]) != set(NAMES):
        raise ValueError("Expected exactly five frozen channels")
    if report["selection"]["selected_recipe"] != "legacy":
        raise ValueError("Expected the archived validation-selected legacy arm")
    if report["release_approved"] is not False or report["accuracy"] != "FAIL":
        raise ValueError("Unexpected source acceptance state")
    output = {
        "source_commit": SOURCE_COMMIT,
        "source_report_sha256": REPORT_SHA256,
        "scope": "Post-selection diagnostic; no candidate fitted or selected",
        "oracle_use": "Impossibility bound only; never an evaluation score for a fitted candidate",
        "previous_test_status": "Already exposed; regression evidence only for future work",
        "quality_profile": report["quality_profile"],
        "model_artifacts_modified": False,
        "release_approved": False,
        "accuracy": "FAIL",
        "channels": {},
    }
    for name in NAMES:
        row = report["channels"][name]
        result = {}
        for split, metrics in (
            ("validation_selected_legacy", row["refinement_validation"]["legacy"]),
            ("previously_exposed_test", row["fresh_test_candidate"]),
        ):
            item = scalar_diagnostic(
                metrics,
                FROZEN_POLICY["rms_error_max_db"],
                FROZEN_POLICY["peak_error_max_db"],
            )
            item["scalar_full_esr_pass_impossible"] = (
                item["oracle_minimum_full_esr"] >= FROZEN_POLICY["full_esr_max_exclusive"]
            )
            result[split] = item
        output["channels"][name] = result
    return output


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--report", type=Path,
        default=Path(__file__).resolve().parents[2] / "reports/refinement-20261011.json",
    )
    parser.add_argument("--out", type=Path, required=True)
    args = parser.parse_args()
    raw = args.report.read_bytes()
    if hashlib.sha256(raw).hexdigest() != REPORT_SHA256:
        raise RuntimeError("Source report hash mismatch; diagnostic requires the pinned evidence")
    if args.out.resolve() == args.report.resolve():
        raise RuntimeError("Cannot overwrite source evidence")
    output = audit_report(json.loads(raw))
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(json.dumps(output, indent=2, allow_nan=False) + "\n")
    print(json.dumps({
        "channels_with_scalar_esr_obstruction": [
            name for name, row in output["channels"].items()
            if row["previously_exposed_test"]["scalar_full_esr_pass_impossible"]
        ],
        "channels_with_scalar_level_obstruction": [
            name for name, row in output["channels"].items()
            if not row["previously_exposed_test"]["simultaneous_level_feasible"]
        ],
        "model_artifacts_modified": False,
        "release_approved": False,
    }))


if __name__ == "__main__":
    main()
