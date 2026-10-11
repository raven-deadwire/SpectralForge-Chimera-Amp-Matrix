#!/usr/bin/env python3
"""One owned CTest invocation; an independent, once-only native sample."""
import argparse
import ctypes
import hashlib
import json
import mmap
import os
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import threading
import time
import xml.etree.ElementTree as ET

TEST = 'ChimeraCabPanelStateTests'
HEADER = struct.Struct('<8s15Q')
ENTRY = struct.Struct('<8Q')
EVENTS = {
    1: 'sessionBegin', 2: 'sessionEnd', 10: 'sceneBegin', 11: 'sceneEnd',
    12: 'showingBegin', 13: 'showingEnd', 20: 'clickBegin', 21: 'clickEnd',
    22: 'triggerBegin', 23: 'triggerEnd', 24: 'listener',
    25: 'predicateBegin', 26: 'predicateEnd', 27: 'originalFailure',
    28: 'failureProbeBegin', 29: 'failureProbeEnd',
    40: 'dispatchBegin', 41: 'dispatchEnd', 42: 'cfBegin', 43: 'cfEnd',
    44: 'nextBegin', 45: 'nextEnd', 46: 'sendBegin', 47: 'sendEnd',
    48: 'modalFilterBegin', 49: 'modalFilterEnd', 50: 'autoreleaseBegin', 51: 'autoreleaseEnd',
    60: 'queuePostBegin', 61: 'queuePostEnd', 62: 'sourceBegin', 63: 'sourceEnd',
    64: 'callbackBegin', 65: 'callbackEnd', 80: 'observerEarly', 81: 'observerLate',
}
END_TO_BEGIN = {2:1, 11:10, 13:12, 21:20, 23:22, 26:25, 29:28,
                41:40, 43:42, 45:44, 47:46, 49:48, 51:50,
                61:60, 63:62, 65:64}


def write_json(path, data):
    path.write_text(json.dumps(data, indent=2)+'\n')


def sha(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def symbols(build, out):
    inventory = json.loads(subprocess.check_output(
        ['ctest', '--test-dir', str(build), '-C', 'Release', '--show-only=json-v1'], text=True))
    write_json(out/'ctest-inventory.json', inventory)
    matches = [t for t in inventory['tests'] if t['name'] == TEST]
    assert len(matches) == 1
    binary = Path(matches[0]['command'][0]).resolve()
    assert binary.is_file()
    dsym = Path(str(binary)+'.dSYM')
    subprocess.run(['dsymutil', str(binary), '-o', str(dsym)], check=True)
    uuid = subprocess.check_output(['dwarfdump', '--uuid', str(binary), str(dsym)], text=True)
    dylibs = subprocess.check_output(['otool', '-L', str(binary)], text=True)
    assert 'asan' not in dylibs.lower()
    manifest = dict(test=TEST, command=matches[0]['command'], binary=str(binary),
                    binary_sha256=sha(binary), dsym=str(dsym), uuids=uuid, dylibs=dylibs)
    write_json(out/'binary-manifest.json', manifest)
    commands = json.loads((build/'compile_commands.json').read_text())
    relevant = [r for r in commands if any(r['file'].endswith(p) for p in
        ('/Tests/CabPanelStateTests.cpp', '/Tests/NativeMessageLoop.mm', '/ColdClickTrace.mm',
         '/juce_events/juce_events.mm', '/juce_events/juce_events.cpp'))]
    assert relevant
    for command in relevant:
        assert '-O3' in command['command'] and '-DNDEBUG' in command['command']
        assert '-g' in command['command'] and '-fsanitize' not in command['command']
    write_json(out/'observed-compile-commands.json', relevant)


def header(data):
    values = HEADER.unpack_from(data)
    assert values[0] == b'CHMTRC01' and values[1] == 1
    assert values[2] == 262144 and values[3] == ENTRY.size
    assert len(data) == HEADER.size + values[2]*ENTRY.size
    assert values[6] > 0 and values[7] > 0
    return dict(capacity=values[2], pid=values[4], main_thread=values[5],
                numerator=values[6], denominator=values[7], next=values[8], dropped=values[9],
                start_ticks=values[10], ended=values[11])


def read_entry(data, index):
    offset = HEADER.size + index*ENTRY.size
    # Live reads only trigger a best-effort sampler. Final evidence is decoded
    # after process exit. Never treat a reservation index as a complete record.
    expected = index+1
    if struct.unpack_from('<Q', data, offset)[0] != expected:
        return None
    copy = data[offset:offset+ENTRY.size]
    values = ENTRY.unpack(copy)
    if values[0] != expected or data[offset:offset+ENTRY.size] != copy:
        return None
    if values[1] == 0 or values[2] == 0 or values[3] not in EVENTS:
        return None
    return values


def decode(trace, out):
    data = trace.read_bytes()
    h = header(data)
    rows, holes = [], []
    scale = h['numerator']/h['denominator']/1e6
    for i in range(min(h['next'], h['capacity'])):
        value = read_entry(data, i)
        if value is None:
            holes.append(i+1)
            continue
        seq, ticks, thread, event, a, b, c, d = value
        rows.append(dict(seq=seq, ms=(ticks-h['start_ticks'])*scale,
                         thread=thread, event=EVENTS[event], code=event, a=a, b=b, c=c, d=d))
    with (out/'trace-records.jsonl').open('w') as stream:
        for row in rows:
            stream.write(json.dumps(row, separators=(',', ':'))+'\n')
    by_sequence = {r['seq']:r for r in rows}
    spans = []
    for end in rows:
        if end['code'] not in END_TO_BEGIN or end['code'] == 2:
            continue
        start = by_sequence.get(end['a'])
        if start and start['code'] == END_TO_BEGIN[end['code']]:
            spans.append(dict(kind=start['event'], begin_seq=start['seq'], end_seq=end['seq'],
                              thread=start['thread'], begin_ms=start['ms'], end_ms=end['ms'],
                              elapsed_ms=end['ms']-start['ms'], a=start['a'], b=start['b']))
    main = [r for r in rows if r['thread'] == h['main_thread']]
    scene = next((r for r in main if r['code'] == 10), None)
    click = next((r for r in main if r['code'] == 20 and scene and r['seq'] > scene['seq']), None)
    end = next((r for r in main if r['code'] == 21 and click and r['a'] == click['seq']), None)
    first = [r for r in main if click and r['seq'] >= click['seq']
             and (end is None or r['seq'] <= end['seq'])]
    original_failure = [r for r in rows if r['code'] == 27]
    summary = dict(header=h, record_count=len(rows), unpublished_records=holes,
                   original_failure_records=original_failure,
                   first_scene_click=click, first_scene_click_end=end,
                   first_scene_click_records=first,
                   longest_main_spans=sorted([s for s in spans if s['thread']==h['main_thread']],
                                             key=lambda s:s['elapsed_ms'], reverse=True)[:60],
                   limitations=['No private AppKit/QuartzCore function is identified by a CF boundary alone.',
                                'The independent sample is an aggregate tree, not per-stack timestamps.',
                                'Observing a passing instrumented flow is NOREPRO, not release acceptance.'])
    write_json(out/'trace-summary.json', summary)
    write_json(out/'trace-spans.json', spans)
    return h, holes


def run(build, out):
    manifest = json.loads((out/'binary-manifest.json').read_text())
    binary = Path(manifest['binary']).resolve()
    assert sha(binary) == manifest['binary_sha256']
    trace = out/'native-trace.bin'
    assert not trace.exists()
    command = ['ctest', '--test-dir', str(build), '-C', 'Release', '-R', '^'+TEST+'$',
               '-V', '--output-on-failure', '--no-tests=error', '--output-junit', 'cab-results.xml']
    env = dict(os.environ, CHIMERA_COLD_CLICK_TRACE=str(trace))
    receipt = dict(purpose='DIAGNOSTIC ONLY', command=command, test_invocations=1,
                   sample_invocations=0, sample_duration_seconds=5, sample_interval_ms=10,
                   trigger='first scene click active, native stage observed for at least 100 ms',
                   started_epoch=time.time())
    process = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, env=env)
    receipt['ctest_pid'] = process.pid
    reader_errors = []

    def drain():
        log = None
        try:
            log = (out/'cab-ctest.log').open('wb')
        except Exception as error:
            reader_errors.append('open log: '+str(error))
        try:
            while True:
                block = process.stdout.read1(65536)
                if not block:
                    break
                if log is not None:
                    try:
                        log.write(block)
                    except Exception as error:
                        reader_errors.append('write log: '+str(error))
                        try: log.close()
                        except Exception: pass
                        log = None
                try:
                    sys.stdout.buffer.write(block); sys.stdout.buffer.flush()
                except Exception as error:
                    if not reader_errors or reader_errors[-1] != 'console: '+str(error):
                        reader_errors.append('console: '+str(error))
        except Exception as error:
            reader_errors.append('read pipe: '+str(error))
        finally:
            if log is not None:
                try: log.close()
                except Exception as error: reader_errors.append('close log: '+str(error))

    reader = threading.Thread(target=drain, daemon=True)
    reader_started = False
    try:
        reader.start()
        reader_started = True
    except Exception as error:
        reader_errors.append('reader startup: '+str(error))
        drain()  # Preserve the one original process even if observation cannot start.
    sampler = sample_stream = mapped = mapped_stream = None
    error = None
    try:
        libproc = ctypes.CDLL('/usr/lib/libproc.dylib')
        libproc.proc_pidpath.argtypes = [ctypes.c_int, ctypes.c_void_p, ctypes.c_uint32]
        libproc.proc_pidpath.restype = ctypes.c_int
        system = ctypes.CDLL(None)
        system.mach_absolute_time.restype = ctypes.c_uint64
        seen, pending, cursor = {}, set(), 0
        first_scene = first_click = first_end = None
        active_stages = {}
        while process.poll() is None:
            if mapped is None:
                if not trace.exists() or trace.stat().st_size < HEADER.size:
                    time.sleep(.02); continue
                mapped_stream = trace.open('rb')
                mapped = mmap.mmap(mapped_stream.fileno(), 0, access=mmap.ACCESS_READ)
                try:
                    h = header(mapped)
                except (AssertionError, struct.error):
                    mapped.close(); mapped_stream.close(); mapped = mapped_stream = None
                    time.sleep(.02); continue
            h = header(mapped)
            high = min(h['next'], h['capacity'])
            candidates = sorted(pending | set(range(cursor, high)))
            cursor = high
            new = []
            for index in candidates:
                value = read_entry(mapped, index)
                if value is None:
                    pending.add(index); continue
                pending.discard(index)
                seen[value[0]] = value
                new.append(value)
            for value in sorted(new):
                seq, ticks, thread, event, a, b, c, d = value
                if thread != h['main_thread']:
                    continue
                if event == 10 and first_scene is None:
                    first_scene = value
                if event == 20 and first_scene and first_click is None:
                    first_click = value
                if event == 21 and first_click and a == first_click[0]:
                    first_end = value
                if event in (42, 44, 46):
                    active_stages[seq] = value
                if event in (43, 45, 47):
                    active_stages.pop(a, None)
            now = system.mach_absolute_time()
            slow = [r for r in active_stages.values()
                    if first_click and r[0] > first_click[0]
                    and (now-r[1])*h['numerator']/h['denominator']/1e6 >= 100]
            if first_click and first_end is None and slow and sampler is None:
                pid = h['pid']
                identity = subprocess.check_output(['/bin/ps', '-p', str(pid), '-o', 'ppid=,uid='], text=True).split()
                assert identity == [str(process.pid), str(os.getuid())], identity
                buf = ctypes.create_string_buffer(4096)
                assert libproc.proc_pidpath(pid, buf, len(buf)) > 0
                assert Path(os.fsdecode(buf.value)).resolve() == binary
                sample_command = ['/usr/bin/sample', str(pid), '5', '10', '-mayDie', '-fullPaths',
                                  '-file', str(out/'cold-click.sample.txt')]
                receipt.update(sample_invocations=1, selected_pid=pid, selected_executable=str(binary),
                               sample_command=sample_command, sample_started_epoch=time.time(),
                               sample_trigger_stage=list(sorted(slow)[-1]), first_click_record=list(first_click))
                sample_stream = (out/'sample-driver.log').open('wb')
                sampler = subprocess.Popen(sample_command, stdout=sample_stream, stderr=subprocess.STDOUT)
            time.sleep(.02)
    except Exception as exc:
        error = type(exc).__name__+': '+str(exc)
    finally:
        # A collector failure never retries, alters, or prematurely kills CTest.
        test_exit = process.wait()
        if reader_started: reader.join()
        receipt.update(ctest_exit=test_exit, finished_epoch=time.time(), observer_error=error,
                       reader_errors=reader_errors)
        # Seal the authoritative result before any optional sampler cleanup.
        (out/'ctest-exit-code.txt').write_text(str(test_exit)+'\n')
        write_json(out/'run-receipt.json', receipt)
        temporary = build/'Testing/Temporary'
        if temporary.exists(): shutil.copytree(temporary, out/'ctest-temporary', dirs_exist_ok=True)
        if sampler is not None:
            try:
                try:
                    sample_exit = sampler.wait(timeout=30)
                except subprocess.TimeoutExpired:
                    sampler.terminate()
                    receipt['sample_collector_timeout'] = True
                    sample_exit = sampler.wait(timeout=10)
                receipt.update(sample_exit=sample_exit, sample_finished_epoch=time.time())
            except Exception as exc:
                receipt['sample_cleanup_error'] = type(exc).__name__+': '+str(exc)
        if sample_stream: sample_stream.close()
        if mapped: mapped.close()
        if mapped_stream: mapped_stream.close()
        sample_file = out/'cold-click.sample.txt'
        receipt['sampling_status'] = ('not_triggered' if receipt['sample_invocations']==0 else
            'available' if receipt.get('sample_exit')==0 and sample_file.is_file()
            and sample_file.stat().st_size>0 else 'missing_or_partial')
        write_json(out/'run-receipt.json', receipt)
    integrity_error = None
    try:
        junit = ET.parse(build/'cab-results.xml').getroot()
        tests = list(junit.iter('testcase'))
        assert len(tests) == 1 and tests[0].attrib['name'] == TEST
        h, holes = decode(trace, out)
        assert h['ended'] == 1 and h['dropped'] == 0 and not holes
    except Exception as exc:
        integrity_error = type(exc).__name__+': '+str(exc)
    result = dict(original_ctest_exit=test_exit, original_result='FAIL' if test_exit else 'PASS_NOREPRO',
                  observer_error=error, trace_integrity_error=integrity_error,
                  sampling_status=receipt['sampling_status'],
                  release_approval=False, test_invocations=1,
                  note='Original failure is retained independently of successful trace collection.')
    write_json(out/'diagnostic-verdict.json', result)
    print(json.dumps(result, indent=2))
    return test_exit or (1 if integrity_error or error or reader_errors
                        or receipt['sampling_status']=='missing_or_partial' else 0)


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('mode', choices=['symbols', 'run'])
    parser.add_argument('--build', type=Path, default=Path('build-macos-cold-click'))
    args = parser.parse_args()
    build = args.build.resolve()
    out = build/'diagnostics'
    if args.mode == 'symbols':
        symbols(build, out)
    else:
        sys.exit(run(build, out))
