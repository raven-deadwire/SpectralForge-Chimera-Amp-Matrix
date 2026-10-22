#!/usr/bin/env python3
"""Pinned real-C++ A/B regression. No install, packaging or release actions."""
import argparse
import gzip
import hashlib
import json
import math
import platform
import subprocess
import tempfile
from pathlib import Path
import numpy as np
from measure_multiband_bypass_candidate import ROOT, BASE, RATES, SPLITS, TONES, db


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--output', type=Path, required=True)
    ap.add_argument('--skip-cpu', action='store_true')
    args = ap.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    sources = ['Source/MultibandDynamicsCore.h', 'Tests/MultibandDynamicsCoreTests.cpp',
               'Tools/multiband_response_probe.cpp', 'Tools/experiments/multiband_bypass_bench.cpp',
               'Tools/measure_multiband_bypass_fix.py']
    result = dict(baseline_sha=BASE,
                  source_sha256={p: hashlib.sha256((ROOT/p).read_bytes()).hexdigest() for p in sources},
                  platform=platform.platform(), numpy=np.__version__,
                  compiler=subprocess.check_output(['g++', '--version'], text=True).splitlines()[0],
                  cpu_model=next((s.split(':',1)[1].strip() for s in Path('/proc/cpuinfo').read_text().splitlines() if s.startswith('model name')), platform.processor()),
                  actual_di_listening_tested=False, installed_daw_tested=False, release_approved=False)
    static, transitions, phases, cpu, raw_cpu = [], [], [], [], []
    curves = {}
    with tempfile.TemporaryDirectory(prefix='chimera-mb-fix-') as tmp:
        tmp = Path(tmp)
        old = tmp/'old'; old.mkdir()
        header = subprocess.check_output(['git', 'show', f'{BASE}:Source/MultibandDynamicsCore.h'], cwd=ROOT)
        (old/'MultibandDynamicsCore.h').write_bytes(header)
        # Enforce that LR4 split/compensation and de-esser source remain intact.
        before, after = header.decode(), (ROOT/sources[0]).read_text()
        for start, end in [('struct SVFCoefficients', 'struct Detector'), ('struct DeEsserSettings', None)]:
            segment = lambda s: s[s.index(start):s.index(end) if end else len(s)]
            assert segment(before) == segment(after), f'Unintended change: {start}'
        exes, benches = {}, {}
        for variant, include in [('baseline',old), ('fixed',ROOT/'Source')]:
            for target, source, collection in [('probe','Tools/multiband_response_probe.cpp',exes),
                                                ('bench','Tools/experiments/multiband_bypass_bench.cpp',benches)]:
                exe = tmp/f'{variant}-{target}'
                subprocess.run(['g++','-O2','-std=c++20','-I'+str(include),str(ROOT/source),'-o',str(exe)], check=True)
                collection[variant] = exe
        def run(variant, *a):
            return np.frombuffer(subprocess.check_output([str(exes[variant]), *map(str,a)]), dtype=np.float32).astype(float)
        configurations = [(1,[0]*4)]+[(m,g) for m in [0,.25,.5,.75,1] for g in [[-6]*4,[-6,3,-9,6]]]
        configurations += [(1,[-24,24,-24,24]), (1,[24,-24,24,-24]), (1,[-24]*4)]
        for sr in RATES:
            for cross in SPLITS:
                for mix, gains in configurations:
                    samples = {v:run(v,'impulse',sr,*cross,mix,*gains) for v in exes}
                    h = {v:np.fft.rfft(x) for v,x in samples.items()}
                    error = float(np.max(np.abs(20*np.log10(np.abs(h['fixed']/h['baseline'])))))
                    # Independent minimum-phase oracle from real cepstrum of
                    # the baseline magnitude; does not reuse factorisation code.
                    cep = np.fft.irfft(np.log(np.maximum(np.abs(h['baseline']),1e-20)))
                    causal = cep.copy(); causal[1:len(cep)//2] *= 2; causal[len(cep)//2+1:] = 0
                    oracle = np.exp(np.fft.rfft(causal))
                    phase_error = float(np.max(np.abs(np.angle(h['fixed']/oracle)))*180/np.pi)
                    static.append(dict(rate=sr,crossovers=cross,mix=mix,gains_db=gains,
                                       max_magnitude_delta_db=error,max_minimum_phase_oracle_error_deg=phase_error))
                    assert error < .001, static[-1]
                    assert phase_error < .01, static[-1]
                    if mix==1 and gains==[0]*4 and cross==SPLITS[0]:
                        for v,x in samples.items():
                            n=np.arange(len(x))
                            def at(f): return x@np.exp(-2j*np.pi*f*n/sr)
                            for hz in TONES:
                                z=at(hz)
                                phases.append(dict(variant=v,rate=sr,hz=hz,magnitude_db=db(abs(z)),phase_deg=float(np.angle(z)*180/np.pi),group_delay_ms=float(-np.angle(at(hz+.01)/at(hz-.01))/(2*np.pi*.02)*1000)))
                for gains in [[0]*4,[-6,3,-9,6],[-24,24,-24,24]]:
                    for hz in TONES:
                        for mode in ['on','off']:
                            for variant in exes:
                                x=run(variant,mode,sr,hz,.15,*cross,*gains).reshape(2,-1)
                                z=(x[1]+1j*x[0])/.25; pre=int(sr*.02); ramp=int(math.ceil(sr*.01))
                                env=20*np.log10(np.maximum(np.abs(z),1e-30))
                                endpoint=float(np.mean(env[-int(sr*.02):] if mode=='on' else env[:int(sr*.01)]))
                                low,high=min(0,endpoint),max(0,endpoint)
                                step=np.abs(z[1:]-np.exp(2j*np.pi*hz/sr)*z[:-1])*.25
                                row=dict(variant=variant,rate=sr,crossovers=cross,gains_db=gains,hz=hz,mode=mode,
                                         min_envelope_db=float(min(env[pre:])),max_envelope_db=float(max(env[pre:])),
                                         endpoint_gain_db=endpoint,undershoot_db=float(min(env)-low),overshoot_db=float(max(env)-high),
                                         carrier_removed_step_peak_dbfs=db(max(step[pre-1:])),
                                         boundary_step_peak_dbfs=db(max(step[pre-1],step[pre+ramp-1])),
                                         phase_motion_deg=float(np.ptp(np.unwrap(np.angle(z*np.exp(-2j*np.pi*hz*np.arange(len(z))/sr))))*180/np.pi))
                                transitions.append(row)
                                if variant=='fixed':
                                    assert row['undershoot_db']>-.05 and row['overshoot_db']<.05, row
                                    if gains==[0]*4: assert max(abs(row['min_envelope_db']),abs(row['max_envelope_db']))<1e-5
                                if sr==48000 and cross==SPLITS[0] and hz in [30.8677,700]:
                                    curves[(hz,mode,tuple(gains),variant)] = (np.arange(len(z))/sr*1000-20,env)
            print(f'{sr}: static magnitude/minimum-phase and transition regressions PASS',flush=True)
        old700=[r for r in transitions if r['variant']=='baseline' and r['rate']==48000 and r['crossovers']==SPLITS[0] and r['hz']==700 and r['gains_db']==[0]*4]
        assert next(r for r in old700 if r['mode']=='off')['min_envelope_db']<-28
        assert next(r for r in old700 if r['mode']=='on')['min_envelope_db']<-59
        if not args.skip_cpu:
            for sr in RATES:
                for channels in [1,2]:
                    for block in [32,128,512]:
                        for mode in ['off','on','toggle']:
                            samples={v:[] for v in exes}; count=max(1000,math.ceil(sr/block))
                            for rep in range(5):
                                for variant in (['baseline','fixed'] if rep%2==0 else ['fixed','baseline']):
                                    data=json.loads(subprocess.check_output([str(benches[variant]),'baseline',str(sr),str(channels),str(block),str(count),mode]))
                                    raw_cpu.append(dict(variant=variant,rate=sr,channels=channels,block=block,mode=mode,repeat=rep,**data))
                                    samples[variant].append(data)
                            row=dict(rate=sr,channels=channels,block=block,mode=mode,repeats=5,callbacks_per_repeat=count)
                            for variant,runs in samples.items():
                                c=np.concatenate([r['cpu_us'] for r in runs]);w=np.concatenate([r['wall_us'] for r in runs])
                                row[variant]=dict(cpu_p50_us=float(np.median(c)),cpu_p99_us=float(np.percentile(c,99)),wall_p99_us=float(np.percentile(w,99)),wall_max_us=float(max(w)),cpu_deadline_misses=int(sum(c>block/sr*1e6)),wall_deadline_misses=int(sum(w>block/sr*1e6)),repeat_p50_us=[float(np.median(r['cpu_us'])) for r in runs])
                            row['cpu_delta_percent']=(row['fixed']['cpu_p50_us']/row['baseline']['cpu_p50_us']-1)*100
                            cpu.append(row)
                print(f'{sr}: paired CPU PASS (measurements, not a DAW real-time qualification)',flush=True)
        result.update(static=static,phase=phases,transitions=transitions,cpu=cpu,regression='PASS')
    (args.output/'measurements.json').write_text(json.dumps(result,indent=2)+'\n')
    if raw_cpu:
        with gzip.open(args.output/'cpu-callbacks.json.gz','wt') as f: json.dump(raw_cpu,f,separators=(',',':'))
    import matplotlib; matplotlib.use('Agg')
    import matplotlib.pyplot as plt
    fig,axes=plt.subplots(2,2,figsize=(11,7),layout='constrained')
    for ax,(hz,mode,gains) in zip(axes.flat,[(700,'off',[0]*4),(700,'on',[0]*4),(30.8677,'on',[-24,24,-24,24]),(30.8677,'off',[-24,24,-24,24])]):
        for variant in ['baseline','fixed']:
            t,y=curves[(hz,mode,tuple(gains),variant)]; ax.plot(t,y,label=variant)
        ax.set(xlim=(-2,25),xlabel='After toggle (ms)',ylabel='Envelope (dB)',title=f'{hz:g} Hz {mode.upper()}, gains {gains}')
        ax.legend(); ax.grid(alpha=.2)
    fig.savefig(args.output/'transitions.png',dpi=140);plt.close(fig)
    print(f'PASS {len(static)} impulse pairs, {len(transitions)} transition records, {len(cpu)} CPU cells',flush=True)


if __name__=='__main__': main()
