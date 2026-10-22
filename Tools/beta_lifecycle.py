#!/usr/bin/env python3
"""Generate source-bound beta lifecycle metadata from an explicit UTC expiry."""
from __future__ import annotations

import argparse
from datetime import datetime, timedelta, timezone
import json
from pathlib import Path
import re

from chimera_version import identity, release_channel

UTC_PATTERN = re.compile(r"^[0-9]{4}-[0-9]{2}-[0-9]{2}T[0-9]{2}:[0-9]{2}:[0-9]{2}Z$")
SHA_PATTERN = re.compile(r"^[0-9a-f]{40}$")
WARNING_LEAD_SECONDS = 14 * 24 * 60 * 60
POLICY_PATH = "Validation/beta-lifecycle-policy.json"
# Fixed synthetic CI fixture, never an owner deadline or a release default.
QUALIFICATION_EXPIRY = "2099-01-01T00:00:00Z"


def parse_utc(value: str) -> datetime:
    if not UTC_PATTERN.fullmatch(value):
        raise ValueError("beta expiry must be an absolute UTC timestamp: YYYY-MM-DDTHH:MM:SSZ")
    try:
        result = datetime.strptime(value, "%Y-%m-%dT%H:%M:%SZ").replace(tzinfo=timezone.utc)
    except ValueError as exc:
        raise ValueError("beta expiry is not a valid UTC timestamp") from exc
    if result.strftime("%Y-%m-%dT%H:%M:%SZ") != value:
        raise ValueError("beta expiry is not canonical UTC")
    return result


def build_metadata(root: Path, source_sha: str, expiry_utc: str,
                   now: datetime | None = None) -> dict[str, object]:
    if not SHA_PATTERN.fullmatch(source_sha):
        raise ValueError("source SHA must be exactly 40 lowercase hexadecimal characters")
    release = identity(root, source_sha)
    if "-beta." not in release["version"]:
        raise ValueError("beta lifecycle metadata may only be generated for a beta source")
    expiry = parse_utc(expiry_utc)
    observed = now or datetime.now(timezone.utc)
    if observed.tzinfo is None:
        raise ValueError("current time must be timezone-aware")
    observed = observed.astimezone(timezone.utc)
    if expiry <= observed:
        raise ValueError("beta expiry must be later than the packaging time")
    return {
        "schema": "spectralforge.chimera.beta-lifecycle",
        "schema_version": 1,
        "product": "SpectralForge Chimera",
        "product_version": release["product_version"],
        "version": release["version"],
        "source_sha": source_sha,
        "build_id": release["build_id"],
        "expires_at_utc": expiry_utc,
        "expires_at_unix": int(expiry.timestamp()),
        "warning_lead_seconds": WARNING_LEAD_SECONDS,
        "network_required": False,
        "active_instance_grace": True,
    }


def configure_metadata(root: Path, source_sha: str, qualification_only: bool = False,
                       now: datetime | None = None) -> dict:
    release = identity(root, source_sha)
    base = {"schema": "spectralforge.chimera.beta-lifecycle", "schema_version": 1,
            "product": "SpectralForge Chimera", **release,
            "expires_at_utc": "", "expires_at_unix": 0,
            "warning_lead_seconds": WARNING_LEAD_SECONDS,
            "network_required": False, "active_instance_grace": True,
            "mode": "not-beta", "expiry_approved": False,
            "new_expired_instance": "muted-output-state-preserved"}
    if not release_channel(root).startswith("beta."):
        return base
    policy = json.loads((root / POLICY_PATH).read_text(encoding="utf-8"))
    if policy.get("schema") != "spectralforge.chimera.beta-policy.v1" or policy.get("version") != release["version"]:
        raise ValueError("Beta policy version differs from the source version")
    if (type(policy.get("expiry_approved")) is not bool
        or policy.get("warning_lead_seconds") != WARNING_LEAD_SECONDS
        or policy.get("new_expired_instance") != base["new_expired_instance"]
        or policy.get("active_instance_grace") is not True):
        raise ValueError("Unsupported beta lifecycle policy")
    approved = policy["expiry_approved"]
    if approved:
        expiry = policy.get("expires_at_utc")
        if not isinstance(expiry, str):
            raise ValueError("Approved beta requires an explicit UTC expiry")
        mode = "approved"
    elif policy.get("expires_at_utc") is not None:
        raise ValueError("Unapproved beta policy must leave the deadline unset")
    elif qualification_only:
        expiry, mode = QUALIFICATION_EXPIRY, "qualification-only"
    else:
        return {**base, "mode": "pending-approval"}
    return {**base, **build_metadata(root, source_sha, expiry, now),
            "mode": mode, "expiry_approved": approved}


def validate_metadata(metadata: dict, root: Path, source_sha: str, *,
                      publication: bool = False, now: datetime | None = None) -> None:
    mode = metadata.get("mode")
    expected = configure_metadata(root, source_sha, mode == "qualification-only", now)
    if metadata != expected:
        raise ValueError("Beta metadata differs from configured source identity/policy")
    if release_channel(root).startswith("beta."):
        if mode == "pending-approval":
            raise ValueError("Beta expiry is unset: no distributable package may be staged")
        if publication and (mode != "approved" or metadata.get("expiry_approved") is not True):
            raise ValueError("Qualification-only beta may not be published; owner expiry approval required")


def expiry_kst(metadata: dict) -> str:
    expiry = metadata["expires_at_utc"]
    if not expiry:
        return ""
    return parse_utc(expiry).astimezone(timezone(timedelta(hours=9))).strftime("%Y-%m-%d %H:%M:%S")


def notice(metadata: dict) -> str:
    expiry = metadata["expires_at_utc"] or "NOT SET / DEVELOPMENT ONLY"
    return (f"SpectralForge Chimera {metadata['version']}\n"
            f"Build: {metadata['build_id']} / {metadata['source_sha']}\n"
            f"Beta expiry (UTC): {expiry}\n"
            f"Beta expiry (KST / UTC+09:00): {expiry_kst(metadata) or 'NOT SET / DEVELOPMENT ONLY'}\n"
            f"Mode: {metadata['mode']}\n\n"
            "EN: An instance opened before expiry keeps processing until it is destroyed. "
            "New instances opened at or after expiry have muted output. Projects, presets, automation, "
            "A/B snapshots and embedded IRs can still be restored and saved. "
            "Use SETTINGS > Beta / Updates; offline use does not require GitHub. "
            "After correcting a mistaken system clock, use RECHECK CLOCK.\n\n"
            "DE: Eine vor Ablauf geöffnete Instanz verarbeitet bis zu ihrer Zerstörung weiter. "
            "Nach Ablauf neu geöffnete Instanzen geben keinen Ton aus. Projekte, Presets, Automation, "
            "A/B und eingebettete IRs bleiben ladbar und speicherbar. "
            "Updates: SETTINGS > Beta / Updates. Nach einer Uhrkorrektur RECHECK CLOCK wählen.\n\n"
            "KO: 만료 전에 연 인스턴스는 제거될 때까지 오디오 처리를 유지합니다. "
            "만료 후 새로 연 인스턴스의 출력은 음소거됩니다. 프로젝트·프리셋·자동화·A/B·내장 IR은 "
            "계속 불러오고 저장할 수 있습니다. SETTINGS > Beta / Updates에서 업데이트를 확인하세요. "
            "시스템 시각 오류를 수정한 뒤 RECHECK CLOCK을 누르세요. 오프라인 사용에 GitHub 연결은 필요하지 않습니다.\n")


def write_header(path: Path, metadata: dict[str, object]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(
        "#pragma once\n"
        f'#define CHIMERA_BETA_PACKAGE_VERSION "{metadata["version"]}"\n'
        f'#define CHIMERA_BETA_SOURCE_SHA "{metadata["source_sha"]}"\n'
        f'#define CHIMERA_BETA_EXPIRY_UTC "{metadata["expires_at_utc"]}"\n'
        f'#define CHIMERA_BETA_EXPIRY_KST "{expiry_kst(metadata)}"\n'
        f'#define CHIMERA_BETA_EXPIRY_UNIX {metadata["expires_at_unix"]}LL\n'
        f'#define CHIMERA_BETA_WARNING_LEAD_SECONDS {metadata["warning_lead_seconds"]}LL\n'
        f'#define CHIMERA_BETA_CONFIGURED {int(bool(metadata["expires_at_unix"]))}\n'
        f'#define CHIMERA_BETA_MODE "{metadata.get("mode", "qualification-only")}"\n',
        encoding="utf-8",
    )


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--source-sha", required=True)
    parser.add_argument("--expiry-utc")
    parser.add_argument("--configure", action="store_true")
    parser.add_argument("--qualification-only", action="store_true")
    parser.add_argument("--now-utc", help="Injected UTC packaging time for deterministic verification")
    parser.add_argument("--json", type=Path, required=True)
    parser.add_argument("--header", type=Path, required=True)
    args = parser.parse_args()
    now = parse_utc(args.now_utc) if args.now_utc else None
    if args.configure:
        if args.expiry_utc or args.now_utc:
            parser.error("Configured builds read the committed policy and real UTC clock")
        metadata = configure_metadata(args.root, args.source_sha, args.qualification_only)
    else:
        if not args.expiry_utc:
            parser.error("--expiry-utc is required for generator fixtures")
        metadata = build_metadata(args.root, args.source_sha, args.expiry_utc, now)
    args.json.parent.mkdir(parents=True, exist_ok=True)
    args.json.write_text(json.dumps(metadata, indent=2) + "\n", encoding="utf-8")
    write_header(args.header, metadata)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
