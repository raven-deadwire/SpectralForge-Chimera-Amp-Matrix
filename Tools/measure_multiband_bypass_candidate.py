#!/usr/bin/env python3
"""Independent, non-publishing A/B experiment. Reuses multiband_response_probe.
CPU values are local offline core costs, not whole-plugin/DAW qualification.
"""
import argparse, hashlib, json, math, os, platform, subprocess, tempfile
from pathlib import Path
import numpy as np
ROOT = Path(__file__).resolve().parents[1]
RATES = [44100, 48000, 96000, 192000]
SPLITS = [[120, 700, 3500], [40, 56, 78.4], [1000, 1400, 1960], [7000, 10000, 16000]]
TONES = [27.5, 30.8677, 40, 55, 80, 120, 700, 3500, 10000]
BASE = '3a942a125d652a234d0f481fca3af2bdf6897635'
def git(*args):
    return subprocess.check_output(['git', *args], cwd=ROOT, text=True).strip()
def db(x): return float(20*np.log10(max(float(x), 1e-30)))
def main():
    ap=argparse.ArgumentParser();ap.add_argument('--output',type=Path,required=True)
    ap.add_argument('--cxx',default='g++');ap.add_argument('--skip-cpu',action='store_true');args=ap.parse_args()
    args.output.mkdir(parents=True,exist_ok=True)
    source_paths=['Source/MultibandDynamicsCore.h','Tools/experiments/MultibandBypassCandidate.h',
                  'Tools/multiband_response_probe.cpp','Tools/experiments/multiband_bypass_bench.cpp',
                  'Tests/MultibandBypassCandidateTests.cpp','Tools/measure_multiband_bypass_candidate.py']
    hashes={p:hashlib.sha256((ROOT/p).read_bytes()).hexdigest() for p in source_paths}
    preserved=['Source','Assets','Installer','CMakeLists.txt','cmake','VERSION','RELEASE_CHANNEL',
               'Validation/beta-lifecycle-policy.json']
    changed=git('diff','--name-only',BASE,'--',*preserved)
    assert not changed, f'Product baseline changed: {changed}'
    result=dict(baseline_sha=BASE,experiment_git_sha=git('rev-parse','HEAD'),source_sha256=hashes,
                platform=platform.platform(),compiler=subprocess.check_output([args.cxx,'--version'],text=True).splitlines()[0],
                numpy_version=np.__version__,cpu_model=next((s.split(':',1)[1].strip() for s in Path('/proc/cpuinfo').read_text().splitlines() if s.startswith('model name')),platform.processor()),
                product_paths_unchanged=True,host_parameters_changed=0,explicit_delay_samples=0,
                actual_di_listening_tested=False,installed_daw_tested=False,release_approved=False,
                provisional_screen=dict(min_envelope_db=-1,max_envelope_db=1,max_transition_ms=20,max_active_cpu_increase_percent=10),
                notes=['Screen limits are research criteria, not user-approved product specifications.',
                       'Quadrature envelope/phase apply to neutral linear time-varying tones only.',
                       'Click proxy is carrier-removed complex first difference, not a listening verdict.',
                       'CPU includes setSettings/process and clock overhead; input copy is outside timed region.'])
    with tempfile.TemporaryDirectory(prefix='chimera-mb-bypass-') as tmp:
        tmp=Path(tmp);exes={}
        for name,defines in [('baseline',[]),('candidate',['-DCHIMERA_MB_BYPASS_CANDIDATE'])]:
            exe=tmp/name
            subprocess.run([args.cxx,'-O2','-std=c++20',*defines,'-I'+str(ROOT/'Source'),str(ROOT/'Tools/multiband_response_probe.cpp'),'-o',str(exe)],check=True)
            exes[name]=exe
        def run(name,*a):return np.frombuffer(subprocess.check_output([str(exes[name]),*map(str,a)]),dtype=np.float32)
        # All 176 original impulse configurations: exact array equality implies
        # identical magnitude/phase/group delay/internal parallel mix in this set.
        static=[];phase_points=[]
        for sr in RATES:
            for cross in SPLITS:
                for mix,gains in [(1,[0]*4)]+[(m,g) for m in [0,.25,.5,.75,1] for g in [[-6]*4,[-6,3,-9,6]]]:
                    a=run('baseline','impulse',sr,*cross,mix,*gains);b=run('candidate','impulse',sr,*cross,mix,*gains)
                    assert np.array_equal(a,b),'Settled impulse response changed'
                    static.append(dict(rate=sr,crossovers=cross,mix=mix,gains_db=gains,bit_exact=True))
                    if mix==1 and gains==[0]*4 and cross==SPLITS[0]:
                        n=np.arange(len(a));h=a.astype(float)
                        def at(f):return h@np.exp(-2j*np.pi*f*n/sr)
                        for hz in TONES:
                            v=at(hz);phase_points.append(dict(rate=sr,hz=hz,magnitude_db=db(abs(v)),phase_deg=float(np.angle(v)*180/np.pi),group_delay_ms=float(-np.angle(at(hz+.01)/at(hz-.01))/(2*np.pi*.02)*1000),candidate_complex_difference=0.))
        result['static_impulses']=static;result['settled_phase']=phase_points
        print('PASS 176 bit-exact impulse comparisons',flush=True)
        rows=[];curves={}
        for sr in RATES:
            for cross in SPLITS:
                for hz in TONES:
                    for mode in ['on','off']:
                        for name in exes:
                            # 20 ms pre + 300 ms post; not the old 10 ms-only window.
                            samples=run(name,mode,sr,hz,.3,*cross).reshape(2,-1).astype(float)
                            z=(samples[1]+1j*samples[0])/.25
                            pre=int(sr*.02);tail=z[pre:];env=np.abs(tail)
                            # Expected steady reference: raw OFF, final settled ON.
                            w=2*np.pi*hz/sr;idx=np.arange(len(z));demod=z*np.exp(-1j*w*idx)
                            final=complex(np.mean(demod[-int(sr*.03):]))
                            error=np.abs(demod[pre:]-final)
                            bad=np.flatnonzero(error>1e-4)
                            settling_ms=float((bad[-1]+1)/sr*1000) if len(bad) else 0.
                            # Entire transition + settling interval, including both edges.
                            proxy=np.abs(z[1:]-np.exp(1j*w)*z[:-1])*.25
                            instant=np.diff(np.unwrap(np.angle(z)))*sr/(2*np.pi)-hz
                            row=dict(variant=name,rate=sr,crossovers=cross,hz=hz,mode=mode,
                                     min_envelope_db=db(env.min()),max_envelope_db=db(env.max()),
                                     time_below_minus3_ms=float(np.count_nonzero(env<10**(-3/20))/sr*1000),
                                     carrier_removed_step_peak_dbfs=db(proxy[pre-1:].max()),
                                     max_abs_instant_frequency_deviation_hz=float(np.max(np.abs(instant[pre-1:]))),
                                     settle_to_complex_reference_1e4_ms=settling_ms,
                                     final_20ms_max_envelope_error_db=float(np.max(np.abs(20*np.log10(np.maximum(env[-int(sr*.02):],1e-30))))))
                            rows.append(row)
                            if sr==48000 and cross in SPLITS[:2] and hz in [27.5,80,120,700]:
                                curves[(tuple(cross),hz,mode,name)]=(np.arange(len(z))/sr*1000-20,20*np.log10(np.maximum(np.abs(z),1e-30)))
            print(f'{sr}: transition sweep complete',flush=True)
        result['transitions']=rows
        old700=[r for r in rows if r['variant']=='baseline' and r['rate']==48000 and r['crossovers']==SPLITS[0] and r['hz']==700]
        assert next(r for r in old700 if r['mode']=='off')['min_envelope_db'] < -28
        assert next(r for r in old700 if r['mode']=='on')['min_envelope_db'] < -59
        new700=[r for r in rows if r['variant']=='candidate' and r['crossovers']==SPLITS[0] and r['hz']==700]
        assert all(r['min_envelope_db']>-1 and r['max_envelope_db']<1 for r in new700),'700 Hz regression returned'
        result['700hz_regression']='PASS; baseline failure reproduced, candidate bounded within +/-1 dB at all four rates'
        failures=[r for r in rows if r['variant']=='candidate' and (r['min_envelope_db'] < -1 or r['max_envelope_db']>1)]
        result['envelope_screen_failures']=len(failures)
        result['worst_candidate_min']=min((r for r in rows if r['variant']=='candidate'),key=lambda r:r['min_envelope_db'])
        result['worst_candidate_max']=max((r for r in rows if r['variant']=='candidate'),key=lambda r:r['max_envelope_db'])
        result['cpu']=[];raw_cpu=[]
        if not args.skip_cpu:
            bench=tmp/'bench';subprocess.run([args.cxx,'-O2','-std=c++20','-I'+str(ROOT/'Source'),str(ROOT/'Tools/experiments/multiband_bypass_bench.cpp'),'-o',str(bench)],check=True)
            for sr in RATES:
                for channels in [1,2]:
                    for block in [32,128,512]:
                        for mode in ['off','on','toggle']:
                            samples={n:[] for n in exes}
                            # Alternating AB/BA order; five independent repetitions.
                            # 1 second minimum so toggle has multiple complete cycles.
                            count=max(1000,math.ceil(sr/block))
                            for rep in range(5):
                                for name in (['baseline','candidate'] if rep%2==0 else ['candidate','baseline']):
                                    r=json.loads(subprocess.check_output([str(bench),name,str(sr),str(channels),str(block),str(count),mode],text=True))
                                    raw_cpu.append(dict(variant=name,rate=sr,channels=channels,block=block,mode=mode,repetition=rep,**r))
                                    samples[name].append(r)
                            entry=dict(rate=sr,channels=channels,block=block,mode=mode,repetitions=5,callbacks_per_repeat=count)
                            for name,repeats in samples.items():
                                wall=np.concatenate([r['wall_us'] for r in repeats]);cpu=np.concatenate([r['cpu_us'] for r in repeats])
                                entry[name]=dict(thread_cpu_p50_us=float(np.median(cpu)),thread_cpu_p99_us=float(np.percentile(cpu,99)),wall_p50_us=float(np.median(wall)),wall_p99_us=float(np.percentile(wall,99)),wall_max_us=float(max(wall)),deadline_misses=int(np.count_nonzero(wall>block/sr*1e6)),cpu_deadline_misses=int(np.count_nonzero(cpu>block/sr*1e6)),repeat_cpu_p50_us=[float(np.median(r['cpu_us'])) for r in repeats])
                            entry['cpu_p50_delta_percent']=(entry['candidate']['thread_cpu_p50_us']/entry['baseline']['thread_cpu_p50_us']-1)*100
                            result['cpu'].append(entry)
                print(f'{sr}: paired CPU sweep complete',flush=True)
            import gzip
            with gzip.open(args.output/'cpu-callbacks.json.gz','wt') as f:json.dump(raw_cpu,f,separators=(',',':'))
        # Acoustic screen failure is preserved; no inference from a single tone.
        result['integration_verdict']='REJECT_CANDIDATE' if failures else 'HOLD_FOR_MODULATION_CPU_AND_DAW_REVIEW'
        (args.output/'measurements.json').write_text(json.dumps(result,indent=2)+'\n')
        import matplotlib;matplotlib.use('Agg')
        import matplotlib.pyplot as plt
        fig,axes=plt.subplots(2,2,figsize=(12,8),layout='constrained')
        for ax,(cross,hz,mode) in zip(axes.flat,[(SPLITS[0],700,'off'),(SPLITS[0],700,'on'),(SPLITS[1],120,'on'),(SPLITS[1],80,'off')]):
            for name in exes:
                t,y=curves[(tuple(cross),hz,mode,name)];ax.plot(t,y,label=name)
            ax.axhline(-1,color='grey',ls=':',lw=.7);ax.axhline(1,color='grey',ls=':',lw=.7)
            ax.set(xlim=(-2,165),ylim=(-65,8),xlabel='Time after toggle (ms)',ylabel='Quadrature envelope (dB)',title=f'{hz} Hz {mode.upper()}, crossover {cross}')
            ax.legend();ax.grid(alpha=.2)
        fig.savefig(args.output/'transitions.png',dpi=140);plt.close(fig)
        print(json.dumps({k:result[k] for k in ['700hz_regression','envelope_screen_failures','worst_candidate_min','worst_candidate_max','integration_verdict']},indent=2))
if __name__=='__main__':main()
