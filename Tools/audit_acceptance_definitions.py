#!/usr/bin/env python3
"""Verify the definition proposal ledger and reused evidence, never issue release PASS.

Exit 0 means diagnostic integrity, not acceptance. No check receipt is produced.
Read-only: production policies, raw receipts and published artifacts are untouched.
"""
from __future__ import annotations
import argparse
from collections import Counter
import csv
import hashlib
import json
import re
from pathlib import Path
import subprocess
import xml.etree.ElementTree as ET
import zipfile

import triage_release_acceptance as triage

ROOT=Path(__file__).resolve().parents[1]
FOLDER=ROOT/'docs/evidence/acceptance-definitions-20261016'
BASE='921a9aed8dc3889018a9456f19784abefd57f8b0'
PRODUCT='5c858442d0ed8360703e0ce63241a567736608a9'
PR41='07dbfa5b0da24ae7a6000ad6dc215ba2aea67c46'
require=triage.gate.require

def digest(data): return hashlib.sha256(data).hexdigest()
def git_bytes(ref,path):
    return subprocess.check_output(['git','show',ref+':'+path],cwd=ROOT)

def load_ledger(): return triage.gate.load_json(FOLDER/'definition-proposals.json')

def validate_registry(ledger):
    require(ledger['schema']=='spectralforge.chimera.acceptance-definition-proposals.v1','Unknown proposal schema')
    require(ledger['base_tool_source_sha']==BASE and ledger['audited_product_source_sha']==PRODUCT,'Proposal source mismatch')
    require(ledger['original_id_definitions_recovered']==0 and ledger['approved_definitions']==0
        and ledger['new_gate_passes']==0 and ledger['new_waivers']==0
        and ledger['publication_authorized'] is False and ledger['policy_claims']==[], 'Proposal cannot authorize or claim acceptance')
    policy=triage.source_json(PRODUCT,'Validation/release-policy.json')
    gaps=triage.source_json(PRODUCT,'Validation/weekly-publication-policy.json')
    expected={g['id'] for g in gaps['known_validation_gaps'] if g['gap_type']=='ORIGINAL_DEFINITION_MISSING'}
    rows=ledger['rows']; ids=[r['id'] for r in rows]
    require(len(rows)==len(set(ids))==86 and set(ids)==expected and ledger['proposed_definitions']==86,'Missing, duplicate or unexpected ID')
    require(len({r['proposal_id'] for r in rows})==86,'Duplicate proposal assignment')
    for row in rows:
        require(row['stage']==row['id'].split('.')[0] and row['proposal_id']=='AR20261016-'+row['id'],'Invalid proposed assignment')
        require(row['original_definition'] is None and row['original_process_owner'] is None
            and row['original_producer'] is None and row['original_id_mapping_recovered'] is False,'Invented original definition/ownership')
        require(row['decision']=='PROPOSED_NOT_APPROVED' and row['gate_status']=='BLOCKED','Proposal promoted to PASS/N/A/approval')
        require(row['strict_hard_gate']==(row['id'] in policy['hard_gates']),'Hard-gate membership changed')
        require(row['definition_basis'] in ('RECOVERED_BEHAVIOR_NEW_ASSIGNMENT','CURRENT_SCOPE_REPLACEMENT','NEW_PROTOCOL_EXTENSION'),'Unknown definition basis')
        for field in ('title_ko','assertion_ko','remaining_work_ko','reuse_limit'):
            require(isinstance(row[field],str) and bool(row[field].strip()),'Empty assertion/protocol/gap')
        require(bool(row['contract_source_ids']) and all(s in ledger['sources'] for s in row['contract_source_ids']),'Unresolved contract source')
        require(all(e in ledger['evidence'] for e in row['evidence_ids']),'Unresolved execution evidence')
    return policy

def verify_sources_and_policy(ledger):
    for path,expected in ledger['protected_files'].items():
        require(digest(git_bytes(BASE,path))==expected,'Pinned policy fingerprint mismatch: '+path)
        require(digest((ROOT/path).read_bytes())==expected,'Protected policy/tool changed: '+path)
    for key,source in ledger['sources'].items():
        require(key==source['commit_sha'][:8]+':'+source['path'],'Source key mismatch')
        require(digest(git_bytes(source['commit_sha'],source['path']))==source['sha256'],'Contract source hash mismatch: '+key)
    # This research intentionally leaves every pre-existing tracked file unchanged.
    changed=subprocess.check_output(['git','diff','--name-only',BASE],cwd=ROOT,text=True).splitlines()
    allowed_prefix='docs/evidence/acceptance-definitions-20261016/'
    allowed={'Tools/audit_acceptance_definitions.py','Tools/test_acceptance_definitions.py',
        '.github/workflows/acceptance-definitions.yml','docs/ACCEPTANCE_DEFINITION_RECOVERY_20261016.md'}
    require(all(p in allowed or p.startswith(allowed_prefix) for p in changed),'Out-of-scope tracked changes')
    with (FOLDER/'definitions.csv').open(encoding='utf-8-sig',newline='') as f:
        exported=list(csv.DictReader(f))
    require([r['id'] for r in exported]==[r['id'] for r in ledger['rows']],'CSV inventory differs')
    for exported_row,row in zip(exported,ledger['rows']):
        for field in ('assertion_ko','remaining_work_ko','decision','gate_status'):
            require(exported_row[field]==row[field],'CSV condition differs')

def verify_reused_evidence(ledger):
    for key,item in ledger['evidence'].items():
        require(item['release_claims']==[],'Supporting evidence contains release claim')
        require(item['product_source_sha']==(PR41 if key=='E.pr41-calculator' else PRODUCT),'Cross-source evidence transfer')
        path=ROOT/item['archive_path']
        require(digest(path.read_bytes())==item['archive_sha256'],'Evidence archive hash mismatch')
        with zipfile.ZipFile(path) as z:
            require(len(z.namelist())==len(set(z.namelist())),'Duplicate evidence member')
            if 'records' in item:
                require(item['run_id']==38386305674 and item['run_attempt']==1,'CTest run/attempt mismatch')
                expected_platforms=[]
                for platform in ('windows','macos','linux'):
                    junit=ET.fromstring(z.read(f'evidence/{platform}/raw/ctest-results.xml'))
                    if any(c.get('name')==key[2:] for c in junit.iter('testcase')):
                        expected_platforms.append(platform)
                require([r['platform'] for r in item['records']]==expected_platforms,'Omitted/duplicate CTest platform')
                for record in item['records']:
                    require(record['status']=='executed_pass' and record['test']==key[2:],'CTest identity mismatch')
                    jp,lp=record['junit_member'],record['raw_log_member']
                    require(jp==f'evidence/{record["platform"]}/raw/ctest-results.xml'
                        and lp==f'evidence/{record["platform"]}/raw/LastTest.log','CTest platform mismatch')
                    require(digest(z.read(jp))==record['junit_sha256'] and digest(z.read(lp))==record['raw_log_sha256'],'Raw evidence hash mismatch')
                    cases=[c for c in ET.fromstring(z.read(jp)).iter('testcase') if c.get('name')==record['test']]
                    require(len(cases)==1 and cases[0].get('status')=='run' and
                        not any(cases[0].find(t) is not None for t in ('failure','error','skipped')),'Test missing/skipped/failed')
                    raw=z.read(lp).decode('utf-8-sig',errors='replace').replace('\r\n','\n')
                    marker='Testing: '+record['test']+'\n'; start=raw.find(marker)
                    require(start>=0,'Missing raw-log test')
                    following=re.search(r'\n\d+/\d+ Testing:',raw[start+len(marker):])
                    section=raw[start:start+len(marker)+following.start()] if following else raw[start:]
                    require('Test Passed.' in section and all(line in section for line in record['assertion_excerpts']),'Fabricated/wrong-case raw-log excerpt')
            else:
                raw=z.read(item['member'])
                require(digest(raw)==item['member_sha256'],'Receipt hash mismatch')
                require(triage.parse(raw)==item['receipt'],'Receipt copied inaccurately')
    metadata=triage.gate.load_json(FOLDER/'github-metadata.json')
    upgrade=ledger['evidence']['E.upgrade']; result=upgrade['receipt']
    artifact=next(a for a in metadata['upgrade_artifacts']['artifacts'] if a['id']==11993179895)
    require(artifact['digest']=='sha256:'+upgrade['archive_sha256'],'Upgrade differs from GitHub digest')
    require(metadata['upgrade_run']['head_sha']==BASE and metadata['upgrade_run']['conclusion']=='success'
        and metadata['upgrade_run']['run_attempt']==1,'Upgrade workflow mismatch')
    require(result['tool_source_sha']==BASE and result['product_source_sha']==PRODUCT
        and result['run_id']=='38777238017' and result['run_attempt']=='1'
        and result['candidate_build_run']=='38386305674' and result['success'] is True
        and result['qualification_only'] is True and result['daw_project_acceptance'] is False
        and result['publication_approved'] is False and len(result['checks'])==5,'Upgrade scope/identity invalid')
    calc=ledger['evidence']['E.pr41-calculator']; receipt=calc['receipt']
    require(metadata['pr41_artifact']['digest']=='sha256:'+calc['archive_sha256'],'Calculator differs from GitHub digest')
    require(metadata['pr41_run']['conclusion']==calc['whole_workflow_conclusion']=='failure','Historical failed workflow hidden')
    require(receipt['revision']['commit_sha']==PR41 and receipt['revision']['run_id']=='38057692032'
        and receipt['revision']['run_attempt']=='1' and receipt['policy_claims']==[]
        and receipt['release_verdict']=='BLOCKED' and receipt['component_status']=='PASS','Calculator scope/identity invalid')
    with zipfile.ZipFile(ROOT/calc['archive_path']) as z:
        for name,expected in receipt['files'].items():
            require(digest(z.read(name))==expected,'PR41 raw output hash mismatch')
        require(triage.parse(z.read('cases.json'))==calc['cases'],'PR41 case copy mismatch')
        cases=calc['cases']
        require(cases['tests_run']==len(cases['cases'])==13 and
            cases['failures']==cases['errors']==cases['skipped']==0
            and all(c['status']=='PASS' for c in cases['cases']),'PR41 component test failure')
        for path,expected in receipt['source_inputs'].items():
            require(digest(git_bytes(PR41,path))==expected,'PR41 source-input hash mismatch')
    return {'pr41_component_cases_reused':13,'pr41_whole_workflow':'failure',
        'beta2_to_beta3_installer_assertions_reused':5}

def audit(ledger):
    validate_registry(ledger)
    verify_sources_and_policy(ledger)
    reused=verify_reused_evidence(ledger)
    original=ROOT/'docs/evidence/beta3-acceptance-20261016'
    snapshot=triage.gate.load_json(original/'github-snapshot.json')
    result=triage.audit(original/'ci-gate-38386305674.zip',snapshot,
        *[triage.source_json(PRODUCT,p) for p in ('Validation/release-policy.json','Validation/ci-check-map.json',
            'Validation/weekly-publication-policy.json','Validation/waivers.json')])
    require(result['counts']==dict(total=128,**{'pass':21,'blocked':107,'not_applicable':0}), 'Original gate drift')
    # Read-only diagnostic probes expose known gaps; they are never acceptance tests.
    import evaluate_signature_benchmark as benchmark
    findings={'beta3_accepts_boolean_metrics':benchmark.finite(True) and benchmark.finite(False),
        'candidate_checklist_still_names_beta2':'Candidate package/tag: **1.3.1-beta.2 / v1.3.1-beta.2**' in
            git_bytes(PRODUCT,'docs/RELEASE_CHECKLIST.md').decode('utf-8'),
        'pr41_evaluator_differs_from_beta3':git_bytes(PR41,'Tools/evaluate_signature_benchmark.py')!=git_bytes(PRODUCT,'Tools/evaluate_signature_benchmark.py')}
    return {'diagnostic_only':True,'integrity':'verified','audited_product_source_sha':PRODUCT,
        'base_tool_source_sha':BASE,'definition_count':86,'original_id_definitions_recovered':0,
        'proposed_not_approved':86,'definition_basis_counts':dict(Counter(r['definition_basis'] for r in ledger['rows'])),
        'stage_counts':dict(Counter(r['stage'] for r in ledger['rows'])),
        'existing_evidence_records':len(ledger['evidence']),'reused_evidence':reused,
        'source_files_verified':len(ledger['sources']),'protected_files_verified':len(ledger['protected_files']),
        'strict_counts':result['counts'],'strict_verdict':result['strict_verdict'],
        'new_gate_passes':0,'new_waivers':0,'publication_authorized':False,'known_gaps':findings}

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path)
    args=parser.parse_args()
    result=audit(load_ledger())
    rendered=json.dumps(result,ensure_ascii=False,indent=2)+'\n'
    if args.output:
        args.output.parent.mkdir(parents=True,exist_ok=True)
        with args.output.open('x',encoding='utf-8') as f: f.write(rendered)
    print(rendered,end='')
    return 0

if __name__=='__main__': raise SystemExit(main())
