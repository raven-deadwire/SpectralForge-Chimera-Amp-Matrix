#!/usr/bin/env python3
"""Bounded verifier checks; generated intervals are synthetic test fixtures only.

Checks the preserved configure failure and targeted clock/summary/provenance
corruptions. It never runs DSP, fetches an artifact, or changes an original ZIP.
"""
import argparse
import copy
import csv
import io
import json
from pathlib import Path
import sys
import tempfile
import zipfile

sys.dont_write_bytecode = True
import analyze_diagnostic_artifacts as audit


def raw_rows(key, sentinel=False):
    rows, endpoint = [], 0
    for i in range(1600):
        cpu = 4000 if i == 8 else 2 if i == 9 else 3
        wall = 2000 if i == 8 else 5000 if i in (9, 10) else 100
        endpoint += 10
        r = dict(schema_version='1', engine=key[1], suite=key[0], sample_rate=str(key[2]),
                 block_size=str(key[3]), channels=str(key[4]), callback_index=str(i),
                 phase=audit.phases(sentinel)[i >= 1200], phase_index=str(i if i < 1200 else i - 1200),
                 automation_requested=str(int(not sentinel and i < 80)), wall_us=str(wall),
                 budget_us=str(key[3] * 1e6 / key[2]), deadline_miss=str(int(wall > key[3] * 1e6 / key[2])),
                 thread_cpu_us=str(cpu), thread_cpu_valid='1', thread_cycles='0', thread_cycles_valid='0',
                 clock_start_error='0', clock_end_error='0', thread_cpu_start_us=str(endpoint),
                 thread_cpu_end_us=str(endpoint + cpu), thread_cycles_start='0', thread_cycles_end='0')
        endpoint += cpu
        if sentinel: r.update(workload='arithmetic_only', worker_running='0')
        rows.append(r)
    return rows


def csv_bytes(rows, sentinel=False):
    stream = io.StringIO(newline='')
    writer = csv.DictWriter(stream, fieldnames=audit.SENTINEL_FIELDS if sentinel else audit.FIELDS)
    writer.writeheader(); writer.writerows(rows)
    return stream.getvalue().encode()


def synthetic_population(sentinel=False, count=66, partial=False):
    a = audit.Audit({'label': 'SYNTHETIC-VERIFIER-FIXTURE'}, Path('.'))
    a.meta = dict(archive='SYNTHETIC-VERIFIER-FIXTURE', run_id='0', run_attempt='0',
                 controller_sha='0' * 40, frozen_source_sha=audit.FROZEN_SHA, order='ABBA')
    prefix = 'raw/'
    a.files = {}
    for key in sorted(audit.grid(sentinel))[:count]:
        relative = f'{key[0]}/{key[1]}/{key[2]}-{key[3]}-{key[4]}.csv'
        rows = raw_rows(key, sentinel)
        if partial: rows = rows[:9]
        a.files[prefix + relative] = csv_bytes(rows, sentinel)
    group = dict(prefix=prefix, workload_kind='sentinel' if sentinel else 'diagnostic-cab',
                 trial='synthetic', requested_caller_qos='inherited')
    return a, group


def known_summary(population, diagnostic=True):
    """Fixture values are defined above, not copied from verifier output stats."""
    routes, all_misses = [], []
    for route in population['routes']:
        key = route['_key']; budget = key[3] * 1e6 / key[2]
        rows = route['_rows']; misses = [r for r in rows if int(r['wall_us']) > budget]
        relative = f'{key[0]}/{key[1]}/{key[2]}-{key[3]}-{key[4]}.csv'
        routes.append(dict(key=list(key), samples=1600, budget_us=budget, p50_us=100.0, p99_us=100.0,
            p99_pass=True, max_wall_us=5000.0, max_callback_index=9, max_paired_cpu_us=2.0,
            deadline_misses=len(misses), cpu_over_budget_misses=sum(int(r['thread_cpu_us']) > budget for r in misses),
            csv_path=relative, sha256=audit.sha(csv_bytes(rows, key[0].startswith('scheduler-sentinel-')))))
        all_misses += [dict(r, csv_path=relative) for r in misses]
    return dict(raw_root='synthetic-only', diagnostic_only=diagnostic, routes=routes, route_count=66,
                callbacks=105600, p99_pass_count=66, deadline_misses=len(all_misses), max_wall_us=5000.0,
                misses_with_cpu_over_budget=sum(r['cpu_over_budget_misses'] for r in routes), all_misses=all_misses)


def paired_log(route, sentinel=False):
    key = route['_key']; rows = route['_rows']; budget = key[3] * 1e6 / key[2]
    base = f'engine={key[1]} suite={key[0]} sr={key[2]} block={key[3]} channels={key[4]}'
    lines = []
    sections = [('all', 0, 1600, 1587, 10),
                ('paced_synthetic' if sentinel else 'worker_and_automation', 0, 1200, 1191, 10),
                ('unpaced_synthetic' if sentinel else 'forced_six_slot_publication', 1200, 1600, 1596, 1599)]
    for phase, start, end, p99, maximum in sections:
        miss_count = sum(int(r['wall_us']) > budget for r in rows[start:end])
        line = f'PAIRED_TIMING {base} phase={phase} samples={end-start} thread_cpu_valid_samples={end-start} '
        line += f'thread_cycles_valid_samples=0 misses={miss_count} misses_with_thread_cycles=0'
        for label, index in [('wall_p99', p99), ('wall_max', maximum)]:
            for field in ['callback_index', 'wall_us', 'thread_cpu_us', 'thread_cpu_valid', 'thread_cycles', 'thread_cycles_valid']:
                line += f' {label}_{field}={rows[index][field]}'
        lines.append(line)
    misses = sum(int(r['wall_us']) > budget for r in rows)
    if sentinel:
        lines.append(f'SENTINEL_TIMING {base} samples=1600 p50_us=100 p99_us=100 max_us=5000 budget_us={budget} misses={misses}/1600 acceptance_gate=none')
    else:
        lines += [f'PHASE name=worker_and_automation blocks=1200 p50_us=100 p99_us=100 thread_cpu_p99_us=3 max_us=5000 misses={misses}',
                  'PHASE name=forced_six_slot_publication blocks=400 p50_us=100 p99_us=100 thread_cpu_p99_us=3 max_us=100 misses=0',
                  f'TIMING engine={key[1]} sr={key[2]} block={key[3]} channels={key[4]} p50_us=100 p99_us=100 thread_cpu_p99_us=3 max_us=5000 misses={misses}/1600',
                  f'WORKER_SCHEDULING {base} start_succeeded=1 entered_run=1 requested_juce_priority=-1 observed_juce_priority=-1 worker_priority_query=native']
    return '\n'.join(lines)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--failed-archives', type=Path, required=True)
    parser.add_argument('--archive-base', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    results = []
    def positive(name, action):
        action(); results.append(dict(check=name, outcome='accepted_valid_fixture'))
    def corrupt(name, action):
        try:
            action()
        except (ValueError, KeyError, TypeError) as error:
            results.append(dict(check=name, outcome='rejected_corruption', reason=str(error)))
        else:
            raise AssertionError('corruption accepted: ' + name)

    spec = audit.loads(args.failed_archives.read_text())['archives'][0]
    original = args.archive_base / spec['path']; before_sha = audit.sha(original.read_bytes())
    a = audit.Audit(spec, args.archive_base).run()
    positive('preserved_configure_failure_is_incomplete_not_zero_miss_success', lambda: audit.require(
        a.result['valid'] and a.result['status'] == 'verified_incomplete_failure'
        and a.result['observed_samples'] == 0 and not a.result['juce_verified']
        and not a.result['source_after_verified'] and len(a.groups) == 9
        and all(g['full_population_deadline_misses'] is None for g in a.groups), 'failed-run coverage misclassified'))
    bad = copy.deepcopy(spec); bad['expected_controller_sha'] = 'f' * 40
    corrupt('wrong_external_controller_binding', lambda: audit.Audit(bad, args.archive_base).run())
    bad = copy.deepcopy(spec); bad['sha256'] = '0' * 64
    corrupt('archive_digest_mismatch', lambda: audit.Audit(bad, args.archive_base).run())
    with tempfile.TemporaryDirectory(prefix='diagnostic-verifier-', dir=args.output.parent) as temporary:
        temporary = Path(temporary)
        with zipfile.ZipFile(original) as archive:
            payloads = {name: archive.read(name) for name in archive.namelist()}
        for name, change in [
            ('member_hash_mismatch', lambda files: files.update({'configure.log': files['configure.log'] + b'corruption'})),
            ('unlisted_member', lambda files: files.update({'unlisted.txt': b'corruption'})),
        ]:
            modified = dict(payloads); change(modified)
            path = temporary / (name + '.zip')
            with zipfile.ZipFile(path, 'w', compression=zipfile.ZIP_DEFLATED) as archive:
                for member, data in modified.items(): archive.writestr(member, data)
            bad = copy.deepcopy(spec); bad['path'] = path.name; bad['sha256'] = audit.sha(path.read_bytes())
            bad['artifact_metadata']['digest'] = 'sha256:' + bad['sha256']
            bad['artifact_metadata']['size_in_bytes'] = path.stat().st_size
            corrupt(name, lambda: audit.Audit(bad, temporary).run())

    fixture, group = synthetic_population()
    population = fixture.population(group)
    positive('full_66_by_1600_grid_including_zero_miss_populations', lambda: audit.require(
        group['complete'] and group['observed_samples'] == 105600 and not fixture.errors
        and any(r['deadline_misses'] == 0 for r in population['routes']), 'valid grid rejected'))
    summary = known_summary(population)
    positive('raw_independent_summary_and_first_maximum_tie', lambda: audit.compare_summary(summary, population, True))
    changed = copy.deepcopy(summary); changed['routes'][0]['p99_us'] = 100.01
    corrupt('reported_p99_changed', lambda: audit.compare_summary(changed, population, True))
    changed = copy.deepcopy(summary); changed['all_misses'] = changed['all_misses'][1:]
    corrupt('raw_miss_deleted_from_report', lambda: audit.compare_summary(changed, population, True))
    route = population['routes'][0]
    log = paired_log(route)
    positive('paired_logs_use_last_maximum_tie_and_match_original_phases', lambda: audit.compare_timing_logs(log, [route], False, True))
    corrupt('paired_cpu_at_maximum_changed', lambda: audit.compare_timing_logs(
        log.replace('wall_max_thread_cpu_us=3', 'wall_max_thread_cpu_us=4', 1), [route], False, True))
    corrupt('timing_misses_changed', lambda: audit.compare_timing_logs(
        log.replace(f"misses={route['deadline_misses']}/1600", 'misses=999/1600'), [route], False, True))
    # Raw clock envelope can legitimately exceed wall; do not reject that.
    positive('enclosing_cpu_interval_can_exceed_wall', lambda: audit.check_row(
        route['_rows'][8], 8, route['_key'], route['budget_us'], False, None))
    for name, index, changes in [
        ('changed_budget', 0, {'budget_us': '999999'}),
        ('suppressed_deadline_flag', 9, {'deadline_miss': '0'}),
        ('wrong_phase_boundary', 1200, {'phase': 'worker_and_automation'}),
        ('wrong_phase_index', 1200, {'phase_index': '1200'}),
        ('wrong_automation', 80, {'automation_requested': '1'}),
        ('duplicate_callback', 1, {'callback_index': '0'}),
        ('cpu_delta_mismatch', 0, {'thread_cpu_us': '2'}),
        ('clock_error', 0, {'clock_start_error': '1'}),
        ('unexpected_cycle_clock', 0, {'thread_cycles_valid': '1'}),
        ('nonfinite_wall', 0, {'wall_us': 'NaN'}),
    ]:
        row = dict(route['_rows'][index], **changes)
        corrupt(name, lambda: audit.check_row(row, index, route['_key'], route['budget_us'], False, None))
    corrupt('clock_moves_back_between_callbacks', lambda: audit.check_row(
        route['_rows'][1], 1, route['_key'], route['budget_us'], False, 999999))
    # Discard full rows after focused checks to keep the fixture memory bounded.
    del fixture, population, summary, route

    partial, group = synthetic_population(count=1, partial=True)
    partial.population(group)
    positive('partial_rows_retained_but_no_full_population_or_gate_pass_imputed', lambda: audit.require(
        not partial.errors and group['observed_samples'] == 9 and not group['complete']
        and group['full_population_deadline_misses'] is None and partial.routes[0]['p99_pass'] is None,
        'partial sample population misclassified'))
    sentinel, group = synthetic_population(sentinel=True, count=1)
    pop = sentinel.population(group); route = pop['routes'][0]
    positive('distinct_sentinel_fields_phases_and_receipts', lambda: audit.compare_timing_logs(paired_log(route, True), [route], True, True))
    row = dict(route['_rows'][0], worker_running='1')
    corrupt('sentinel_claims_ir_worker', lambda: audit.check_row(row, 0, route['_key'], route['budget_us'], True, None))

    # A minimal completed scheduling receipt validates observed QoS, not labels.
    lines = [f'SCHEDULING_DIAGNOSTIC_PROTOCOL diagnostic_only=1 product_change=0 frozen_source={audit.FROZEN_SHA} caller_qos=user-interactive workload=unchanged-frozen-cab worker_priority_unchanged=1 process_priority_unchanged=1 realtime_policy_requested=0 no_daw_or_coreaudio_callback=1',
        'SCHEDULING_DIAGNOSTIC_CONTEXT phase=entry caller_context=macos_base_qos caller_thread_qos=17',
        'SCHEDULING_DIAGNOSTIC_CLOCK steady_clock_is_steady=1 thread_cpu_clock_getres_result=0 thread_cpu_clock_getres_error=0 thread_cpu_resolution_ns=1 clocks_have_different_envelopes=1 wall_minus_cpu_not_exclusive_scheduler_time=1 cpu_clock_not_proof_of_dsp_execution=1',
        'SCHEDULING_DIAGNOSTIC_QOS mode=user-interactive setter_called=1 setter_result=0 requested=33 observed=33',
        'SCHEDULING_DIAGNOSTIC_CONTEXT phase=before-workload caller_context=macos_base_qos caller_thread_qos=33',
        'SCHEDULING_DIAGNOSTIC_CONTEXT phase=after-workload caller_context=macos_base_qos caller_thread_qos=33',
        'SCHEDULING_DIAGNOSTIC_RESULT workload_exit=0 diagnostic_only=1 product_acceptance=unchanged_external_gate']
    log = '\n'.join(lines)
    receipt = dict(returncode=0, scheduling=[line for line in lines if line.startswith((
        'SCHEDULING_DIAGNOSTIC_CONTEXT ', 'SCHEDULING_DIAGNOSTIC_QOS ', 'SCHEDULING_DIAGNOSTIC_RESULT '))])
    positive('observed_high_qos_established_only_after_setter', lambda: audit.check_qos(log, 'user-interactive', False, True, receipt))
    corrupt('high_qos_label_with_wrong_actual_qos', lambda: audit.check_qos(
        log.replace('phase=before-workload caller_context=macos_base_qos caller_thread_qos=33',
                    'phase=before-workload caller_context=macos_base_qos caller_thread_qos=17'), 'user-interactive', False, True, receipt))
    assert audit.sha(original.read_bytes()) == before_sha
    report = dict(passed=True, source='Synthetic verifier fixtures plus the original retained configure-failure archive.',
        performance_evidence=False, original_failed_archive_sha256=before_sha, original_archive_unchanged=True,
        synthetic_fixtures_deleted=True, accepted_valid_checks=sum(r['outcome'] == 'accepted_valid_fixture' for r in results),
        rejected_corruption_checks=sum(r['outcome'] == 'rejected_corruption' for r in results), checks=results)
    args.output.write_text(json.dumps(report, indent=2) + '\n')
    print(f"{len(results)} bounded verifier checks passed; no synthetic result is performance evidence.")


if __name__ == '__main__':
    main()
