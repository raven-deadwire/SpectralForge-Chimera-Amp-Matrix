#!/usr/bin/env python3
"""Verify that A12/A13/I1 CI check IDs have unique, concrete producer ownership."""
from __future__ import annotations

import json
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
policy = json.loads((ROOT / "Validation/release-policy.json").read_text(encoding="utf-8"))
mapping = json.loads((ROOT / "Validation/ci-check-map.json").read_text(encoding="utf-8"))

assert mapping["schema"] == "spectralforge.chimera.ci-check-map"
assert mapping["schema_version"] == 1
assert mapping["policy_version"] == policy["policy_version"]

expected = set(policy["stages"]["A12"]["required_checks"] + policy["stages"]["A13"]["required_checks"] + policy["stages"]["I1"]["required_checks"])
actual = set(mapping["checks"])
assert actual == expected, f"CI check map mismatch: missing={sorted(expected-actual)}, extra={sorted(actual-expected)}"

owners = set()
for check_id, definition in mapping["checks"].items():
    for field in ("name", "producer", "platform", "artifact"):
        assert isinstance(definition.get(field), str) and definition[field], f"{check_id} missing {field}"
    owner = (definition["producer"], definition["platform"])
    assert owner not in owners, f"Duplicate CI producer ownership: {owner}"
    owners.add(owner)

print(f"PASS: {len(actual)} A12/A13/I1 checks have explicit CI producer and artifact ownership")
