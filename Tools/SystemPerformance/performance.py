#!/usr/bin/env python3
"""Source/binary-bound performance evidence. Python 3.10+, standard library only.

This tool never certifies requirements or changes a release gate. DAW callback
telemetry must come from a host/adapter; a process CPU meter is not a substitute.
"""
import argparse
import csv
import hashlib
import json
import math
import os
import platform
import re
import shutil
import subprocess
import sys
import time
from datetime import datetime, timezone
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SCHEMA = ROOT / 'Validation/system-performance/measurement.schema.json'
CALLBACK_COLUMNS = ['index', 'start_ns', 'end_ns', 'thread_cpu_ns', 'deadline_ns', 'frames']
MEMORY_COLUMNS = ['elapsed_seconds', 'pid', 'rss_bytes']


def fail(message):
    raise ValueError(message)


def strict_json(text):
    def pairs(items):
        result = {}
        for key, value in items:
            if key in result:
                fail(f'duplicate JSON key: {key}')
            result[key] = value
        return result
    return json.loads(text, object_pairs_hook=pairs,
                      parse_constant=lambda v: fail(f'non-finite JSON number: {v}'))


def read_json(path):
    return strict_json(Path(path).read_text(encoding='utf-8'))


def write_json(path, value):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, ensure_ascii=False, indent=2, allow_nan=False) + '\n', encoding='utf-8')


def digest(value):
    return hashlib.sha256(json.dumps(value, sort_keys=True, separators=(',', ':'),
                                     ensure_ascii=False, allow_nan=False).encode()).hexdigest()


def file_hash(path):
    h = hashlib.sha256()
    with Path(path).open('rb') as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b''):
            h.update(chunk)
    return h.hexdigest()


def schema_validate(value, schema, path='$'):
    """Validate the closed JSON Schema vocabulary used by our versioned schema.

    Unsupported keywords are rejected, not silently ignored. The same file can
    also be validated by any Draft 2020-12 implementation.
    """
    supported = {'$schema', '$id', 'title', 'description', 'type', 'properties',
                 'required', 'additionalProperties', 'items', 'enum', 'minimum',
                 'maximum', 'minLength', 'pattern', 'minItems', 'maxItems'}
    if set(schema) - supported:
        fail(f'unsupported schema keyword at {path}: {set(schema) - supported}')
    types = schema.get('type', [])
    types = [types] if isinstance(types, str) else types
    actual = ('null' if value is None else 'boolean' if type(value) is bool else
              'integer' if type(value) is int else 'number' if type(value) is float else
              'string' if type(value) is str else 'array' if type(value) is list else
              'object' if type(value) is dict else 'unknown')
    if types and actual not in types and not (actual == 'integer' and 'number' in types):
        fail(f'{path}: expected {types}, got {actual}')
    if 'enum' in schema and not any(type(value) is type(x) and value == x for x in schema['enum']):
        fail(f'{path}: invalid enum')
    if actual in ('integer', 'number'):
        if not math.isfinite(value):
            fail(f'{path}: non-finite number')
        if value < schema.get('minimum', -math.inf) or value > schema.get('maximum', math.inf):
            fail(f'{path}: number outside range')
    if actual == 'string':
        if len(value) < schema.get('minLength', 0) or ('pattern' in schema and not re.search(schema['pattern'], value)):
            fail(f'{path}: invalid string')
    if actual == 'array':
        if not schema.get('minItems', 0) <= len(value) <= schema.get('maxItems', math.inf):
            fail(f'{path}: invalid item count')
        for i, item in enumerate(value):
            schema_validate(item, schema.get('items', {}), f'{path}[{i}]')
    if actual == 'object':
        if set(schema.get('required', [])) - value.keys():
            fail(f'{path}: missing fields {set(schema.get("required", [])) - value.keys()}')
        props = schema.get('properties', {})
        if schema.get('additionalProperties') is False and value.keys() - props.keys():
            fail(f'{path}: unexpected fields {value.keys() - props.keys()}')
        for key in value.keys() & props.keys():
            schema_validate(value[key], props[key], f'{path}.{key}')


def command(args):
    try:
        return (subprocess.check_output(args, text=True, errors='replace',
                                        stderr=subprocess.DEVNULL, timeout=5) or '').strip() or None
    except (OSError, subprocess.SubprocessError):
        return None


def text_file(path):
    try:
        return Path(path).read_text().strip()
    except OSError:
        return None


def environment():
    system = platform.system()
    cpu = platform.processor() or None
    ram = None
    physical = None
    power = None
    constraints = []
    if system == 'Linux':
        info = text_file('/proc/cpuinfo') or ''
        match = re.search(r'^model name\s*:\s*(.+)$', info, re.M)
        cpu = match.group(1) if match else cpu
        mem = re.search(r'^MemTotal:\s*(\d+)', text_file('/proc/meminfo') or '', re.M)
        ram = int(mem.group(1)) * 1024 if mem else None
        for name in ('cpu.max', 'memory.max', 'cpuset.cpus.effective'):
            value = text_file('/sys/fs/cgroup/' + name)
            if value:
                constraints.append(f'cgroup {name}={value}')
        power = text_file('/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor')
        if not Path('/dev/snd').exists():
            constraints.append('No /dev/snd audio device exposed')
    elif system == 'Darwin':
        cpu = command(['sysctl', '-n', 'machdep.cpu.brand_string']) or cpu
        ram_text = command(['sysctl', '-n', 'hw.memsize'])
        physical_text = command(['sysctl', '-n', 'hw.physicalcpu'])
        ram = int(ram_text) if ram_text and ram_text.isdigit() else None
        physical = int(physical_text) if physical_text and physical_text.isdigit() else None
        power = command(['pmset', '-g', 'custom'])
    elif system == 'Windows':
        # CIM query is read-only and fixed, without interpolated shell input.
        data = command(['powershell', '-NoProfile', '-Command',
                        '$c=Get-CimInstance Win32_Processor; $s=Get-CimInstance Win32_ComputerSystem; '
                        '@{cpu=($c.Name -join "; "); cores=($c.NumberOfCores | Measure-Object -Sum).Sum; '
                        'ram=$s.TotalPhysicalMemory} | ConvertTo-Json -Compress'])
        if data:
            d = strict_json(data)
            # CIM may return null/zero under a restricted Windows account.
            # Keep unavailable facts null instead of aborting or inventing RAM.
            cpu = d.get('cpu') or None
            physical = int(d['cores']) if d.get('cores') and d['cores'] > 0 else None
            ram = int(d['ram']) if d.get('ram') and d['ram'] > 0 else None
        power = command(['powercfg', '/getactivescheme'])
    return {'id': 'observed-' + system.lower(), 'os_family': system,
            'os_version': platform.platform(), 'architecture': platform.machine(),
            'cpu_model': cpu, 'physical_cores': physical, 'logical_cores': os.cpu_count(),
            'allocated_logical_cores': len(os.sched_getaffinity(0)) if hasattr(os, 'sched_getaffinity') else None,
            'ram_bytes': ram, 'virtualization': 'UNKNOWN', 'resource_constraints': constraints,
            'daw_name': None, 'daw_version': None, 'plugin_format': None,
            'interface_model': None, 'interface_firmware': None, 'driver_name': None,
            'driver_version': None, 'power_mode': power, 'power_source': None,
            'scheduler': None, 'notes': ['Auto-detected host facts; fill host/device/power fields before measurement.',
                                        'Host-visible RAM/cores are not necessarily the container allocation.']}


def scenarios():
    rows = []
    def add(group, mode, dual, count, buffer, shift, ui, transport='playback', seconds=60):
        settings = {'group': group, 'mode': mode, 'dual_mode': dual,
                    'instances': count, 'sample_rate_hz': 48000, 'buffer_samples': buffer,
                    'oversampling_factor': 4, 'transpose_semitones': shift,
                    'transpose_bypassed': shift == 0, 'editor': ui,
                    'analyzer': 'on' if ui == 'open' else 'off', 'transport': transport,
                    'warmup_seconds': 10, 'duration_seconds': seconds,
                    'instance_routing': 'parallel_tracks', 'repeat': 1,
                    'cab_paths_per_instance': {'Classic': 1, 'Dual': 2, 'Matrix': 3}[mode]}
        key = f'{group}-{mode.lower()}-{dual or "none"}-i{count}-b{buffer}-t{shift}-{transport}'
        rows.append({'id': key, 'settings': settings, 'status': 'NOT_RUN',
                     'reason': 'Scheduled; no measurement attempted', 'metrics': None, 'evidence': None})
    for mode in ('Classic', 'Dual', 'Matrix'):
        for count in (1, 2, 4):
            for buffer in (128, 256):
                for shift in (0, -2):
                    add('core', mode, 'Blend' if mode == 'Dual' else None, count, buffer, shift, 'closed')
    for count in (1, 2, 4):
        for buffer in (128, 256):
            for shift in (0, -2):
                add('dual_crossover', 'Dual', 'Crossover', count, buffer, shift, 'closed')
    for mode in ('Classic', 'Dual', 'Matrix'):
        for buffer in (128, 256):
            for shift in (0, -2):
                add('gui_focus', mode, 'Blend' if mode == 'Dual' else None, 4, buffer, shift, 'open')
    for transport in ('playback', 'recording'):
        add('soak', 'Matrix', None, 4, 128, -2, 'open', transport, 1800)
    return rows


def make_study(env, blocked=None):
    rows = scenarios()
    if blocked:
        for row in rows:
            row.update(status='BLOCKED', reason=blocked)
    return {'schema_version': 1, 'created_utc': datetime.now(timezone.utc).isoformat(),
            'requirements_certified': False,
            'tool': {'name': 'Chimera SystemPerformance', 'version': '1',
                     'script_sha256': file_hash(__file__)},
            'product': {'source_sha': None, 'version': None, 'build_id': None,
                        'build_type': 'Release', 'binary_sha256': None, 'package_sha256': None,
                        'binary_identity_method': 'sha256-loaded-executable', 'notes': []},
            'environment': env,
            'workload': {'input_sha256': None, 'preset_bank_sha256': None, 'ir_manifest_sha256': None,
                         'description': None},
            'runs': rows}


def target_environments():
    """Requested platform/format slots, never invented workstation inventory."""
    slots = []
    for family, arch, formats in (
        ('Windows', 'x86_64', ('VST3', 'Standalone', 'AAX')),
        ('Darwin', 'x86_64', ('VST3', 'AU', 'Standalone', 'AAX')),
        ('Darwin', 'arm64', ('VST3', 'AU', 'Standalone', 'AAX')),
        ('Linux', 'x86_64', ('VST3', 'Standalone')),
    ):
        for fmt in formats:
            env = {k: None for k in read_json(SCHEMA)['properties']['environment']['properties']}
            env.update(id=f'{family.lower()}-{arch}-{fmt.lower()}', os_family=family,
                       architecture=arch, plugin_format=fmt, virtualization='UNKNOWN',
                       resource_constraints=[], notes=['Planning slot, not observed hardware.'])
            slots.append({'environment': env, 'status': 'BLOCKED', 'scenario_count': len(scenarios()),
                          'reason': ('Native hardware/DAW/interface unavailable; signed AAX and Pro Tools acceptance also unavailable'
                                     if fmt == 'AAX' else 'Native hardware/DAW/interface unavailable in this execution session')})
    return {'schema_version': 1, 'scope': 'Planning slots; no performance measurements', 'targets': slots}


def number(value, name, integer=False):
    try:
        result = int(value) if integer else float(value)
    except (TypeError, ValueError):
        fail(f'invalid {name}: {value!r}')
    if not math.isfinite(result) or result < 0:
        fail(f'invalid {name}: {value!r}')
    return result


def csv_rows(path, columns):
    with Path(path).open(newline='', encoding='utf-8') as stream:
        reader = csv.DictReader(stream)
        if reader.fieldnames != columns:
            fail(f'{path}: expected CSV header {columns}')
        for row in reader:
            if None in row or any(value is None for value in row.values()):
                fail(f'{path}: malformed CSV row')
            yield row


def percentile(values, q):
    return sorted(values)[max(0, math.ceil(len(values) * q) - 1)]


def summarize(callbacks, memory, settings):
    wall, cpu, deadline_misses = [], [], []
    first = last = None
    budget = settings['buffer_samples'] / settings['sample_rate_hz'] * 1e9
    for row in csv_rows(callbacks, CALLBACK_COLUMNS):
        index = number(row['index'], 'index', True)
        start, end = (number(row[k], k, True) for k in ('start_ns', 'end_ns'))
        frames = number(row['frames'], 'frames', True)
        if index != len(wall) or frames != settings['buffer_samples'] or end < start:
            fail('non-contiguous callback index, unexpected block size or inverted timestamp')
        if last is not None and start < last:
            fail('callback trace overlaps or is not monotonic; use one whole-graph callback')
        if first is None:
            first = start
        last = end
        wall.append(end - start)
        if row['thread_cpu_ns'] != '':
            cpu.append(number(row['thread_cpu_ns'], 'thread_cpu_ns', True))
        if row['deadline_ns'] != '':
            deadline = number(row['deadline_ns'], 'deadline_ns', True)
            deadline_misses.append(end > deadline)
    if not wall:
        fail('empty callback trace')
    if len(cpu) not in (0, len(wall)) or len(deadline_misses) not in (0, len(wall)):
        fail('partial counters: omit an unavailable counter for every row, never selectively')
    samples = list(csv_rows(memory, MEMORY_COLUMNS))
    if len(samples) < 2:
        fail('at least two RSS samples required')
    times = [number(r['elapsed_seconds'], 'elapsed_seconds') for r in samples]
    rss = [number(r['rss_bytes'], 'rss_bytes', True) for r in samples]
    pids = [number(r['pid'], 'pid', True) for r in samples]
    if len(set(pids)) != 1 or any(b <= a for a, b in zip(times, times[1:])):
        fail('memory process identity/time changed')
    if any(b - a > 5.0 for a, b in zip(times, times[1:])):
        fail('memory sampling gap exceeds 5 seconds')
    period_us = budget / 1000
    return {'callback_count': len(wall), 'audio_seconds': len(wall) * budget / 1e9,
            'callback_span_seconds': (last - first) / 1e9,
            'callback_wall_mean_us': sum(wall) / len(wall) / 1000,
            'callback_wall_p99_us': percentile(wall, .99) / 1000,
            'callback_wall_peak_us': max(wall) / 1000,
            'callback_wall_load_percent': 100 * sum(wall) / (len(wall) * budget),
            'callback_thread_cpu_load_percent': 100 * sum(cpu) / (len(cpu) * budget) if cpu else None,
            'callback_thread_cpu_p99_us': percentile(cpu, .99) / 1000 if cpu else None,
            'callback_thread_cpu_peak_us': max(cpu) / 1000 if cpu else None,
            'compute_over_budget_count': sum(t > budget for t in wall),
            'scheduled_deadline_miss_count': sum(deadline_misses) if deadline_misses else None,
            'period_us': period_us, 'rss_start_bytes': rss[0], 'rss_sampled_peak_bytes': max(rss),
            'rss_end_bytes': rss[-1], 'rss_growth_bytes': rss[-1] - rss[0],
            'rss_observed_seconds': times[-1] - times[0],
            'rss_max_sample_gap_seconds': max(b-a for a,b in zip(times, times[1:])),
            'xruns': None, 'glitches': None, 'stability_status': 'NOT_RUN',
            'session_reopen_status': 'NOT_RUN', 'measurement_kind': None}


def check_identity(study, run, receipt):
    schema_validate(receipt, read_json(ROOT / 'Validation/system-performance/receipt.schema.json'))
    expected = {'run_id': run['id'], 'product_binary_sha256': study['product']['binary_sha256'],
                'product_manifest_sha256': digest(study['product']),
                'environment_sha256': digest(study['environment']),
                'settings_sha256': digest(run['settings']), 'workload_sha256': digest(study['workload'])}
    for key, value in expected.items():
        if receipt[key] != value:
            fail(f'receipt identity mismatch: {key}')
    if receipt['settings_verified'] is not True:
        fail('actual loaded settings/parallel routing must be verified by the adapter or operator')
    if receipt['telemetry_dropped_rows'] != 0:
        fail('dropped telemetry rows invalidate timing evidence')
    if receipt['warmup_seconds'] < run['settings']['warmup_seconds']:
        fail('insufficient warmup')
    if receipt['measurement_kind'] == 'daw_device':
        env = study['environment']
        for key in ('daw_name', 'daw_version', 'plugin_format', 'interface_model', 'driver_name', 'driver_version', 'power_mode', 'power_source'):
            if not env[key]:
                fail(f'actual DAW/device measurement requires environment.{key}')
        if env['power_source'] == 'UNKNOWN':
            fail('actual DAW/device measurement requires observed AC/battery power source')
    return expected


def measure(study, run, callbacks, memory, receipt):
    check_identity(study, run, receipt)
    if any(int(row['pid']) != receipt['process_id'] for row in csv_rows(memory, MEMORY_COLUMNS)):
        fail('memory process ID does not match callback receipt')
    metrics = summarize(callbacks, memory, run['settings'])
    settings = run['settings']
    tolerance = settings['buffer_samples'] / settings['sample_rate_hz']
    target = settings['duration_seconds']
    if metrics['audio_seconds'] + tolerance < target:
        fail('callback coverage shorter than requested audio duration')
    if receipt['measurement_kind'] != 'offline_processor' and metrics['callback_span_seconds'] + tolerance < target:
        fail('paced/device wall-clock observation shorter than requested duration')
    if metrics['rss_observed_seconds'] + 2 < target:
        fail('RSS observation shorter than requested duration')
    metrics.update(measurement_kind=receipt['measurement_kind'], xruns=receipt['xruns'],
                   glitches=receipt['glitches'], session_reopen_status=receipt['session_reopen_status'])
    if settings['group'] == 'soak':
        # A synthetic long render never becomes a DAW playback/recording pass.
        if receipt['measurement_kind'] != 'daw_device':
            metrics['stability_status'] = 'BLOCKED'
        elif receipt['continuous_transport'] is False or receipt['xruns'] or receipt['glitches'] or receipt['crashed'] or (metrics['scheduled_deadline_miss_count'] or 0) > 0 or receipt['session_reopen_status'] == 'FAIL' or (settings['transport'] == 'recording' and receipt['recording_verified'] is False):
            metrics['stability_status'] = 'FAIL'
        elif receipt['transport_seconds'] + tolerance < target or receipt['continuous_transport'] is None or receipt['xruns'] is None or receipt['glitches'] is None:
            metrics['stability_status'] = 'BLOCKED'
        elif settings['transport'] == 'recording' and not receipt['recording_verified']:
            metrics['stability_status'] = 'BLOCKED'
        else:
            metrics['stability_status'] = 'PASS'
    return metrics


def artifact_path(root, ref):
    path = (root / ref['path']).resolve()
    if not path.is_relative_to(root.resolve()) or not path.is_file():
        fail('missing evidence or path escapes study directory')
    if file_hash(path) != ref['sha256']:
        fail(f'evidence digest mismatch: {path.name}')
    return path


def validate(study, root=None):
    schema_validate(study, read_json(SCHEMA))
    if study['requirements_certified'] is not False:
        fail('this tool cannot certify system requirements')
    env = study['environment']
    if env['plugin_format'] == 'AU' and env['os_family'] != 'Darwin':
        fail('AU requires macOS')
    if env['plugin_format'] == 'AAX' and env['os_family'] == 'Linux':
        fail('AAX is a Windows/macOS target, not Linux')
    seen = set()
    for run in study['runs']:
        if run['id'] in seen:
            fail('duplicate run id')
        seen.add(run['id'])
        s = run['settings']
        if (s['mode'] == 'Dual') != (s['dual_mode'] is not None):
            fail('Dual mode must identify Blend/Crossover; other modes must use null')
        if s['transpose_bypassed'] != (s['transpose_semitones'] == 0):
            fail('transpose bypass/shift mismatch')
        if s['cab_paths_per_instance'] != {'Classic': 1, 'Dual': 2, 'Matrix': 3}[s['mode']]:
            fail('default protocol requires all mode-specific CAB paths')
        if s['group'] == 'soak' and s['duration_seconds'] < 1800:
            fail('soak must request at least 1800 seconds')
        if run['status'] in ('NOT_RUN', 'BLOCKED'):
            if run['metrics'] is not None or run['evidence'] is not None:
                fail('unexecuted rows cannot contain measurements')
        elif run['status'] == 'MEASURED':
            for key in ('source_sha', 'version', 'build_id', 'binary_sha256'):
                if study['product'][key] is None:
                    fail(f'measured row requires product.{key}')
            if study['product']['build_type'] != 'Release':
                fail('system requirements comparison requires a Release binary')
            for key in ('cpu_model', 'ram_bytes', 'os_version', 'architecture'):
                if study['environment'][key] is None:
                    fail(f'measured row requires environment.{key}')
            if any(study['workload'][k] is None for k in study['workload']):
                fail('measured row requires input, preset bank and IR manifest identities')
            if not run['metrics'] or not run['evidence'] or root is None:
                fail('measured row requires raw evidence for recomputation')
            paths = {k: artifact_path(root, v) for k,v in run['evidence'].items()}
            calculated = measure(study, run, paths['callbacks'], paths['memory'], read_json(paths['receipt']))
            if calculated != run['metrics']:
                fail('metrics do not match raw evidence')
        elif run['metrics'] is not None or run['evidence'] is not None:
            fail('FAILED collection uses null metrics; retain partial raw files separately')
    return study


def rss_bytes(pid):
    if platform.system() == 'Linux':
        value = text_file(f'/proc/{pid}/status') or ''
        match = re.search(r'^VmRSS:\s+(\d+)', value, re.M)
        if not match:
            fail('RSS unavailable or process exited')
        return int(match.group(1)) * 1024
    if platform.system() == 'Darwin':
        value = command(['ps', '-o', 'rss=', '-p', str(pid)])
        if not value:
            fail('RSS unavailable or process exited')
        return int(value) * 1024
    if platform.system() == 'Windows':
        import ctypes
        from ctypes import wintypes
        class Counters(ctypes.Structure):
            _fields_ = [('cb', wintypes.DWORD), ('PageFaultCount', wintypes.DWORD)] + [(n, ctypes.c_size_t) for n in
                ('PeakWorkingSetSize', 'WorkingSetSize', 'QuotaPeakPagedPoolUsage', 'QuotaPagedPoolUsage',
                 'QuotaPeakNonPagedPoolUsage', 'QuotaNonPagedPoolUsage', 'PagefileUsage', 'PeakPagefileUsage')]
        kernel = ctypes.WinDLL('kernel32', use_last_error=True)
        psapi = ctypes.WinDLL('psapi', use_last_error=True)
        kernel.OpenProcess.argtypes = [wintypes.DWORD, wintypes.BOOL, wintypes.DWORD]
        kernel.OpenProcess.restype = wintypes.HANDLE
        kernel.CloseHandle.argtypes = [wintypes.HANDLE]
        psapi.GetProcessMemoryInfo.argtypes = [wintypes.HANDLE, ctypes.POINTER(Counters), wintypes.DWORD]
        psapi.GetProcessMemoryInfo.restype = wintypes.BOOL
        handle = kernel.OpenProcess(0x0400 | 0x0010, False, pid)
        if not handle:
            fail('cannot open process for RSS sampling')
        try:
            counters = Counters()
            counters.cb = ctypes.sizeof(counters)
            if not psapi.GetProcessMemoryInfo(handle, ctypes.byref(counters), counters.cb):
                fail('GetProcessMemoryInfo failed')
            return counters.WorkingSetSize
        finally:
            kernel.CloseHandle(handle)
    fail('unsupported memory sampler platform')


def process_identity(pid):
    if platform.system() == 'Linux':
        stat = text_file(f'/proc/{pid}/stat')
        if not stat:
            fail('process identity unavailable')
        # comm may contain spaces/parentheses; starttime is field 22.
        fields = stat.rsplit(')', 1)[-1].split()
        return fields[19]
    if platform.system() == 'Darwin':
        value = command(['ps', '-o', 'lstart=', '-p', str(pid)])
    elif platform.system() == 'Windows':
        import ctypes
        from ctypes import wintypes
        kernel = ctypes.WinDLL('kernel32', use_last_error=True)
        kernel.OpenProcess.argtypes = [wintypes.DWORD, wintypes.BOOL, wintypes.DWORD]
        kernel.OpenProcess.restype = wintypes.HANDLE
        kernel.CloseHandle.argtypes = [wintypes.HANDLE]
        kernel.GetProcessTimes.argtypes = [wintypes.HANDLE] + [ctypes.POINTER(wintypes.FILETIME)] * 4
        handle = kernel.OpenProcess(0x1000, False, pid)
        if not handle:
            fail('process identity unavailable')
        try:
            created, exited, used, system = (wintypes.FILETIME() for _ in range(4))
            if not kernel.GetProcessTimes(handle, ctypes.byref(created), ctypes.byref(exited), ctypes.byref(used), ctypes.byref(system)):
                fail('process identity unavailable')
            value = str((created.dwHighDateTime << 32) | created.dwLowDateTime)
        finally:
            kernel.CloseHandle(handle)
    else:
        value = None
    if not value:
        fail('process identity unavailable')
    return value


def sample_memory(pid, seconds, interval, out):
    if seconds <= 0 or not .1 <= interval <= 2 or not math.isfinite(seconds):
        fail('positive duration and 0.1–2 second sample interval required')
    if pid <= 0:
        fail('positive PID required')
    identity = process_identity(pid)
    start = time.monotonic()
    with Path(out).open('w', newline='', encoding='utf-8') as stream:
        writer = csv.writer(stream)
        writer.writerow(MEMORY_COLUMNS)
        while True:
            if process_identity(pid) != identity:
                fail('process exited or PID was reused')
            elapsed = time.monotonic() - start
            writer.writerow([f'{elapsed:.9f}', pid, rss_bytes(pid)])
            stream.flush()
            if elapsed >= seconds:
                break
            time.sleep(min(interval, seconds - elapsed))


def write_table(study, path):
    env = study['environment']
    def safe(v):
        return str(v if v is not None else 'UNKNOWN').replace('|', '/').replace('\n', ' ')
    lines = ['# Performance environment and observations', '',
             'No certified CPU/RAM minimum or instance capacity is established by this table.', '',
             '| Environment field | Observed / declared value |', '| --- | --- |']
    lines += [f'| {k} | {safe(v)} |' for k,v in env.items()]
    lines += ['', '| Run | Status | wall p99 / peak (µs) | thread CPU (%) | compute over budget / scheduled miss | sampled RSS peak (bytes) | soak |',
              '| --- | --- | ---: | ---: | --- | ---: | --- |']
    for row in study['runs']:
        m = row['metrics']
        vals = (f"{m['callback_wall_p99_us']:.3f} / {m['callback_wall_peak_us']:.3f}",
                safe(m['callback_thread_cpu_load_percent']),
                f"{m['compute_over_budget_count']} / {safe(m['scheduled_deadline_miss_count'])}",
                str(m['rss_sampled_peak_bytes']), m['stability_status']) if m else ('—', '—', '—', '—', row['status'])
        lines.append(f"| {row['id']} | {row['status']} | " + ' | '.join(vals) + ' |')
    lines += ['', '## Reasons', ''] + [f"- {status}: {reason}" for status,reason in sorted({(r['status'],r['reason']) for r in study['runs']})]
    Path(path).write_text('\n'.join(lines) + '\n', encoding='utf-8')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    subs = parser.add_subparsers(dest='cmd', required=True)
    p = subs.add_parser('init'); p.add_argument('--out', type=Path, required=True); p.add_argument('--blocked')
    p = subs.add_parser('targets'); p.add_argument('--out', type=Path, required=True)
    p = subs.add_parser('validate'); p.add_argument('study', type=Path)
    p = subs.add_parser('table'); p.add_argument('study', type=Path); p.add_argument('--out', type=Path, required=True)
    p = subs.add_parser('bind'); p.add_argument('study', type=Path)
    for name in ('binary', 'input', 'preset-bank', 'ir-manifest', 'out'):
        p.add_argument('--' + name, type=Path, required=True)
    for name in ('source-sha', 'version', 'build-id', 'description'):
        p.add_argument('--' + name, required=True)
    p = subs.add_parser('receipt-template'); p.add_argument('study', type=Path); p.add_argument('--run', required=True); p.add_argument('--out', type=Path, required=True)
    p = subs.add_parser('sample-memory'); p.add_argument('--pid', type=int, required=True); p.add_argument('--seconds', type=float, default=1800); p.add_argument('--interval', type=float, default=1); p.add_argument('--out', type=Path, required=True)
    p = subs.add_parser('ingest'); p.add_argument('study', type=Path); p.add_argument('--run', required=True)
    for name in ('callbacks', 'memory', 'receipt', 'out'):
        p.add_argument('--' + name, type=Path, required=True)
    args = parser.parse_args()
    if args.cmd == 'init':
        study = make_study(environment(), args.blocked)
        validate(study); write_json(args.out, study)
    elif args.cmd == 'sample-memory':
        sample_memory(args.pid, args.seconds, args.interval, args.out)
    elif args.cmd == 'targets':
        write_json(args.out, target_environments())
    else:
        study = validate(read_json(args.study), args.study.parent)
        if args.cmd == 'table':
            write_table(study, args.out)
        elif args.cmd == 'bind':
            if any(r['status'] == 'MEASURED' for r in study['runs']):
                fail('cannot rebind existing measurements; initialize a new study')
            study['product'].update(source_sha=args.source_sha, version=args.version,
                                    build_id=args.build_id, binary_sha256=file_hash(args.binary))
            study['workload'].update(input_sha256=file_hash(args.input),
                                     preset_bank_sha256=file_hash(args.preset_bank),
                                     ir_manifest_sha256=file_hash(args.ir_manifest), description=args.description)
            validate(study); write_json(args.out, study)
        elif args.cmd in ('ingest', 'receipt-template'):
            run = next((r for r in study['runs'] if r['id'] == args.run), None)
            if run is None:
                fail('unknown run ID')
            if args.cmd == 'receipt-template':
                receipt = {'schema_version': 1, 'run_id': run['id'],
                           'product_binary_sha256': study['product']['binary_sha256'],
                           'product_manifest_sha256': digest(study['product']),
                           'environment_sha256': digest(study['environment']),
                           'settings_sha256': digest(run['settings']), 'workload_sha256': digest(study['workload']),
                           'measurement_kind': None, 'collector_name_version': None,
                           'callback_scope': 'full_product_graph', 'process_id': None, 'settings_verified': None,
                           'telemetry_dropped_rows': None, 'warmup_seconds': None,
                           'transport_seconds': None, 'continuous_transport': None,
                           'recording_verified': None, 'xruns': None, 'glitches': None,
                           'crashed': None, 'session_reopen_status': 'NOT_RUN',
                           'operator': None, 'observed_utc': None, 'notes': []}
                write_json(args.out, receipt)  # deliberately incomplete until observed
            else:
                if args.out.parent.resolve() != args.study.parent.resolve():
                    fail('output must remain beside input study so prior evidence paths remain valid')
                receipt = read_json(args.receipt)
                run['metrics'] = measure(study, run, args.callbacks, args.memory, receipt)
                run['status'] = 'MEASURED'
                run['reason'] = 'Raw observations imported; performance/requirements certification remains separate'
                run['evidence'] = {}
                # Content-addressed evidence is never overwritten by a later trial.
                for key in ('callbacks', 'memory', 'receipt'):
                    source = getattr(args, key)
                    sha = file_hash(source)
                    relative = Path('raw') / sha / source.name
                    target = args.out.parent / relative
                    target.parent.mkdir(parents=True, exist_ok=True)
                    if source.resolve() != target.resolve():
                        shutil.copyfile(source, target)
                    run['evidence'][key] = {'path': relative.as_posix(), 'sha256': sha}
                validate(study, args.out.parent); write_json(args.out, study)
        print(f'{args.cmd}: OK (requirements remain uncertified)')


if __name__ == '__main__':
    try:
        main()
    except (ValueError, OSError, KeyError) as exc:
        print(f'ERROR: {exc}', file=sys.stderr)
        sys.exit(2)
