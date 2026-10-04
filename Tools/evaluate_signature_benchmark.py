#!/usr/bin/env python3
"""Evaluate three Deadwire Signature renders against frozen relative master targets."""
from __future__ import annotations
import argparse,json,math,sys
from pathlib import Path
from itertools import combinations

SONGS=("crom","wild","azhi")

def clamp01(x): return max(0.0,min(1.0,x))
def load(path): return json.loads(Path(path).read_text(encoding="utf-8-sig"))
def finite(x): return isinstance(x,(int,float)) and math.isfinite(float(x))

def evaluate(policy,renders):
    if renders.get("schema")!="spectralforge.chimera.signature-renders" or renders.get("schema_version")!=1:
        raise ValueError("Unsupported signature render schema")
    if renders.get("dsp_valid") is not True:
        return {"schema":"spectralforge.chimera.signature-benchmark","schema_version":1,"verdict":"INVALID","reasons":["DSP validity gate failed"]}
    values=renders.get("renders",{})
    for song in SONGS:
        if song not in values: raise ValueError(f"Missing render {song}")
        for metric in policy["metrics"]:
            if not finite(values[song].get(metric)): raise ValueError(f"Missing/non-finite {song}.{metric}")

    metrics={}
    weighted=0.0
    sw=policy["score"]
    for metric,spec in policy["metrics"].items():
        ref=[float(policy["reference"][s][metric]) for s in SONGS]
        ren=[float(values[s][metric]) for s in SONGS]
        raw=[r-o for r,o in zip(ren,ref)]
        bias=sum(raw)/3.0
        centered=[x-bias for x in raw]
        rmse=math.sqrt(sum(x*x for x in centered)/3.0)
        valid=[]; passed=0; gap_errors=[]
        for a,b in combinations(range(3),2):
            og=ref[a]-ref[b]
            if abs(og)<=spec["tie"]: continue
            rg=ren[a]-ren[b]
            order_ok=(og>0 and rg>=spec["min_render_gap"]) or (og<0 and rg<=-spec["min_render_gap"])
            valid.append({"a":SONGS[a],"b":SONGS[b],"reference_gap":og,"render_gap":rg,"gap_error":rg-og,"order_pass":order_ok})
            passed+=int(order_ok);gap_errors.append(abs(rg-og))
        order_score=passed/len(valid) if valid else 1.0
        gap_mae=sum(gap_errors)/len(gap_errors) if gap_errors else 0.0
        shape_score=clamp01(1.0-rmse/spec["shape_fail"])
        gap_score=clamp01(1.0-gap_mae/spec["gap_fail"])
        combined=sw["shape_weight"]*shape_score+sw["order_weight"]*order_score+sw["gap_weight"]*gap_score
        metrics[metric]={
            "reference":dict(zip(SONGS,ref)),"render":dict(zip(SONGS,ren)),
            "raw_delta":dict(zip(SONGS,raw)),"common_bias":bias,"centered_delta":dict(zip(SONGS,centered)),
            "shape_rmse":rmse,"shape_score":shape_score,"pairs":valid,
            "order_score":order_score,"gap_mae":gap_mae,"gap_score":gap_score,"score":combined
        }
        weighted+=combined*spec["weight"]

    c,w,a=(values[s] for s in SONGS)
    hard={
      "H1": a["rms_80_200_db"]>c["rms_80_200_db"]>w["rms_80_200_db"]
            and a["rms_80_200_db"]-c["rms_80_200_db"]>=0.7
            and c["rms_80_200_db"]-w["rms_80_200_db"]>=0.4,
      "H2": a["side_80_200_pct"]<=min(c["side_80_200_pct"],w["side_80_200_pct"])-3.0,
      "H3": w["side_4000_8000_pct"]>c["side_4000_8000_pct"]>a["side_4000_8000_pct"]
            and w["side_4000_8000_pct"]-c["side_4000_8000_pct"]>=1.5
            and c["side_4000_8000_pct"]-a["side_4000_8000_pct"]>=1.0,
      "H4": w["crest_db"]>=max(c["crest_db"],a["crest_db"])+0.25
    }
    secondary={
      "S1":w["rms_1500_4000_db"]>max(c["rms_1500_4000_db"],a["rms_1500_4000_db"]),
      "S2":a["side_1500_4000_pct"]>max(c["side_1500_4000_pct"],w["side_1500_4000_pct"]),
      "S3":a["dr_80_200_db"]<min(c["dr_80_200_db"],w["dr_80_200_db"]),
      "S4":w["dr_1500_4000_db"]>max(c["dr_1500_4000_db"],a["dr_1500_4000_db"])
    }
    overall=weighted*100.0
    critical=[name for name,spec in policy["metrics"].items() if spec.get("critical") and metrics[name]["score"]<sw["critical_metric_min"]]
    reasons=[]
    for name,passed in hard.items():
        if not passed: reasons.append(f"{name} hard gate failed")
    secondary_count=sum(secondary.values())
    if secondary_count<policy["secondary_required"]: reasons.append(f"Secondary gates {secondary_count}/4; need {policy['secondary_required']}")
    if overall<sw["overall_pass"]: reasons.append(f"Overall score {overall:.2f} < {sw['overall_pass']:.2f}")
    if critical: reasons.append("Critical metric score below threshold: "+", ".join(critical))
    verdict="PASS" if not reasons else "REVISE"
    return {
      "schema":"spectralforge.chimera.signature-benchmark","schema_version":1,
      "verdict":verdict,"overall_score":overall,"metrics":metrics,
      "hard_gates":hard,"secondary_gates":secondary,"secondary_passed":secondary_count,
      "critical_below_threshold":critical,"reasons":reasons,
      "tuning_targets":[r for r in reasons]
    }

def main():
    p=argparse.ArgumentParser();p.add_argument("--policy",required=True);p.add_argument("--renders",required=True);p.add_argument("--output",required=True)
    a=p.parse_args()
    try:
        result=evaluate(load(a.policy),load(a.renders))
        Path(a.output).parent.mkdir(parents=True,exist_ok=True)
        Path(a.output).write_text(json.dumps(result,indent=2)+"\n",encoding="utf-8")
        print(json.dumps({"verdict":result["verdict"],"overall_score":result.get("overall_score"),"reasons":result.get("reasons",[])},indent=2))
        return 0 if result["verdict"]=="PASS" else 1
    except Exception as exc:
        print(f"SIGNATURE BENCHMARK INFRASTRUCTURE ERROR: {exc}",file=sys.stderr);return 2
if __name__=="__main__": raise SystemExit(main())
