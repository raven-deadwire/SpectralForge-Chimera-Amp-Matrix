#!/usr/bin/env python3
"""Run isolated preparation checks, never the production plugin/installer workflow."""
import argparse
import json
import platform
from pathlib import Path
import re
import shutil
import subprocess
import sys
from build_preview import build

ROOT=Path(__file__).resolve().parent

def main():
    parser=argparse.ArgumentParser();parser.add_argument('--browser',help='Optional installed Chromium executable')
    args=parser.parse_args();reports=ROOT/'reports';reports.mkdir(exist_ok=True)
    steps=[]
    def run(name,command):
        result=subprocess.run([str(x) for x in command],cwd=ROOT,text=True,encoding='utf-8',errors='replace',capture_output=True,timeout=120)
        text=result.stdout+result.stderr;(reports/(name+'.txt')).write_text(text,encoding='utf-8')
        steps.append(dict(name=name,passed=result.returncode==0,exit_code=result.returncode))
        if result.returncode: raise RuntimeError(name+' failed:\n'+text[-5000:])
        print('PASS '+name)
        return text
    summary=dict(schema_version=1,environment=dict(platform=platform.platform(),python=platform.python_version()),
                 test_data='Synthetic fixtures only; no real NAM/IR asset quality audit',
                 production_plugin_built=False,production_dsp_changed=False,windows_daw_tested=False,
                 audio_processing_implemented=False,steps=steps)
    try:
        for command in ('cmake','node'):
            if not shutil.which(command): raise RuntimeError(command+' is required')
        build()
        run('native-configure',['cmake','-S',ROOT,'-B',ROOT/'_build'])
        run('native-build',['cmake','--build',ROOT/'_build','--config','Release','-j','2'])
        extension='.exe' if sys.platform=='win32' else ''
        executable=ROOT/'_build'/('ChimeraBoardStateTests'+extension)
        if not executable.is_file():executable=ROOT/'_build'/'Release'/executable.name
        native=run('native-tests',[executable]);summary['native_cases']=int(re.search(r'(\d+) native preparation cases passed',native)[1])
        run('ctest',['ctest','--test-dir',ROOT/'_build','-C','Release','--output-on-failure'])
        run('native-fixture',[executable,'--emit',reports/'native-state.cbp'])
        py=run('python-tests',[sys.executable,'-m','unittest','discover','-s','tests','-p','test_*.py','-v'])
        summary['python_cases']=int(re.search(r'Ran (\d+) tests',py)[1])
        js=run('js-tests',['node','tests/state_test.js',reports/'native-state.cbp',reports/'js-state.cbp'])
        summary['javascript_cases']=int(re.search(r'(\d+) JavaScript preparation cases passed',js)[1])
        control=run('control-state-tests',['node','tests/control_state_test.js'])
        summary['control_state_cases']=int(re.search(r'(\d+) control-state cases passed',control)[1])
        amp=run('amp-state-tests',['node','tests/amp_state_test.js'])
        summary['amp_state_cases']=int(re.search(r'(\d+) amplifier state cases passed',amp)[1])
        run('cross-language-return',[executable,'--read',reports/'js-state.cbp'])
        if args.browser:
            ui=run('browser-tests',[sys.executable,'tests/browser_test.py','--browser',args.browser])
            summary['browser_cases']=int(re.search(r'(\d+) browser UI cases passed',ui)[1])
            amp_ui=run('amp-browser-tests',[sys.executable,'tests/amp_browser_test.py','--browser',args.browser])
            summary['amp_browser_cases']=int(re.search(r'(\d+) amplifier browser cases passed',amp_ui)[1])
        else:summary['browser_cases']='not_run';summary['amp_browser_cases']='not_run'
        summary['status']='passed'
    except (OSError,RuntimeError,subprocess.TimeoutExpired) as exc:
        summary['status']='failed';summary['error']=str(exc);print(exc,file=sys.stderr)
    finally:
        (reports/'summary.json').write_text(json.dumps(summary,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    return 0 if summary['status']=='passed' else 1
if __name__=='__main__':raise SystemExit(main())
