#!/usr/bin/env python3
"""Independently verify frozen-CAB scheduling diagnostics, including failed runs.

Usage: python analyze_diagnostic_artifacts.py --archives manifest.json --output out
Only the Python standard library is needed. ZIP paths are relative to the input
manifest unless absolute. No extraction, runner imports, archive edits, deadline
changes, missing-population imputation, or failed-run filtering occurs.

Exit 0: structurally verified complete evidence with no recorded execution/gate
failure. Exit 3: verified incomplete evidence or a retained execution/gate failure.
Exit 2: integrity, source binding, raw, receipt, or reported-summary inconsistency.
An exit 0 is diagnostic validation, never proof of a product fix or DAW safety.
"""
from __future__ import annotations

import argparse
from collections import Counter
import csv
import hashlib
import io
import json
import math
from pathlib import Path, PurePosixPath
import re
import xml.etree.ElementTree as ET
import zipfile

FROZEN_SHA = '7b20cd9d1f90e3e2969c250712b199e893cf50cd'
JUCE_SHA = 'd6181bde38d858c283c3b7bf699ce6340c050b5d'
TESTS = ('ChimeraOriginalCabIntegrationTests', 'ChimeraOriginalCabRealtimeTests',
         'ChimeraCabExpansionIntegrationTests', 'ChimeraCabExtendedTimingTests',
         'ChimeraCabLayoutIntegrationTests')
ORDERS = {
    'ABBA': [('01-A', 'inherited'), ('02-B', 'user-interactive'),
             ('03-B', 'user-interactive'), ('04-A', 'inherited')],
    'BAAB': [('01-B', 'user-interactive'), ('02-A', 'inherited'),
             ('03-A', 'inherited'), ('04-B', 'user-interactive')],
}
SUITES = [('original', []), ('expanded', ['--expanded']),
          ('layouts', ['--layouts']), ('extended', ['--expanded', '--extended-buffers'])]
FIELDS = ('schema_version,engine,suite,sample_rate,block_size,channels,callback_index,'
          'phase,phase_index,automation_requested,wall_us,budget_us,deadline_miss,'
          'thread_cpu_us,thread_cpu_valid,thread_cycles,thread_cycles_valid,'
          'clock_start_error,clock_end_error,thread_cpu_start_us,thread_cpu_end_us,'
          'thread_cycles_start,thread_cycles_end').split(',')
SENTINEL_FIELDS = FIELDS + ['workload', 'worker_running']
KEY_FIELDS = ('suite', 'engine', 'sample_rate', 'block_size', 'channels')
META_FIELDS = ['archive', 'run_id', 'run_attempt', 'controller_sha', 'frozen_source_sha',
               'order', 'trial', 'workload_kind', 'requested_caller_qos', 'csv_path', 'csv_sha256']
ROUTE_FIELDS = META_FIELDS + list(KEY_FIELDS) + [
    'samples', 'population_complete', 'budget_us', 'observed_p50_us', 'observed_p99_us',
    'p99_us', 'p99_pass', 'max_wall_us', 'max_callback_index_first',
    'max_callback_index_last', 'max_paired_cpu_us_first', 'deadline_misses',
    'misses_cpu_over_budget', 'misses_cpu_at_or_below_budget', 'misses_cpu_at_most_tenth_wall',
    'paced_samples', 'paced_misses', 'unpaced_samples', 'unpaced_misses',
    'after_scheduled_sleep_samples', 'after_scheduled_sleep_misses']
LIMITATIONS = [
    'CAB wall intervals and enclosing thread-CPU intervals have different boundaries; '
    'wall minus CPU is not exclusively scheduling delay, and CPU time is not proof of DSP execution.',
    'Sentinel intervals contain fixed arithmetic, no CAB, IR worker, automation, editor, or audio callback. '
    'Their exposure differs from CAB intervals; do not subtract sentinel elapsed times from CAB timings.',
    'Paced means the first 1200 samples, with a 1 ms sleep after indices divisible by 8. '
    'The index-modulo-8 residue 1 is a scheduled-sleep position proxy, not measured wake-up latency.',
    'Caller QoS observations are snapshots, not traces of priority boosts, processor migration, or host activity.',
    'The ordinary gate is separate from all four diagnostic CAB and four sentinel populations. '
    'The 66 entries are route populations, not 66 CPU-only envelopes or a DAW/CoreAudio routing matrix.',
    'The ordinary CTest route order is original, expanded, extended, layouts; the diagnostic probe order '
    'is original, expanded, layouts, extended. The ordinary gate is not an identical-order A/A trial.',
    'Caller QoS also covers preparation, warmup, and sleeps and may change worker publication or fade timing. '
    'A QoS comparison measures the whole test-context intervention; retain the separate forced-publication phase.',
    'No product performance fix, no-glitch guarantee, or relaxed acceptance policy follows from this verification.',
]


def require(ok, message):
    if not ok:
        raise ValueError(message)


def loads(data):
    def reject_constant(value):
        raise ValueError('non-finite JSON constant: ' + value)
    def unique(pairs):
        result = {}
        for key, value in pairs:
            require(key not in result, 'duplicate JSON key: ' + key)
            result[key] = value
        return result
    return json.loads(data, parse_constant=reject_constant, object_pairs_hook=unique)


def sha(data):
    return hashlib.sha256(data).hexdigest()


def digest(value):
    result = str(value).removeprefix('sha256:').lower()
    require(re.fullmatch('[0-9a-f]{64}', result), 'invalid SHA-256')
    return result


def integer(value):
    require(re.fullmatch(r'-?[0-9]+', str(value)), 'invalid integer: ' + str(value))
    return int(value)


def number(value):
    result = float(value)
    require(math.isfinite(result), 'non-finite numeric field')
    return result


def close(actual, expected, label, printed=False):
    # CSV/JSON preserve 17 significant digits; ordinary C++ logs print 12.
    require(math.isclose(number(actual), expected,
                         rel_tol=6e-12 if printed else 1e-12,
                         abs_tol=1e-9), label + ': numeric mismatch')


def grid(sentinel=False):
    return {(('scheduler-sentinel-' if sentinel else '') + suite,
             ('sentinel-' if sentinel else '') + engine, sr, block, channels)
            for suite, engines, blocks in (
                ('integration', ('original-v1', 'expanded-v2', 'array-v3-6x10'), (64, 256)),
                ('extended', ('expanded-v2',), (32, 64, 128, 256, 512)))
            for engine in engines for sr in (44100, 48000, 96000)
            for block in blocks for channels in (1, 2)}


def row_key(row):
    return (row['suite'], row['engine'], integer(row['sample_rate']),
            integer(row['block_size']), integer(row['channels']))


def log_key(record):
    return (record['suite'], record['engine'], int(number(record['sr'])),
            integer(record['block']), integer(record['channels']))


def phases(sentinel):
    return ('paced_control', 'burst_control') if sentinel else (
        'worker_and_automation', 'forced_six_slot_publication')


def check_row(row, index, key, budget, sentinel, previous_end):
    require(None not in row and None not in row.values(), 'malformed CSV row')
    require(row_key(row) == key and row['schema_version'] == '1', 'raw identity/schema mismatch')
    require(integer(row['callback_index']) == index, 'missing, duplicated, or out-of-order callback')
    require(row['phase'] == phases(sentinel)[index >= 1200], 'phase mismatch')
    require(integer(row['phase_index']) == (index if index < 1200 else index - 1200), 'phase index mismatch')
    require(row['automation_requested'] == str(int(not sentinel and index < 80)), 'automation mismatch')
    if sentinel:
        require(row['workload'] == 'arithmetic_only' and row['worker_running'] == '0', 'sentinel workload mismatch')
    wall = number(row['wall_us'])
    require(wall >= 0, 'negative wall duration')
    close(row['budget_us'], budget, 'unchanged block-period deadline')
    require(row['deadline_miss'] == str(int(wall > budget)), 'deadline miss flag disagrees with wall time')
    require(row['thread_cpu_valid'] == '1' and row['thread_cycles_valid'] == '0', 'native clock validity mismatch')
    require(row['clock_start_error'] == row['clock_end_error'] == '0', 'native clock read error')
    cpu, start, end = (number(row[f]) for f in ('thread_cpu_us', 'thread_cpu_start_us', 'thread_cpu_end_us'))
    require(0 <= start <= end and cpu >= 0, 'invalid CPU endpoints')
    close(cpu, end - start, 'paired CPU delta')
    require(previous_end is None or start >= previous_end, 'cumulative CPU clock moved backwards')
    require(all(integer(row[f]) == 0 for f in ('thread_cycles', 'thread_cycles_start', 'thread_cycles_end')),
            'unused native cycle fields must remain zero')
    return end


def stats(rows, budget):
    if not rows:
        return dict(samples=0, deadline_misses=0, misses_cpu_over_budget=0,
                    misses_cpu_at_or_below_budget=0, misses_cpu_at_most_tenth_wall=0,
                    max_wall_us=None, observed_p50_us=None, observed_p99_us=None)
    ordered = sorted(rows, key=lambda r: (number(r['wall_us']), integer(r['callback_index'])))
    misses = [r for r in rows if r['deadline_miss'] == '1']
    return dict(samples=len(rows), deadline_misses=len(misses),
                misses_cpu_over_budget=sum(number(r['thread_cpu_us']) > budget for r in misses),
                misses_cpu_at_or_below_budget=sum(number(r['thread_cpu_us']) <= budget for r in misses),
                misses_cpu_at_most_tenth_wall=sum(number(r['thread_cpu_us']) <= .1 * number(r['wall_us']) for r in misses),
                max_wall_us=number(ordered[-1]['wall_us']),
                observed_p50_us=number(ordered[len(rows) // 2]['wall_us']),
                observed_p99_us=number(ordered[int(len(rows) * .99)]['wall_us']))


def parse_lines(text):
    return [(line.split(' ', 1)[0], dict(re.findall(r'([A-Za-z0-9_]+)=([^\s]+)', line)), line)
            for line in text.splitlines() if line.strip()]


def compare_summary(reported, population, diagnostic):
    routes, misses = population['routes'], population['raw_misses']
    require(population['complete'], 'reported full summary has missing/incomplete raw populations')
    require(reported['diagnostic_only'] is diagnostic, 'summary diagnostic/gate identity mismatch')
    expected_routes = []
    for route in routes:
        expected_routes.append(dict(key=list(route['_key']), samples=route['samples'], budget_us=route['budget_us'],
            p50_us=route['observed_p50_us'], p99_us=route['p99_us'], p99_pass=route['p99_pass'],
            max_wall_us=route['max_wall_us'], max_callback_index=route['max_callback_index_first'],
            max_paired_cpu_us=route['max_paired_cpu_us_first'], deadline_misses=route['deadline_misses'],
            cpu_over_budget_misses=route['misses_cpu_over_budget'], csv_path=route['_relative'], sha256=route['csv_sha256']))
    require(reported['routes'] == expected_routes, 'reported route p50/p99/max/paired CPU/miss/hash summary differs from raw')
    expected = dict(route_count=66, callbacks=105600,
        p99_pass_count=sum(r['p99_pass'] for r in routes), deadline_misses=len(misses),
        max_wall_us=max(r['max_wall_us'] for r in routes),
        misses_with_cpu_over_budget=sum(r['misses_cpu_over_budget'] for r in routes), all_misses=misses)
    for key, value in expected.items():
        require(reported[key] == value, 'reported group ' + key + ' differs from raw')


def compare_timing_logs(text, routes, sentinel, require_full):
    """Check every present numerical receipt; require all receipts on completion."""
    by_key = {r['_key']: r for r in routes}
    timing_seen, pair_seen, phase_seen, worker_seen = set(), set(), set(), set()
    worker_failures = []
    current = None
    for prefix, receipt, _ in parse_lines(text):
        if prefix == 'PAIRED_TIMING':
            key = log_key(receipt); current = key
            require(key in by_key, 'paired log route has no verified raw CSV')
            phase = receipt['phase']; pair_id = (key, phase)
            require(pair_id not in pair_seen, 'duplicate paired timing receipt'); pair_seen.add(pair_id)
            phase_names = ('all', 'paced_synthetic', 'unpaced_synthetic') if sentinel else (
                'all', 'worker_and_automation', 'forced_six_slot_publication')
            require(phase in phase_names, 'unexpected paired phase')
            rows = by_key[key]['_rows']
            selected = rows if phase == 'all' else rows[:1200] if phase == phase_names[1] else rows[1200:]
            require(bool(selected), 'paired phase has no raw rows')
            ordered = sorted(selected, key=lambda r: (number(r['wall_us']), integer(r['callback_index'])))
            expected = dict(samples=len(selected), thread_cpu_valid_samples=len(selected),
                            thread_cycles_valid_samples=0, misses=sum(r['deadline_miss'] == '1' for r in selected),
                            misses_with_thread_cycles=0)
            for field, value in expected.items():
                require(integer(receipt[field]) == value, 'paired log ' + field + ' mismatch')
            # Frozen C++ uses LAST maximum when tied; runner JSON uses FIRST.
            for label, selected_row in [('wall_p99', ordered[int(len(ordered) * .99)]), ('wall_max', ordered[-1])]:
                for field in ['callback_index', 'thread_cpu_valid', 'thread_cycles', 'thread_cycles_valid']:
                    require(integer(receipt[label + '_' + field]) == integer(selected_row[field]), 'paired log index/validity/cycle mismatch')
                for field in ['wall_us', 'thread_cpu_us']:
                    close(receipt[label + '_' + field], number(selected_row[field]), 'paired log ' + field, printed=True)
        elif prefix in ('TIMING', 'SENTINEL_TIMING'):
            require((prefix == 'SENTINEL_TIMING') == sentinel, 'CAB/sentinel timing receipt crossed')
            key = log_key(receipt) if sentinel else current
            require(key in by_key, 'timing receipt has no preceding matching raw route')
            require((receipt['engine'], int(number(receipt['sr'])), integer(receipt['block']), integer(receipt['channels'])) == key[1:],
                    'timing receipt identity mismatch')
            require(key not in timing_seen, 'duplicate timing receipt'); timing_seen.add(key)
            route = by_key[key]
            for field, expected in [('p50_us', route['observed_p50_us']), ('p99_us', route['observed_p99_us']), ('max_us', route['max_wall_us'])]:
                close(receipt[field], expected, 'timing log ' + field, printed=True)
            require(receipt['misses'] == str(route['deadline_misses']) + '/' + str(route['samples']), 'timing log misses mismatch')
            if sentinel:
                require(receipt['acceptance_gate'] == 'none' and integer(receipt['samples']) == route['samples'], 'sentinel gate/sample receipt mismatch')
                close(receipt['budget_us'], route['budget_us'], 'sentinel budget', printed=True)
            else:
                cpus = sorted(number(r['thread_cpu_us']) for r in route['_rows'])
                close(receipt['thread_cpu_p99_us'], cpus[int(len(cpus) * .99)], 'independent CPU p99', printed=True)
        elif prefix == 'PHASE':
            require(not sentinel and current in by_key, 'CAB phase receipt has no raw route')
            phase = receipt['name']; require(phase in phases(False), 'unexpected CAB phase receipt')
            phase_id = (current, phase); require(phase_id not in phase_seen, 'duplicate CAB phase receipt'); phase_seen.add(phase_id)
            rows = [r for r in by_key[current]['_rows'] if r['phase'] == phase]
            observed = stats(rows, by_key[current]['budget_us'])
            require(integer(receipt['blocks']) == len(rows) and integer(receipt['misses']) == observed['deadline_misses'], 'phase count/miss mismatch')
            for field, value in [('p50_us', observed['observed_p50_us']), ('p99_us', observed['observed_p99_us']), ('max_us', observed['max_wall_us'])]:
                close(receipt[field], value, 'phase log ' + field, printed=True)
            cpus = sorted(number(r['thread_cpu_us']) for r in rows)
            close(receipt['thread_cpu_p99_us'], cpus[int(len(cpus) * .99)], 'phase CPU p99', printed=True)
        elif prefix == 'WORKER_SCHEDULING':
            key = log_key(receipt); require(not sentinel and key in by_key, 'worker receipt has no CAB raw route')
            require(key not in worker_seen, 'duplicate worker scheduling receipt'); worker_seen.add(key)
            require(receipt['worker_priority_query'] == 'native', 'worker receipt is not a native observation')
            require(receipt['requested_juce_priority'] == '-1', 'IR worker priority intervention was not allowed')
            if receipt['start_succeeded'] != '1' or receipt['entered_run'] != '1' or receipt['observed_juce_priority'] != '-1':
                worker_failures.append(dict(key=list(key), receipt=receipt))
            # Worker start/observed-priority failures are retained as execution failures elsewhere;
            # recording them is not itself corruption. Do not replace them with a pass.
        elif prefix in ('PROFILE', 'PROFILE_PROTOCOL', 'WORKER_PROFILE'):
            raise ValueError('stage profiling changed the diagnostic measurement envelope')
    if require_full:
        require(timing_seen == set(by_key), 'missing full timing receipt grid')
        expected_phases = ('all', 'paced_synthetic', 'unpaced_synthetic') if sentinel else ('all', *phases(False))
        require(pair_seen == {(key, phase) for key in by_key for phase in expected_phases}, 'missing full paired receipt grid')
        if not sentinel:
            require(phase_seen == {(key, phase) for key in by_key for phase in phases(False)}, 'missing full phase receipt grid')
            require(worker_seen == set(by_key), 'missing native worker scheduling receipt grid')
    return dict(timing_receipts=len(timing_seen), paired_receipts=len(pair_seen), phase_receipts=len(phase_seen),
                worker_receipts=len(worker_seen), worker_failures=worker_failures)


def check_qos(text, requested, sentinel, completed, command):
    parsed = parse_lines(text)
    result = {'requested': requested, 'contexts': [], 'qos_requests': [], 'results': []}
    counts = Counter(prefix for prefix, _, _ in parsed)
    expected_qos = 17 if requested == 'inherited' else 33
    for prefix, receipt, _ in parsed:
        if prefix == 'SCHEDULING_DIAGNOSTIC_PROTOCOL':
            for key, value in dict(diagnostic_only='1', product_change='0', frozen_source=FROZEN_SHA,
                caller_qos=requested, workload='synthetic-sentinel' if sentinel else 'unchanged-frozen-cab',
                worker_priority_unchanged='1', process_priority_unchanged='1', realtime_policy_requested='0',
                no_daw_or_coreaudio_callback='1').items():
                require(receipt[key] == value, 'diagnostic protocol mismatch: ' + key)
        elif prefix == 'SCHEDULING_DIAGNOSTIC_CLOCK':
            for key in ('steady_clock_is_steady', 'clocks_have_different_envelopes',
                        'wall_minus_cpu_not_exclusive_scheduler_time', 'cpu_clock_not_proof_of_dsp_execution'):
                require(receipt[key] == '1', 'clock protocol mismatch')
            require(receipt['thread_cpu_clock_getres_result'] == receipt['thread_cpu_clock_getres_error'] == '0', 'clock resolution query failed')
            require(number(receipt['thread_cpu_resolution_ns']) > 0, 'invalid CPU clock resolution')
            result['clock'] = receipt
        elif prefix == 'SCHEDULING_DIAGNOSTIC_CONTEXT':
            require(receipt['caller_context'] == 'macos_base_qos', 'non-macOS caller context')
            if receipt['phase'] != 'entry':
                require(integer(receipt['caller_thread_qos']) == expected_qos, 'actual caller QoS differs from intended condition')
            result['contexts'].append(receipt)
        elif prefix == 'SCHEDULING_DIAGNOSTIC_QOS':
            require(receipt['mode'] == requested, 'QoS mode receipt mismatch')
            require(receipt['setter_called'] == str(int(requested == 'user-interactive')), 'QoS setter receipt mismatch')
            if requested == 'user-interactive' and completed:
                require(receipt['setter_result'] == '0' and receipt['requested'] == receipt['observed'] == '33', 'QoS intervention not established')
            result['qos_requests'].append(receipt)
        elif prefix == 'SCHEDULING_DIAGNOSTIC_RESULT':
            require(receipt['diagnostic_only'] == '1' and receipt['product_acceptance'] == 'unchanged_external_gate', 'diagnostic result identity mismatch')
            require(integer(receipt['workload_exit']) == command['returncode'], 'workload exit differs from command receipt')
            result['results'].append(receipt)
        elif prefix == 'SCHEDULER_SENTINEL_PROTOCOL':
            require(sentinel, 'sentinel protocol in CAB command')
            for key, value in dict(diagnostic_only='1', acceptance_gate='none', audio_callbacks='0',
                cab_instances='0', ir_workers='0', ui_instances='0', measured_intervals='1600',
                paced_intervals='1200', unpaced_intervals='400', sleep_ms='1', sleep_every='8',
                arithmetic_iterations='256', synthetic_exposure_differs_from_cab='1', no_elapsed_time_subtraction='1').items():
                require(receipt[key] == value, 'sentinel protocol mismatch: ' + key)
        elif prefix in ('CALLER_SCHEDULING', 'WORKER_SCHEDULING'):
            require(receipt.get('caller_context') == 'macos_base_qos'
                    and integer(receipt['caller_thread_qos']) == expected_qos, 'frozen CAB caller QoS receipt mismatch')
    selected = [line for prefix, _, line in parsed if prefix in (
        'SCHEDULING_DIAGNOSTIC_CONTEXT', 'SCHEDULING_DIAGNOSTIC_QOS', 'SCHEDULING_DIAGNOSTIC_RESULT')]
    require(command.get('scheduling') == selected, 'runner scheduling receipt differs from original log')
    if completed:
        for prefix in ('SCHEDULING_DIAGNOSTIC_PROTOCOL', 'SCHEDULING_DIAGNOSTIC_CLOCK', 'SCHEDULING_DIAGNOSTIC_QOS', 'SCHEDULING_DIAGNOSTIC_RESULT'):
            require(counts[prefix] == 1, 'missing/duplicate scheduling receipt: ' + prefix)
        require([r['phase'] for r in result['contexts']] == ['entry', 'before-workload', 'after-workload'], 'context snapshots missing/duplicated/out of order')
        require(counts['SCHEDULER_SENTINEL_PROTOCOL'] == int(sentinel), 'sentinel protocol missing or crossed')
    result['actual_qos_available'] = any(r['phase'] == 'before-workload' for r in result['contexts'])
    return result


class Audit:
    def __init__(self, spec, base):
        self.spec, self.base = spec, base
        self.errors, self.notes, self.routes, self.misses, self.groups, self.qos = [], [], [], [], [], []
        self.phase_rows, self.residue_rows = [], []
        self.result = {'archive': spec.get('label'), 'integrity_verified': False, 'errors': self.errors,
                       'notes': self.notes, 'groups': self.groups, 'limitations': LIMITATIONS}

    def guarded(self, context, operation):
        try:
            return operation()
        except (ValueError, KeyError, TypeError, IndexError, ET.ParseError) as error:
            self.errors.append(context + ': ' + str(error))
            return None

    def read_json(self, path):
        return loads(self.files[path])

    def source_and_report(self):
        a, report = self.spec, self.report
        require(report['schema_version'] == 1 and report['diagnostic_only'] is True
                and report['product_fix_demonstrated'] is False, 'invalid diagnostic report identity')
        require(report['frozen_product_sha'] == a['expected_frozen_sha'] == FROZEN_SHA, 'frozen product SHA mismatch')
        require(a['expected_juce_sha'] == JUCE_SHA, 'expected JUCE SHA must remain pinned')
        require(a['expected_order'] in ORDERS, 'unknown prespecified order')
        require(report['planned_order'] == [list(p) for p in ORDERS[a['expected_order']]], 'order differs from prespecified manifest')
        require(report.get('order_name', a['expected_order']) == a['expected_order'], 'reported order name mismatch')
        require(isinstance(report['complete'], bool), 'complete must be boolean')
        self.result['declared_complete'] = report['complete']
        self.result['reported_error'] = report.get('error')
        before = report['source_before']
        require(before['sha'] == FROZEN_SHA and before['clean'] is True, 'source before is not frozen/clean')
        after = report.get('source_after')
        if after:
            require(after['sha'] == FROZEN_SHA and after['clean'] is True and after['tree'] == before['tree'], 'source after changed/dirty')
        elif report['complete']:
            raise ValueError('complete report lacks source-after attestation')
        self.result['source_after_verified'] = bool(after)
        self.result['juce_verified'] = report.get('juce_sha') == JUCE_SHA
        if 'juce_sha' in report or report['complete']:
            require(self.result['juce_verified'], 'JUCE SHA mismatch or absent on completion')
        for stage in ('before', 'after'):
            key = 'environment_' + stage
            if key not in report:
                require(stage == 'after' and not report['complete'], 'missing environment attestation')
                continue
            env = self.read_json(stage + '/environment.json')
            require(report[key] == env, 'embedded environment differs from original member')
            runner = env['runner']
            require(runner['RUNNER_OS'] == 'macOS' and env['uname']['system'] == 'Darwin', 'non-native macOS artifact')
            require(str(runner['GITHUB_RUN_ID']) == str(a['expected_run_id'])
                    and str(runner['GITHUB_RUN_ATTEMPT']) == str(a['expected_run_attempt']), 'run identity mismatch')
        controller = report['controller']
        require({'CabSchedulingProbe.cpp', 'CMakeLists.txt', 'run_experiment.py'} <= set(controller), 'missing controller file attestations')
        for name, value in controller.items():
            require(PurePosixPath(name).name == name, 'controller hash has invalid path'); digest(value)
        if 'expected_controller_files' in a:
            require(controller == a['expected_controller_files'], 'controller file hashes differ from independently bound sources')
        self.result['controller_files_independently_compared'] = 'expected_controller_files' in a
        self.result['controller_file_hashes'] = controller
        self.result['environment_before'] = report['environment_before']
        if 'environment_after' in report:
            self.result['environment_after'] = report['environment_after']

    def population(self, group):
        prefix, sentinel = group['prefix'], group['workload_kind'] == 'sentinel'
        paths = sorted(p for p in self.files if p.startswith(prefix) and p.endswith('.csv'))
        routes, raw_misses, keys = [], [], set()
        group['raw_csv_files_observed'] = len(paths)
        for path in paths:
            def read_route():
                raw = self.files[path]
                reader = csv.DictReader(io.StringIO(raw.decode('utf-8')))
                require(reader.fieldnames == (SENTINEL_FIELDS if sentinel else FIELDS), 'raw CSV columns changed')
                rows = list(reader)
                require(0 < len(rows) <= 1600, 'CSV has zero or more than 1600 samples')
                key = row_key(rows[0]); require(key in grid(sentinel) and key not in keys, 'duplicate or unexpected raw population')
                relative = f'{key[0]}/{key[1]}/{key[2]}-{key[3]}-{key[4]}.csv'
                require(path == prefix + relative, 'CSV path and population identity differ')
                budget, previous_end = key[3] * 1e6 / key[2], None
                for index, row in enumerate(rows):
                    try:
                        previous_end = check_row(row, index, key, budget, sentinel, previous_end)
                    except ValueError as error:
                        raise ValueError(f'callback {index}: {error}') from error
                keys.add(key)
                meta = dict(self.meta, trial=group['trial'], workload_kind=group['workload_kind'],
                    requested_caller_qos=group['requested_caller_qos'], csv_path=path, csv_sha256=sha(raw))
                observed = stats(rows, budget)
                first_max = next(r for r in rows if number(r['wall_us']) == observed['max_wall_us'])
                last_max = next(r for r in reversed(rows) if number(r['wall_us']) == observed['max_wall_us'])
                paced = [r for r in rows if integer(r['callback_index']) < 1200]
                unpaced = rows[1200:]
                after_sleep = [r for r in paced if integer(r['callback_index']) % 8 == 1]
                complete = len(rows) == 1600
                route = dict(meta, **dict(zip(KEY_FIELDS, key)), **observed, population_complete=complete,
                    budget_us=budget, p99_us=observed['observed_p99_us'] if complete else None,
                    p99_pass=observed['observed_p99_us'] < budget if complete else None,
                    max_callback_index_first=integer(first_max['callback_index']),
                    max_callback_index_last=integer(last_max['callback_index']),
                    max_paired_cpu_us_first=number(first_max['thread_cpu_us']),
                    paced_samples=len(paced), paced_misses=sum(r['deadline_miss'] == '1' for r in paced),
                    unpaced_samples=len(unpaced), unpaced_misses=sum(r['deadline_miss'] == '1' for r in unpaced),
                    after_scheduled_sleep_samples=len(after_sleep),
                    after_scheduled_sleep_misses=sum(r['deadline_miss'] == '1' for r in after_sleep),
                    _key=key, _relative=relative, _rows=rows)
                routes.append(route); self.routes.append(route)
                for row in rows:
                    if row['deadline_miss'] == '1':
                        self.misses.append(dict(meta, **row))
                        raw_misses.append(dict(row, csv_path=relative))
                for phase in phases(sentinel):
                    subset = [r for r in rows if r['phase'] == phase]
                    self.phase_rows.append(dict(meta, **dict(zip(KEY_FIELDS, key)), phase=phase, **stats(subset, budget)))
            self.guarded(path, read_route)
        complete = keys == grid(sentinel) and all(r['population_complete'] for r in routes)
        group.update(observed_routes=len(routes), observed_samples=sum(r['samples'] for r in routes),
            observed_complete_routes=sum(r['population_complete'] for r in routes), complete=complete,
            missing_routes=[list(k) for k in sorted(grid(sentinel) - keys)],
            observed_deadline_misses=sum(r['deadline_misses'] for r in routes),
            full_population_deadline_misses=sum(r['deadline_misses'] for r in routes) if complete else None,
            p99_pass_count=sum(r['p99_pass'] is True for r in routes),
            max_wall_us=max((r['max_wall_us'] for r in routes), default=None),
            misses_cpu_over_budget=sum(r['misses_cpu_over_budget'] for r in routes),
            misses_cpu_at_or_below_budget=sum(r['misses_cpu_at_or_below_budget'] for r in routes),
            misses_cpu_at_most_tenth_wall=sum(r['misses_cpu_at_most_tenth_wall'] for r in routes))
        for field in ('paced_samples', 'paced_misses', 'unpaced_samples', 'unpaced_misses',
                      'after_scheduled_sleep_samples', 'after_scheduled_sleep_misses'):
            group[field] = sum(r[field] for r in routes)
        for residue in range(8):
            selected = [(row, r['budget_us']) for r in routes for row in r['_rows']
                        if integer(row['callback_index']) < 1200 and integer(row['callback_index']) % 8 == residue]
            self.residue_rows.append(dict(self.meta, trial=group['trial'], workload_kind=group['workload_kind'],
                requested_caller_qos=group['requested_caller_qos'], paced_index_mod8=residue,
                scheduled_sleep_position_proxy=residue == 1, observed_samples=len(selected),
                observed_deadline_misses=sum(r['deadline_miss'] == '1' for r, _ in selected),
                misses_cpu_over_budget=sum(r['deadline_miss'] == '1' and number(r['thread_cpu_us']) > budget for r, budget in selected),
                full_population_complete=complete))
        return dict(routes=routes, raw_misses=raw_misses, complete=complete)

    def ordinary(self, population):
        report, complete = self.report, self.report['complete']
        ordinary = report.get('ordinary_gate')
        if not ordinary:
            require(not complete, 'missing ordinary gate receipt on completion')
            return dict(state='not_run', passed=False, tests_observed=0)
        inventory = self.read_json('ordinary-gate/ctest-inventory.json')
        require(sorted(t['name'] for t in inventory['tests']) == sorted(TESTS), 'ordinary inventory differs from five frozen CTests')
        junit_path = 'ordinary-gate/ctest-results.xml'
        if junit_path not in self.files:
            require(not complete, 'complete report lacks ordinary JUnit')
            return dict(state='incomplete', passed=False, tests_observed=0, returncode=ordinary['returncode'])
        junit = ET.fromstring(self.files[junit_path]); cases = list(junit.iter('testcase'))
        require(sorted(t.attrib['name'] for t in cases) == sorted(TESTS), 'ordinary JUnit missing or duplicated CTests')
        tests = [dict(name=t.attrib['name'], passed=t.attrib.get('status') == 'run'
            and all(t.find(f) is None for f in ('failure', 'error', 'skipped')), seconds=float(t.attrib.get('time', '0'))) for t in cases]
        require(ordinary.get('tests') == tests, 'ordinary reported test outcomes differ from JUnit')
        text = '\n'.join(t.findtext('system-out', '') for t in cases)
        require('This part of the test output was removed' not in text, 'ordinary JUnit output truncated')
        log_checks = compare_timing_logs(text, population['routes'], False, population['complete'])
        cmd = ordinary['command']
        for flag, value in [('--parallel', '1'), ('--timeout', '600'), ('-C', 'Release')]:
            require(flag in cmd and cmd[cmd.index(flag) + 1] == value, 'ordinary command condition changed')
        require('--verbose' in cmd and '--no-tests=error' in cmd, 'ordinary test condition changed')
        require('-R' in cmd and cmd[cmd.index('-R') + 1] == '^(' + '|'.join(TESTS) + ')$', 'ordinary selected test regex changed')
        passed = all(t['passed'] for t in tests) and ordinary['returncode'] == 0
        if passed and population['complete']:
            require(all(r['p99_pass'] for r in population['routes']), 'ordinary gate reports pass despite raw p99 failure')
        return dict(state=('passed' if passed else 'failed') if population['complete'] else 'incomplete',
                    passed=passed and population['complete'], tests_observed=len(tests), tests=tests,
                    returncode=ordinary['returncode'], log_checks=log_checks)

    def commands(self, group, population, trial):
        sentinel = group['workload_kind'] == 'sentinel'
        commands = trial.get('sentinel_commands' if sentinel else 'commands', []) if trial else []
        require(len(commands) <= 4, 'more than four commands in one population')
        if self.report['complete']:
            require(len(commands) == 4, 'missing diagnostic suite commands')
        text = []
        for index, command in enumerate(commands):
            name, flags = SUITES[index]
            args = ['--caller-qos', group['requested_caller_qos']]
            if sentinel: args.append('--scheduler-sentinel')
            args += flags
            require(command['command'][1:] == args, 'diagnostic flags or suite order changed')
            require(PurePosixPath(command['command'][0]).name == 'ChimeraCabSchedulingProbe', 'different diagnostic executable')
            log = 'trials/' + group['trial'] + ('/sentinel' if sentinel else '') + '/' + name + '.log'
            require(PurePosixPath(command['log']).name == name + '.log', 'command log identity mismatch')
            content = self.files[log].decode('utf-8'); text.append(content)
            completed = any(p == 'SCHEDULING_DIAGNOSTIC_RESULT' for p, _, _ in parse_lines(content))
            receipt = check_qos(content, group['requested_caller_qos'], sentinel, completed, command)
            if completed and command['returncode'] == 0 and not sentinel:
                parsed = parse_lines(content)
                callbacks = [r for p, r, _ in parsed if p == 'CALLBACK']
                require(len(callbacks) == 1 and callbacks[0]['callback_new'] == callbacks[0]['callback_delete'] == '0',
                        'successful CAB command lacks unchanged allocation receipt')
                require(all(r['start_succeeded'] == r['entered_run'] == '1' and r['observed_juce_priority'] == '-1'
                            for p, r, _ in parsed if p == 'WORKER_SCHEDULING'),
                        'successful CAB command conflicts with worker scheduling failure')
                measured_keys = {log_key(r) for p, r, _ in parsed if p == 'PAIRED_TIMING'}
                require(all(r['p99_pass'] is True for r in population['routes'] if r['_key'] in measured_keys),
                        'successful CAB command conflicts with raw p99 failure')
            self.qos.append(dict(self.meta, trial=group['trial'], workload_kind=group['workload_kind'], suite_command=name,
                                 returncode=command['returncode'], log=log, **receipt))
            if self.report['complete']:
                require(completed, 'complete report has unfinished diagnostic command')
        if text or population['complete']:
            return compare_timing_logs('\n'.join(text), population['routes'], sentinel, population['complete'])
        return dict(timing_receipts=0, paired_receipts=0, phase_receipts=0, worker_receipts=0)

    def run(self):
        a = self.spec
        path = Path(a['path']); path = path if path.is_absolute() else self.base / path
        payload = path.read_bytes()
        require(sha(payload) == digest(a['sha256']), 'archive hash differs from expected download digest')
        artifact = a['artifact_metadata']
        require(artifact['id'] == a['artifact_id'] and artifact['size_in_bytes'] == len(payload), 'artifact id/size metadata mismatch')
        require(digest(artifact['digest']) == sha(payload), 'GitHub artifact digest mismatch')
        require(str(artifact['workflow_run']['id']) == str(a['expected_run_id'])
                and artifact['workflow_run']['head_sha'] == a['expected_controller_sha'], 'controller/run artifact metadata mismatch')
        require(re.fullmatch('[0-9a-f]{40}', a['expected_controller_sha']), 'invalid controller commit SHA')
        self.meta = dict(archive=a['label'], run_id=str(a['expected_run_id']), run_attempt=str(a['expected_run_attempt']),
            controller_sha=a['expected_controller_sha'], frozen_source_sha=a['expected_frozen_sha'], order=a['expected_order'])
        self.result.update(self.meta, artifact_id=a['artifact_id'], archive_sha256=sha(payload),
                           artifact_url=artifact['url'], controller_binding='external GitHub artifact workflow_run.head_sha')
        with zipfile.ZipFile(io.BytesIO(payload)) as archive:
            names = archive.namelist(); require(len(names) == len(set(names)), 'duplicate ZIP members')
            for name in names:
                require(not name.startswith('/') and '\\' not in name and '..' not in PurePosixPath(name).parts, 'unsafe ZIP member path')
            self.files = {p: archive.read(p) for p in names if not p.endswith('/')}
        checksums = self.read_json('SHA256SUMS.json')
        require(set(checksums) == set(self.files) - {'SHA256SUMS.json'}, 'checksum manifest has missing or extra members')
        for name, value in checksums.items():
            require(sha(self.files[name]) == digest(value), 'member checksum mismatch: ' + name)
        self.result['integrity_verified'] = True
        self.result['member_count'] = len(self.files)
        self.report = self.read_json('report.json')
        self.guarded('source/report', self.source_and_report)
        report = self.report
        expected_order = ORDERS[a['expected_order']]
        trials = report.get('trials', [])
        self.guarded('trial ordering', lambda: require(
            [(t['label'], t['requested_caller_qos']) for t in trials] == expected_order[:len(trials)]
            and len(trials) <= 4 and (not report['complete'] or len(trials) == 4), 'trial order/identity/completeness mismatch'))
        by_trial = {t['label']: t for t in trials}
        definitions = [('ordinary', 'ordinary-cab', 'unmodified-ordinary', 'ordinary-gate/raw/')]
        for trial, qos in expected_order:
            definitions += [(trial, 'diagnostic-cab', qos, f'trials/{trial}/cab-timing-diagnostics/'),
                            (trial, 'sentinel', qos, f'trials/{trial}/sentinel/cab-timing-diagnostics/')]
        roots = [d[3] for d in definitions]
        self.guarded('raw layout', lambda: require(all(any(p.startswith(root) for root in roots)
            for p in self.files if p.endswith('.csv')), 'unexpected CSV population outside declared independent roots'))
        for trial, kind, qos, prefix in definitions:
            group = dict(self.meta, trial=trial, workload_kind=kind, requested_caller_qos=qos, prefix=prefix,
                         expected_routes=66, expected_samples=105600)
            self.groups.append(group)
            population = self.population(group)
            if report['complete'] and not population['complete']:
                self.errors.append(prefix + ': declared complete but population grid/rows are incomplete')
            if trial == 'ordinary':
                self.result['ordinary_gate'] = self.guarded('ordinary gate', lambda: self.ordinary(population))
                entry, summary_key, summary_path = report.get('ordinary_gate', {}), 'summary', 'ordinary-gate/summary.json'
            else:
                entry = by_trial.get(trial, {})
                self.guarded(prefix + ' trial identity', lambda: require(not entry or entry['diagnostic_only'] is True, 'trial not diagnostic-only'))
                group['log_checks'] = self.guarded(prefix + ' commands/logs', lambda: self.commands(group, population, entry))
                summary_key = 'sentinel_summary' if kind == 'sentinel' else 'cab_summary'
                summary_path = f'trials/{trial}/sentinel/summary.json' if kind == 'sentinel' else f'trials/{trial}/cab-summary.json'
            def summary_check():
                if summary_key in entry or summary_path in self.files:
                    independent_member = self.read_json(summary_path)
                    require(entry[summary_key] == independent_member, 'embedded and separate reported summaries differ')
                    compare_summary(independent_member, population, trial != 'ordinary')
                    return True
                require(not report['complete'], 'complete report lacks numerical summary')
                return False
            group['reported_summary_matches_raw'] = self.guarded(prefix + ' summary', summary_check)
            # One full run has 950400 intervals. Keep only the current group's
            # detailed rows in memory; preserved miss fields and scalar results
            # remain available for the outputs, and the immutable ZIP is primary.
            for route in population['routes']:
                del route['_rows']
        all_commands = list(report.get('commands', []))
        if report.get('ordinary_gate'): all_commands.append(report['ordinary_gate'])
        for trial in trials:
            all_commands += trial.get('commands', []) + trial.get('sentinel_commands', [])
        self.result['recorded_command_failures'] = [dict(log=PurePosixPath(c.get('log', '')).name, returncode=c['returncode'])
            for c in all_commands if c['returncode'] != 0]
        if report['complete']:
            for field in ('probe_sha256',): self.guarded(field, lambda: digest(report[field]))
            self.guarded('product binary identities', lambda: require(set(report['product_binaries']) == set(TESTS[:2])
                and all(digest(v) for v in report['product_binaries'].values()), 'missing ordinary binary hashes'))
        self.result['observed_complete'] = all(g['complete'] for g in self.groups)
        self.result['observed_routes'] = len(self.routes)
        self.result['observed_samples'] = sum(r['samples'] for r in self.routes)
        self.result['expected_routes'] = 594
        self.result['expected_samples'] = 950400
        self.result['observed_deadline_misses'] = len(self.misses)
        ordinary_passed = bool(self.result.get('ordinary_gate') and self.result['ordinary_gate']['passed'])
        failure = bool(report.get('error') or self.result['recorded_command_failures'] or not ordinary_passed)
        self.result['recorded_execution_or_gate_failure'] = failure
        self.result['valid'] = not self.errors
        self.result['status'] = ('invalid' if self.errors else 'verified_complete' if report['complete']
            and self.result['observed_complete'] and not failure else 'verified_complete_with_failure' if report['complete']
            and self.result['observed_complete'] else 'verified_incomplete_failure')
        self.files.clear()
        return self


def write_json(path, value):
    path.write_text(json.dumps(value, indent=2, allow_nan=False) + '\n')


def write_csv(path, rows, fields=None):
    fields = fields or list(rows[0]) if rows else fields or ['archive']
    with path.open('w', newline='') as stream:
        writer = csv.DictWriter(stream, fieldnames=fields, extrasaction='ignore')
        writer.writeheader(); writer.writerows(rows)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--archives', type=Path, required=True)
    parser.add_argument('--archive-base', type=Path, help='Optional base directory for relative ZIP paths; default: manifest directory')
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    spec = loads(args.archives.read_text()); archives = spec['archives'] if isinstance(spec, dict) else spec
    require(isinstance(archives, list) and archives, 'manifest needs at least one archive')
    base = args.archive_base.resolve() if args.archive_base else args.archives.resolve().parent
    names = ('validation.json', 'route-summary.csv', 'deadline-misses.csv', 'condition-summary.json',
             'condition-summary.csv', 'condition-aggregate.json', 'condition-aggregate.csv',
             'phase-summary.csv', 'paced-residue-summary.csv', 'qos-receipts.json')
    inputs = {args.archives.resolve()}
    for a in archives:
        path = Path(a['path']); inputs.add((path if path.is_absolute() else base / path).resolve())
    require(all((args.output / name).resolve() not in inputs for name in names), 'output would overwrite an input')
    audits, seen = [], set()
    for archive in archives:
        audit = Audit(archive, base); audits.append(audit)
        try:
            require(archive['label'] not in seen, 'duplicate archive label'); seen.add(archive['label'])
            audit.run()
        except (OSError, ValueError, KeyError, TypeError, IndexError, zipfile.BadZipFile) as error:
            audit.errors.append(str(error)); audit.result.update(valid=False, status='invalid')
        print(f"{archive.get('label')}: {audit.result['status']}; observed "
              f"{audit.result.get('observed_routes', len(audit.routes))}/594 populations, "
              f"{audit.result.get('observed_samples', 0)}/950400 intervals")
    routes = [r for a in audits for r in a.routes]
    misses = [r for a in audits for r in a.misses]
    groups = [g for a in audits for g in a.groups]
    valid = all(a.result.get('valid') for a in audits)
    complete_success = valid and all(a.result['status'] == 'verified_complete' for a in audits)
    args.output.mkdir(parents=True, exist_ok=True)
    write_json(args.output / 'validation.json', dict(valid=valid, complete_success=complete_success,
        interpretation='Evidence verification only; sentinel synthetic intervals are not audio callbacks.',
        archives=[a.result for a in audits], limitations=LIMITATIONS))
    write_json(args.output / 'condition-summary.json', groups)
    write_json(args.output / 'qos-receipts.json', [r for a in audits for r in a.qos])
    write_csv(args.output / 'route-summary.csv', routes, ROUTE_FIELDS)
    write_csv(args.output / 'deadline-misses.csv', misses, META_FIELDS + SENTINEL_FIELDS)
    scalar_groups = [{k: v for k, v in g.items() if not isinstance(v, (list, dict))} for g in groups]
    write_csv(args.output / 'condition-summary.csv', scalar_groups)
    aggregates = []
    condition_keys = sorted({(g['archive'], g['workload_kind'], g['requested_caller_qos']) for g in groups})
    for archive, workload, qos in condition_keys:
        members = [g for g in groups if (g['archive'], g['workload_kind'], g['requested_caller_qos']) == (archive, workload, qos)]
        item = {k: members[0][k] for k in ('archive', 'run_id', 'run_attempt', 'controller_sha', 'frozen_source_sha', 'order')}
        item.update(workload_kind=workload, requested_caller_qos=qos,
                    planned_trial_count=len(members), complete_trial_count=sum(g['complete'] for g in members),
                    complete=all(g['complete'] for g in members))
        for field in ('expected_routes', 'expected_samples', 'observed_routes', 'observed_samples',
                      'observed_complete_routes', 'observed_deadline_misses', 'p99_pass_count',
                      'misses_cpu_over_budget', 'misses_cpu_at_or_below_budget', 'misses_cpu_at_most_tenth_wall',
                      'paced_samples', 'paced_misses', 'unpaced_samples', 'unpaced_misses',
                      'after_scheduled_sleep_samples', 'after_scheduled_sleep_misses'):
            item[field] = sum(g[field] for g in members)
        item['full_population_deadline_misses'] = item['observed_deadline_misses'] if item['complete'] else None
        item['max_wall_us'] = max((g['max_wall_us'] for g in members if g['max_wall_us'] is not None), default=None)
        aggregates.append(item)
    write_json(args.output / 'condition-aggregate.json', aggregates)
    write_csv(args.output / 'condition-aggregate.csv', aggregates)
    write_csv(args.output / 'phase-summary.csv', [r for a in audits for r in a.phase_rows])
    write_csv(args.output / 'paced-residue-summary.csv', [r for a in audits for r in a.residue_rows])
    return 0 if complete_success else 3 if valid else 2


if __name__ == '__main__':
    raise SystemExit(main())
