#!/usr/bin/env python3
"""Independent, read-only verification of already produced EQ CI receipts.

This script neither builds nor runs tests. It only augments a scratch paired
performance report with the actual JUnit records and functional CSV checks.
"""
import argparse
import csv
import hashlib
import json
import math
from pathlib import Path
import xml.etree.ElementTree as ET

RATES = (44100, 48000, 96000, 192000)
SIZES = (2048, 4096, 8192)
TESTS = {
    'ChimeraGateProcessorTests', 'ChimeraGraphicalEQTests',
    'ChimeraGraphicalEQUITests', 'ChimeraEQAnalyzerTests',
    'ChimeraEQAnalyzerPerformanceTests', 'ChimeraEQHighRateAttributionTests',
}


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('artifact', type=Path)
    ap.add_argument('--report-prefix', type=Path, required=True)
    args = ap.parse_args()
    root = args.artifact.resolve()
    issues = []
    hashes = {}
    cases = {}

    def require(value, message):
        if not value:
            issues.append(message)

    def read_bytes(name):
        b = (root / name).read_bytes()
        hashes[name] = hashlib.sha256(b).hexdigest()
        return b

    def rows(name, count):
        result = list(csv.DictReader(read_bytes(name).decode('utf-8-sig').splitlines()))
        require(len(result) == count, f'{name}: expected {count} rows; observed {len(result)}')
        rates = sorted({int(float(r['rate'])) for r in result})
        require(rates == list(RATES), f'{name}: unexpected sample rates {rates}')
        return result

    def n(row, key):
        value = float(row[key])
        require(math.isfinite(value), f'Nonfinite {key}: {row}')
        return value

    def i(row, key):
        value = n(row, key)
        require(value == int(value), f'Noninteger {key}: {row}')
        return int(value)

    suite = ET.fromstring(read_bytes('eq-results.xml'))
    tests = []
    for t in suite.iter('testcase'):
        passed = t.get('status') == 'run' and not any(t.find(name) is not None for name in ('failure', 'error', 'skipped'))
        tests.append({'name': t.get('name'), 'status': 'PASSED' if passed else 'NOT_PASSED',
                      'seconds': float(t.get('time', '0'))})
    require({t['name'] for t in tests} == TESTS and len(tests) == 6, 'CTest: expected exact 6 test names')
    require(all(t['status'] == 'PASSED' for t in tests), 'CTest: test did not run or pass')
    require(suite.get('tests') == '6' and all(suite.get(k, '0') == '0' for k in ('failures', 'errors', 'skipped', 'disabled')),
            'CTest: aggregate reports failures/skips or wrong test count')

    name = 'eq-analyzer-temporal-coverage.csv'
    temporal = rows(name, 24)
    grid = set()
    zero_first = []
    for r in temporal:
        rate, size, batch, hop = (i(r, f) for f in ('rate', 'fft_size', 'ui_batch_samples', 'analysis_hop_samples'))
        key = (rate, size, r['burst_position'])
        require(key not in grid, f'{name}: duplicate {key}')
        grid.add(key)
        require(batch == rate // 25 and hop == size // 4 and i(r, 'ui_interval_ms') == 40, f'{name}: cadence {key}')
        require(i(r, 'fft_windows_at_40ms') == batch // hop, f'{name}: FFT count {key}')
        require(i(r, 'extra_observation_samples') == size // 2 + hop, f'{name}: bounded tail {key}')
        require(n(r, 'input_energy') > 1e-6 and i(r, 'fifo_dropped_samples') == 0, f'{name}: energy/loss {key}')
        require(n(r, 'bounded_display_peak_db') > -60, f'{name}: bounded attack absent {key}')
        if r['burst_position'] == 'early' and batch > size:
            require(n(r, 'display_peak_db_at_40ms') > -60, f'{name}: early attack absent at paint {key}')
        if i(r, 'fft_windows_at_40ms') == 0:
            zero_first.append({'rate': rate, 'fft_size': size, 'burst_position': r['burst_position']})
    require(grid == {(r, s, p) for r in RATES for s in SIZES for p in ('early', 'late')}, f'{name}: unexpected grid')
    cases[name] = {'rows': len(temporal), 'sample_rates': list(RATES),
                   'minimum_bounded_display_db': min(n(r, 'bounded_display_peak_db') for r in temporal),
                   'drops': sum(i(r, 'fifo_dropped_samples') for r in temporal),
                   'legitimate_zero_fft_at_first_40ms': zero_first,
                   'interpretation': 'Each attack detected with the bounded N/2+N/4 observation tail; early high-rate attacks also present at 40 ms.'}

    name = 'eq-analyzer-partition-invariance.csv'
    partition = rows(name, 180)
    grid = set()
    for r in partition:
        rate, size, smoothing, start = (i(r, f) for f in ('rate', 'fft_size', 'smoothing', 'burst_start'))
        key = (rate, size, smoothing, start)
        require(key not in grid, f'{name}: duplicate {key}')
        grid.add(key)
        total = 2 * (rate // 25) + size
        require(i(r, 'fft_windows') == total // (size // 4), f'{name}: windows {key}')
        require(i(r, 'checked_ui_boundaries') == math.ceil(total / (rate // 25)), f'{name}: boundaries {key}')
        require(all(0 <= n(r, f) <= .002 for f in ('max_raw_error_db', 'max_envelope_error_db', 'max_pixel_error_db')), f'{name}: parity {key}')
        require(n(r, 'max_display_db') > -60 and i(r, 'fifo_dropped_samples') == 0, f'{name}: attack/loss {key}')
    expected = {(r, s, m, a) for r in RATES for s in SIZES for m in range(3)
                for a in (64, s // 4 - 1, s // 4 + 1, r // 25 - 192, r // 25 + 1)}
    require(grid == expected, f'{name}: unexpected grid')
    cases[name] = {'rows': len(partition), 'sample_rates': list(RATES),
                   'max_errors_db': {f: max(n(r, f) for r in partition) for f in ('max_raw_error_db', 'max_envelope_error_db', 'max_pixel_error_db')},
                   'drops': sum(i(r, 'fifo_dropped_samples') for r in partition)}

    name = 'eq-analyzer-fifo-stalls.csv'
    stalls = rows(name, 48)
    grid = set()
    overflow = []
    for r in stalls:
        rate, size = (i(r, f) for f in ('rate', 'fft_size'))
        full = r['fixture'] == 'full_capacity'
        stall = 0 if full else i(r, 'simulated_stall_ms')
        key = (rate, size, stall)
        require(key not in grid, f'{name}: duplicate {key}')
        grid.add(key)
        p, c, d, l, q, windows = (i(r, f) for f in ('produced_samples', 'consumed_samples', 'discarded_samples', 'dropped_samples', 'pending_samples', 'fft_windows'))
        require(i(r, 'capacity_samples') == 65536 and p == (65536 if full else rate * stall // 1000), f'{name}: fixture {key}')
        require(p == c + d + l + q and l == max(0, p - 65536), f'{name}: accounting {key}')
        require(q == 0 and windows <= 65536 // (size // 4) + 1 and i(r, 'recovery_complete') == 1, f'{name}: recovery/bound {key}')
        if l:
            require(c == 0 and d == 65536 and windows == 0, f'{name}: discontinuity handling {key}')
            overflow.append({'rate': rate, 'fft_size': size, 'stall_ms': stall, 'produced': p, 'dropped': l, 'discarded': d, 'recovery_complete': 1})
        else:
            require(c == p and d == 0 and windows == p // (size // 4), f'{name}: lossless continuity {key}')
    require(grid == {(r, s, t) for r in RATES for s in SIZES for t in (0, 100, 250, 500)}, f'{name}: unexpected grid')
    cases[name] = {'rows': len(stalls), 'sample_rates': list(RATES), 'lossless_rows': len(stalls) - len(overflow),
                   'intentional_overflow_rows': overflow, 'all_recovery_complete': True,
                   'interpretation': '192 kHz x 500 ms produces 96,000 samples; its expected 30,464 FIFO drops plus 65,536 discarded stale frames are intentional recovery tests.'}

    name = 'eq-analyzer-audio-invariance.csv'
    audio = rows(name, 12)
    grid = set()
    for r in audio:
        key = i(r, 'rate'), i(r, 'initial_fft_size')
        require(key not in grid, f'{name}: duplicate {key}')
        grid.add(key)
        require(i(r, 'host_latency_samples') == 42 and n(r, 'max_audio_error') == 0 and i(r, 'bit_identical') == 1,
                f'{name}: audio/latency changed {key}')
        require(i(r, 'block') == 128 and i(r, 'channels') == 2 and i(r, 'checked_blocks') == 768, f'{name}: fixture {key}')
        require(all(i(r, f) > 0 for f in ('tone_fifo_drops', 'final_fifo_drops', 'tone_discontinuities', 'final_discontinuities')),
                f'{name}: expected intentional overflow/recovery absent {key}')
    require(grid == {(r, s) for r in RATES for s in SIZES}, f'{name}: unexpected grid')
    cases[name] = {'rows': len(audio), 'sample_rates': list(RATES), 'bit_identical_rows': 12, 'max_audio_error': 0,
                   'host_latency_samples': sorted({i(r, 'host_latency_samples') for r in audio}),
                   'checked_blocks_per_row': 768,
                   'intentional_tone_drops_values': sorted({i(r, 'tone_fifo_drops') for r in audio}),
                   'intentional_final_drops_values': sorted({i(r, 'final_fifo_drops') for r in audio}),
                   'interpretation': 'ON/OFF whole-processor outputs remain bit-identical despite deliberate FIFO overflow and analyzer reconfiguration.'}

    name = 'eq-analyzer-callback.csv'
    callback = rows(name, 48)
    grid = set()
    for r in callback:
        rate, size, block = (i(r, f) for f in ('rate', 'fft_size', 'block'))
        key = rate, size, block
        require(key not in grid, f'{name}: duplicate {key}')
        grid.add(key)
        require(i(r, 'capture') == (size != 0) and i(r, 'callbacks') == 600 and i(r, 'channels') == 2 and i(r, 'active_eq_bands') == 24, f'{name}: fixture {key}')
        require(i(r, 'audio_allocations') == 0, f'{name}: audio callback allocation {key}')
    require(grid == {(r, s, b) for r in RATES for s in (0, *SIZES) for b in (64, 128, 512)}, f'{name}: unexpected grid')
    cases[name] = {'rows': len(callback), 'sample_rates': list(RATES), 'total_observed_callbacks': sum(i(r, 'callbacks') for r in callback),
                   'audio_allocations': sum(i(r, 'audio_allocations') for r in callback),
                   'observed_overruns': sum(i(r, 'overruns') for r in callback),
                   'overrun_rows': [r for r in callback if i(r, 'overruns')],
                   'interpretation': 'Observed software fixture timing only; not hardware or DAW acceptance.'}

    name = 'eq-analyzer-cpu.csv'
    cpu = rows(name, 12)
    grid = set()
    for r in cpu:
        key = i(r, 'rate'), i(r, 'fft_size')
        require(key not in grid, f'{name}: duplicate {key}')
        grid.add(key)
        require(i(r, 'measured_update_calls') == 160 and i(r, 'ui_hz') == 25 and i(r, 'fft_windows') > 0 and i(r, 'fifo_dropped_samples') == 0,
                f'{name}: update/window/loss {key}')
        require(0 <= i(r, 'zero_fft_update_calls') < 160, f'{name}: zero FFT calls {key}')
    require(grid == {(r, s) for r in RATES for s in SIZES}, f'{name}: unexpected grid')
    cases[name] = {'rows': len(cpu), 'sample_rates': list(RATES), 'drops': sum(i(r, 'fifo_dropped_samples') for r in cpu),
                   'observed_zero_fft_update_calls': [{'rate': i(r, 'rate'), 'fft_size': i(r, 'fft_size'), 'calls': 160, 'zero_fft_calls': i(r, 'zero_fft_update_calls')} for r in cpu],
                   'interpretation': 'Zero-call rows are included in the measured consumer sample set. CPU/UI attribution is outside this independent report.'}

    provenance = json.loads(read_bytes('eq-attribution-provenance.json'))
    version = json.loads(read_bytes('chimera-build-version.json'))
    report_file = Path(str(args.report_prefix) + '.json')
    report = json.loads(report_file.read_text())
    require(len(report['datasets']) == 1, 'Scratch paired report must have one dataset')
    dataset = report['datasets'][0]
    require(provenance['source_commit'] == dataset['source_commit'] == version['source_sha'], 'Source identity differs between receipts')
    require(provenance['performance_acceptance'] == 'NOT_ASSESSED' and provenance['physical_daw_device_tested'] is False,
            'Untested physical DAW acceptance improperly promoted')
    require(provenance['public_beta_changed'] is False, 'Public beta provenance unexpectedly changed')
    require(provenance['preserved_transient_repair']['status'] == 'PREVIOUS_FUNCTIONAL_PASS_PRESERVED', 'Prior transient repair not preserved')
    non_improving = [c for c in dataset['cells'] if c['update_cost_reduction_percent'] <= 0]
    cpu_non_improving = [c for c in dataset['cells'] if c['cpu_cost_reduction_percent'] is not None and c['cpu_cost_reduction_percent'] <= 0]
    result = {'gate': 'PASS' if not issues else 'FAIL', 'issues': issues,
              'source_commit': provenance['source_commit'], 'runner': provenance['runner'], 'build': provenance['build'],
              'ctest_suite': suite.attrib, 'ctests': tests, 'functional_cases': cases, 'receipts_sha256': hashes,
              'backend': {'selected_by_checked_JUCE_source_and_default_build_config': 'AppleFFT / vDSP' if provenance['runner']['RUNNER_OS'] == 'macOS' else 'FFTFallback',
                          'backend_reported_directly_in_CSV': False, 'proof_limit': 'Local JUCE 8.0.8 backend/source audit; no remote binary symbol dump supplied.'},
              'preserved_transient_repair': provenance['preserved_transient_repair'],
              'physical_daw_acceptance': 'NOT_ASSESSED', 'public_beta_changed': False,
              'non_improving_update_cells': non_improving,
              'non_improving_cpu_cells': cpu_non_improving}
    report['functional_receipt_verification'] = result
    report_file.write_text(json.dumps(report, indent=2) + '\n')
    mdfile = Path(str(args.report_prefix) + '.md')
    marker = '\n## Independent CTest and functional receipt verification\n'
    original = mdfile.read_text().split(marker)[0].rstrip()
    lines = [marker, f"Receipt consistency: **{result['gate']}**. These checks read completed artifacts; no benchmark was rerun.",
             f"Source: `{provenance['source_commit']}`. Runner: `{provenance['runner']['platform']}`.",
             f"Build-selected FFT engine from JUCE 8.0.8 source/default configuration: {result['backend']['selected_by_checked_JUCE_source_and_default_build_config']}; artifact CSV does not directly name the engine.",
             '\n| Actual CTest record | Status | Seconds |\n|---|---|---:|']
    lines += [f"| {t['name']} | {t['status']} | {t['seconds']:.4f} |" for t in tests]
    lines += ['\nAll six dedicated CSV grids below cover 44.1, 48, 96, and 192 kHz.',
              '\n| Receipt | Rows | Independently checked result |\n|---|---:|---|',
              f"| Temporal coverage | 24 | All bounded attacks > −60 dB; lowest {cases['eq-analyzer-temporal-coverage.csv']['minimum_bounded_display_db']:.4f} dB; exact hop counts; zero drops |",
              f"| Partition invariance | 180 | Max raw/envelope/pixel errors {cases['eq-analyzer-partition-invariance.csv']['max_errors_db']}; zero drops |",
              '| FIFO stalls | 48 | Exact produced = consumed + discarded + dropped + pending; 45 lossless cases; 3 deliberate overflow/recovery cases |',
              '| Whole-processor audio invariance | 12 | Bit-identical ON/OFF audio, error 0, host latency 42 samples; deliberate overflow exercised |',
              f"| Audio callback fixture | 48 | {cases['eq-analyzer-callback.csv']['total_observed_callbacks']} callbacks; allocation count 0; observed overruns {cases['eq-analyzer-callback.csv']['observed_overruns']} |",
              '| Analyzer cadence fixture | 12 | 160 measured calls per cell; zero-call updates retained; zero FIFO drops |',
              '\nThe zero-FFT first 40 ms observations at 44.1/48 kHz with 8192 FFT are expected: the 2048-sample hop exceeds the 1764/1920-sample UI batch. Every attack is present within the explicit bounded observation tail.',
              '\nThe 192 kHz × 500 ms stall intentionally produces 96,000 samples for a 65,536-frame FIFO: 30,464 drops and 65,536 discarded stale frames are verified in each FFT size, followed by fresh-window recovery. The audio-invariance fixture separately forces overflow while preserving output. These are not lossless-input cases.',
              '\nThe generic `eq-cpu.csv` has only 48/96 kHz and legacy `eq-analyzer-full-callback.csv` only 48 kHz; they are not used to claim four-rate coverage. Native UI/producer attribution belongs to its separate report.',
              '\n## Retained non-improving paired observations\n',
              f"{len(non_improving)} of 48 cells have a candidate update median at least as high as baseline. No cell is excluded. Negative reduction values below mean increased measured cost.",
              '\n| Rate | FFT | Fixture | Update reduction % | Actual thread CPU reduction % | Adjacent-pair update reductions % |\n|---:|---:|---|---:|---:|---|']
    for c in non_improving:
        cadence = f"{c['ui_interval_ms']} ms" if c['ui_interval_ms'] else c['fixture']
        cpu_gain = c['cpu_cost_reduction_percent']
        cpu_text = f'{cpu_gain:.3f}' if cpu_gain is not None else 'NOT_COMPARABLE'
        pairs = ', '.join(f'{v:.3f}' for v in c['adjacent_pair_update_reductions_percent'])
        lines.append(f"| {c['rate']} | {c['fft_size']} | {cadence} | {c['update_cost_reduction_percent']:.3f} | {cpu_text} | {pairs} |")
    if not non_improving:
        lines.append('| — | — | None | — | — | — |')
    cpu_only_non_improving = [c for c in cpu_non_improving if c['update_cost_reduction_percent'] > 0]
    if cpu_only_non_improving:
        lines += ['\nThe following cells improve update wall time but have non-lower measured thread CPU medians (ties or increases; the CPU measurement includes FIFO feed):']
        if dataset.get('cpu_timer_assessment'):
            lines += ['These Windows counter observations are quantization-limited. A 0% ratio is a tie in the measured CPU medians, not a CPU increase or evidence of equal execution cost.']
        lines += [f"{c['rate']} Hz, FFT {c['fft_size']}, {c['ui_interval_ms'] or c['fixture']}: update reduction {c['update_cost_reduction_percent']:.3f}%, CPU reduction {c['cpu_cost_reduction_percent']:.3f}%." for c in cpu_only_non_improving]
    lines += ['\nAll raw repetitions remain in the paired report JSON and source CSV. Mixed signs alone do not establish a scheduling cause or disprove a real regression. Wall and thread CPU measure different scopes: update wall excludes FIFO feed; the thread CPU observation includes it.',
              '\nPhysical DAW/audio-interface performance remains **NOT_ASSESSED**. Source and measurements remain attached to this run, separate from minimal local, full-linked local, or subsequent CI results.']
    if issues:
        lines += ['\n### Receipt issues\n'] + ['- ' + issue for issue in issues]
    mdfile.write_text(original + '\n' + '\n\n'.join(lines).replace('|\n\n|', '|\n|') + '\n')
    print(json.dumps({'artifact': str(root), 'gate': result['gate'], 'ctests': tests, 'cases': cases,
                      'non_improving_update_cells': len(non_improving), 'issues': issues,
                      'report': str(report_file)}, indent=2))
    raise SystemExit(0 if not issues else 1)


if __name__ == '__main__':
    main()
