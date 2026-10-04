#!/usr/bin/env python3
"""Regression tests for the PASS/BLOCKED/NOT_APPLICABLE release gate."""
from __future__ import annotations
import importlib.util
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location("gate",ROOT/"Tools/evaluate_release_gate.py")
gate=importlib.util.module_from_spec(spec);spec.loader.exec_module(gate)

COMMIT="0123456789abcdef"
POLICY={
    "schema":"spectralforge.chimera.release-policy","schema_version":1,"policy_version":1,
    "stages":{"A":{"name":"Unit stage","required":True}},
    "profiles":{"unit":{"required_stages":["A"]}}
}
WAIVERS={"waivers":[{"id":"W1","check_id":"A.2","scope":{"platform":"windows"},"reason":"not produced","approved":True,"expires":None}]}

def check(cid,*,hard=False,required=True,applicable=True,waiver=None,executed=True,exit_code=0,passed=True,commit=COMMIT,deps=None,na="FORBIDDEN"):
    return {
        "schema":"spectralforge.chimera.validation.check","schema_version":1,
        "id":cid,"stage":"A","name":cid,"scope":{"platform":"windows"},
        "policy":{"required":required,"hard_gate":hard,"na_policy":na},
        "dependencies":deps or [],
        "assertion":{"passed":passed},
        "execution":{"executed":executed,"exit_code":exit_code},
        "evidence":{"commit_sha":commit,"policy_version":1,"fixture_sha256":None},
        "applicability":{"applicable":applicable,"waiver_id":waiver},
        "failure":{"type":"TEST_FAILURE","message":"fixture failure"}
    }

def require(condition,message):
    if not condition: raise RuntimeError(message)

def run():
    good={"A.1":check("A.1",hard=True)}
    result=gate.evaluate_release(POLICY,WAIVERS,good,"unit",COMMIT)
    require(result["verdict"]=="PASS","verified hard gate must pass")

    waived={"A.1":check("A.1",hard=True),"A.2":check("A.2",applicable=False,waiver="W1",na="ALLOWED_WITH_REASON")}
    result=gate.evaluate_release(POLICY,WAIVERS,waived,"unit",COMMIT)
    require(result["verdict"]=="PASS" and any(c["computed_status"]=="NOT_APPLICABLE" for c in result["checks"]),"approved N/A must satisfy a non-hard check")

    hard_na={"A.1":check("A.1",hard=True,applicable=False,waiver="W1",na="ALLOWED_WITH_REASON")}
    result=gate.evaluate_release(POLICY,WAIVERS,hard_na,"unit",COMMIT)
    require(result["verdict"]=="BLOCKED" and result["hard_gates"]["blocked"]==1,"hard gate N/A must be blocked")

    stale={"A.1":check("A.1",hard=True,commit="old")}
    result=gate.evaluate_release(POLICY,WAIVERS,stale,"unit",COMMIT)
    require(result["verdict"]=="BLOCKED" and result["checks"][0]["reason_code"]=="STALE_COMMIT","stale PASS evidence must be blocked")

    failed={"A.1":check("A.1",hard=True,passed=False)}
    result=gate.evaluate_release(POLICY,WAIVERS,failed,"unit",COMMIT)
    require(result["verdict"]=="BLOCKED" and result["checks"][0]["reason_code"]=="ASSERTION_FAILED","failed assertion must be blocked")

    dependencies={"A.1":check("A.1",passed=False),"A.3":check("A.3",deps=["A.1"])}
    result=gate.evaluate_release(POLICY,WAIVERS,dependencies,"unit",COMMIT)
    child=next(c for c in result["checks"] if c["id"]=="A.3")
    require(child["computed_status"]=="BLOCKED" and child["reason_code"]=="DEPENDENCY_BLOCKED","blockers must propagate downstream")

    all_na={"A.2":check("A.2",applicable=False,waiver="W1",na="ALLOWED_WITH_REASON")}
    result=gate.evaluate_release(POLICY,WAIVERS,all_na,"unit",COMMIT)
    require(result["verdict"]=="BLOCKED" and result["stages"][0]["computed_status"]=="BLOCKED","required stage must not be satisfied entirely by N/A")

    optional_failure={"A.1":check("A.1",hard=True),"A.optional":check("A.optional",required=False,passed=False)}
    result=gate.evaluate_release(POLICY,WAIVERS,optional_failure,"unit",COMMIT)
    require(result["verdict"]=="PASS","optional failure must remain visible without blocking a required stage")

    missing_stage=gate.evaluate_release(POLICY,WAIVERS,{},"unit",COMMIT)
    require(missing_stage["verdict"]=="BLOCKED" and missing_stage["stages"][0]["counts"]["total"]==0,"missing reports must never become N/A")

    print("PASS: release gate rejects hard-gate N/A, all-N/A required stages, stale evidence, failed required assertions, dependency gaps and missing reports; optional failures do not silently become required.")

if __name__=="__main__":
    run()
