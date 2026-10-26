#!/usr/bin/env python3
"""Issue and verify beta tester codes that lift the beta expiry.

A code names one tester and is signed with the owner's private key. Chimera
verifies it offline with the public key in Source/TesterCodeKey.h, so it works
for every later beta build signed with the same key. Keep the private key file
out of the repository; anyone holding it can issue codes.

Format:  CHT1-<base64url payload>.<base64url signature>
Payload: canonical JSON {"iat","id","name","scope":"beta","v":1}
Signature: RSA full-domain hash (FDH) over the payload, see encode().

  python Tools/tester_code.py keygen --private ~/chimera-tester-key.json --header Source/TesterCodeKey.h
  python Tools/tester_code.py issue --private ~/chimera-tester-key.json --name "Tester Name"
  python Tools/tester_code.py verify --code CHT1-...
"""
from __future__ import annotations

import argparse
import base64
import datetime
import hashlib
import json
import re
import secrets
import sys
from pathlib import Path

PREFIX = "CHT1-"
DOMAIN = b"chimera-tester-v1"
PUBLIC_EXPONENT = 65537
MAX_NAME = 64
ROOT = Path(__file__).resolve().parents[1]
DEFAULT_HEADER = ROOT / "Source" / "TesterCodeKey.h"


def b64url(data: bytes) -> str:
    return base64.urlsafe_b64encode(data).rstrip(b"=").decode("ascii")


def unb64url(text: str) -> bytes:
    if not re.fullmatch(r"[A-Za-z0-9_-]*", text):
        raise ValueError("Invalid base64url text")
    data = base64.urlsafe_b64decode(text + "=" * (-len(text) % 4))
    if b64url(data) != text:  # unused trailing bits must be zero: one spelling per code
        raise ValueError("Non-canonical base64url text")
    return data


def encode(payload: bytes, modulus_bytes: int) -> int:
    """0x00 || first (k-1) bytes of SHA-256(DOMAIN || i || payload), i = 0, 1, ..."""
    stream = b""
    counter = 0
    while len(stream) < modulus_bytes - 1:
        stream += hashlib.sha256(DOMAIN + counter.to_bytes(4, "big") + payload).digest()
        counter += 1
    return int.from_bytes(b"\x00" + stream[:modulus_bytes - 1], "big")


def canonical_payload(name: str, code_id: str, issued: str) -> bytes:
    return json.dumps({"iat": issued, "id": code_id, "name": name, "scope": "beta", "v": 1},
                      sort_keys=True, separators=(",", ":"), ensure_ascii=False).encode("utf-8")


def check_name(name: str) -> str:
    name = name.strip()
    if not name or len(name) > MAX_NAME or any(ord(c) < 32 for c in name):
        raise ValueError(f"Tester name must be 1-{MAX_NAME} printable characters")
    return name


def sign(payload: bytes, n: int, d: int) -> str:
    k = (n.bit_length() + 7) // 8
    signature = pow(encode(payload, k), d, n).to_bytes(k, "big")
    return PREFIX + b64url(payload) + "." + b64url(signature)


def verify(code: str, n: int, e: int = PUBLIC_EXPONENT) -> dict:
    code = "".join(code.split())
    if not code.startswith(PREFIX) or code.count(".") != 1:
        raise ValueError("Not a Chimera tester code")
    payload_text, signature_text = code[len(PREFIX):].split(".")
    payload, signature = unb64url(payload_text), unb64url(signature_text)
    k = (n.bit_length() + 7) // 8
    s = int.from_bytes(signature, "big")
    if len(signature) != k or s >= n or pow(s, e, n) != encode(payload, k):
        raise ValueError("Signature does not match the public key")
    fields = json.loads(payload.decode("utf-8"))
    if (not isinstance(fields, dict) or fields.get("v") != 1 or fields.get("scope") != "beta"
            or payload != canonical_payload(check_name(str(fields.get("name", ""))),
                                            str(fields.get("id", "")), str(fields.get("iat", "")))):
        raise ValueError("Unsupported tester code payload")
    return fields


def probable_prime(bits: int) -> int:
    def miller_rabin(n: int) -> bool:
        for p in (3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37):
            if n % p == 0:
                return n == p
        d, r = n - 1, 0
        while d % 2 == 0:
            d //= 2
            r += 1
        for _ in range(64):
            x = pow(secrets.randbelow(n - 3) + 2, d, n)
            if x in (1, n - 1):
                continue
            for _ in range(r - 1):
                x = pow(x, 2, n)
                if x == n - 1:
                    break
            else:
                return False
        return True
    while True:
        candidate = secrets.randbits(bits) | (1 << (bits - 1)) | (1 << (bits - 2)) | 1
        if miller_rabin(candidate):
            return candidate


def generate_key(bits: int = 2048) -> dict:
    while True:
        p, q = probable_prime(bits // 2), probable_prime(bits // 2)
        n, phi = p * q, (p - 1) * (q - 1)
        if p != q and n.bit_length() == bits and phi % PUBLIC_EXPONENT:
            return {"schema": "spectralforge.chimera.tester-key.v1", "n": format(n, "x"),
                    "e": PUBLIC_EXPONENT, "d": format(pow(PUBLIC_EXPONENT, -1, phi), "x")}


def header_text(n: int) -> str:
    hexed = format(n, "x")
    lines = "\n".join(f'    "{hexed[i:i + 64]}"' for i in range(0, len(hexed), 64))
    return ("#pragma once\n"
            "// Public half of the beta tester code key (Tools/tester_code.py keygen).\n"
            "// The private key is kept by the owner and never committed.\n"
            "namespace spectralforge::beta::tester {\n"
            f"inline constexpr int publicExponent = {PUBLIC_EXPONENT};\n"
            "inline constexpr const char* publicModulusHex =\n"
            f"{lines};\n"
            "} // namespace spectralforge::beta::tester\n")


def modulus_from_header(path: Path) -> int:
    text = path.read_text(encoding="utf-8")
    block = text.split("publicModulusHex =", 1)[1].split(";", 1)[0]
    return int("".join(re.findall(r'"([0-9a-f]+)"', block)), 16)


def load_private(path: Path) -> dict:
    key = json.loads(path.read_text(encoding="utf-8"))
    if key.get("schema") != "spectralforge.chimera.tester-key.v1" or key.get("e") != PUBLIC_EXPONENT:
        raise ValueError("Not a Chimera tester private key")
    return key


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = parser.add_subparsers(dest="command", required=True)
    keygen = sub.add_parser("keygen", help="create a key pair; writes the private key and the public header")
    keygen.add_argument("--private", type=Path, required=True)
    keygen.add_argument("--header", type=Path, default=DEFAULT_HEADER)
    keygen.add_argument("--bits", type=int, default=2048)
    issue = sub.add_parser("issue", help="issue a code for one tester")
    issue.add_argument("--private", type=Path, required=True)
    issue.add_argument("--name", required=True)
    issue.add_argument("--issued", default=datetime.date.today().isoformat())
    issue.add_argument("--id", default="")
    check = sub.add_parser("verify", help="verify a code against the public header")
    check.add_argument("--code", required=True)
    check.add_argument("--header", type=Path, default=DEFAULT_HEADER)
    args = parser.parse_args()

    if args.command == "keygen":
        if args.private.exists():
            raise SystemExit(f"Refusing to overwrite {args.private}")
        key = generate_key(args.bits)
        args.private.write_text(json.dumps(key, indent=2) + "\n", encoding="utf-8")
        args.private.chmod(0o600)
        args.header.write_text(header_text(int(key["n"], 16)), encoding="utf-8")
        print(f"Private key: {args.private} (keep it offline; never commit)\nPublic header: {args.header}")
    elif args.command == "issue":
        key = load_private(args.private)
        datetime.date.fromisoformat(args.issued)
        payload = canonical_payload(check_name(args.name), args.id or secrets.token_hex(4), args.issued)
        code = sign(payload, int(key["n"], 16), int(key["d"], 16))
        verify(code, int(key["n"], 16))
        print(code)
    else:
        fields = verify(args.code, modulus_from_header(args.header))
        print(json.dumps(fields, ensure_ascii=False))
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except ValueError as error:
        print(f"error: {error}", file=sys.stderr)
        sys.exit(1)
