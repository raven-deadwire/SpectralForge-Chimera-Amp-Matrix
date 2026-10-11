#!/usr/bin/env python3
import json
from pathlib import Path
import runpy
import subprocess
import sys
from tempfile import TemporaryDirectory
from unittest.mock import patch
import produce_ctest_validation as producer
from produce_ctest_validation import test_results

with TemporaryDirectory() as directory:
    root=Path(directory); junit=root/"ctest.xml"; inventory=root/"inventory.json"
    inventory.write_text(json.dumps({"tests":[{"name":"One"},{"name":"Two"}]}))
    junit.write_text('<testsuite><testcase name="One" status="run"/><testcase name="Two" status="run"/></testsuite>')
    assert test_results(junit,inventory)[1]
    for case in ['<testcase name="Two" status="notrun"/>', '<testcase name="Two" status="run"><failure/></testcase>',
                 '<testcase name="Two" status="run"><skipped/></testcase>', '', '<testcase name="Unexpected" status="run"/>']:
        junit.write_text('<testsuite><testcase name="One" status="run"/>'+case+'</testsuite>')
        assert not test_results(junit,inventory)[1], "Incomplete/skipped/failed/extra matrix certified"
    junit.write_text('<testsuite><testcase name="One" status="run"/><testcase name="One" status="run"/></testsuite>')
    try:test_results(junit,inventory)
    except ValueError:pass
    else:raise AssertionError("Duplicate case certified")
print("PASS CTest producer refuses skipped, failed, missing, duplicate and unconfigured tests")

# Exercise the real policy consumers with the legacy Windows default encoding.
# A UTF-8 BOM and Korean owner text must survive without changing gate outcomes.
with TemporaryDirectory() as directory:
    root = Path(directory)
    policy_path = producer.ROOT / "Validation/release-policy.json"
    policy = json.loads(policy_path.read_text(encoding="utf-8-sig"))
    policy["release_scope"]["encoding_probe"] = "소유자 승인"
    fixture = root / "policy.json"
    fixture.write_text(json.dumps(policy, ensure_ascii=False), encoding="utf-8-sig")
    read_text = Path.read_text

    def windows_read_text(path, encoding=None, errors=None):
        return read_text(fixture if path == policy_path else path,
                         encoding=encoding or "cp1252", errors=errors)

    try:
        read_text(fixture, encoding="cp1252")
    except UnicodeDecodeError:
        pass
    else:
        raise AssertionError("Encoding regression fixture does not expose cp1252 failure")
    junit = root / "ctest.xml"
    inventory = root / "inventory.json"
    junit.write_text('<testsuite><testcase name="EncodingFixture" status="run"/></testsuite>', encoding="utf-8")
    inventory.write_text(json.dumps({"tests": [{"name": "EncodingFixture"}]}), encoding="utf-8")
    commit = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=producer.ROOT, text=True).strip()
    output = root / "validation"
    argv = ["produce_ctest_validation.py", "--commit", commit, "--platform", "windows",
            "--junit", str(junit), "--inventory", str(inventory), "--output", str(output)]
    with patch.object(Path, "read_text", windows_read_text):
        runpy.run_path(str(producer.ROOT / "Tools/test_release_scope.py"), run_name="__main__")
        with patch.object(sys, "argv", argv):
            assert producer.main() == 0
    result = json.loads((output / "release-gate.json").read_text(encoding="utf-8"))
    assert result["verdict"] == "BLOCKED" and not result["ready"]
print("PASS UTF-8 owner policy works under simulated cp1252; synthetic fixture keeps release BLOCKED")
