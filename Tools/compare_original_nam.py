#!/usr/bin/env python3
"""Measure local NAM captures against the actual OriginalAmpProcessor at 4x.

Reports observations, not hardware equivalence or musical acceptance. NAM
weights and rendered audio stay outside the repository. Requires numpy/scipy.
"""
import argparse
import concurrent.futures
import hashlib
import json
import subprocess
from pathlib import Path

import numpy as np
from scipy.io import wavfile
from validate_nam import SR, fixture, metrics

LEVELS = [-54, -48, -42, -36, -30, -24, -12]
FREQUENCIES = [100, 400, 1200]
PRESETS = ["fenrir", "surtr", "nidhoggr", "fimbulvetr", "ragnarok"]


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def read(path):
    rate, data = wavfile.read(path)
    if rate != SR or data.ndim != 1 or data.dtype.kind != 'f':
        raise ValueError(f"Expected mono float {SR} Hz: {path}")
    return data.astype(np.float64)


def run(command):
    result = subprocess.run([str(x) for x in command], capture_output=True, text=True)
    if result.returncode:
        raise RuntimeError(f"Render failed: {command[0]}\n{result.stderr}")
    return result.stdout.strip()


def make_probe():
    # One second per step: 750 ms settling, 250 ms coherent FFT measurement.
    t = np.arange(SR) / SR
    parts = [10 ** (db / 20) * np.sin(2 * np.pi * hz * t) * np.minimum(t / .01, 1)
             for hz in FREQUENCIES for db in LEVELS]
    return np.concatenate(parts + [fixture(False), fixture(True)]).astype(np.float32)


def measure(data):
    if len(data) != (len(LEVELS) * len(FREQUENCIES) + 16) * SR or not np.isfinite(data).all():
        raise ValueError("Invalid render length or non-finite samples")
    response = []
    for fi, hz in enumerate(FREQUENCIES):
        readings = []
        for li, db in enumerate(LEVELS):
            start = (fi * len(LEVELS) + li) * SR
            v = data[start + 3 * SR // 4:start + SR]
            v = v - np.mean(v)
            spectrum = abs(np.fft.rfft(v))
            bins = np.array([int(hz * k / 4) for k in range(1, 13)])
            harmonics = spectrum[bins]
            fundamental = 2 * harmonics[0] / len(v)
            readings.append(dict(input_peak_dbfs=db,
                                 rms_dbfs=float(20 * np.log10(np.sqrt(np.mean(v*v)) + 1e-30)),
                                 fundamental_peak_dbfs=float(20*np.log10(fundamental + 1e-30)),
                                 thd_2_to_12=float(np.linalg.norm(harmonics[1:]) / max(harmonics[0], 1e-30))))
        weak, strong = readings[1], readings[5]  # -48 to -24 dBFS, same capture chain
        response.append(dict(hz=hz, readings=readings,
                             growth_24db_input_db=strong['rms_dbfs']-weak['rms_dbfs'],
                             fundamental_growth_24db_input_db=strong['fundamental_peak_dbfs']-weak['fundamental_peak_dbfs']))
    offset = len(LEVELS)*len(FREQUENCIES)*SR
    musical = {}
    for i, name in enumerate(['pluck', 'chord']):
        v = data[offset+i*8*SR:offset+(i+1)*8*SR]
        # The first note is at 0.5 s. Sustain/attack uses a fixed, phase-agnostic RMS window.
        attack = np.sqrt(np.mean(v[int(.51*SR):int(.56*SR)]**2))
        sustain = np.sqrt(np.mean(v[int(.8*SR):int(1.0*SR)]**2))
        musical[name] = dict(rms_dbfs=float(20*np.log10(np.sqrt(np.mean(v*v))+1e-30)),
                             peak_dbfs=float(20*np.log10(np.max(abs(v))+1e-30)),
                             first_note_sustain_attack_db=float(20*np.log10((sustain+1e-30)/(attack+1e-30))))
    return dict(response=response, synthetic_notes=musical,
                peak_dbfs=float(20*np.log10(np.max(abs(data))+1e-30)))


def main():
    parser = argparse.ArgumentParser()
    for name in ['models','manifest','nam-render','original-render','out']:
        parser.add_argument('--'+name, type=Path, required=True)
    parser.add_argument('--workers', type=int, default=2)
    parser.add_argument('--states', type=Path, help='Additional named renderer states, retaining the frozen v1 suite')
    args = parser.parse_args()
    args.out.mkdir(parents=True, exist_ok=True)
    manifest = json.loads(args.manifest.read_text())
    probe = make_probe()
    input_path = args.out/'probe.wav'
    wavfile.write(input_path, SR, probe)
    states = {'default': {}, 'gain_max': {'controls': {'gain': 1.0}}}
    states.update({p: {'preset': f'original.nastrond.{p}.v1'} for p in PRESETS})
    for macro in ['clank','crush','impact','rot','bloom']:
        for value in [0,1]:
            states[f'{macro}_{value}'] = {'controls': {macro: value}}
    if args.states:
        extra = json.loads(args.states.read_text())
        if not isinstance(extra, dict) or any(not name.replace('_', '').isalnum() for name in extra):
            raise ValueError('Additional states need safe nonempty names')
        if set(states).intersection(extra):
            raise ValueError('Additional states must not replace frozen v1 states')
        states.update(extra)
    originals, original_audio = {}, {}
    for name, state in states.items():
        config = args.out/(name+'.json')
        config.write_text(json.dumps(state))
        dest = args.out/(name+'.f32')
        stamp = args.out/(name+'.stamp.json')
        key = digest(args.original_render)+digest(input_path)+digest(config)
        cached = json.loads(stamp.read_text()) if stamp.exists() else {}
        if not dest.exists() or cached.get('key') != key or cached.get('output_sha256') != digest(dest):
            renderer_info = run([args.original_render,input_path,dest,config])
            stamp.write_text(json.dumps(dict(key=key,renderer=renderer_info,output_sha256=digest(dest))))
        else:
            renderer_info = cached['renderer']
        y = np.fromfile(dest,dtype=np.float32).astype(np.float64)
        originals[name] = dict(state=state,renderer=renderer_info,measurements=measure(y),audio_sha256=digest(dest))
        original_audio[name] = y
        print('ORIGINAL',name,flush=True)

    def reference_case(item):
        model = args.models/item['file']
        if digest(model) != item['sha256']:
            raise ValueError(f"Reference hash mismatch: {model}")
        if float(item['sample_rate']) != SR:
            raise ValueError('Reference sample rate differs')
        variants = [('digital_equal',0.)]
        cal = item['metadata'].get('input_level_dbu')
        if cal is not None:
            variants.append(('metadata_11_5_dbu',11.5-float(cal)))
        result = dict(family=item['family'],model_id=item['model_id'],name=item['name'],
                      capture_scope=item['capture_scope'],gain_group=item['gain_group'],sha256=item['sha256'],variants={})
        for variant, gain in variants:
            base = args.out/f"nam-{item['model_id']}-{variant}"
            inp = base.with_suffix('.input.wav')
            dest = base.with_suffix('.wav')
            wavfile.write(inp,SR,(probe*10**(gain/20)).astype(np.float32))
            key = item['sha256']+digest(inp)+digest(args.nam_render)+':slim=1'
            stamp = base.with_suffix('.stamp')
            cached = json.loads(stamp.read_text()) if stamp.exists() and stamp.read_text().startswith('{') else {}
            if not dest.exists() or cached.get('key') != key or cached.get('output_sha256') != digest(dest):
                run([args.nam_render,'--slim','1',model,inp,dest])
                stamp.write_text(json.dumps(dict(key=key,output_sha256=digest(dest))))
            y = read(dest)
            comparisons = {}
            offset = len(LEVELS)*len(FREQUENCIES)*SR
            # Cab-inclusive captures cannot establish amp-only spectral parity.
            if item['head_timbre_comparison_eligible']:
                for name, original in original_audio.items():
                    comparisons[name] = {kind: metrics(y[offset+i*8*SR:offset+(i+1)*8*SR],
                                                       original[offset+i*8*SR:offset+(i+1)*8*SR])
                                         for i,kind in enumerate(['pluck','chord'])}
            result['variants'][variant] = dict(input_gain_db=gain,measurements=measure(y),
                                               spectral_comparisons=comparisons,audio_sha256=digest(dest))
        print('NAM',item['family'],item['model_id'],flush=True)
        return result

    with concurrent.futures.ThreadPoolExecutor(max_workers=args.workers) as pool:
        references = list(pool.map(reference_case,manifest['references']))
    report = dict(schema=1,manifest_sha256=digest(args.manifest),probe_sha256=digest(input_path),
                  nam_renderer_sha256=digest(args.nam_render),original_renderer_sha256=digest(args.original_render),
                  protocol=dict(sample_rate=SR,frequencies_hz=FREQUENCIES,input_peak_dbfs=LEVELS,
                                settling_seconds=.75,measurement_seconds=.25,nam_slim=1,original_oversampling=4,
                                musical_input='Deterministic synthetic plucks/chords, not a recorded guitar DI',
                                digital_equal='Identical WAV; no reference or candidate input adjustment',
                                calibrated='VH4 only: 11.5 dBu common-source convention, not measured hardware calibration of Nastrond',
                                output='No level matching for dynamics; RMS matching only for phase-free spectrum comparisons',
                                acceptance='Observations only; no hardware-match or owner listening PASS'),
                  original=originals,references=references)
    (args.out/'comparison.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n')
    print('COMPLETE',len(references),'NAM and',len(originals),'Original states',flush=True)


if __name__ == '__main__':
    main()
