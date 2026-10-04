#!/usr/bin/env python3
import json
from pathlib import Path
from tempfile import TemporaryDirectory
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
