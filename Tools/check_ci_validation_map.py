#!/usr/bin/env python3
"""Verify the reviewed automatic allowlist and concrete producer ownership."""
from __future__ import annotations

import json
import re
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
policy = json.loads((ROOT / "Validation/release-policy.json").read_text(encoding="utf-8"))
mapping = json.loads((ROOT / "Validation/ci-check-map.json").read_text(encoding="utf-8"))

assert mapping["schema"] == "spectralforge.chimera.ci-check-map"
assert mapping["schema_version"] == 1
assert mapping["policy_version"] == policy["policy_version"]

expected = set(policy["stages"]["A12"]["required_checks"] + policy["stages"]["A13"]["required_checks"] + policy["stages"]["I1"]["required_checks"])
# Deliberate allowlist: adding a map row cannot promote an undefined A check,
# same-DI, listening, reference acceptance or commercial host acceptance.
expected |= {"E1.DSP", "E1.STATE_UI", "E1.LEGACY"}
actual = set(mapping["checks"])
assert actual == expected, f"CI check map mismatch: missing={sorted(expected-actual)}, extra={sorted(actual-expected)}"

owners = set()
for check_id, definition in mapping["checks"].items():
    for field in ("name", "producer", "platform", "artifact"):
        assert isinstance(definition.get(field), str) and definition[field], f"{check_id} missing {field}"
    owner = (definition["producer"], definition["platform"])
    assert owner not in owners, f"Duplicate CI producer ownership: {owner}"
    owners.add(owner)
    if check_id.startswith("E1."):
        assert definition["producer"] == "ctest-contract:" + check_id
        assert definition["platform"] == ("windows" if check_id == "E1.STATE_UI" else "all")
        assert definition.get("definition_source") == "docs/E670FE_IMPLEMENTATION.md#compatibility"
        assert isinstance(definition.get("tests"), dict) and definition["tests"]
        for tests in (definition["tests"], definition.get("windows_tests", {})):
            for name, patterns in tests.items():
                assert isinstance(name, str) and name.startswith("Chimera")
                assert isinstance(patterns, list) and patterns
                for pattern in patterns:
                    assert isinstance(pattern, str) and pattern and not re.fullmatch(pattern, "")
        if check_id == "E1.LEGACY":
            assert "ChimeraUITests" in definition.get("windows_tests", {})

print(f"PASS: {len(actual)} A12/A13/I1/E1 checks have explicit CI producer and artifact ownership")
