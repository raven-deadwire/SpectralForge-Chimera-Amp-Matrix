#!/usr/bin/env python3
"""Offline secondary audit: exact schedule, raw summaries and statistical decision.

Run after the native run. No benchmark executes and no observations change.
Usage: python audit_statistics.py artifact.zip output.json
"""
import copy
import hashlib
import json
import math
from pathlib import Path
import sys
import tempfile
import zipfile

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'Tools/CabScalarMemory'))
import run as experiment

def equivalent(a, b, path='root'):
    if isinstance(a, dict):
        assert set(a) == set(b), path
        for key in a:
            equivalent(a[key], b[key], path + '/' + key)
    elif isinstance(a, list):
        assert len(a) == len(b), path
        for i, (left, right) in enumerate(zip(a, b)):
            equivalent(left, right, path + '/' + str(i))
    elif isinstance(a, float):
        assert math.isfinite(a) and math.isclose(a, b, rel_tol=1e-12, abs_tol=1e-9), (path, a, b)
    else:
        assert a == b, (path, a, b)

def audit(archive):
    with zipfile.ZipFile(archive) as z:
        names = z.namelist()
        assert len(names) == len(set(names)), 'Duplicate ZIP entries'
        manifest = json.loads(z.read('manifest.json'))
        files = {n for n in names if not n.endswith('/')}
        assert files == set(manifest) | {'manifest.json'}, 'Unmanifested or missing file'
        for name, record in manifest.items():
            data = z.read(name)
            assert len(data) == record['bytes']
            assert hashlib.sha256(data).hexdigest() == record['sha256'], name
        report = json.loads(z.read('report.json'))
        recorded_analysis = json.loads(z.read('analysis.json'))
        assert report['parent'] == experiment.PARENT
        baseline=z.read('scalar-header.h').decode()
        candidate=z.read('candidate-header.h')
        assert experiment.generate(baseline,report['kind']).encode()==candidate
        assert hashlib.sha256(candidate).hexdigest()==report['header_sha256']
        assert report['head_immediate_parent']==experiment.PARENT
        assert report['complete'] and not report['errors']
        assert len(report['candidate'])==40 and len(report['controller_sha'])==40
        assert report['rounds'] == experiment.ROUNDS == 6
        assert report['compiler_commands_match']
        assert hashlib.sha256(z.read('generated-probe.cpp')).hexdigest()==report['probe_sha256']
        normalized={}
        for arm in ('A','B'):
            source=report['sources'][arm]['path']
            build=str(Path(report['binaries'][arm]['control']['path']).parents[2])
            juce=str(Path(report['binaries']['A']['control']['path']).parents[2]/'_deps/juce-src')
            selected={}
            for c in json.loads(z.read(f'build-metadata/{arm}/compile_commands.json')):
                if c['file'].endswith('/probe.cpp'):
                    mode='profile' if 'CHIMERA_CAB_PROFILE=1' in c['command'] else 'control'
                    assert mode not in selected
                    selected[mode]=c['command'].replace(juce,'{JUCE}').replace(source,'{SOURCE}').replace(build,'{BUILD}')
            assert set(selected)=={'control','profile'}
            normalized[arm]=selected
        assert normalized['A']==normalized['B']

        expected = []
        for round_ in range(6):
            order = 'ABBA' if round_ % 2 == 0 else 'BAAB'
            for mode in (('control', 'profile') if round_ % 2 == 0 else ('profile', 'control')):
                for group in (('AA', 'AB') if round_ % 2 == 0 else ('AB', 'AA')):
                    for slot, logical in enumerate(order):
                        expected.append(dict(round=round_, order=order, mode=mode,
                            experiment=group, slot=slot, logical=logical,
                            arm=logical if group == 'AB' else 'A'))
        assert len(report['trials']) == len(expected) == 96
        recomputed = copy.deepcopy(report['trials'])
        command_specs = []
        with tempfile.TemporaryDirectory(prefix='s8t-audit-') as tmp:
            for actual, wanted in zip(recomputed, expected):
                assert all(actual[key] == value for key, value in wanted.items()), wanted
                assert actual['binary_sha256'] == report['binaries'][actual['arm']][actual['mode']]['sha256']
                label = f'r{actual["round"]}-{actual["mode"]}-{actual["experiment"]}-{actual["slot"]}-{actual["logical"]}'
                rows = []
                for engine in experiment.ENGINES:
                    prefix = f'trials/{label}/{engine}/'
                    folder = Path(tmp) / label / engine
                    for name in files:
                        if name.startswith(prefix) and name.endswith('.csv'):
                            relative = Path(name[len(prefix):])
                            assert not relative.is_absolute() and '..' not in relative.parts
                            target = folder / relative
                            target.parent.mkdir(parents=True, exist_ok=True)
                            target.write_bytes(z.read(name))
                    rows.extend(experiment.read_trial(folder, actual['mode'], engine))
                    command_specs.append((f'trials/{label}/{engine}.log',
                        [report['binaries'][actual['arm']][actual['mode']]['path'],
                         *experiment.ENGINES[engine]]))
                equivalent(rows, actual['rows'], label)
                actual['rows'] = rows
        # All compiles and original gate invocations precede the ordered 288 trials.
        commands = report['commands']
        assert len(commands) == 294
        assert [Path(c['log']).name for c in commands[:4]] == [
            'A-configure.log', 'A-build.log', 'B-configure.log', 'B-build.log']
        assert all(c['returncode'] == 0 for c in commands[:4])
        for command, (suffix, invocation) in zip(commands[6:], command_specs):
            assert command['log'].endswith('/' + suffix)
            assert command['command'] == invocation
            if command['returncode']:
                assert 'CAB-only p99 exceeds one block period (runner CPU gate)' in z.read(suffix).decode()
        for arm in ('A', 'B'):
            log = z.read(f'gates/{arm}/console.log').decode()
            assert report['gates'][arm]['frozen_bits_and_latency'] == (
                experiment.contracts_present(log) and report['gates'][arm]['returncode'] == 0)
            assert report['sources'][arm]['clean']
        analysis = experiment.analyze(recomputed)
        equivalent(analysis, recorded_analysis)
        assert report['performance_demonstrated'] == analysis['performance_demonstrated']
        contracts = all(experiment.gates_pass(report['gates'][arm]) for arm in ('A', 'B'))
        assert report['cab_contracts_pass'] == contracts
        deadlines=all(r['misses']['B']['deadline_misses']<=r['misses']['A']['deadline_misses']
            and r['misses']['B']['cpu_over_budget']<=r['misses']['A']['cpu_over_budget']
            for r in analysis['rows'] if r['mode']=='control' and r['phase']=='forced_six_slot_publication')
        assert report['deadline_nonincrease']==deadlines
        assert report['review_eligible']==(report['complete'] and contracts and analysis['performance_demonstrated'] and deadlines)
        assert report['integration_eligible'] is False
        assert report['release_approved'] is False
        return dict(archive_sha256=hashlib.sha256(Path(archive).read_bytes()).hexdigest(),
            manifest_files_verified=len(manifest), trial_order_verified=96,
            command_order_verified=294, all_raw_summaries_recomputed=3456,
            statistical_rows_recomputed=len(analysis['rows']),
            exact_statistical_decision_reproduced=True, cab_contracts_pass=contracts,
            performance_demonstrated=analysis['performance_demonstrated'],
            integration_eligible=report['integration_eligible'], release_approved=False,
            nonzero_diagnostic_exits=sum(c['returncode'] != 0 for c in commands[6:]))

if __name__ == '__main__':
    result = audit(Path(sys.argv[1]))
    Path(sys.argv[2]).write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps(result, indent=2))
