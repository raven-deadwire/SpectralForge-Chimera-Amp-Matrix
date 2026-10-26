#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Write a module-definition (.def) file listing a Windows DLL's exports.

Windows has no libobs development package. CI builds the module against the
libobs headers of a release tag and an import library generated from that
release's own obs.dll (`lib /def:obs.def /machine:x64 /out:obs.lib`), so the
module links exactly the symbols the shipped OBS exports.
"""
import argparse
import struct
import sys
from pathlib import Path


def exports(dll: bytes) -> list[str]:
    if dll[:2] != b"MZ":
        raise ValueError("not a PE image")
    pe = struct.unpack_from("<I", dll, 0x3C)[0]
    if dll[pe:pe + 4] != b"PE\0\0":
        raise ValueError("missing PE signature")
    sections = struct.unpack_from("<H", dll, pe + 6)[0]
    optional_size = struct.unpack_from("<H", dll, pe + 20)[0]
    optional = pe + 24
    magic = struct.unpack_from("<H", dll, optional)[0]
    if magic != 0x20B:
        raise ValueError("only PE32+ (x64) images are supported")
    export_rva, export_size = struct.unpack_from("<II", dll, optional + 112)
    if export_size == 0:
        return []
    table = optional + optional_size
    ranges = []
    for i in range(sections):
        header = table + 40 * i
        virtual_size, virtual_address, raw_size, raw_pointer = struct.unpack_from("<IIII", dll, header + 8)
        ranges.append((virtual_address, max(virtual_size, raw_size), raw_pointer))

    def offset(rva: int) -> int:
        for start, size, raw in ranges:
            if start <= rva < start + size:
                return raw + rva - start
        raise ValueError(f"RVA {rva:#x} is outside every section")

    directory = offset(export_rva)
    names_count = struct.unpack_from("<I", dll, directory + 24)[0]
    names_rva = struct.unpack_from("<I", dll, directory + 32)[0]
    names = []
    for i in range(names_count):
        name_rva = struct.unpack_from("<I", dll, offset(names_rva) + 4 * i)[0]
        start = offset(name_rva)
        names.append(dll[start:dll.index(b"\0", start)].decode("ascii"))
    return names


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("dll", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    names = exports(args.dll.read_bytes())
    if not names:
        print(f"{args.dll} exports nothing", file=sys.stderr)
        return 1
    args.output.write_text(f"LIBRARY {args.dll.stem}\nEXPORTS\n" + "".join(f"    {n}\n" for n in names), encoding="ascii")
    print(f"{len(names)} exports from {args.dll.name} -> {args.output}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
