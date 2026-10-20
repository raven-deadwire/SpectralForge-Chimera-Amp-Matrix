#!/usr/bin/env python3
"""Run the production processor host; reuse #103 raw validation, never #93 acceptance."""
import argparse
import csv
from datetime import datetime, timezone
from pathlib import Path
import shutil
import subprocess
import time
import performance as perf


def verify_host(h, binary_hash, input_hash, state_hash, ir_hash, instances, block):
    expected = dict(schema='chimera.full-graph.v1', synthetic=True,
                    requirements_certified=False, publication_allowed=False,
                    build_config='Release', binary_sha256=binary_hash,
                    input_sha256=input_hash, state_sha256=state_hash, ir_sha256=ir_hash,
                    instances=instances, block=block, sample_rate=48000, oversampling=4,
                    parameters_unchanged=True, finite=True, dropped_rows=0,
                    callback_cpp_allocations=0)
    for k, v in expected.items():
        if type(h.get(k)) is not type(v) or h[k] != v:
            raise ValueError(f'host evidence mismatch: {k}')
    if len(h['instance_energy']) != instances or any(x <= 0 for x in h['instance_energy']):
        raise ValueError('inactive instance')
    if h['instance_calls'] != [h['callback_count']] * instances:
        raise ValueError('missing processor callbacks')


def run(args):
    out = args.out.resolve()
    if out.exists():
        raise ValueError('output directory must be new')
    binary, input_file, state, ir = (p.resolve(strict=True) for p in (args.host, args.input, args.state, args.ir))
    hashes = [perf.file_hash(p) for p in (binary, input_file, state, ir)]
    source_inputs = args.source_inputs.resolve(strict=True)
    source_hash = perf.file_hash(source_inputs)
    proc = subprocess.Popen([str(binary), str(input_file), str(state), str(ir), str(out),
                             str(args.instances), str(args.block), str(args.seconds), str(args.warmup)],
                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    try:
        limit = time.monotonic() + args.warmup + 180
        while not (out/'ready').exists():
            if proc.poll() is not None:
                raise ValueError('host setup failed: '+proc.communicate()[0])
            if time.monotonic() > limit:
                raise ValueError('host setup timed out')
            time.sleep(.05)
        identity = perf.process_identity(proc.pid)
        start = time.monotonic()
        with (out/'memory.csv').open('x', newline='') as f:
            writer = csv.writer(f); writer.writerow(perf.MEMORY_COLUMNS)
            writer.writerow([0, proc.pid, perf.rss_bytes(proc.pid)])
            (out/'go').write_text('go', encoding='utf-8')
            # Windows monotonic can tick in ~15ms steps. Do not emit a second
            # t=0 sample immediately after the boundary observation.
            time.sleep(.25)
            while proc.poll() is None:
                if time.monotonic()-start > args.seconds*10+120:
                    raise ValueError('host measurement timed out; retain partial raw data')
                # Keep the host alive until its final report, but only sample
                # the callback period, not teardown.
                if not (out/'host.json').exists():
                    if perf.process_identity(proc.pid) != identity:
                        raise ValueError('process identity changed')
                    writer.writerow([time.monotonic()-start, proc.pid, perf.rss_bytes(proc.pid)])
                    f.flush()
                time.sleep(.25)
        output = proc.communicate()[0]
        (out/'host.log').write_text(output, encoding='utf-8')
        if proc.returncode:
            raise ValueError(f'host failed ({proc.returncode}); evidence retained: {output}')
    finally:
        if proc.poll() is None:
            proc.terminate(); proc.wait(timeout=15)  # only this child
    if hashes != [perf.file_hash(p) for p in (binary, input_file, state, ir)]:
        raise ValueError('binary/input/state/IR changed while running')
    h = perf.read_json(out/'host.json')
    verify_host(h, *hashes, args.instances, args.block)
    if source_hash != h['source_fingerprint'] or perf.file_hash(source_inputs) != source_hash:
        raise ValueError('source inventory differs from the compiled fingerprint')
    shutil.copyfile(source_inputs, out/'source-inputs.txt')
    for i, sha in enumerate(h['resolved_state_sha256']):
        if perf.file_hash(out/f'resolved-{i}.chimera') != sha:
            raise ValueError('resolved state digest mismatch')
    study = perf.make_study(perf.environment(), 'Not run by this independent processor observation')
    # Explicit host identity, never impersonate Standalone packaging or a DAW.
    study['environment'].update(daw_name='ChimeraFullGraphHost (synthetic)', daw_version='1',
                                plugin_format='Standalone')
    study['product'].update(source_sha=h['source_sha'], version=h['version'],
                            build_id='full-graph:'+h['source_fingerprint'], binary_sha256=hashes[0])
    manifest = {'policy': 'common external A IR on every active CAB lane; B mix zero',
                'ir_sha256': hashes[3], 'resolved_state_sha256': h['resolved_state_sha256']}
    perf.write_json(out/'ir-manifest.json', manifest)
    study['workload'].update(input_sha256=hashes[1], preset_bank_sha256=hashes[2],
                            ir_manifest_sha256=perf.file_hash(out/'ir-manifest.json'),
                            description='Synthetic paced full processor graph; closed editor; fixed common IR; input loops without crossfade')
    mode = ['Classic', 'Dual', 'Matrix'][h['mode']]
    group = 'dual_crossover' if mode == 'Dual' and h['dualtype'] else 'core'
    row = next(r for r in study['runs'] if r['settings']['group'] == group
               and r['settings']['mode'] == mode and r['settings']['instances'] == args.instances
               and r['settings']['buffer_samples'] == args.block
               and r['settings']['transpose_semitones'] == h['transpose'])
    # Smoke runs are labeled as short custom observations, not the 60s matrix.
    row['id'] += f'-paced-{args.seconds}s'
    row['settings'].update(duration_seconds=args.seconds, warmup_seconds=args.warmup)
    receipt = dict(schema_version=1, run_id=row['id'], product_binary_sha256=hashes[0],
                   product_manifest_sha256=perf.digest(study['product']),
                   environment_sha256=perf.digest(study['environment']),
                   settings_sha256=perf.digest(row['settings']), workload_sha256=perf.digest(study['workload']),
                   measurement_kind='paced_processor', collector_name_version='ChimeraFullGraphHost/1',
                   callback_scope='full_product_graph', process_id=proc.pid, settings_verified=True,
                   telemetry_dropped_rows=h['dropped_rows'],
                   warmup_seconds=min(h['warmup_audio_seconds'], h['warmup_wall_seconds']),
                   transport_seconds=0, continuous_transport=None, recording_verified=None,
                   xruns=None, glitches=None, crashed=False, session_reopen_status='NOT_RUN',
                   operator='independent processor harness', observed_utc=datetime.now(timezone.utc).isoformat(),
                   notes=['Synthetic scheduler, no audio device/DAW/editor; no release acceptance.',
                          'Thread CPU excludes workers. Windows GetThreadTimes is coarse; zeros are quantization.',
                          'C++ new probe covers callback thread only, not malloc/OS/workers.',
                          'host.json sha256='+perf.file_hash(out/'host.json')])
    perf.write_json(out/'receipt.json', receipt)
    row['metrics'] = perf.measure(study, row, out/'callbacks.csv', out/'memory.csv', receipt)
    row.update(status='MEASURED', reason='Synthetic full processor observation, NOT hardware certification', evidence={})
    for name in ('callbacks', 'memory', 'receipt'):
        src = out/(name+('.json' if name == 'receipt' else '.csv'))
        sha = perf.file_hash(src)
        dst = out/'raw'/sha/src.name; dst.parent.mkdir(parents=True, exist_ok=True);shutil.copyfile(src,dst)
        row['evidence'][name] = dict(path=dst.relative_to(out).as_posix(), sha256=sha)
    perf.validate(study, out)
    perf.write_json(out/'study.json',study)
    perf.write_table(study,out/'results.md')
    controls = {}
    for name in ('empty-graph', 'clock-only'):
        controls[name] = perf.summarize(out/(name+'.csv'), out/'memory.csv', row['settings'])
    perf.write_json(out/'overhead.json', dict(
        scope='Unpaced controls: diagnostic upper cost, not subtracted from product times',
        controls=controls, requirements_certified=False, publication_allowed=False))
    # Supplemental evidence has an independent manifest; the existing validator
    # remains the authority for CSV metrics. No second acceptance gate is added.
    perf.write_json(out/'bundle-hashes.json', {
        p.name: perf.file_hash(p) for p in out.iterdir()
        if p.is_file() and p.name not in ('ready', 'go')})
    print(f'Validated synthetic observation: {out}')


def main():
    p = argparse.ArgumentParser(description=__doc__)
    for name in ('host','input','state','ir','out','source-inputs'):
        p.add_argument('--'+name, type=Path, required=True)
    p.add_argument('--instances', type=int, choices=(1,2,4), default=1)
    p.add_argument('--block', type=int, choices=(128,256), default=128)
    p.add_argument('--seconds', type=int, default=60)
    p.add_argument('--warmup', type=int, default=10)
    args = p.parse_args()
    if not 1 <= args.seconds <= 1800 or not 1 <= args.warmup <= 60:
        p.error('duration out of bounds')
    run(args)


if __name__ == '__main__':
    main()
