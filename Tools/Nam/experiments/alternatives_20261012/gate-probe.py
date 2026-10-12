#!/usr/bin/env python3
"""Minimal official TONE3000 A2 import-contract probes against the pinned tool.

No training, acoustic qualification, or publication approval is performed.
The deliberately invalid condition_dsp probe is an ignored-field diagnostic,
never a compatible recurrent-model candidate.
"""

import argparse
import copy
import hashlib
import json
import subprocess
import tempfile
from pathlib import Path

import numpy as np


PINS = {
    "plugin": "ef6f178ae1ac6412b55fec6f058d86e640e5aaaf",
    "core": "1f42f88535884450104b8711d7595019afa0495b",
    "juce": "d6181bde38d858c283c3b7bf699ce6340c050b5d",
}


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--tool", type=Path, required=True)
    parser.add_argument("--model", type=Path, required=True)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--build-record", type=Path, required=True)
    args = parser.parse_args()
    build = json.loads(args.build_record.read_text())
    expected_binary = build["binaries"]["ChimeraTone3000Tool"]["sha256"]
    if digest(args.tool) != expected_binary:
        raise RuntimeError("Tool binary differs from the recorded pinned build")
    if build["dependencies"] != {
        "player": PINS["plugin"], "core": PINS["core"], "juce": PINS["juce"]
    }:
        raise RuntimeError("Unexpected dependency pins")
    model = json.loads(args.model.read_text())
    if model.get("architecture") != "WaveNet":
        raise RuntimeError("Positive control must be a bare A2 WaveNet")

    n = 12000
    t = np.arange(n, dtype=np.float64) / 48000.0
    rng = np.random.default_rng(26101203)
    x = (0.17 * np.sin(2 * np.pi * 110 * t)
         + 0.06 * np.sin(2 * np.pi * 701 * t)
         + rng.normal(0, 0.007, n)).astype("<f4")
    variants = [("unchanged_a2", model, "accepted")]

    dilation = copy.deepcopy(model)
    dilation["config"]["layers"][0]["dilations"][-1] += 1
    variants.append(("changed_dilation", dilation, "rejected_at_a2_gate"))

    head = copy.deepcopy(model)
    if head["config"]["layers"][0]["head"]["kernel_size"] != 16:
        raise RuntimeError("Unexpected original head size")
    head["config"]["layers"][0]["head"]["kernel_size"] = 32
    variants.append(("changed_head_kernel", head, "rejected_at_a2_gate"))

    lstm = copy.deepcopy(model)
    lstm["architecture"] = "LSTM"
    variants.append(("lstm_architecture_marker", lstm, "rejected_at_a2_gate"))

    invalid_condition = copy.deepcopy(model)
    invalid_condition["config"]["condition_dsp"] = {
        "version": "0.5.4",
        "architecture": "INTENTIONALLY_UNREGISTERED_DIAGNOSTIC",
        "config": {},
        "weights": [],
        "sample_rate": 48000,
    }
    variants.append(("invalid_condition_dsp_field", invalid_condition,
                     "accepted_but_not_executed"))

    results = []
    baseline_output = None
    with tempfile.TemporaryDirectory(prefix="a2-gate-probe-") as td:
        temp = Path(td)
        inp = temp / "input.f32"
        x.tofile(inp)
        job = temp / "job.json"
        job.write_text(json.dumps({"block_size": 64, "normalize": False}))
        for name, variant, expected in variants:
            variant_path = temp / (name + ".nam")
            variant_path.write_text(json.dumps(variant, separators=(",", ":")))
            out = temp / (name + ".f32")
            proc = subprocess.run(
                [str(args.tool.resolve()), "render", str(variant_path), str(inp),
                 str(out), str(job)], capture_output=True, text=True, timeout=60,
            )
            result = {
                "probe": name, "expected": expected,
                "model_sha256": digest(variant_path), "exit_code": proc.returncode,
                "stderr": proc.stderr.strip(), "output_created": out.exists(),
            }
            if proc.returncode == 0:
                meta = json.loads(proc.stdout)
                y = np.fromfile(out, dtype="<f4")
                if y.size != n or not np.isfinite(y).all():
                    raise RuntimeError("Invalid control render")
                result["a2_shape_gate"] = meta["a2_gate"]
                result["a2_channels"] = meta["a2_channels"]
                result["output_sha256"] = digest(out)
                result["output_peak"] = float(np.max(np.abs(y)))
                if name == "unchanged_a2":
                    baseline_output = y
                    result["observed"] = "accepted"
                    result["expectation_met"] = result["output_peak"] > 0
                elif name == "invalid_condition_dsp_field":
                    if baseline_output is None:
                        raise RuntimeError("Missing positive control")
                    delta = y.astype(np.float64) - baseline_output
                    result["bit_exact_to_unchanged"] = bool(np.array_equal(y, baseline_output))
                    result["max_abs_delta_to_unchanged"] = float(np.max(np.abs(delta)))
                    result["observed"] = "accepted_but_not_executed"
                    result["expectation_met"] = result["bit_exact_to_unchanged"]
                    result["interpretation"] = (
                        "The unsupported conditioner is not constructed or executed by the A2 fast path; "
                        "a successful load must not be reported as recurrent-conditioner support."
                    )
                else:
                    result["observed"] = "unexpected_acceptance"
                    result["expectation_met"] = False
            else:
                at_gate = "TONE3000 rejects non-A2 model" in proc.stderr and not out.exists()
                result["observed"] = "rejected_at_a2_gate" if at_gate else "different_error"
                result["expectation_met"] = expected == "rejected_at_a2_gate" and at_gate
            results.append(result)

    report = {
        "schema": 1,
        "experiment": "A2 target import-contract and ignored-field probes",
        "dependency_pins": PINS,
        "binary_sha256": digest(args.tool),
        "original_control_model_sha256": digest(args.model),
        "original_control_model": args.model.name,
        "samples": n, "sample_rate": 48000, "block_size": 64,
        "input_sha256": hashlib.sha256(x.tobytes()).hexdigest(),
        "input_seed": 26101203,
        "all_expectations_met": all(r["expectation_met"] for r in results),
        "probes": results,
        "release_approved": False,
        "scope": (
            "Pinned unmodified A2 shape detector and NamEngine called by the existing offline harness. "
            "No plugin GUI/DAW validation, browser deployment validation, or acoustic acceptance is implied."
        ),
    }
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n")
    print(json.dumps({"all_expectations_met": report["all_expectations_met"],
                      "probes": [{"name": r["probe"], "observed": r["observed"],
                                  "pass": r["expectation_met"]} for r in results]}))
    if not report["all_expectations_met"]:
        raise SystemExit(1)


if __name__ == "__main__":
    main()
