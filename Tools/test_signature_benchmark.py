#!/usr/bin/env python3
from __future__ import annotations
import importlib.util,json
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location("sigbench",ROOT/"Tools/evaluate_signature_benchmark.py")
m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
policy=json.loads((ROOT/"Validation/signature-benchmark-policy.json").read_text())
def require(x,msg):
    if not x: raise RuntimeError(msg)
perfect={"schema":"spectralforge.chimera.signature-renders","schema_version":1,"dsp_valid":True,"renders":policy["reference"]}
r=m.evaluate(policy,perfect)
require(r["verdict"]=="PASS" and abs(r["overall_score"]-100)<1e-6,"reference-shaped fixture must PASS")
bad=json.loads(json.dumps(perfect))
bad["renders"]["wild"]["crest_db"]=9.0
bad["renders"]["wild"]["side_4000_8000_pct"]=38.0
r=m.evaluate(policy,bad)
require(r["verdict"]=="REVISE" and not r["hard_gates"]["H3"] and not r["hard_gates"]["H4"],"hard gate failures must force REVISE")
invalid={"schema":"spectralforge.chimera.signature-renders","schema_version":1,"dsp_valid":False,"renders":{}}
r=m.evaluate(policy,invalid)
require(r["verdict"]=="INVALID","DSP-invalid fixture must be INVALID")
print("PASS: signature benchmark produces PASS/REVISE/INVALID and enforces hard gates independently of score.")
