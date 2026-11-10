#!/usr/bin/env python3
"""Run the headless VST3 host check over rates x engines x shifts (T6 DAW prep).

Builds nothing; point it at the research plug-in and TransposeResearchHostCheck
built from Tools/TransposeNext/plugin (see docs/TRANSPOSE_T6_LISTENING_DAW.md).
Inputs are synthetic (holdout plucks and long notes); this checks the plug-in
wrapper and host behaviour, not sound quality.
"""
import argparse, hashlib, json, os, shutil, subprocess, tempfile
from pathlib import Path
from weak_attack_holdout import holdout_input
from latency_targets import long_notes
from weak_attack_study import FREQUENCIES
from legacy_metrics import sha

def main():
    p = argparse.ArgumentParser(); p.add_argument('--plugin', type=Path, required=True); p.add_argument('--host-check', type=Path, required=True)
    p.add_argument('--out', type=Path, required=True); p.add_argument('--rates', default='44100,48000,96000'); p.add_argument('--shifts', default='-2,-1,1,2'); p.add_argument('--engines', default='baseline,v2,v3')
    p.add_argument("--no-xvfb", action="store_true", help="Run the headless host directly without a display server")
    a = p.parse_args(); a.out.mkdir(parents=True, exist_ok=True)
    if not a.host_check.is_file() or not a.plugin.exists():
        p.error('--host-check and --plugin must point to existing build artifacts')
    # JUCE's GUI initialiser needs an X display on Linux even without an editor.
    wrap = ['xvfb-run', '-a'] if not a.no_xvfb and os.name == 'posix' and not os.environ.get('DISPLAY') and shutil.which('xvfb-run') and os.uname().sysname == 'Linux' else []
    rows = []
    with tempfile.TemporaryDirectory() as w:
        for rate in map(int, a.rates.split(',')):
            for inst, f in FREQUENCIES.items():
                for name, (x, _) in (('plucks', holdout_input(f, 'random_soft_after_loud', 8139, rate)), ('long_notes', long_notes(f, rate))):
                    ip = Path(w)/'in.f32'; x.tofile(ip)
                    for engine in a.engines.split(','):
                        for shift in map(int, a.shifts.split(',')):
                            jp = Path(w)/'r.json'
                            jp.unlink(missing_ok=True)  # never reuse a prior run's result after a host failure
                            r = subprocess.run([*wrap, str(a.host_check), str(a.plugin), str(ip), str(rate), str(shift), engine, str(jp)], capture_output=True, text=True)
                            row = {'rate': rate, 'input': f'{inst}/{name}', 'engine': engine, 'shift': shift, 'exit': r.returncode}
                            try:
                                row.update(json.loads(jp.read_text()) if jp.exists() else {'error': r.stderr[-500:] or 'host produced no JSON'})
                            except (ValueError, OSError) as exc:
                                row['error'] = f'invalid host report: {exc}'
                            if r.returncode and 'error' not in row:
                                row['error'] = f'host exit {r.returncode}: {r.stderr[-500:]}'
                            rows.append(row); print(rate, inst, name, engine, shift, r.returncode, flush=True)
    checks = sorted({k for row in rows for k, v in row.items() if isinstance(v, dict) and 'pass' in v})
    plugin_files = {str(f.relative_to(a.plugin)): sha(f) for f in sorted(a.plugin.rglob('*')) if f.is_file()} if a.plugin.is_dir() else {a.plugin.name: sha(a.plugin)}
    binaries = [value for path, value in plugin_files.items()
                if Path(path).suffix in ('.so', '.vst3') or 'MacOS' in Path(path).parts]
    summary = {'kind': 'synthetic_host_check_not_actual_DAW', 'runs': len(rows), 'errors': sum('error' in r for r in rows),
               'plugin_files_sha256': plugin_files,
               'plugin_sha256': sha(a.plugin) if a.plugin.is_file() else (binaries[0] if len(binaries) == 1 else None),
               'plugin_bundle_sha256': hashlib.sha256(json.dumps(plugin_files, sort_keys=True).encode()).hexdigest(),
               'plugin_bundle_hash_method': 'sha256 of sorted JSON relative-path/file-sha256 map',
               'host_check_sha256': sha(a.host_check), 'checks': {}}
    for engine in a.engines.split(','):
        sel = [r for r in rows if r['engine'] == engine and 'error' not in r]
        summary['checks'][engine] = {c: {'pass': sum(r.get(c, {}).get('pass', False) for r in sel), 'of': len(sel), 'max_detail': max((r[c]['detail'] for r in sel if isinstance(r.get(c, {}).get('detail'), (int, float))), default=None)} for c in checks}
        big = [v for r in sel for v in r.get('large_block_max_abs_diff', {}).values()]
        summary['checks'][engine]['large_block_max_abs_diff'] = {'nonzero': sum(v > 0 for v in big), 'of': len(big), 'max': max(big) if big else None}
        fails = [(r['rate'], r['input'], r['shift']) for r in sel if not r.get('variable_blocks_1_512_bit_identical', {}).get('pass', False)]
        summary['checks'][engine]['variable_block_failures'] = fails
    summary['ok'] = bool(rows and checks and not summary['errors'] and
                         all(r.get('all_hard_checks_pass') is True for r in rows) and
                         all(r.get(c, {}).get('pass', False) for r in rows for c in checks))
    (a.out/'host_check.json').write_text(json.dumps({'summary': summary, 'rows': rows}, indent=1)+'\n')
    print(json.dumps(summary, indent=1))
    return 0 if summary['ok'] else 1

if __name__ == '__main__':
    raise SystemExit(main())
