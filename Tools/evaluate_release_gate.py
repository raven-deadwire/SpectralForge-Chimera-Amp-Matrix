#!/usr/bin/env python3
"""Compute PASS/BLOCKED/NOT_APPLICABLE checks and the Chimera release gate."""
from __future__ import annotations
import argparse
import json
from pathlib import Path
import sys
from datetime import datetime, timezone

ALLOWED_STATUS={"PASS","BLOCKED","NOT_APPLICABLE"}
ALLOWED_NA={"FORBIDDEN","ALLOWED_WITH_REASON"}

class ValidationError(RuntimeError):
    pass

def load_json(path: Path):
    try:
        return json.loads(path.read_text(encoding="utf-8-sig"))
    except Exception as exc:
        raise ValidationError(f"Cannot read JSON {path}: {exc}") from exc

def require(condition: bool, message: str):
    if not condition:
        raise ValidationError(message)

def validate_check(doc: dict, source: str):
    require(doc.get("schema")=="spectralforge.chimera.validation.check",f"{source}: wrong schema")
    require(doc.get("schema_version")==1,f"{source}: unsupported schema version")
    for key in ("id","stage","name","policy","dependencies","execution","evidence","applicability"):
        require(key in doc,f"{source}: missing {key}")
    require(isinstance(doc["id"],str) and doc["id"],f"{source}: invalid id")
    require(isinstance(doc["stage"],str) and doc["stage"],f"{source}: invalid stage")
    require(isinstance(doc["dependencies"],list),f"{source}: dependencies must be an array")
    policy=doc["policy"]
    require(isinstance(policy,dict),f"{source}: policy must be an object")
    for key in ("required","hard_gate","na_policy"):
        require(key in policy,f"{source}: policy missing {key}")
    require(policy["na_policy"] in ALLOWED_NA,f"{source}: invalid na_policy")
    require(isinstance(doc["execution"],dict) and isinstance(doc["execution"].get("executed"),bool),f"{source}: invalid execution")
    require(isinstance(doc["applicability"],dict) and isinstance(doc["applicability"].get("applicable"),bool),f"{source}: invalid applicability")

def scope_matches(check_scope: dict, waiver_scope: dict) -> bool:
    return all(check_scope.get(key)==value for key,value in waiver_scope.items())

def waiver_valid(check: dict, waivers: dict) -> tuple[bool,str|None]:
    waiver_id=check.get("applicability",{}).get("waiver_id")
    if not waiver_id:
        return False,None
    for waiver in waivers.get("waivers",[]):
        if waiver.get("id")!=waiver_id or waiver.get("check_id")!=check["id"] or waiver.get("approved") is not True:
            continue
        if not scope_matches(check.get("scope",{}),waiver.get("scope",{})):
            continue
        expires=waiver.get("expires")
        if expires:
            try:
                deadline=datetime.fromisoformat(expires.replace("Z","+00:00"))
                if deadline.tzinfo is None:
                    deadline=deadline.replace(tzinfo=timezone.utc)
                if deadline <= datetime.now(timezone.utc):
                    return False,waiver_id
            except ValueError:
                return False,waiver_id
        return True,waiver_id
    return False,waiver_id

def blocked(check: dict, reason_code: str, blocker_type: str, message: str, dependency_state="SATISFIED") -> dict:
    return {
        "id":check["id"],"stage":check["stage"],"name":check["name"],
        "computed_status":"BLOCKED","reason_code":reason_code,
        "dependency_state":dependency_state,
        "applicability_state":"APPLICABLE" if check["applicability"]["applicable"] else "NOT_APPLICABLE_REQUESTED",
        "blocker":{"type":blocker_type,"reason":message}
    }

def evaluate_checks(checks: dict[str,dict], policy: dict, waivers: dict, current_commit: str) -> dict[str,dict]:
    active_policy=policy["policy_version"]
    central_hard=set(policy.get("hard_gates",[]))
    results={}
    visiting=set()

    def evaluate(check_id: str) -> dict:
        if check_id in results:
            return results[check_id]
        if check_id in visiting:
            raise ValidationError(f"Dependency cycle contains {check_id}")
        check=checks.get(check_id)
        if check is None:
            synthetic={
                "id":check_id,"stage":"UNKNOWN","name":"Missing validation check",
                "applicability":{"applicable":True}
            }
            result=blocked(synthetic,"MISSING_CHECK","ARTIFACT_MISSING",f"Required dependency check {check_id} is missing","MISSING")
            results[check_id]=result
            return result
        visiting.add(check_id)

        dependency_failures=[]
        for dependency in check["dependencies"]:
            dep=evaluate(dependency)
            if dep["computed_status"] not in ("PASS","NOT_APPLICABLE"):
                dependency_failures.append(dependency)
        if dependency_failures:
            result=blocked(check,"DEPENDENCY_BLOCKED","DEPENDENCY",
                           "Dependencies not satisfied: "+", ".join(dependency_failures),"BLOCKED")
            visiting.remove(check_id);results[check_id]=result;return result

        applicable=check["applicability"]["applicable"]
        if not applicable:
            if check_id in central_hard or check["policy"]["hard_gate"] or check["policy"]["na_policy"]=="FORBIDDEN":
                result=blocked(check,"NA_FORBIDDEN","DEPENDENCY","NOT_APPLICABLE is forbidden for this check")
            else:
                approved,waiver_id=waiver_valid(check,waivers)
                if approved:
                    result={
                        "id":check["id"],"stage":check["stage"],"name":check["name"],
                        "computed_status":"NOT_APPLICABLE","reason_code":"APPROVED_WAIVER",
                        "dependency_state":"SATISFIED","applicability_state":"WAIVED",
                        "waiver_id":waiver_id,"blocker":None
                    }
                else:
                    result=blocked(check,"WAIVER_REQUIRED","DEPENDENCY",
                                   "NOT_APPLICABLE requires an approved, in-scope, unexpired waiver")
            visiting.remove(check_id);results[check_id]=result;return result

        execution=check["execution"]
        if not execution["executed"]:
            result=blocked(check,"NOT_EXECUTED","NOT_EXECUTED","Required validation was not executed")
            visiting.remove(check_id);results[check_id]=result;return result

        evidence=check.get("evidence",{})
        requires_commit=check["policy"].get("requires_current_commit",True)
        if requires_commit and evidence.get("commit_sha")!=current_commit:
            result=blocked(check,"STALE_COMMIT","STALE_EVIDENCE",
                           f"Evidence commit {evidence.get('commit_sha')} does not match {current_commit}")
            visiting.remove(check_id);results[check_id]=result;return result
        if check["policy"].get("requires_policy_version",True) and evidence.get("policy_version")!=active_policy:
            result=blocked(check,"STALE_POLICY","STALE_EVIDENCE",
                           f"Evidence policy {evidence.get('policy_version')} does not match {active_policy}")
            visiting.remove(check_id);results[check_id]=result;return result
        fixture_expected=check["policy"].get("fixture_sha256")
        if fixture_expected and evidence.get("fixture_sha256")!=fixture_expected:
            result=blocked(check,"STALE_FIXTURE","STALE_EVIDENCE","Fixture SHA-256 does not match active policy")
            visiting.remove(check_id);results[check_id]=result;return result

        exit_code=execution.get("exit_code")
        if exit_code not in (None,0):
            failure=check.get("failure") or {}
            result=blocked(check,"EXECUTION_FAILED",failure.get("type","TEST_FAILURE"),
                           failure.get("message",f"Validation exited with code {exit_code}"))
            visiting.remove(check_id);results[check_id]=result;return result
        assertion=check.get("assertion")
        if assertion is not None and assertion.get("passed") is not True:
            failure=check.get("failure") or {}
            result=blocked(check,"ASSERTION_FAILED",failure.get("type","TEST_FAILURE"),
                           failure.get("message","Validation assertion failed"))
            visiting.remove(check_id);results[check_id]=result;return result

        result={
            "id":check["id"],"stage":check["stage"],"name":check["name"],
            "computed_status":"PASS","reason_code":"VERIFIED",
            "dependency_state":"SATISFIED","applicability_state":"APPLICABLE",
            "blocker":None
        }
        visiting.remove(check_id);results[check_id]=result;return result

    for check_id in sorted(checks):
        evaluate(check_id)
    return results

def evaluate_release(policy: dict, waivers: dict, checks: dict[str,dict], profile_name: str, current_commit: str) -> dict:
    require(policy.get("schema")=="spectralforge.chimera.release-policy","Wrong release policy schema")
    require(policy.get("schema_version")==1,"Unsupported release policy schema version")
    profile=policy.get("profiles",{}).get(profile_name)
    require(profile is not None,f"Unknown release profile: {profile_name}")
    required_stages=profile.get("required_stages",[])
    results=evaluate_checks(checks,policy,waivers,current_commit)

    stage_results={}
    check_stage={}
    for stage_id,stage_policy in policy.get("stages",{}).items():
        for check_id in stage_policy.get("required_checks",[]):
            if check_id in check_stage and check_stage[check_id]!=stage_id:
                raise ValidationError(f"Required check {check_id} belongs to multiple stages")
            check_stage[check_id]=stage_id

    for stage_id in required_stages:
        stage_policy=policy["stages"].get(stage_id,{})
        expected=list(stage_policy.get("required_checks",[]))
        if not expected:
            expected=[cid for cid,doc in checks.items() if doc.get("stage")==stage_id and doc.get("policy",{}).get("required")]
        members=[r for cid,r in results.items() if checks.get(cid,{}).get("stage")==stage_id]
        missing=[cid for cid in expected if cid not in checks]
        required_members=[results[cid] for cid in expected if cid in results]
        blockers=[r for r in required_members if r["computed_status"]=="BLOCKED"]
        blocker_rows=[{"id":r["id"],**(r.get("blocker") or {})} for r in blockers]
        blocker_rows += [{"id":cid,"type":"ARTIFACT_MISSING","reason":"Required validation check report is missing"} for cid in missing]

        required_statuses=[r["computed_status"] for r in required_members]
        all_na=bool(required_statuses) and not missing and all(x=="NOT_APPLICABLE" for x in required_statuses)
        allow_stage_na=stage_policy.get("allow_not_applicable",False)
        if missing or blockers:
            status="BLOCKED"
        elif not expected:
            status="BLOCKED"
            blocker_rows=[{"id":stage_id,"type":"ARTIFACT_MISSING","reason":"Required stage has no policy-owned required checks"}]
        elif all_na and not allow_stage_na:
            status="BLOCKED"
            blocker_rows=[{"id":stage_id,"type":"DEPENDENCY","reason":"Required stage cannot be satisfied entirely by NOT_APPLICABLE checks"}]
        elif all_na:
            status="NOT_APPLICABLE"
        else:
            status="PASS"

        statuses=[r["computed_status"] for r in members]
        stage_results[stage_id]={
            "id":stage_id,"name":stage_policy.get("name",stage_id),
            "computed_status":status,
            "counts":{"total":len(expected),"pass":sum(results[c]["computed_status"]=="PASS" for c in expected if c in results),
                      "blocked":len(blocker_rows),"not_applicable":sum(results[c]["computed_status"]=="NOT_APPLICABLE" for c in expected if c in results)},
            "reported_counts":{"total":len(members),"pass":statuses.count("PASS"),"blocked":statuses.count("BLOCKED"),"not_applicable":statuses.count("NOT_APPLICABLE")},
            "blockers":blocker_rows
        }

    active_hard=[]
    hard_blocked=[]
    for hard_id in policy.get("hard_gates",[]):
        stage_id=check_stage.get(hard_id,checks.get(hard_id,{}).get("stage"))
        if stage_id not in required_stages:
            continue
        if hard_id not in results:
            row={"id":hard_id,"stage":stage_id,"name":hard_id,"computed_status":"BLOCKED",
                 "reason_code":"MISSING_CHECK","dependency_state":"MISSING","applicability_state":"APPLICABLE",
                 "blocker":{"type":"ARTIFACT_MISSING","reason":"Hard-gate validation report is missing"}}
        else:
            row=results[hard_id]
        active_hard.append(row)
        if row["computed_status"]!="PASS":
            hard_blocked.append(row)
    if not policy.get("hard_gates"):
        active_hard=[r for cid,r in results.items()
                     if checks.get(cid,{}).get("stage") in required_stages and checks.get(cid,{}).get("policy",{}).get("hard_gate")]
        hard_blocked=[r for r in active_hard if r["computed_status"]!="PASS"]
    blocked_stages=[s for s in stage_results.values() if s["computed_status"]=="BLOCKED"]
    verdict="PASS" if not blocked_stages and not hard_blocked else "BLOCKED"

    counts={"total":len([r for cid,r in results.items() if checks.get(cid,{}).get("stage") in required_stages]),
            "pass":0,"blocked":0,"not_applicable":0}
    for cid,r in results.items():
        if checks.get(cid,{}).get("stage") in required_stages:
            key=r["computed_status"].lower()
            counts[key]+=1
    return {
        "schema":"spectralforge.chimera.release-gate","schema_version":1,
        "policy_version":policy["policy_version"],"profile":profile_name,
        "revision":{"commit_sha":current_commit},"verdict":verdict,"ready":verdict=="PASS",
        "counts":{"checks":counts,"stages":{
            "total":len(stage_results),
            "pass":sum(s["computed_status"]=="PASS" for s in stage_results.values()),
            "blocked":sum(s["computed_status"]=="BLOCKED" for s in stage_results.values()),
            "not_applicable":sum(s["computed_status"]=="NOT_APPLICABLE" for s in stage_results.values())}},
        "hard_gates":{"total":len(active_hard),"pass":sum(r["computed_status"]=="PASS" for r in active_hard),"blocked":len(hard_blocked)},
        "checks":[results[k] for k in sorted(results) if checks.get(k,{}).get("stage") in required_stages],
        "stages":[stage_results[k] for k in required_stages],
        "blockers":[{"id":r["id"],**(r.get("blocker") or {})} for r in hard_blocked]
            +[b for s in blocked_stages for b in s["blockers"] if b.get("id") not in {r["id"] for r in hard_blocked}]
    }

def render_summary(result: dict) -> str:
    lines=[f"# Chimera release gate: {result['verdict']}",
           "",f"- Profile: \`{result['profile']}\`",f"- Commit: \`{result['revision']['commit_sha']}\`",
           f"- Checks: {result['counts']['checks']['pass']} PASS / {result['counts']['checks']['blocked']} BLOCKED / {result['counts']['checks']['not_applicable']} NOT_APPLICABLE",
           f"- Hard gates: {result['hard_gates']['pass']} / {result['hard_gates']['total']} PASS","",
           "| Stage | Status | PASS | BLOCKED | N/A |","|---|---|---:|---:|---:|"]
    for stage in result["stages"]:
        c=stage["counts"];lines.append(f"| {stage['id']} {stage['name']} | {stage['computed_status']} | {c['pass']} | {c['blocked']} | {c['not_applicable']} |")
    if result["blockers"]:
        lines+=["","## Blocking path"]
        seen=set()
        for item in result["blockers"]:
            if item["id"] in seen: continue
            seen.add(item["id"]);lines.append(f"- \`{item['id']}\` — {item.get('type','BLOCKED')}: {item.get('reason','Blocked')}")
    return "\n".join(lines)+"\n"

def read_checks(folder: Path) -> dict[str,dict]:
    checks={}
    for path in sorted(folder.glob("*.json")):
        doc=load_json(path);validate_check(doc,str(path))
        if doc["id"] in checks:
            raise ValidationError(f"Duplicate check id {doc['id']}")
        checks[doc["id"]]=doc
    return checks

def main() -> int:
    parser=argparse.ArgumentParser()
    parser.add_argument("--policy",type=Path,required=True)
    parser.add_argument("--waivers",type=Path,required=True)
    parser.add_argument("--profile",required=True)
    parser.add_argument("--commit",required=True)
    parser.add_argument("--checks",type=Path,required=True)
    parser.add_argument("--output",type=Path,required=True)
    parser.add_argument("--summary",type=Path)
    args=parser.parse_args()
    try:
        policy=load_json(args.policy);waivers=load_json(args.waivers);checks=read_checks(args.checks)
        result=evaluate_release(policy,waivers,checks,args.profile,args.commit)
        args.output.parent.mkdir(parents=True,exist_ok=True)
        args.output.write_text(json.dumps(result,indent=2)+"\n",encoding="utf-8")
        summary=render_summary(result)
        if args.summary:
            args.summary.parent.mkdir(parents=True,exist_ok=True);args.summary.write_text(summary,encoding="utf-8")
        print(summary,end="")
        return 0 if result["verdict"]=="PASS" else 1
    except ValidationError as exc:
        print(f"VALIDATION INFRASTRUCTURE ERROR: {exc}",file=sys.stderr)
        return 2

if __name__=="__main__":
    raise SystemExit(main())
