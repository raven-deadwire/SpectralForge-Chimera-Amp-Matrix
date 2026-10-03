#!/usr/bin/env python3
"""Self-test for the policy-owned validation check producer."""
from __future__ import annotations
import argparse
import importlib.util
import json
from pathlib import Path
import tempfile

ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location("producer",ROOT/"Tools/produce_validation_check.py")
producer=importlib.util.module_from_spec(spec)
assert spec.loader
spec.loader.exec_module(producer)
policy=json.loads((ROOT/"Validation/release-policy.json").read_text(encoding="utf-8"))

def args(**overrides):
    base=dict(
        check_id="A2.01",name="Signature definition fixture",commit="a"*40,
        not_applicable=False,not_executed=False,exit_code=0,assertion="pass",
        depends_on=None,artifact=["fixture.json"],fixture_sha256=None,scope_json=None,
        waiver_id=None,failure_type=None,failure_message=None,
    )
    base.update(overrides)
    return argparse.Namespace(**base)

report=producer.build_report(policy,args())
assert report["stage"]=="A2"
assert report["policy"]["required"] is True
assert report["policy"]["hard_gate"] is False
assert report["policy"]["na_policy"]=="ALLOWED_WITH_REASON"
assert report["evidence"]["policy_version"]==policy["policy_version"]
assert report["assertion"]["passed"] is True

hard=producer.build_report(policy,args(check_id="A1.04",name="Hard gate"))
assert hard["stage"]=="A1"
assert hard["policy"]["hard_gate"] is True
assert hard["policy"]["na_policy"]=="FORBIDDEN"

try:
    producer.build_report(policy,args(check_id="A1.04",not_applicable=True,waiver_id=None))
    raise AssertionError("missing waiver id accepted")
except ValueError:
    pass

try:
    producer.build_report(policy,args(check_id="A2.01",not_executed=True,assertion="pass"))
    raise AssertionError("not-executed assertion accepted")
except ValueError:
    pass

print("PASS: validation producer derives stage/required/hard-gate policy and refuses invented execution state")
