#!/usr/bin/env python3
"""Reproducible head-only alias-candidate spectra and descriptive CPU evidence.

Requires NumPy. Never changes product approval or evaluates an absolute CPU gate.
See docs/NIFLHEIMR_MEASUREMENTS.md for units, limitations and comparison rules.
"""
from __future__ import annotations

import argparse
from contextlib import contextmanager
from datetime import datetime, timezone
import hashlib
import itertools
import json
import math
import os
from pathlib import Path
import platform
import random
import shutil
import struct
import subprocess
import sys
import tempfile
import time

import numpy as np

SCHEMA = "spectralforge.niflheimr.measurements.v1"
PROTOCOL = "niflheimr-coherent-foldback-process-cpu.v1"
ROOT = Path(__file__).resolve().parents[1]
MANUAL = {key: "PENDING_MANUAL_EVIDENCE" for key in (
    "instrument_di_same_ir_listening", "musical_acceptance", "commercial_daw_lifecycle",
    "target_hardware_cpu_acceptance", "windows_macos_product_acceptance")}


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def save_json(path, data):
    Path(path).write_text(json.dumps(data, ensure_ascii=False, indent=2, allow_nan=False) + "\n", encoding="utf-8")


@contextmanager
def measurement_stage(destination):
    stage = Path(tempfile.mkdtemp(prefix=destination.name + ".partial-", dir=destination.parent))
    try:
        yield stage
    except Exception as error:
        failed = stage.with_name(stage.name.replace(".partial-", ".failed-"))
        save_json(stage / "failure.json", {"status": "FAILED_NOT_ACCEPTANCE_EVIDENCE",
                  "release_approved": False, "error": str(error)})
        stage.rename(failed)
        raise RuntimeError(f"{error}; diagnostic files preserved at {failed}") from error
    finally:
        if stage.exists():
            shutil.rmtree(stage)


def header_fingerprint(directory):
    # WindowsPath ordering folds case; CMake's list is case-sensitive. Specify
    # the portable ordering explicitly, independently of host path semantics.
    records = "".join(f"{p.name}:{digest(p)}\n" for p in sorted(directory.glob("*.h"), key=lambda p: p.name))
    return hashlib.sha256(records.encode()).hexdigest()


def command(*args):
    result = subprocess.run(args, cwd=ROOT, capture_output=True, text=True, encoding="utf-8", errors="replace")
    return result.stdout.strip() if result.returncode == 0 else None


def read_optional(path):
    try:
        return Path(path).read_text().strip()
    except OSError:
        return None


def environment(label):
    cpu = platform.processor()
    if sys.platform.startswith("linux"):
        cpu = next((line.split(":", 1)[1].strip() for line in (read_optional("/proc/cpuinfo") or "").splitlines()
                    if line.startswith("model name")), cpu)
    elif sys.platform == "darwin":
        cpu = command("sysctl", "-n", "machdep.cpu.brand_string") or cpu
    elif os.name == "nt":
        cpu = os.environ.get("PROCESSOR_IDENTIFIER", cpu)
    return {
        "machine_label": label, "cpu_model": cpu or "unknown", "machine": platform.machine(),
        "system": platform.system(), "os_release": platform.release(), "os_version": platform.version(),
        "logical_cpus": os.cpu_count(),
        "affinity": sorted(os.sched_getaffinity(0)) if hasattr(os, "sched_getaffinity") else None,
        "power_governor": read_optional("/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor"),
        "cgroup_cpu_max": read_optional("/sys/fs/cgroup/cpu.max"),
        "load_average_start": list(os.getloadavg()) if hasattr(os, "getloadavg") else None,
        "python": platform.python_version(), "numpy": np.__version__,
        "ci": {key: os.environ.get(key) for key in (
            "CI", "GITHUB_ACTIONS", "GITHUB_RUN_ID", "GITHUB_RUN_ATTEMPT", "GITHUB_JOB", "GITHUB_SHA",
            "RUNNER_OS", "RUNNER_ARCH", "ImageOS", "ImageVersion")},
        "scheduling": "ordinary offline process; no realtime priority or CPU isolation requested",
    }


def float_wav(path, rate, samples):
    audio = np.asarray(samples, dtype="<f4")
    if audio.ndim == 1:
        audio = audio[:, None]
    frames, channels = audio.shape
    raw = audio.tobytes()
    fmt = struct.pack("<HHIIHHH", 3, channels, rate, rate * channels * 4, channels * 4, 32, 0)
    chunks = b"fmt " + struct.pack("<I", len(fmt)) + fmt
    chunks += b"fact" + struct.pack("<II", 4, frames) + b"data" + struct.pack("<I", len(raw)) + raw
    Path(path).write_bytes(b"RIFF" + struct.pack("<I", len(chunks) + 4) + b"WAVE" + chunks)


def read_wav(path, *, raw=None):
    if raw is None:
        raw = Path(path).read_bytes()
    if raw[:4] != b"RIFF" or raw[8:12] != b"WAVE" or len(raw) != struct.unpack_from("<I", raw, 4)[0] + 8:
        raise ValueError("Invalid RIFF output")
    pos, audio, fmt = 12, None, None
    while pos < len(raw):
        chunk, length = struct.unpack_from("<4sI", raw, pos)
        payload = raw[pos + 8:pos + 8 + length]
        if chunk == b"fmt ":
            fmt = struct.unpack_from("<HHIIHH", payload)
        elif chunk == b"data":
            audio = np.frombuffer(payload, dtype="<f4").copy()
        pos += 8 + length + length % 2
    if fmt is None or fmt[0] != 3 or fmt[5] != 32 or audio is None or not np.isfinite(audio).all():
        raise ValueError("Expected finite float32 WAV")
    return fmt[2], audio.reshape(-1, fmt[1])


def read_output(path, row, expected_bytes, observations):
    """One read supplies both integrity verification and spectral analysis."""
    before = path.stat()
    raw = path.read_bytes()
    observation = {"phase": "parent_first_read", "file": path.name,
                   "bytes": before.st_size, "bytes_read": len(raw), "file_identifier": str(before.st_ino),
                   "post_read_bytes": path.stat().st_size,
                   "sha256": hashlib.sha256(raw).hexdigest(), "expected_bytes": expected_bytes}
    observations.append(observation)
    # Write before raising so the very first parent read survives a failed run.
    save_json(path.parent / "io-observations.json", observations)
    if (observation["sha256"] != row["sha256"] or
            any(observation[key] != expected_bytes for key in ("bytes", "bytes_read", "post_read_bytes"))):
        (path.parent / (path.name + ".first-read.bin")).write_bytes(raw)
        raise ValueError(f"Output integrity mismatch: {path}; expected={row['sha256']}; "
                         f"actual={observation['sha256']}; bytes={len(raw)}; expected_bytes={expected_bytes}")
    return raw


def spectrum(samples):
    """One-sided bin power (FS^2); sum = window-weighted mean square."""
    x = np.asarray(samples, dtype=np.float64)
    window = .5 - .5 * np.cos(2 * np.pi * np.arange(len(x)) / len(x))
    power = np.abs(np.fft.rfft(x * window)) ** 2 / (len(x) * np.sum(window ** 2))
    power[1:-1] *= 2
    return power


def masks(n, tone_bin, max_order=127, guard=2):
    bins = n // 2 + 1
    harmonic = np.zeros(bins, dtype=bool)
    folded = np.zeros(bins, dtype=bool)
    dc = np.arange(bins) <= guard
    def mark(mask, centre):
        mask[max(0, centre - guard):min(bins, centre + guard + 1)] = True
    for order in range(1, (n // 2 - 1) // tone_bin + 1):
        mark(harmonic, order * tone_bin)
    for order in range(2, max_order + 1):
        if order * tone_bin >= n // 2:
            centre = (order * tone_bin) % n
            mark(folded, min(centre, n - centre))
    folded &= ~(harmonic | dc)
    return harmonic, folded, ~(harmonic | dc)


def db(power):
    return 10 * math.log10(max(float(power), 1e-30))


def alias_metrics(power, n, tone_bin, max_order=127):
    harmonic, folded, residual = masks(n, tone_bin, max_order)
    total = float(power.sum())
    fundamental = float(power[tone_bin-2:tone_bin+3].sum())
    return {
        "output_total_dbfs": db(total), "fundamental_dbfs": db(fundamental),
        "foldback_candidate_dbfs": db(power[folded].sum()),
        "foldback_candidate_dbc": db(power[folded].sum()) - db(fundamental),
        "off_harmonic_residual_dbfs": db(power[residual].sum()),
        "off_harmonic_residual_dbc": db(power[residual].sum()) - db(fundamental),
        "candidate_bins": int(folded.sum()), "max_fold_order": max_order,
        "fundamental_identifiable": fundamental > 1e-20,
    }


def summarize_cpu(cpu):
    budget = cpu["block_budget_us"]
    summaries = []
    for repeat in cpu["repetitions"]:
        values = np.asarray(repeat["block_us"], dtype=float)
        if len(values) != cpu["measured_blocks_per_repeat"] or not np.isfinite(values).all() or (values < 0).any():
            raise ValueError("Invalid raw CPU timings")
        summaries.append({"repeat": repeat["repeat"], "median_us": float(np.median(values)),
                          "p95_us": float(np.percentile(values, 95)), "p99_us": float(np.percentile(values, 99)),
                          "max_us": float(values.max()), "mean_us": float(values.mean()),
                          "fraction_of_block_budget": float(values.mean() / budget),
                          "observed_over_budget_blocks": int((values > budget).sum())})
    medians = [row["median_us"] for row in summaries]
    return {"repeats": summaries, "median_of_repeat_medians_us": float(np.median(medians)),
            "repeat_median_min_us": min(medians), "repeat_median_max_us": max(medians),
            "verdict": "MEASURED_NO_HARD_CPU_GATE"}


def comparison(current, baseline):
    """Only compare matching CPU environment/build/protocol and identical stimuli."""
    result = {"baseline_source": baseline.get("source"), "release_approved": False,
              "status": "NOT_COMPARABLE", "reasons": [], "rows": []}
    for field in ("schema", "protocol", "configuration"):
        if current.get(field) != baseline.get(field):
            result["reasons"].append(field)
    stable_environment = ("machine_label", "cpu_model", "machine", "system", "os_release", "os_version",
                          "logical_cpus", "affinity", "power_governor", "cgroup_cpu_max", "scheduling")
    for key in stable_environment:
        if current["environment"].get(key) != baseline["environment"].get(key):
            result["reasons"].append("environment." + key)
    for key in ("RUNNER_OS", "RUNNER_ARCH", "ImageOS", "ImageVersion"):
        if current["environment"]["ci"].get(key) != baseline["environment"]["ci"].get(key):
            result["reasons"].append("ci." + key)
    for key in ("configuration", "compiler_id", "compiler_version", "cxx_flags", "release_flags", "debug_flags",
                "relwithdebinfo_flags", "minsize_flags", "juce_version", "processor", "cxx_standard",
                "osx_architectures", "msvc_runtime", "recommended_config_flags"):
        if current["build"].get(key) != baseline["build"].get(key):
            result["reasons"].append("build." + key)
    if current["environment"].get("cpu_model") == "unknown":
        result["reasons"].append("unknown CPU model")
    old = {row["id"]: row for row in baseline["cpu"]}
    if set(old) != {row["id"] for row in current["cpu"]}:
        result["reasons"].append("CPU route coverage")
    for row in current["cpu"]:
        prior = old.get(row["id"])
        if prior and (row["input_sha256"] != prior["input_sha256"] or row["controls"] != prior["controls"]):
            result["reasons"].append("stimulus/controls: " + row["id"])
    if result["reasons"]:
        return result
    for row in current["cpu"]:
        prior = old[row["id"]]
        now, before = row["summary"], prior["summary"]
        denominator = before["median_of_repeat_medians_us"]
        result["rows"].append({"id": row["id"], "baseline_median_us": denominator,
                               "current_median_us": now["median_of_repeat_medians_us"],
                               "ratio": now["median_of_repeat_medians_us"] / denominator if denominator > 0 else None,
                               "repeat_median_ranges_overlap": max(now["repeat_median_min_us"], before["repeat_median_min_us"])
                               <= min(now["repeat_median_max_us"], before["repeat_median_max_us"])})
    result["status"] = "DESCRIPTIVE_COMPARISON_ONLY"
    result["note"] = "Unpaired offline runs; shared-runner contention/turbo/thermal noise remain confounders. No automatic regression or acceptance verdict."
    return result


def compare_alias_factors(rows):
    groups = {}
    for row in rows:
        key = (row["channel_key"], row["rate"], row["gain"], row["block_size"], row["tone_bin"])
        groups.setdefault(key, {})[row["oversampling"]] = row
    result = []
    for factors in groups.values():
        reference = factors.get(1)
        for factor, row in sorted(factors.items()):
            if factor == 1:
                continue
            item = {"id": row["id"], "reference_id": reference["id"] if reference else None,
                    "status": "DESCRIPTIVE_COMPARISON_ONLY" if reference else "NO_1X_REFERENCE"}
            if reference:
                for metric in ("foldback_candidate_dbc", "off_harmonic_residual_dbc"):
                    now = np.mean([window[0][metric] for window in row["windows"]])
                    before = np.mean([window[0][metric] for window in reference["windows"]])
                    item[metric + "_delta_vs_1x"] = float(now - before)
            result.append(item)
    return result


def profile(name):
    return ({"rates": [48000], "gains": [.75], "factors": [1, 4], "blocks": [128],
             "tones_hz": [6011], "fft_frames": 8192, "settle_seconds": 1.,
             "repeats": 2, "measured_blocks": 64, "warmup_blocks": 384} if name == "smoke" else
            {"rates": [44100, 48000, 96000], "gains": [.5, .75, 1.], "factors": [1, 2, 4, 8],
             "blocks": [64, 256], "tones_hz": [997, 3001, 6011], "fft_frames": 32768,
             "settle_seconds": 2., "repeats": 5, "measured_blocks": 512, "warmup_blocks": 1536})


def execute(args):
    renderer = args.renderer.resolve()
    if not renderer.is_file():
        raise ValueError("Renderer does not exist")
    destination = args.output.resolve()
    if destination.exists():
        raise ValueError("Output must be a new directory")
    config = profile(args.profile)
    for key in ("rates", "gains", "factors", "blocks"):
        if getattr(args, key) is not None:
            config[key] = getattr(args, key)
    if any(rate not in (44100, 48000, 88200, 96000, 176400, 192000) for rate in config["rates"]):
        raise ValueError("Unsupported measurement rate")
    if any(not math.isfinite(g) or not 0 <= g <= 1 for g in config["gains"]):
        raise ValueError("GAIN must be finite and between zero and one")
    if any(f not in (1, 2, 4, 8) for f in config["factors"]) or any(not 1 <= b <= 8192 for b in config["blocks"]):
        raise ValueError("Invalid factor or block size")
    if any(len(set(config[key])) != len(config[key]) for key in ("rates", "gains", "factors", "blocks")):
        raise ValueError("Duplicate routes are not allowed")
    config.update({"profile": args.profile, "blend": 1., "input_peak_dbfs": -12., "audio_channels": 2,
                   "alias_block_size": 256, "max_fold_order": 127, "guard_bins": 2, "order_seed": args.seed})
    source = {"head": command("git", "rev-parse", "HEAD"),
              "tree": command("git", "rev-parse", "HEAD^{tree}"),
              "tracked_worktree_status": command("git", "status", "--porcelain", "--untracked-files=no"),
              "harness_sha256": digest(__file__), "renderer_binary_sha256": digest(renderer)}
    evidence = {"schema": SCHEMA, "protocol": PROTOCOL, "configuration": config, "source": source,
                "created_utc": datetime.now(timezone.utc).isoformat(), "environment": environment(args.machine_label),
                "release_approved": False, "manual_acceptance": MANUAL,
                "scope": "synthetic head-only production wrapper; no cab/IR, normalization, real DI or DAW",
                "alias": [], "cpu": [], "execution_order": [], "files": {}}
    started = time.monotonic()
    destination.parent.mkdir(parents=True, exist_ok=True)
    with measurement_stage(destination) as stage:
        (stage / "inputs").mkdir()
        (stage / "spectra").mkdir()
        (stage / "runs").mkdir()
        inputs = {}
        n = config["fft_frames"]
        amplitude = 10 ** (config["input_peak_dbfs"] / 20)
        for rate in config["rates"]:
            settle = math.ceil(rate * config["settle_seconds"] / 256) * 256
            for requested in config["tones_hz"]:
                k = round(requested * n / rate) | 1  # odd bin: coprime to power-of-two FFT
                length = settle + 2 * n + 256  # includes reported latency and two steady-state windows
                x = amplitude * np.sin(2 * np.pi * k * np.arange(length) / n)
                path = stage / "inputs" / f"sine-{rate}-{k}.wav"
                float_wav(path, rate, np.column_stack((x, x)))
                inputs[(rate, requested)] = (path, k, settle)
            # Periodic multitone stimulus with independent stereo phases. A whole
            # number of periods prevents a boundary transient when the benchmark loops.
            t = np.arange(n) / n
            x = np.sin(2 * np.pi * 23 * t) + .4 * np.sin(2 * np.pi * 131 * t) + .2 * np.sin(2 * np.pi * 1093 * t)
            y = np.sin(2 * np.pi * 19 * t + .7) + .4 * np.sin(2 * np.pi * 151 * t) + .2 * np.sin(2 * np.pi * 1097 * t)
            signal = np.column_stack((x, y)) * (amplitude / 1.6)
            path = stage / "inputs" / f"cpu-{rate}.wav"
            float_wav(path, rate, signal)
            inputs[(rate, "cpu")] = (path, None, None)
        routes = []
        for rate, gain, factor in itertools.product(config["rates"], config["gains"], config["factors"]):
            routes.extend(("alias", rate, gain, factor, config["alias_block_size"], tone) for tone in config["tones_hz"])
            routes.extend(("cpu", rate, gain, factor, block, "cpu") for block in config["blocks"])
        random.Random(args.seed).shuffle(routes)
        for index, (kind, rate, gain, factor, block, tone) in enumerate(routes):
            route = f"{kind}-sr{rate}-g{gain:g}-os{factor}-b{block}-t{tone}"
            print(f"[{index+1}/{len(routes)}] {route}", flush=True)
            evidence["execution_order"].append(route)
            input_path, tone_bin, settle = inputs[(rate, tone)]
            controls = stage / "controls.json"
            save_json(controls, {"controls": {"gain": gain, "blend": config["blend"]}})
            out = stage / "runs" / route
            argv = [str(renderer), str(input_path), str(out), "--controls", str(controls),
                    "--oversampling", str(factor), "--block-size", str(block), "--tail-seconds", "0", "--input-kind", "synthetic"]
            if kind == "cpu":
                argv += ["--benchmark-repeats", str(config["repeats"]), "--benchmark-blocks", str(config["measured_blocks"]),
                         "--benchmark-warmup-blocks", str(config["warmup_blocks"])]
            process = subprocess.run(argv, capture_output=True, text=True, encoding="utf-8", errors="replace", timeout=900)
            (stage / "runs" / (route + ".log")).write_text(process.stdout + process.stderr, encoding="utf-8")
            if process.returncode:
                raise RuntimeError(f"Renderer failed: {route}\n{process.stdout}\n{process.stderr}")
            manifest = json.loads((out / "manifest.json").read_text(encoding="utf-8"))
            observations = [json.loads(line[len("NIFLHEIMR_IO "):]) for line in process.stdout.splitlines()
                            if line.startswith("NIFLHEIMR_IO ")]
            verify_manifest(manifest, source, input_path, rate, factor, block)
            if "build" in evidence and evidence["build"] != manifest["build"]:
                raise ValueError("Build identity changed during measurement")
            evidence["build"] = manifest["build"]
            evidence["compiled_source_hashes"] = manifest["source_hashes"]
            for row in manifest["outputs"]:
                wav = out / row["file"]
                raw = read_output(wav, row, 58 + manifest["output_frames"] * manifest["audio_channels"] * 4,
                                  observations)
                if len(row["controls"]) != 14 or abs(row["controls"]["gain"] - gain) > 1e-6 or row["controls"]["blend"] != config["blend"]:
                    raise ValueError("Resolved controls do not match requested route")
                if kind == "cpu" and (len(row["cpu"]["repetitions"]) != config["repeats"] or
                        row["cpu"]["warmup_blocks_per_repeat"] != config["warmup_blocks"] or
                        row["cpu"]["measured_blocks_per_repeat"] != config["measured_blocks"]):
                    raise ValueError("Incomplete CPU repetition coverage")
                identifier = route + "-" + row["channel_key"]
                entry = {"id": identifier, "channel_key": row["channel_key"], "channel_name": row["channel_name"],
                         "rate": rate, "gain": gain, "oversampling": factor, "block_size": block,
                         "controls": row["controls"], "input_sha256": manifest["input_sha256"],
                         "manifest": str((out / "manifest.json").relative_to(stage))}
                if kind == "cpu":
                    entry["summary"] = summarize_cpu(row["cpu"])
                    evidence["cpu"].append(entry)
                else:
                    actual_rate, audio = read_wav(wav, raw=raw)
                    if actual_rate != rate or audio.shape[1] != 2:
                        raise ValueError("Unexpected render format")
                    offset = settle + row["latency_samples"]
                    if len(audio) < offset + 2 * n:
                        raise ValueError("Insufficient steady-state samples")
                    power = np.array([[spectrum(audio[offset + segment*n:offset+(segment+1)*n, channel])
                                       for channel in range(2)] for segment in range(2)])
                    path = stage / "spectra" / (identifier + ".npz")
                    harmonic, fold, residual = masks(n, tone_bin, config["max_fold_order"])
                    _, source_audio = read_wav(input_path)
                    source_power = spectrum(source_audio[settle:settle+n, 0])
                    np.savez_compressed(path, frequency_hz=np.fft.rfftfreq(n, 1/rate), power_fs2=power,
                                        input_power_fs2=source_power, harmonic_mask=harmonic,
                                        foldback_candidate_mask=fold, off_harmonic_mask=residual)
                    entry.update({"spectrum": str(path.relative_to(stage)), "tone_bin": tone_bin, "fft_frames": n,
                                  "tone_hz": tone_bin * rate / n, "start_sample": offset,
                                  "latency_samples": row["latency_samples"],
                                  "input_metrics": alias_metrics(source_power, n, tone_bin),
                                  "windows": [[alias_metrics(p, n, tone_bin) for p in segment] for segment in power]})
                    evidence["alias"].append(entry)
                if not args.keep_audio:
                    wav.unlink()  # float spectra + render hashes remain; opt in to large WAV archive
        controls.unlink()
        if digest(renderer) != source["renderer_binary_sha256"]:
            raise ValueError("Renderer changed during measurement")
        evidence["alias_oversampling_comparison"] = compare_alias_factors(evidence["alias"])
        evidence["status"] = "MEASURED_TECHNICAL_ONLY"
        evidence["elapsed_seconds"] = time.monotonic() - started
        evidence["environment"]["load_average_end"] = list(os.getloadavg()) if hasattr(os, "getloadavg") else None
        evidence["audio_retained"] = args.keep_audio
        if args.baseline:
            baseline = json.loads(args.baseline.read_text(encoding="utf-8"))
            evidence["comparison"] = comparison(evidence, baseline)
            evidence["comparison"]["baseline_file_sha256"] = digest(args.baseline)
        else:
            evidence["comparison"] = {"status": "NO_BASELINE", "rows": []}
        for path in sorted(stage.rglob("*")):
            if path.is_file():
                evidence["files"][str(path.relative_to(stage))] = digest(path)
        save_json(stage / "evidence.json", evidence)
        # Destination is new; publish the complete run atomically on the same filesystem.
        if destination.exists():
            raise ValueError("Output appeared during measurement")
        stage.rename(destination)
    return evidence


def verify_manifest(manifest, source, input_path, rate, factor, block):
    if manifest.get("release_approved") is not False or len(manifest.get("outputs", [])) != 5:
        raise ValueError("Renderer contract mismatch")
    if manifest["configured_git_head"] != source["head"]:
        raise ValueError("Renderer configured HEAD is stale; rerun CMake configure/build")
    for name, expected in manifest["source_hashes"].items():
        if digest(ROOT / "Source" / name) != expected:
            raise ValueError("Stale renderer source: " + name)
    if header_fingerprint(ROOT / "Source") != manifest["build"]["source_header_set_sha256"]:
        raise ValueError("Stale renderer source-header set")
    if digest(ROOT / "Tests/RenderNiflheimr.cpp") != manifest["build"]["renderer_sha256"]:
        raise ValueError("Stale renderer measurement code")
    if manifest["input_sha256"] != digest(input_path):
        raise ValueError("Input hash mismatch")
    if (manifest["sample_rate"], manifest["oversampling_factor"], manifest["block_size"]) != (rate, factor, block):
        raise ValueError("Renderer configuration mismatch")
    if len({row["channel_key"] for row in manifest["outputs"]}) != 5:
        raise ValueError("Missing channel coverage")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--renderer", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--profile", choices=("smoke", "standard"), default="standard")
    parser.add_argument("--rates", type=int, nargs="+")
    parser.add_argument("--gains", type=float, nargs="+")
    parser.add_argument("--factors", type=int, nargs="+")
    parser.add_argument("--blocks", type=int, nargs="+")
    parser.add_argument("--seed", type=int, default=731)
    parser.add_argument("--machine-label", required=True, help="Stable descriptive hardware/runner class; no secrets")
    parser.add_argument("--baseline", type=Path, help="Prior evidence.json; comparisons require compatible provenance")
    parser.add_argument("--keep-audio", action="store_true")
    args = parser.parse_args()
    try:
        evidence = execute(args)
    except (ValueError, RuntimeError, OSError, subprocess.TimeoutExpired, KeyError) as error:
        parser.exit(1, f"Measurement failed: {error}\n")
    print(f"Recorded {len(evidence['alias'])} spectra and {len(evidence['cpu'])} CPU routes; release_approved=false")


if __name__ == "__main__":
    main()
