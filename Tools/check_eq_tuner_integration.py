"""Audit the frozen October 23 candidate's source, registrations and CI coverage."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess

def main():
    p=argparse.ArgumentParser()
    p.add_argument('--inventory',type=Path)
    p.add_argument('--output',type=Path,required=True)
    args=p.parse_args()
    root=Path(__file__).resolve().parents[1]
    contract=json.loads((root/'docs/evidence/eq-tuner-oct23-20261016/input-contract.json').read_text())
    def git(*cmd):return subprocess.check_output(['git',*cmd],cwd=root,text=True).strip()
    report={'source_sha':git('rev-parse','HEAD'),'tree_sha':git('rev-parse','HEAD^{tree}'),
            'inputs':contract['inputs'],'status':'FAIL','actual_daw':'NOT_ASSESSED','release_approved':False}
    try:
        if set(git('ls-tree','-r','--name-only','HEAD','Source').splitlines()) != set(contract['product_source_sha256']):
            raise ValueError('Product source file inventory changed')
        for path,digest in contract['product_source_sha256'].items():
            if hashlib.sha256(subprocess.check_output(['git','show','HEAD:'+path],cwd=root)).hexdigest()!=digest:
                raise ValueError('Product source changed from #88: '+path)
        report['product_source_files_unchanged']=len(contract['product_source_sha256'])
        for path,digest in contract['release_baseline_sha256'].items():
            if hashlib.sha256(subprocess.check_output(['git','show','HEAD:'+path],cwd=root)).hexdigest()!=digest:
                raise ValueError('Latest #68 lifecycle/release documentation lost: '+path)
        report['release_baseline_files_preserved']=len(contract['release_baseline_sha256'])
        cmake=(root/'CMakeLists.txt').read_text()
        names=re.findall(r'add_test\(NAME\s+(\w+)\s',cmake)
        if len(names)!=len(set(names)):raise ValueError('duplicate literal CTest registration')
        required=set().union(*map(set,contract['registered_tests'].values()))
        if required-set(names):raise ValueError('lost CTest registration: '+repr(required-set(names)))
        report['preserved_literal_registrations']=len(required)
        signal=(root/'.github/workflows/signal-path.yml').read_text()
        if 'ChimeraGraphicalEQEditingTests' not in signal or 'ChimeraTunerGeneration.*Tests' not in signal:
            raise ValueError('EQ editing build or tuner generation selection lost')
        for workflow in ('signal-path','graphical-eq','eq-legacy-state','build'):
            text=(root/f'.github/workflows/{workflow}.yml').read_text()
            if 'ref: ${{ github.event.pull_request.head.sha || github.sha }}' not in text:
                raise ValueError('workflow does not checkout candidate head: '+workflow)
            if 'if: always()' not in text:raise ValueError('failure evidence upload missing: '+workflow)
        if args.inventory:
            inventory=json.loads(args.inventory.read_text(encoding='utf-8-sig'))
            actual=[t['name'] for t in inventory['tests']]
            required=set(contract['critical_tests'])|{'ChimeraEQTunerParameterContractTests'}
            if required-set(actual):raise ValueError('critical configured tests missing: '+repr(required-set(actual)))
            if len(actual)!=len(set(actual)):raise ValueError('duplicate configured test name')
            report['configured_ctest_names']=actual
        report['status']='PASS'
    except Exception as exc:
        report['error']=str(exc)
    args.output.parent.mkdir(parents=True,exist_ok=True)
    args.output.write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(report,indent=2))
    return 0 if report['status']=='PASS' else 1

if __name__=='__main__':raise SystemExit(main())
