#!/usr/bin/env python3
"""Exact released-binary/candidate ABI renders. No DAW acceptance is inferred."""
import argparse
import json
from pathlib import Path
import struct
import subprocess
import sys
import xml.etree.ElementTree as ET

from eq_legacy_fixture import BINARY_SHA, canonical, sha, xml
from eq_tuner_parameter_contract import normalized_equal, align_metadata, rows


def audio_equal(left, right):
    if len(left) != 16384 * 2 * 4 or len(right) != len(left):
        raise ValueError('missing/truncated stereo render')
    import math
    a = struct.unpack('<32768f', left)
    b = struct.unpack('<32768f', right)
    if not all(math.isfinite(x) for x in (*a, *b)):
        raise ValueError('nonfinite render')
    if sum(x*x for x in a) <= 1e-12 or sum(x*x for x in b) <= 1e-12:
        raise ValueError('silent render')
    if left != right:
        changed = sum(struct.pack('<f', x) != struct.pack('<f', y) for x, y in zip(a, b))
        raise ValueError(f'nonidentical render: changed_samples={changed}, max_abs={max(abs(x-y) for x,y in zip(a,b)):.17g}; no tolerance waiver')


def isolate(source, target):
    tree = xml(Path(source).read_bytes())
    # Test fixture only. Keep EQ bands, occupancy, A/B, and auxiliary nodes.
    # Enable both EQ banks (legacy A defaults bypass them) and disable unrelated
    # amp/cab/time-dependent effects in the active root. Never mutate originals.
    off = {'gateon', 'transposeon', 'tuneron', 'preon', 'precompon', 'filteron',
           'fuzzon', 'booston', 'boardEnabled', 'buscompon', 'preampon', 'eqon',
           'choruson', 'flangeron', 'phaseron', 'tremoloon', 'delayon', 'reverbon',
           'doubleron', 'metronome', 'lowcomp', 'mode', 'oversampling',
           'toneEQ_bypass', 'finalEQ_bypass', 'cleanCompOn', 'liveLimiterOn'}
    off.update(f'dynamicEQ{i}_target' for i in range(1, 5))
    off.update(f'{prefix}{i}' for prefix in ('ampon', 'cab', 'level', 'bandtone') for i in range(1, 4))
    off.update({'input', 'output'})
    found = set()
    for node in tree.findall('PARAM'):
        key = node.get('id')
        if key in off or (key.startswith('pn_') and key.endswith('_native')) or (key.startswith('nativeAmp_') and key.endswith('_enabled')):
            node.set('value', '0'); found.add(key)
    essential = {'mode', 'gateon', 'boardEnabled', 'output', 'oversampling', 'ampon1', 'ampon2', 'ampon3', 'cab1', 'cab2', 'cab3', 'toneEQ_bypass', 'finalEQ_bypass', 'cleanCompOn', 'liveLimiterOn'}
    off.update(f'dynamicEQ{i}_target' for i in range(1, 5))
    if not essential <= found:
        raise ValueError('isolation parameter missing: ' + repr(essential-found))
    encoded = ET.tostring(tree, encoding='utf-8')
    target.write_bytes(struct.pack('<II', 0x21324356, len(encoded)) + encoded + b'\0')


def compare(label, a, b, results, state=True):
    audio_equal((a/'audio.f32').read_bytes(), (b/'audio.f32').read_bytes())
    if len(rows(a/'parameters.tsv')) != len(rows(b/'parameters.tsv')):
        old, new = (a,b) if len(rows(a/'parameters.tsv')) < len(rows(b/'parameters.tsv')) else (b,a)
        align_metadata(rows(old/'parameters.tsv'), rows(new/'parameters.tsv'))
    normalized_equal(a, b)
    for name in ('render.txt',):
        if (a/name).read_bytes() != (b/name).read_bytes():
            raise ValueError(label + ': ' + name + ' mismatch')
    if state and canonical(xml((a/'component.bin').read_bytes())) != canonical(xml((b/'component.bin').read_bytes())):
        raise ValueError(label + ': serialized state mismatch')
    results.append({'comparison': label, 'status': 'PASS', 'audio_sha256': sha(a/'audio.f32'),
                 'samples': 32768, 'exact_audio_bits': True, 'normalized_values_exact': True,
                 'latency_exact': True, 'serialized_state_exact': state})


def main():
    p = argparse.ArgumentParser()
    for key in ('host', 'released', 'candidate', 'states', 'lanes', 'output'):
        p.add_argument('--' + key, required=True)
    args = p.parse_args()
    root = Path(args.output).resolve(); root.mkdir(parents=True, exist_ok=True)
    result = {'schema': 1, 'status': 'FAIL', 'actual_daw': 'NOT_ASSESSED', 'release_approved': False,
              'scope': 'Classic isolated EQ via real VST3 process, 48 kHz/256 stereo; fixed midpoint queues',
              'comparisons': [], 'runs': []}
    def run(name, binary, state, replay=False):
        out = root/name
        cmd = [str(Path(args.host).resolve()), str(Path(binary).resolve()), str(out),
               'render-replay' if replay else 'render', str(state)]
        if replay: cmd.append(str(Path(args.lanes).resolve()))
        proc = subprocess.run(cmd, capture_output=True, text=True, timeout=120)
        (root/(name+'.log')).write_text(proc.stdout+proc.stderr)
        if proc.returncode: raise ValueError(f'ABI run failed {name}: {proc.stdout[-1000:]} {proc.stderr[-1000:]}')
        result['runs'].append({'name': name, 'binary_sha256': sha(binary), 'input_state_sha256': sha(state),
                               'output_state_sha256': sha(out/'component.bin'), 'audio_sha256': sha(out/'audio.f32')})
        return out
    try:
        if sha(args.released) != BINARY_SHA:
            raise ValueError('released VST3 digest mismatch')
        result.update(host_sha256=sha(args.host), released_sha256=sha(args.released), candidate_sha256=sha(args.candidate))
        for phase in ('deleted', 'reused', 'automated'):
            state = root/(phase+'.chimera')
            isolate(Path(args.states)/'ab-seeded'/(phase+'.chimera'), state)
            c1 = run(phase+'-candidate', args.candidate, state)
            c2 = run(phase+'-candidate-repeat', args.candidate, state)
            compare(phase+' candidate A/A', c1, c2, result['comparisons'])
            b1 = run(phase+'-beta2', args.released, c1/'component.bin')
            b2 = run(phase+'-beta2-repeat', args.released, c1/'component.bin')
            compare(phase+' beta2 A/A', b1, b2, result['comparisons'])
            back = run(phase+'-candidate-back', args.candidate, b1/'component.bin')
            compare(phase+' candidate -> beta2', c1, b1, result['comparisons'])
            compare(phase+' candidate -> beta2 -> candidate', c1, back, result['comparisons'])
            ca = run(phase+'-candidate-automation', args.candidate, state, True)
            ba = run(phase+'-beta2-automation', args.released, state, True)
            if phase == 'reused' and (ca/'audio.f32').read_bytes() == (c1/'audio.f32').read_bytes():
                raise ValueError('automation did not affect active reused-band audio')
            compare(phase+' released numeric automation', ca, ba, result['comparisons'])
        result['active_eq_automation_effect'] = 'PASS'
        result['status'] = 'PASS'
    except Exception as exc:
        result['error'] = str(exc)
    finally:
        (root/'summary.json').write_text(json.dumps(result, indent=2)+'\n')
    print(json.dumps({k: v for k, v in result.items() if k not in ('runs', 'comparisons')}, indent=2))
    print(f"exact comparisons passed: {len(result['comparisons'])}/15; actual DAW NOT_ASSESSED")
    return 0 if result['status'] == 'PASS' else 1


if __name__ == '__main__':
    sys.exit(main())
