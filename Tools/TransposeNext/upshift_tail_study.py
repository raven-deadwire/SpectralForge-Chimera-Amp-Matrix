#!/usr/bin/env python3
"""PR202 tail experiments. Synthetic only; never changes acceptance limits.

Run matrix once per named candidate, then blocks/cpu. All outputs retain input
and renderer hashes. Candidate seeds/configs are preregistered in evidence.
"""
import argparse, gzip, json, platform, subprocess, sys
from pathlib import Path
import numpy as np
from latency_targets import inputs, evaluate, SPEC
from evaluate import attacks
from legacy_metrics import tone_metrics, sha
from weak_attack_study import event_detection, generate
from weak_attack_holdout import lowband_extra
from attack_snr import event_snrs, hf_envelope, event_snr_db


def save(path, data):
    text = (json.dumps(data, indent=1) + '\n').encode()
    path.write_bytes(gzip.compress(text, mtime=0) if path.suffix == '.gz' else text)


def tail_rows(x, y, onsets, rate):
    ei, eo = hf_envelope(x, rate), hf_envelope(y, rate)
    out = []
    for k, o in enumerate(onsets):
        snr = event_snr_db(ei, o, rate)
        for end_ms in (60, 120):
            end = min(o+round(end_ms*.001*rate), onsets[k+1]-round(.005*rate) if k+1<len(onsets) else len(eo))
            if end <= o+round(.020*rate):
                continue
            start = o+round(.015*rate)
            def measure(e):
                main = e[o:start].max(); late = e[start:end].max()
                return float(20*np.log10(max(late, 1e-30)/max(main, 1e-12)))
            out.append({'event': k, 'snr_db': snr, 'end_ms': end_ms,
                        'echo_in_db': measure(ei), 'echo_out_db': measure(eo)})
    return out


def render(a, x, rate, shift, engine=None, block=128, cpu=False):
    ip, op, cp = a.work/'input.f32', a.work/'output.f32', a.work/'callbacks.csv'
    x.tofile(ip)
    cmd = [str(a.renderer), str(ip), str(op), str(rate), str(block), str(shift), engine or a.engine, str(cp)]
    if cpu: cmd.append('cpu')
    r = subprocess.run(cmd, check=True, capture_output=True, text=True)
    return json.loads(r.stdout), np.fromfile(op, '<f4').reshape(-1, 2)


def matrix(a):
    rows = []; spec = json.loads(SPEC.read_text()); parity = []
    for rate in a.rates:
        for key, x, meta, roles in inputs(rate, tuple(a.seeds), a.long_seed):
            before = attacks(x, meta) if 'attack' in roles else None
            snrs = event_snrs(x, meta['onsets'], rate) if before is not None else None
            for shift in spec['conditions']['shifts']:
                native, y = render(a, x, rate, shift)
                row = {'input': key, 'rate': rate, 'shift': shift, 'roles': sorted(roles),
                       'input_sha256': sha(a.work/'input.f32'), 'output_sha256': sha(a.work/'output.f32'),
                       'onset_resets': native['onset_resets'], 'low_onset_resets': native['low_onset_resets'],
                       'onset_samples': native['onset_samples'],
                       'read_min_samples': native['read_min_samples'], 'read_max_samples': native['read_max_samples'],
                       'onset_association': event_detection(native['onset_samples'], {**meta, 'rate': rate})}
                if before is not None:
                    extra = np.array(attacks(y, meta)['attack_lags_ms'])-np.array(before['attack_lags_ms'])
                    row.update(attack_metric_valid=bool(before['attack_lag_max_ms']<=10) and meta['name']!='riff50',
                               attack_additional_max_ms=float(max(extra)), attack_added_ms=extra.tolist(), attack_event_snr_db=snrs)
                if roles & {'short', 'long'}:
                    row['lowband_additional_ms'] = lowband_extra(x, y, meta['onsets'], rate)
                if 'steady' in roles:
                    row['tone'] = tone_metrics(y, rate, meta['fundamental_hz']*2**(shift/12), .12)
                if 'markers' in roles:
                    row['tails'] = tail_rows(x, y, meta['onsets'], rate)
                if a.baseline:
                    old = a.renderer; a.renderer = a.baseline
                    nb, yb = render(a, x, rate, shift)
                    a.renderer = old
                    same = bool(np.array_equal(y, yb)) and native['onset_samples']==nb['onset_samples'] and native['low_onset_resets']==nb['low_onset_resets']
                    parity.append({'input': key, 'rate': rate, 'shift': shift, 'bit_equal': same})
                    if not same: raise AssertionError('baseline changed: '+str(parity[-1]))
                rows.append(row)
            print(rate, key, flush=True)
    report = evaluate(spec, rows)
    report.update(engine=a.engine, renderer_sha256=sha(a.renderer), spec_sha256=sha(SPEC),
                  seeds=a.seeds, long_seed=a.long_seed, platform=platform.platform(), synthetic_only=True,
                  real_DI='NOT_RUN', listening='NOT_RUN', actual_DAW='NOT_RUN',
                  per_rate={str(rate): evaluate(spec, [r for r in rows if r['rate']==rate]) for rate in a.rates})
    save(a.out, {'report': report, 'rows': rows, 'baseline_parity': parity})
    print(json.dumps(report['targets'], indent=1), flush=True)


def blocks(a):
    rows = []
    for rate in a.rates:
        for inst, name in [('bass_B0','riff75'), ('bass_dropA_A0','quiet_markers_over_sustain'), ('guitar_dropA_A1','quieter_markers')]:
            x, _ = generate(name, rate, inst); x.tofile(a.work/'input.f32')
            for shift in (-2,-1,1,2):
                r = subprocess.run([str(a.renderer),str(a.work/'input.f32'),str(rate),str(shift),a.engine],check=True,capture_output=True,text=True)
                differences = json.loads(r.stdout)
                rows.append({'rate':rate,'input':inst+'/'+name,'shift':shift,'differences':differences,'input_sha256':sha(a.work/'input.f32')})
                print(rate,inst,name,shift,max(differences.values()),flush=True)
    save(a.out, {'engine':a.engine,'renderer_sha256':sha(a.renderer),'rows':rows})
    if any(v != 0 for r in rows for v in r['differences'].values()): sys.exit(1)


def cpu(a):
    rows=[]; engines=a.engine.split(',')
    for rate in a.rates:
        x, _=generate('riff75',rate,'bass_B0');x=np.tile(x,(4,1))
        for shift in (-2,2):
            for block in (64,128):
                for repeat in range(5):
                    order=engines if repeat%2==0 else engines[::-1]
                    for engine in order:
                        native,_=render(a,x,rate,shift,engine,block,True)
                        cb=np.genfromtxt(a.work/'callbacks.csv',delimiter=',',names=True)
                        cpu_us=cb['thread_cpu_us']; wall=cb['microseconds']; budget=cb['samples']/rate*1e6
                        rows.append({'rate':rate,'shift':shift,'block':block,'repeat':repeat,'engine':engine,
                                     'input_sha256':sha(a.work/'input.f32'),'native':native,
                                     'cpu':{'mean':float(cpu_us.mean()),'p99':float(np.quantile(cpu_us,.99)), 'max':float(cpu_us.max()), 'deadline_misses':int(np.sum(cpu_us>budget))},
                                     'wall':{'mean':float(wall.mean()),'p99':float(np.quantile(wall,.99)), 'max':float(wall.max()),'deadline_misses':int(np.sum(wall>budget))}})
                print(rate,shift,block,'done',flush=True)
    save(a.out,{'renderer_sha256':sha(a.renderer),'platform':platform.platform(),'T5':'NOT_ACCEPTED_SHARED_VM','rows':rows})


def main():
    p=argparse.ArgumentParser();p.add_argument('mode',choices=['matrix','blocks','cpu']);p.add_argument('--renderer',type=Path,required=True)
    p.add_argument('--engine',required=True);p.add_argument('--out',type=Path,required=True);p.add_argument('--work',type=Path,required=True)
    p.add_argument('--rates',default='44100,48000,96000');p.add_argument('--seeds',default='8139,20261024');p.add_argument('--long-seed',type=int,default=1)
    p.add_argument('--baseline',type=Path)
    a=p.parse_args();a.rates=list(map(int,a.rates.split(',')));a.seeds=list(map(int,a.seeds.split(',')));a.renderer=a.renderer.resolve()
    if a.baseline:a.baseline=a.baseline.resolve()
    a.work.mkdir(parents=True,exist_ok=True);a.out.parent.mkdir(parents=True,exist_ok=True)
    {'matrix':matrix,'blocks':blocks,'cpu':cpu}[a.mode](a)

if __name__=='__main__':main()
