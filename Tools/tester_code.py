#!/usr/bin/env python3
"""Owner-side issuer for finite, offline beta tester allowlists.

Codes retain CHT1-<NAME>-<6 Crockford Base32 characters>. They are random bearer
codes, not HMACs: there is no issuance key in Chimera. Only SHA-256 digests of
complete canonical codes enter a build. Six characters provide only 30 bits;
this is limited beta access, NOT strong licensing or offline-guessing resistance.
A new code outside the built-in list needs a new build. Reserve generic named
codes before building if later assignment without an update is needed.

  python Tools/tester_code.py issue --name TEST01 --private-out ~/tester.private.json
  python Tools/tester_code.py export --private-file ~/tester.private.json --out allowlist.json
  python Tools/tester_code.py verify --allowlist allowlist.json --code-file ~/code.txt

Issuance and export never print codes. Keep private files outside all Git
checkouts, build inputs, artifacts and logs. Actual issuance and importing real
codes require owner authorization; tests use throwaway fixtures only.
"""
from __future__ import annotations

import argparse
import hashlib
import hmac
import json
import os
import re
import secrets
import stat
import sys
from pathlib import Path

PREFIX = "CHT1"
DOMAIN = "chimera-tester-allowlist-v1"
ALPHABET = "0123456789ABCDEFGHJKMNPQRSTVWXYZ"
CHECK_LENGTH = 6
MIN_NAME, MAX_NAME = 2, 16
MAX_CODE, MAX_PASTED = 64, 256
SCHEMA = "spectralforge.chimera.tester-allowlist.v1"
PRIVATE_SCHEMA = "spectralforge.chimera.tester-private.v1"
MAX_ENTRIES = 4096
MAX_MANIFEST_BYTES = 524288


def check_name(name: str) -> str:
    if not isinstance(name, str) or not name.isascii():
        raise ValueError("Tester name must contain ASCII letters or digits")
    normalised = re.sub(r"[\s.\-_]", "", name).upper()
    if not re.fullmatch(rf"[A-Z0-9]{{{MIN_NAME},{MAX_NAME}}}", normalised):
        raise ValueError(f"Tester name must contain {MIN_NAME}-{MAX_NAME} letters A-Z or digits")
    return normalised


def canonical(code: str) -> str:
    """Validate format, normalize typing aids, and never include input in errors."""
    if not isinstance(code, str) or len(code) > MAX_PASTED:
        raise ValueError("Pasted code is too long or invalid")
    if not code.isascii():
        raise ValueError("Tester code must contain ASCII characters")
    code = code.translate(str.maketrans("", "", " \t\r\n"))
    if len(code) > MAX_CODE:
        raise ValueError("Tester code is too long")
    parts = code.split("-")
    if len(parts) != 3 or parts[0].upper() != PREFIX:
        raise ValueError("Not a Chimera beta tester code")
    name = parts[1].upper()
    if not re.fullmatch(rf"[A-Z0-9]{{{MIN_NAME},{MAX_NAME}}}", name):
        raise ValueError("Invalid tester name")
    check = parts[2].upper().replace("O", "0").replace("I", "1").replace("L", "1")
    if len(check) != CHECK_LENGTH or any(c not in ALPHABET for c in check):
        raise ValueError("Invalid check characters")
    return f"{PREFIX}-{name}-{check}"


def issue(name: str) -> str:
    """Create one random code. Do not call for production without owner approval."""
    return f"{PREFIX}-{check_name(name)}-" + "".join(secrets.choice(ALPHABET) for _ in range(CHECK_LENGTH))


def digest(code: str) -> str:
    return hashlib.sha256(f"{DOMAIN}|{canonical(code)}".encode("ascii")).hexdigest()


def validate_digests(values: object) -> list[str]:
    if not isinstance(values, list) or len(values) > MAX_ENTRIES:
        raise ValueError("Invalid allowlist size or type")
    if any(not isinstance(v, str) or not re.fullmatch(r"[0-9a-f]{64}", v) for v in values):
        raise ValueError("Allowlist entries must be lowercase SHA-256 hex digests")
    if len(set(values)) != len(values):
        raise ValueError("Duplicate allowlist entries")
    return values


def verify(code: str, digests: list[str]) -> str:
    values = validate_digests(digests)
    if not values:
        raise ValueError("Tester codes are not enabled in this build")
    normalised = canonical(code)
    candidate = digest(normalised)
    found = False
    for value in values:
        found |= hmac.compare_digest(candidate, value)
    if not found:
        raise ValueError("This code is not registered for Chimera")
    return normalised.split("-")[1]


def read_regular(path: Path, limit: int = MAX_MANIFEST_BYTES) -> str:
    """Bound reads and reject symlinks, FIFOs, devices and invalid UTF-8."""
    if path.is_symlink():
        raise ValueError("Refusing a symbolic link")
    flags = os.O_RDONLY | getattr(os, "O_NONBLOCK", 0) | getattr(os, "O_NOFOLLOW", 0)
    fd = os.open(path, flags)
    with os.fdopen(fd, "rb") as stream:
        info = os.fstat(stream.fileno())
        if not stat.S_ISREG(info.st_mode) or info.st_size > limit:
            raise ValueError("Not a bounded regular file")
        content = stream.read(limit + 1)
        if len(content) > limit:
            raise ValueError("File is too large")
    try:
        return content.decode("utf-8")
    except UnicodeDecodeError:
        raise ValueError("File must contain UTF-8 text") from None


def load_allowlist(path: Path) -> list[str]:
    data = json.loads(read_regular(path))
    if not isinstance(data, dict) or set(data) != {"schema", "digests"} or data.get("schema") != SCHEMA:
        raise ValueError("Not a Chimera tester allowlist")
    return validate_digests(data["digests"])


def load_private(path: Path) -> list[str]:
    ensure_private_location(path)
    data = json.loads(read_regular(path))
    if not isinstance(data, dict) or set(data) != {"schema", "codes"} or data.get("schema") != PRIVATE_SCHEMA:
        raise ValueError("Not a Chimera private tester file")
    values = data["codes"]
    if not isinstance(values, list) or not 1 <= len(values) <= MAX_ENTRIES:
        raise ValueError("Invalid private tester list size")
    codes = [canonical(code) for code in values]
    if len(set(codes)) != len(codes):
        raise ValueError("Duplicate private tester codes")
    return codes


def ensure_private_location(path: Path) -> None:
    resolved = path.resolve()
    if any((parent / ".git").exists() for parent in (resolved.parent, *resolved.parents)):
        raise ValueError("Keep private tester files outside every Git checkout")
    # Reject parent symlinks too: the owner must choose an explicit real location.
    absolute = path.absolute()
    if any(parent.is_symlink() for parent in (absolute, *absolute.parents)):
        raise ValueError("Private tester paths must not use symbolic links")


def write_json_exclusive(path: Path, data: dict, *, private: bool) -> None:
    if private:
        ensure_private_location(path)
    flags = os.O_WRONLY | os.O_CREAT | os.O_EXCL | getattr(os, "O_NOFOLLOW", 0)
    fd = os.open(path, flags, 0o600 if private else 0o644)
    with os.fdopen(fd, "w", encoding="utf-8") as stream:
        json.dump(data, stream, indent=2)
        stream.write("\n")
        stream.flush()
        os.fsync(stream.fileno())


def write_private(path: Path, codes: list[str]) -> None:
    if not 1 <= len(codes) <= MAX_ENTRIES:
        raise ValueError("Invalid private tester list size")
    values = [canonical(code) for code in codes]
    if len(set(values)) != len(values):
        raise ValueError("Duplicate private tester codes")
    write_json_exclusive(path, {"schema": PRIVATE_SCHEMA, "codes": values}, private=True)


def export_allowlist(private_files: list[Path], out: Path) -> None:
    values = sorted({digest(code) for path in private_files for code in load_private(path)})
    validate_digests(values)
    write_json_exclusive(out, {"schema": SCHEMA, "digests": values}, private=False)


class PrivateArgumentParser(argparse.ArgumentParser):
    def error(self, message: str) -> None:
        # argparse normally echoes unknown argument values, which could be a
        # pasted bearer code. Keep usage errors generic too.
        self.exit(2, "error: invalid tester-code arguments; use --help for file-based inputs\n")


def main() -> int:
    parser = PrivateArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = parser.add_subparsers(dest="command", required=True)
    make = sub.add_parser("issue", help="create owner-private random codes; never print them")
    make.add_argument("--name", required=True, action="append")
    make.add_argument("--private-out", required=True, type=Path)
    export = sub.add_parser("export", help="export only digests for an approved build")
    export.add_argument("--private-file", required=True, action="append", type=Path)
    export.add_argument("--out", required=True, type=Path)
    check = sub.add_parser("verify", help="check a code file against a public allowlist")
    check.add_argument("--allowlist", required=True, type=Path)
    check.add_argument("--code-file", required=True, type=Path)
    args = parser.parse_args()
    if args.command == "issue":
        names = [check_name(name) for name in args.name]
        if len(names) > MAX_ENTRIES or len(set(names)) != len(names):
            raise ValueError("Too many or duplicate tester names")
        # Validate the destination before generating anything.
        ensure_private_location(args.private_out)
        if args.private_out.exists() or args.private_out.is_symlink():
            raise ValueError("Refusing to overwrite a private tester file")
        write_private(args.private_out, [issue(name) for name in names])
        print("Private tester file created. Keep it outside repositories and build artifacts.")
    elif args.command == "export":
        export_allowlist(args.private_file, args.out)
        print("Public digest allowlist created. A build update is required to use new entries.")
    else:
        verify(read_regular(args.code_file, MAX_PASTED), load_allowlist(args.allowlist))
        print("Registered tester code.")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (ValueError, OSError) as error:
        # Avoid reflecting arbitrary input, file contents, or private paths in logs.
        print("error: tester-code operation failed; check file format, location and permissions", file=sys.stderr)
        sys.exit(1)
