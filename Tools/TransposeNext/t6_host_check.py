#!/usr/bin/env python3
"""Run the headless VST3 host check over rates x engines x shifts (T6 DAW prep).

Builds nothing; point it at the research plug-in and TransposeResearchHostCheck
built from Tools/TransposeNext/plugin (see docs/TRANSPOSE_T6_LISTENING_DAW.md).
Inputs are synthetic (holdout plucks and long notes); this checks the plug-in
wrapper and host behaviour, not sound quality.
"""
import argparse, json, os, shutil, subprocess, tempfile
from pathlib import Path
from weak_attack_holdout import holdout_input
from latency_targets import long_notes
from weak_attack_study import FREQUENCIES
from legacy_metrics import sha

def main():
    p = argparse.ArgumentParser(); p.add_argument('--plugin', type=Path, required=True); p.add_argument('--host-check', type=Path, required=True)
    p.add_argument('--out', type=Path, required=True); p.add_argument('--rates', default='44100,48000,96000'); p.add_argument('--shifts', default='-2,-1,1,2'); p.add_argument('--engines', default='baseline,v2,v3')
    a = p.parse_args(); a.out.mkdir(parents=True, exist_ok=True)
    # JUCE's GUI initialiser needs an X display on Linux even without an editor.
    wrap = ['xvfb-run', '-a'] if os.name == 'posix' and not os.environ.get('DISPLAY') and shutil.which('xvfb-run') and os.uname().sysname == 'Linux' else []
    rows = []
    with tempfile.TemporaryDirectory() as w:
        for rate in map(int, a.rates.split(',')):
            for inst, f in FREQUENCIES.items():
                for name, (x, _) in (('plucks', holdout_input(f, 'random_soft_after_loud', 8139, rate)), ('long_notes', long_notes(f, rate))):
                    ip = Path(w)/'in.f32'; x.tofile(ip)
                    for engine in a.engines.split(','):
                        for shift in map(int, a.shifts.split(',')):
                            jp = Path(w)/'r.json'
                            r = subprocess.run([*wrap, str(a.host_check), str(a.plugin), str(ip), str(rate), str(shift), engine, str(jp)], capture_output=True, text=True)
                            row = {'rate': rate, 'input': f'{inst}/{name}', 'engine': engine, 'shift': shift, 'exit': r.returncode}
                            row.update(json.loads(jp.read_text()) if jp.exists() else {'error': r.stderr[-500:]})
                            rows.append(row); print(rate, inst, name, engine, shift, r.returncode, flush=True)
    checks = [k for k, v in rows[0].items() if isinstance(v, dict) and 'pass' in v]
    summary = {'runs': len(rows), 'errors': sum('error' in r for r in rows), 'plugin_sha256': sha(next(a.plugin.rglob('*.so'), a.plugin)) if a.plugin.is_dir() else sha(a.plugin),
               'host_check_sha256': sha(a.host_check), 'checks': {}}
    for engine in a.engines.split(','):
        sel = [r for r in rows if r['engine'] == engine and 'error' not in r]
        summary['checks'][engine] = {c: {'pass': sum(r[c]['pass'] for r in sel), 'of': len(sel), 'max_detail': max((r[c]['detail'] for r in sel if isinstance(r[c]['detail'], (int, float))), default=None)} for c in checks}
        big = [v for r in sel for v in r['large_block_max_abs_diff'].values()]
        summary['checks'][engine]['large_block_max_abs_diff'] = {'nonzero': sum(v > 0 for v in big), 'of': len(big), 'max': max(big) if big else None}
        fails = [(r['rate'], r['input'], r['shift']) for r in sel if not r['variable_blocks_1_512_bit_identical']['pass']]
        summary['checks'][engine]['variable_block_failures'] = fails
    (a.out/'host_check.json').write_text(json.dumps({'summary': summary, 'rows': rows}, indent=1)+'\n')
    print(json.dumps(summary, indent=1))

if __name__ == '__main__':
    main()
