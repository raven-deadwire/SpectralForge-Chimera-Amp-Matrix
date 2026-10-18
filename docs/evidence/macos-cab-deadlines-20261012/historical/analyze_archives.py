#!/usr/bin/env python3
"""Audit immutable CAB timing artifact ZIPs without changing their records.

Usage: python analyze_archives.py --archives archives.json --output audit-output

The manifest is a JSON list (or {"archives": [...]}) of records with label, os
(macOS/Linux/Windows), path, sha256 and artifact_id. ZIP paths may be absolute or
relative to the manifest. Optional expected_candidate_sha, expected_run_id and
expected_run_attempt bind downloaded GitHub metadata. The protocol is fixed at
66 populations x 1600 callbacks, original p99 index 1584 and block-period budget.
No extraction or archive editing occurs. Invalid evidence yields validation.json
and exit 2; raw ZIPs remain the authority. CPU time never replaces wall time.
"""
import argparse
from collections import Counter
import csv
import hashlib
import io
import json
import math
from pathlib import Path
import re
import zipfile

FIELDS = ('schema_version,engine,suite,sample_rate,block_size,channels,callback_index,'
          'phase,phase_index,automation_requested,wall_us,budget_us,deadline_miss,'
          'thread_cpu_us,thread_cpu_valid,thread_cycles,thread_cycles_valid,'
          'clock_start_error,clock_end_error,thread_cpu_start_us,thread_cpu_end_us,'
          'thread_cycles_start,thread_cycles_end').split(',')
KEY_FIELDS = ('suite', 'engine', 'sample_rate', 'block_size', 'channels')
PREFIX = 'candidate/cab-timing-diagnostics/'


def require(ok, message):
    if not ok:
        raise ValueError(message)


def number(value, field):
    result = float(value)
    require(math.isfinite(result), f'non-finite {field}: {value}')
    return result


def integer(value, field):
    require(re.fullmatch(r'-?[0-9]+', str(value)) is not None, f'invalid integer {field}: {value}')
    return int(value)


def expected_populations():
    return {(suite, engine, str(sr), str(block), str(ch))
            for suite, engines, blocks in (
                ('integration', ('original-v1', 'expanded-v2', 'array-v3-6x10'), (64, 256)),
                ('extended', ('expanded-v2',), (32, 64, 128, 256, 512)))
            for engine in engines for sr in (44100, 48000, 96000)
            for block in blocks for ch in (1, 2)}


def check_callback(r, index, key, platform, budget):
    require(None not in r and None not in r.values(), 'malformed CSV row')
    require(tuple(r[f] for f in KEY_FIELDS) == key, 'population changes within CSV')
    require(r['schema_version'] == '1', 'unsupported raw schema')
    require(integer(r['callback_index'], 'callback_index') == index, f'missing/duplicate/out-of-order callback {index}')
    require(r['phase'] == ('worker_and_automation' if index < 1200 else 'forced_six_slot_publication'), 'wrong phase')
    require(integer(r['phase_index'], 'phase_index') == (index if index < 1200 else index - 1200), 'wrong phase index')
    require(r['automation_requested'] == str(int(index < 80)), 'wrong automation flag')
    wall = number(r['wall_us'], 'wall_us')
    require(wall >= 0 and math.isclose(number(r['budget_us'], 'budget_us'), budget, rel_tol=1e-12, abs_tol=1e-9),
            'invalid wall duration or changed deadline')
    require(r['deadline_miss'] == str(int(wall > budget)), 'deadline flag mismatch')
    require(r['clock_start_error'] == '0' and r['clock_end_error'] == '0', 'clock read error')
    cpu, start, end = (number(r[f], f) for f in ('thread_cpu_us', 'thread_cpu_start_us', 'thread_cpu_end_us'))
    cycles, cycle_start, cycle_end = (integer(r[f], f) for f in ('thread_cycles', 'thread_cycles_start', 'thread_cycles_end'))
    if platform in ('macOS', 'Linux'):
        require(r['thread_cpu_valid'] == '1' and r['thread_cycles_valid'] == '0', 'wrong POSIX clock validity')
        require(start >= 0 and end >= start and cpu >= 0, 'nonmonotonic thread CPU')
        require(math.isclose(cpu, end - start, rel_tol=1e-9, abs_tol=1e-6), 'paired CPU delta mismatch')
        require(cycles == cycle_start == cycle_end == 0, 'invalid unused cycle fields')
    else:
        require(r['thread_cpu_valid'] == '0' and r['thread_cycles_valid'] == '1', 'wrong Windows clock validity')
        require(cpu == start == end == -1, 'Windows cycles must not be represented as CPU time')
        require(cycle_start >= 0 and cycle_end >= cycle_start and cycles == cycle_end - cycle_start, 'paired cycle delta mismatch')


def check_clock_progression(record, previous_end, platform):
    prefix = 'thread_cycles' if platform == 'Windows' else 'thread_cpu'
    suffix = '' if platform == 'Windows' else '_us'
    convert = integer if platform == 'Windows' else number
    start_field = prefix + '_start' + suffix
    end_field = prefix + '_end' + suffix
    start = convert(record[start_field], start_field)
    end = convert(record[end_field], end_field)
    require(previous_end is None or start >= previous_end,
            'cumulative thread clock goes backwards between callbacks')
    return end


def audit_archive(a, manifest_directory):
    require(a['os'] in ('macOS', 'Linux', 'Windows'), 'unrecognized platform')
    path = Path(a['path'])
    if not path.is_absolute():
        path = manifest_directory / path
    expected_hash = a['sha256'].removeprefix('sha256:').lower()
    require(re.fullmatch(r'[0-9a-f]{64}', expected_hash) is not None, 'invalid expected archive SHA-256')
    payload = path.read_bytes()
    require(hashlib.sha256(payload).hexdigest() == expected_hash, 'archive SHA-256 does not match metadata')
    with zipfile.ZipFile(io.BytesIO(payload)) as z:
        require(len(z.namelist()) == len(set(z.namelist())), 'duplicate ZIP members')
        manifest = json.loads(z.read('manifest.json'))
        env = json.loads(z.read('environment.json'))
        receipt = json.loads(z.read('candidate/receipt.json'))
        require(manifest['candidate'] == receipt, 'candidate receipt differs from manifest')
        source = manifest['candidate_sha']
        require(re.fullmatch(r'[0-9a-f]{40}', source) is not None, 'invalid candidate source SHA')
        for name in ('source', 'source_after'):
            require(receipt[name]['sha'] == source and receipt[name]['clean'] is True, f'candidate {name} not clean or wrong source')
        require(env['runner']['RUNNER_OS'] == a['os'], 'platform does not match artifact metadata')
        for field, observed in [('expected_candidate_sha', source), ('expected_run_id', env['runner']['GITHUB_RUN_ID']),
                                ('expected_run_attempt', env['runner']['GITHUB_RUN_ATTEMPT'])]:
            if field in a:
                require(str(a[field]) == str(observed), f'{field} mismatch')
        file_entries = receipt['timing_sample_files']
        files = {'candidate/' + entry['path']: entry for entry in file_entries}
        require(len(files) == len(file_entries) == 66, 'duplicate or missing receipt CSV file records')
        validations = receipt['timing_sample_validation']
        validation_paths = {'candidate/' + entry['path'] for entry in validations}
        require(len(validations) == len(validation_paths) == 66 and validation_paths == set(files),
                'duplicate or missing receipt validation populations')
        for entry in validations:
            require(entry['samples'] == 1600, 'receipt population does not contain 1600 callbacks')
            require(entry['thread_cpu_valid_samples'] == (0 if a['os'] == 'Windows' else 1600)
                    and entry['thread_cycles_valid_samples'] == (1600 if a['os'] == 'Windows' else 0),
                    'receipt clock population is incomplete')
        paths = sorted(n for n in z.namelist() if n.startswith(PREFIX) and n.endswith('.csv'))
        require(len(paths) == 66 and set(paths) == set(files), 'missing or unexpected raw populations')
        routes, misses, keys = [], [], set()
        phase_counts = Counter()
        expected = expected_populations()
        for csv_path in paths:
            raw = z.read(csv_path)
            raw_hash = hashlib.sha256(raw).hexdigest()
            require(len(raw) == files[csv_path]['bytes'] and raw_hash == files[csv_path]['sha256'], f'CSV digest/size mismatch: {csv_path}')
            reader = csv.DictReader(io.StringIO(raw.decode('utf-8')))
            require(reader.fieldnames == FIELDS, f'wrong CSV columns: {csv_path}')
            records = list(reader)
            require(len(records) == 1600, f'population must retain all 1600 callbacks: {csv_path}')
            key = tuple(records[0][f] for f in KEY_FIELDS)
            require(key in expected and key not in keys, f'duplicate or unexpected population: {key}')
            keys.add(key)
            suite, engine, sr, block, channels = key
            require(csv_path == f'{PREFIX}{suite}/{engine}/{sr}-{block}-{channels}.csv', f'CSV path does not match population: {csv_path}')
            budget = int(block) * 1e6 / int(sr)
            meta = dict(label=a['label'], os=a['os'], source=source, run_id=str(env['runner']['GITHUB_RUN_ID']),
                        csv_path=csv_path, csv_sha256=raw_hash, **dict(zip(KEY_FIELDS, key)))
            previous_clock_end = None
            for index, record in enumerate(records):
                try:
                    check_callback(record, index, key, a['os'], budget)
                    previous_clock_end = check_clock_progression(record, previous_clock_end, a['os'])
                except ValueError as error:
                    raise ValueError(f'{csv_path} callback {index}: {error}') from error
                phase_counts[record['phase']] += 1
                if record['deadline_miss'] == '1':
                    misses.append(dict(meta, **record))  # Preserve every original field as a string.
            ordered = sorted(records, key=lambda r: float(r['wall_us']))
            maximum = ordered[-1]
            row_misses = [r for r in records if r['deadline_miss'] == '1']
            routes.append(dict(meta, samples=1600, deadline_us=budget,
                p50_us=float(ordered[800]['wall_us']), p99_us=float(ordered[1584]['wall_us']),
                p99_callback_index=int(ordered[1584]['callback_index']), p99_pass=float(ordered[1584]['wall_us']) < budget,
                max_wall_us=float(maximum['wall_us']), max_callback_index=int(maximum['callback_index']),
                max_thread_cpu_us=float(maximum['thread_cpu_us']), max_phase=maximum['phase'],
                deadline_misses=len(row_misses), miss_phases=dict(Counter(r['phase'] for r in row_misses)),
                misses_with_thread_cpu_above_deadline=sum(float(r['thread_cpu_us']) > budget for r in row_misses if r['thread_cpu_valid'] == '1')))
        require(keys == expected, 'expected population grid is incomplete')
        cpu_misses = [m for m in misses if m['thread_cpu_valid'] == '1']
        summary = dict(label=a['label'], os=a['os'], run_id=str(env['runner']['GITHUB_RUN_ID']), source=source,
            archive_sha256=expected_hash, artifact_id=a['artifact_id'], route_count=66, callbacks=sum(r['samples'] for r in routes),
            p99_pass=sum(r['p99_pass'] for r in routes), deadline_misses=len(misses), max_wall_us=max(r['max_wall_us'] for r in routes),
            missed_routes=sum(r['deadline_misses'] > 0 for r in routes), phase_counts=dict(phase_counts),
            miss_phases=dict(Counter(m['phase'] for m in misses)), missed_automation_requested=dict(Counter(m['automation_requested'] for m in misses)),
            misses_cpu_over_budget=sum(float(m['thread_cpu_us']) > float(m['budget_us']) for m in cpu_misses),
            misses_cpu_under_budget=sum(float(m['thread_cpu_us']) <= float(m['budget_us']) for m in cpu_misses),
            misses_cpu_at_most_tenth_wall=sum(float(m['thread_cpu_us']) <= .1*float(m['wall_us']) for m in cpu_misses),
            misses_with_valid_cpu_clock=len(cpu_misses), baseline_sha=manifest['baseline_sha'],
            baseline_timing_cases=len(manifest['baseline']['timing']), baseline_misses=sum(int(t['misses'].split('/')[0]) for t in manifest['baseline']['timing']),
            baseline_max_wall_us=max(float(t['max_us']) for t in manifest['baseline']['timing']), order=manifest['order'],
            candidate_configuration=receipt['configuration'], candidate_commands=[c['command'] for c in receipt['commands']],
            candidate_binaries=receipt['binaries'], candidate_tests=receipt['tests'])
        for field in ('sample_rate', 'block_size', 'channels', 'engine', 'suite'):
            summary['by_' + field] = dict(Counter(m[field] for m in misses))
        return summary, routes, misses, env


def write_csv(path, records):
    if not records:
        path.write_text('')
        return
    with path.open('w', newline='') as output:
        writer = csv.DictWriter(output, fieldnames=list(records[0]))
        writer.writeheader()
        writer.writerows(records)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--archives', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    spec = json.loads(args.archives.read_text())
    archives = spec.get('archives') if isinstance(spec, dict) else spec
    require(isinstance(archives, list) and bool(archives), 'manifest must contain at least one archive')
    protected = {args.archives.resolve()}
    for archive in archives:
        source_path = Path(archive['path'])
        protected.add((source_path if source_path.is_absolute() else args.archives.resolve().parent / source_path).resolve())
    for name in ('run-summary.json', 'route-summary.json', 'deadline-misses.json',
                 'environments.json', 'validation.json', 'route-summary.csv', 'deadline-misses.csv'):
        require((args.output / name).resolve() not in protected, 'output would overwrite an input artifact or manifest')
    summaries, routes, misses, environments, validation, seen = [], [], [], {}, [], set()
    for archive in archives:
        tag = str(archive.get('label')) + '-' + str(archive.get('os'))
        try:
            require(tag not in seen, f'duplicate archive label/platform: {tag}')
            seen.add(tag)
            summary, rr, mm, env = audit_archive(archive, args.archives.resolve().parent)
            summaries.append(summary); routes.extend(rr); misses.extend(mm); environments[tag] = env
            validation.append(dict(archive=tag, passed=True, routes=len(rr), callbacks=summary['callbacks']))
            print(f"{tag}: 66 routes, {summary['callbacks']} callbacks, p99 PASS {summary['p99_pass']}/66, "
                  f"misses {summary['deadline_misses']}, max {summary['max_wall_us']:.6f} us")
        except (OSError, ValueError, KeyError, TypeError, zipfile.BadZipFile) as error:
            validation.append(dict(archive=tag, passed=False, error=str(error)))
            print(f'{tag}: INVALID EVIDENCE: {error}')
    args.output.mkdir(parents=True, exist_ok=True)
    for name, value in [('run-summary', summaries), ('route-summary', routes), ('deadline-misses', misses),
                        ('environments', environments), ('validation', {'passed': all(v['passed'] for v in validation), 'archives': validation})]:
        (args.output / (name + '.json')).write_text(json.dumps(value, indent=2) + '\n')
    write_csv(args.output / 'route-summary.csv', routes)
    write_csv(args.output / 'deadline-misses.csv', misses)
    return 0 if all(v['passed'] for v in validation) else 2


if __name__ == '__main__':
    raise SystemExit(main())
