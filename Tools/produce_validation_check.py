#!/usr/bin/env python3
"""Produce one policy-owned Chimera validation check report without inventing PASS evidence."""
from __future__ import annotations
import argparse
import json
from pathlib import Path


def load(path: Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8-sig"))


def locate_check(policy: dict, check_id: str) -> tuple[str, dict]:
    found=[]
    for stage_id, stage in policy.get("stages", {}).items():
        if check_id in stage.get("required_checks", []):
            found.append((stage_id, stage))
    if len(found) != 1:
        raise ValueError(f"{check_id} must belong to exactly one policy stage, found {len(found)}")
    return found[0]


def build_report(policy: dict, args: argparse.Namespace) -> dict:
    stage_id, stage = locate_check(policy, args.check_id)
    hard = args.check_id in set(policy.get("hard_gates", []))
    applicable = not args.not_applicable
    executed = not args.not_executed
    assertion = None if args.assertion is None else {"passed": args.assertion == "pass"}
    if not executed and assertion is not None:
        raise ValueError("not-executed checks cannot carry an assertion")
    if not applicable and not args.waiver_id:
        raise ValueError("not-applicable checks require --waiver-id")
    evidence={
        "commit_sha": args.commit,
        "policy_version": policy["policy_version"],
    }
    if args.fixture_sha256:
        evidence["fixture_sha256"]=args.fixture_sha256
    if args.artifact:
        evidence["artifacts"]=args.artifact
    report={
        "schema":"spectralforge.chimera.validation.check",
        "schema_version":1,
        "id":args.check_id,
        "stage":stage_id,
        "name":args.name or args.check_id,
        "scope":json.loads(args.scope_json) if args.scope_json else {},
        "policy":{
            "required":args.check_id in stage.get("required_checks", []),
            "hard_gate":hard,
            "na_policy":"FORBIDDEN" if hard else "ALLOWED_WITH_REASON",
            "requires_current_commit":True,
            "requires_policy_version":True,
        },
        "dependencies":args.depends_on or [],
        "execution":{"executed":executed,"exit_code":args.exit_code if executed else None},
        "evidence":evidence,
        "applicability":{"applicable":applicable,"waiver_id":args.waiver_id},
        "assertion":assertion,
        "failure":None,
    }
    if args.failure_type or args.failure_message:
        report["failure"]={
            "type":args.failure_type or "TEST_FAILURE",
            "message":args.failure_message or "Validation failed",
        }
    return report


def main() -> int:
    parser=argparse.ArgumentParser()
    parser.add_argument("--policy",type=Path,default=Path("Validation/release-policy.json"))
    parser.add_argument("--check-id",required=True)
    parser.add_argument("--name")
    parser.add_argument("--commit",required=True)
    parser.add_argument("--output",type=Path,required=True)
    parser.add_argument("--depends-on",action="append")
    parser.add_argument("--artifact",action="append")
    parser.add_argument("--fixture-sha256")
    parser.add_argument("--scope-json")
    parser.add_argument("--waiver-id")
    parser.add_argument("--not-applicable",action="store_true")
    parser.add_argument("--not-executed",action="store_true")
    parser.add_argument("--exit-code",type=int,default=0)
    parser.add_argument("--assertion",choices=("pass","fail"))
    parser.add_argument("--failure-type")
    parser.add_argument("--failure-message")
    args=parser.parse_args()
    try:
        policy=load(args.policy)
        report=build_report(policy,args)
        args.output.parent.mkdir(parents=True,exist_ok=True)
        args.output.write_text(json.dumps(report,indent=2)+"\n",encoding="utf-8")
        print(f"WROTE {report['id']} {report['stage']} executed={report['execution']['executed']} assertion={report['assertion']}")
        return 0
    except (OSError, ValueError, json.JSONDecodeError) as exc:
        print(f"VALIDATION PRODUCER ERROR: {exc}")
        return 2


if __name__=="__main__":
    raise SystemExit(main())
