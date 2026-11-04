#!/usr/bin/env python3
"""Compare production and pre-fix meter bodies with a separately built libebur128.

No download, patching or product dependency injection is performed here.
The baseline executable intentionally exits nonzero when defects reproduce.
"""
import argparse
import json
from pathlib import Path
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--include", type=Path, required=True, help="Directory containing ebur128.h")
    parser.add_argument("--library", type=Path, required=True, help="Separately built libebur128 library")
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--baseline", default="ad622ceffdd7404b12de4e473349b1104200048f")
    parser.add_argument("--compiler", default="c++")
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    args.out.mkdir(parents=True, exist_ok=True)
    source = subprocess.check_output(
        ["git", "show", f"{args.baseline}:Source/AutoSet.h"], cwd=root, text=True)
    # Compile the exact old function bodies, not a Python reimplementation.
    weighting = source[source.index("inline std::array<EQCoefficients,2> kWeighting"):
                       source.index("inline float finalResponseDb")]
    meter = source[source.index("inline std::optional<float> integratedLoudness"):
                   source.index("// samples: mono limiter input")]
    header = ('#include "AutoSetLoudness.h"\n'
              'namespace juce {template<typename T> struct MathConstants {'
              'static constexpr T pi=std::numbers::pi_v<T>;};}\n'
              'namespace baseline {using EQCoefficients=spectralforge::autoset::WeightingCoefficients;\n'
              + weighting + meter + '}\n')
    results = {"baseline_ref": args.baseline}
    with tempfile.TemporaryDirectory(prefix="chimera-loudness-") as temporary:
        temp = Path(temporary)
        (temp / "BaselineLoudness.h").write_text(header)
        for name in ("baseline", "reference"):
            executable = temp / name
            command = [args.compiler, "-std=c++20", "-O2", f"-I{root / 'Source'}",
                       f"-I{temp}", f"-I{args.include.resolve()}",
                       str(root / "Tests/AutoSetLoudnessReferenceTests.cpp"),
                       str(args.library.resolve()), f"-Wl,-rpath,{args.library.resolve().parent}",
                       "-o", str(executable)]
            if name == "baseline":
                command.append("-DCHIMERA_BASELINE_LOUDNESS")
            subprocess.run(command, check=True)
            with (args.out / f"{name}-loudness.csv").open("w") as output:
                run = subprocess.run([str(executable)], stdout=output, stderr=subprocess.PIPE, text=True)
            results[name] = {"exit_code": run.returncode, "summary": run.stderr.strip()}
            if name == "reference" and run.returncode:
                raise RuntimeError(run.stderr)
    (args.out / "reference-summary.json").write_text(json.dumps(results, indent=2) + "\n")
    print(json.dumps(results, indent=2))


if __name__ == "__main__":
    main()
