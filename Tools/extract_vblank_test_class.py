#!/usr/bin/env python3
"""Extract the actual fetched JUCE class for a dependency-free fault fixture."""
import argparse
import hashlib
import json
from pathlib import Path


def extract(path: Path):
    source = path.read_text(encoding="utf-8").replace("\r\n", "\n")
    marker = "class VBlankThread : private Thread,"
    if source.count(marker) != 1:
        raise ValueError(f"Expected one actual VBlankThread class in {path}")
    start = source.index(marker)
    end = source.index("\n};", start) + len("\n};")
    body = source[start:end]
    if "class VBlankDispatcher" not in source[end:]:
        raise ValueError("Unexpected JUCE source structure after the extracted class")
    for required in ("~VBlankThread() override", "stopThread (-1);", "void run() override", "output->WaitForVBlank() == S_OK"):
        if body.count(required) != 1:
            raise ValueError(f"Actual class does not contain exactly one {required}")
    licence = source[:source.index("namespace juce")]
    return source, body, licence


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--baseline-source", type=Path, required=True)
    parser.add_argument("--patched-source", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    args = parser.parse_args()
    args.output_dir.mkdir(parents=True, exist_ok=True)
    reports = {}
    bodies = {}
    for label, path in (("baseline", args.baseline_source), ("patched", args.patched_source)):
        source, body, licence = extract(path)
        bodies[label] = body
        digest = hashlib.sha256(body.encode()).hexdigest()
        header = args.output_dir / f"VBlankThread{label.title()}.h"
        header.write_text(licence + f"// Exact extracted JUCE class; SHA-256 {digest}\nnamespace juce\n{{\n" + body + "\n}\n", encoding="utf-8")
        reports[label] = {"source": str(path.resolve()), "source_sha256": hashlib.sha256(source.encode()).hexdigest(), "class_sha256": digest, "header": header.name}
    if bodies["baseline"] == bodies["patched"]:
        raise ValueError("Baseline and patched class bodies are identical; fault comparison is invalid")
    if "for (;;)" not in bodies["baseline"]:
        raise ValueError("Baseline is not the unmodified JUCE unconditional loop")
    (args.output_dir / "extraction.json").write_text(json.dumps({"extraction": "verbatim class body; no test-authored shutdown loop", "sources": reports}, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
