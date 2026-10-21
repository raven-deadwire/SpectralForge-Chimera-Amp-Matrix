#!/usr/bin/env python3
"""Expose late bass energy hidden by a fast broadband impulse peak.

A causal fourth-order 120Hz low-pass is applied to input and rendered output.
The same PR100 impulse-energy scorer is used on both; report both absolute
energy95 and input-relative energy centroid. This is a band-limited synthetic
event diagnostic, not fixed latency/PDC or a perceptual bass-onset estimator.
"""
import argparse,json,subprocess,gzip
from pathlib import Path
import numpy as np
from scipy import signal
from legacy_metrics import fixture,impulse_metrics,sha

def main():
 p=argparse.ArgumentParser();p.add_argument('--renderer',type=Path,required=True);p.add_argument('--out',type=Path,required=True);a=p.parse_args();a.out.mkdir(parents=True,exist_ok=True);a.renderer=a.renderer.resolve()
 rate=48000;x,meta=fixture('impulses',rate);ip=a.out/'input.f32';x.tofile(ip)
 sos=signal.butter(4,120,fs=rate,output='sos');reference=impulse_metrics(signal.sosfilt(sos,x,axis=0),meta);rows=[]
 for engine in ['baseline','corr40-onset','corr40-split']:
  op=a.out/'output.f32';cp=a.out/'callbacks.csv'
  r=subprocess.run([str(a.renderer),str(ip),str(op),str(rate),'128','-2',engine,str(cp)],check=True,capture_output=True,text=True)
  y=np.fromfile(op,dtype='<f4').reshape(-1,2);m=impulse_metrics(signal.sosfilt(sos,y,axis=0),meta)
  extra=[v['centroid_ms']-u['centroid_ms'] for u,v in zip(reference['events'],m['events'])]
  cb=a.out/(engine+'.csv.gz');cb.write_bytes(gzip.compress(cp.read_bytes(),mtime=0))
  rows.append({'engine':engine,'metrics':m,'additional_centroid_ms':extra,'output_sha256':sha(op),'callbacks_sha256':sha(cb),**json.loads(r.stdout)})
  print(engine,'lowband extra centroid',min(extra),max(extra),flush=True)
 (a.out/'results.json').write_text(json.dumps({'status':'BAND_LIMITED_SYNTHETIC_DIAGNOSTIC','renderer_sha256':sha(a.renderer),'input_sha256':sha(ip),'filter':'causal Butterworth lowpass order4 120Hz','reference':reference,'results':rows},indent=2)+'\n')
if __name__=='__main__':main()
