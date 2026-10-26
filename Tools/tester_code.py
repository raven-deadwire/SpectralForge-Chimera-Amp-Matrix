#!/usr/bin/env python3
"""Issue and verify beta tester codes that lift the beta expiry.

A code is short enough to type and names one tester:

    CHT1-<NAME>-<CHECK>        e.g. CHT1-ZIEL-7K3Q9X

CHECK is the first 30 bits of HMAC-SHA256(secret, "chimera-tester-v1|" + NAME)
in Crockford base32 (Source/TesterCode.h computes the same). The secret is kept
by the owner and given to release builds through the CHIMERA_TESTER_SECRET
environment variable (a GitHub Actions secret); it is never committed. A build
configured without it accepts no codes. Anyone holding the secret can issue
codes, so keep the secret file offline.

  python Tools/tester_code.py secret --out ~/chimera-tester-secret.private.json
  python Tools/tester_code.py issue  --secret ~/chimera-tester-secret.private.json --name ZIEL
  python Tools/tester_code.py verify --secret ~/chimera-tester-secret.private.json --code CHT1-ZIEL-XXXXXX
"""
from __future__ import annotations

import argparse
import hashlib
import hmac
import json
import os
import re
import secrets
import sys
from pathlib import Path

PREFIX = "CHT1"
DOMAIN = "chimera-tester-v1"
ALPHABET = "0123456789ABCDEFGHJKMNPQRSTVWXYZ"  # Crockford base32
CHECK_LENGTH = 6
MIN_NAME, MAX_NAME = 2, 16
MAX_CODE, MAX_PASTED = 64, 256
SCHEMA = "spectralforge.chimera.tester-secret.v1"


def check_name(name: str) -> str:
    """Upper-case and drop spaces, dots and hyphens: "Seo Mary" -> "SEOMARY"."""
    normalised = re.sub(r"[\s.\-_]", "", name).upper()
    if not re.fullmatch(rf"[A-Z0-9]{{{MIN_NAME},{MAX_NAME}}}", normalised):
        raise ValueError(f"Tester name must be {MIN_NAME}-{MAX_NAME} letters A-Z or digits (got {name!r})")
    return normalised


def check(secret: bytes, name: str) -> str:
    mac = hmac.new(secret, f"{DOMAIN}|{name}".encode("utf-8"), hashlib.sha256).digest()
    bits = int.from_bytes(mac[:4], "big")
    return "".join(ALPHABET[(bits >> (27 - 5 * i)) & 31] for i in range(CHECK_LENGTH))


def issue(secret: bytes, name: str) -> str:
    name = check_name(name)
    return f"{PREFIX}-{name}-{check(secret, name)}"


def verify(code: str, secret: bytes) -> str:
    """Return the tester name, or raise ValueError. Mirrors tester::verify."""
    if len(secret) < 16:
        raise ValueError("Tester codes are not enabled with this secret")
    if len(code) > MAX_PASTED:
        raise ValueError("Pasted code is too long")
    code = code.translate(str.maketrans("", "", " \t\r\n"))
    if len(code) > MAX_CODE:
        raise ValueError("Code is too long")
    parts = code.split("-")
    if len(parts) != 3 or parts[0].upper() != PREFIX:
        raise ValueError("Not a Chimera beta tester code")
    name = parts[1].upper()
    if not re.fullmatch(rf"[A-Z0-9]{{{MIN_NAME},{MAX_NAME}}}", name) or len(parts[2]) != CHECK_LENGTH:
        raise ValueError("Incomplete tester code")
    typed = parts[2].upper().replace("O", "0").replace("I", "1").replace("L", "1")
    if len(typed) != CHECK_LENGTH or any(c not in ALPHABET for c in typed):
        raise ValueError("Invalid check characters")
    if not hmac.compare_digest(typed, check(secret, name)):
        raise ValueError("Check characters do not match this secret")
    return name


def load_secret(path: Path) -> bytes:
    data = json.loads(path.read_text(encoding="utf-8"))
    if not isinstance(data, dict) or data.get("schema") != SCHEMA:
        raise ValueError("Not a Chimera tester secret file")
    value = data.get("secret")
    if not isinstance(value, str) or not re.fullmatch(r"(?:[0-9a-fA-F]{2}){16,}", value):
        raise ValueError("Tester secret must contain at least 16 complete hexadecimal bytes")
    secret = bytes.fromhex(value)
    if len(secret) < 16:
        raise ValueError("Tester secret is too short")
    return secret


def write_secret(path: Path, value: str) -> None:
    resolved = path.resolve()
    if any((parent / ".git").exists() for parent in (resolved.parent, *resolved.parents)):
        raise ValueError("Keep the tester secret outside every Git checkout")
    # No overwrite or symlink following; POSIX permissions protect the first byte.
    fd = os.open(path, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
    with os.fdopen(fd, "w", encoding="utf-8") as stream:
        json.dump({"schema": SCHEMA, "secret": value}, stream, indent=2)
        stream.write("\n")
        stream.flush()
        os.fsync(stream.fileno())


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = parser.add_subparsers(dest="command", required=True)
    make = sub.add_parser("secret", help="create a new secret file")
    make.add_argument("--out", type=Path, required=True)
    make.add_argument("--show-secret", action="store_true", help="explicitly print the secret for interactive setup; do not log")
    for command in ("issue", "verify"):
        p = sub.add_parser(command)
        p.add_argument("--secret", type=Path, required=True)
    sub.choices["issue"].add_argument("--name", required=True, action="append",
                                      help="tester name; repeat for several testers")
    sub.choices["verify"].add_argument("--code", required=True)
    args = parser.parse_args()

    if args.command == "secret":
        if args.out.exists() or args.out.is_symlink():
            raise SystemExit(f"Refusing to overwrite {args.out}")
        value = secrets.token_bytes(32).hex()
        write_secret(args.out, value)
        print(f"Secret file: {args.out} (keep it offline; never commit)")
        if args.show_secret:
            print("Set this as the CHIMERA_TESTER_SECRET GitHub Actions secret:")
            print(value)
    elif args.command == "issue":
        secret = load_secret(args.secret)
        for name in args.name:
            print(issue(secret, name))
    else:
        print(verify(args.code, load_secret(args.secret)))
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (ValueError, OSError) as error:
        print(f"error: {error}", file=sys.stderr)
        sys.exit(1)
