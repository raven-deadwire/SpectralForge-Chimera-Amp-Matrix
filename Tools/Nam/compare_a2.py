#!/usr/bin/env python3
"""Compare frozen old/new captures on inputs excluded from training and selection."""
import argparse,concurrent.futures,json,subprocess,tempfile
from pathlib import Path
import numpy as np
from scipy.signal import butter,sosfilt
from common import digest,write_json
from capture_channels import VOICES
from train_a2 import scores
from quality_profile import PROFILE, fidelity

def metrics(pred,ref,channel):
    result=scores(pred,ref)[0];windows=[]
    for s in range(6347,len(ref)-4096,4096):
        if np.mean(ref[s:s+4096]**2)>1e-6:windows.append(scores(pred[s:s+4096],ref[s:s+4096],0)[0]['esr'])
    result.update({'window_median':float(np.median(windows)),'window_p95':float(np.quantile(windows,.95)),'window_worst':float(max(windows))})
    bands={}
    for lo,hi in [(40,250),(250,1000),(1000,4000),(4000,12000)]:
        filt=butter(3,[lo,hi],btype='bandpass',fs=48000,output='sos')
        pp=sosfilt(filt,pred)[6347:];rr=sosfilt(filt,ref)[6347:]
        bands[f'{lo}-{hi}Hz']=float(10*np.log10(max(np.mean(pp**2),1e-20)/max(np.mean(rr**2),1e-20)))
    result['band_rms_error_db']=bands
    result['tone3000_fidelity']=fidelity(pred,ref,channel)
    return result

def main():
    p=argparse.ArgumentParser();p.add_argument('--baseline',type=Path,required=True);p.add_argument('--candidate',type=Path,required=True)
    p.add_argument('--data',type=Path,required=True);p.add_argument('--external',type=Path,required=True)
    p.add_argument('--tool',type=Path,required=True);p.add_argument('--out',type=Path,required=True)
    p.add_argument('--workers',type=int,default=3);a=p.parse_args()
    report={'baseline':str(a.baseline),'candidate':str(a.candidate),'source_commit':'d93560445c78552768a0ed592e5ec62c3c648fb6',
      'protocol':'Frozen exported checkpoints; no time alignment, fitted gain, normalization or EQ. Native output units for both versions.',
      'external_provenance':json.loads((a.external/'manifest.json').read_text()),'inputs':{},
      'listening':'not performed in a DAW; numerical comparisons only','quality_profile':PROFILE}
    with tempfile.TemporaryDirectory() as td:
        temp=Path(td);raw=temp/'input.f32';job=temp/'job.json';write_json(job,{'normalize':False,'block_size':64})
        for label,folder in [('unseen_synthetic',a.data/'test'),('official_NAM_v3',a.external)]:
            x=np.load(folder/'input.npy');x.astype('<f4').tofile(raw);entry={'input_sha256':digest(folder/'input.npy'),'seconds':len(x)/48000,'channels':{}}
            def evaluate(voice):
                name=voice[0];out=temp/(name+'-output.f32')
                ref=np.load(folder/(name+'.npy'));versions={}
                for version,models in [('baseline',a.baseline),('candidate',a.candidate)]:
                    nam=models/('Nastrond-'+name+'.nam')
                    runtime=json.loads(subprocess.check_output([str(a.tool),'render',str(nam),str(raw),str(out),str(job)],text=True))
                    y=np.fromfile(out,dtype='<f4');result=metrics(y,ref,name);result['model_sha256']=digest(nam);result['engine']=runtime
                    if label=='official_NAM_v3':
                        result['sections']={}
                        for section,start,end in [('opening_example',.2,9),('chirps',12.15,15),('noise',15.15,17),('program_material',17.15,180.5),('closing_example',181.15,190)]:
                            l,h=round(start*48000),round(end*48000)
                            result['sections'][section]=scores(y[l:h],ref[l:h],0)[0]
                    versions[version]=result
                versions['esr_reduction_percent']=100*(1-versions['candidate']['esr']/versions['baseline']['esr'])
                versions['improved']=versions['candidate']['esr']<versions['baseline']['esr']
                print(json.dumps({'input':label,'channel':name,'baseline_esr':versions['baseline']['esr'],'candidate_esr':versions['candidate']['esr'],'reduction_percent':versions['esr_reduction_percent']}),flush=True)
                return name,versions
            with concurrent.futures.ThreadPoolExecutor(max_workers=a.workers) as pool:
                for name,versions in pool.map(evaluate,VOICES):entry['channels'][name]=versions
            report['inputs'][label]=entry
    report['all_channels_improved_on_both_inputs']=all(v['improved'] for e in report['inputs'].values() for v in e['channels'].values())
    write_json(a.out,report)

if __name__=='__main__':main()
