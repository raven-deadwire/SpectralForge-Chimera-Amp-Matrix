#!/usr/bin/env python3
"""Link isolated BlockEqProbe against the already built, pinned native objects.

This reuses CMake's compile/link arguments without editing any original target.
No shell evaluation is used. Only the new one-file probe is compiled.
"""
import argparse
import hashlib
import json
import shlex
import subprocess
from pathlib import Path


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", type=Path, required=True)
    parser.add_argument("--out-dir", type=Path, required=True)
    parser.add_argument("--record", type=Path, required=True)
    args = parser.parse_args()
    args.build_dir = args.build_dir.resolve()
    args.out_dir = args.out_dir.resolve()
    args.out_dir.mkdir(parents=True, exist_ok=True)
    source = Path(__file__).with_name("BlockEqProbe.cpp").resolve()
    obj = args.out_dir / "BlockEqProbe.cpp.o"
    binary = args.out_dir / "BlockEqProbe"
    commands = subprocess.check_output(
        ["ninja", "-C", str(args.build_dir), "-t", "commands", "ChimeraTone3000Tool"], text=True
    ).splitlines()
    compile_line = next(line for line in commands if line.endswith("/Tone3000Tool.cpp"))
    compile_cmd = shlex.split(compile_line)
    for option, value in (("-MT", str(obj)), ("-MF", str(obj) + ".d"),
                          ("-o", str(obj)), ("-c", str(source))):
        compile_cmd[compile_cmd.index(option) + 1] = value
    link_tokens = shlex.split(next(line for line in commands if line.startswith(": && ")))
    if link_tokens[:2] != [":", "&&"] or link_tokens[-2:] != ["&&", ":"]:
        raise RuntimeError("Unexpected native link-command framing")
    link_tokens = link_tokens[2:-2]
    link_cmd = []
    reused_objects = []
    for token in link_tokens:
        if token.endswith(".o"):
            if token.endswith("/Tone3000Tool.cpp.o"):
                link_cmd.append(str(obj))
            elif token.endswith("/BlockEq.cpp.o") or "/JUCE/modules/" in token:
                original = args.build_dir / token
                if not original.is_file():
                    raise RuntimeError("Missing original compiled object: " + str(original))
                link_cmd.append(str(original))
                reused_objects.append({"path": str(original), "sha256": digest(original)})
        else:
            link_cmd.append(token)
    link_cmd[link_cmd.index("-o") + 1] = str(binary)
    subprocess.run(compile_cmd, cwd=args.build_dir, check=True)
    subprocess.run(link_cmd, cwd=args.build_dir, check=True)
    record = {
        "source_sha256": digest(source), "binary_sha256": digest(binary),
        "binary": str(binary), "compile_arguments": compile_cmd,
        "link_arguments": link_cmd, "reused_objects": reused_objects,
        "upstream_modified": False,
        "scope": "One experimental wrapper plus the already compiled unmodified pinned BlockEq and JUCE objects",
    }
    args.record.parent.mkdir(parents=True, exist_ok=True)
    args.record.write_text(json.dumps(record, indent=2) + "\n")
    print(json.dumps({"binary": str(binary), "sha256": record["binary_sha256"],
                      "reused_objects": len(reused_objects), "upstream_modified": False}))


if __name__ == "__main__":
    main()
