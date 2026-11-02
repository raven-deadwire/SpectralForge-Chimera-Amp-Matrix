# Fine f0 sweep around the predicted critical f0 where one input period equals the
# transient-path minimum travel (hi-lo)/4. Flags splices whose jump sits on the
# search-range edge [travel, travel+1) samples.
import sys,json,subprocess,numpy as np
sys.path.insert(0,str(__import__('pathlib').Path(__file__).resolve().parents[3]/'Tools'/'TransposeNext'))
import v3sinc_validation as V, residual_validation as R
S,exe,out=sys.argv[1],sys.argv[2],sys.argv[3];eng=sys.argv[4] if len(sys.argv)>4 else 'v3-sinc'
def travel(rate,sh):
    delta=1-2**(sh/12);mx=rate*.040;mn=rate*.002;fade=int(np.ceil(rate*.002));sfade=min(4*fade,max(fade,int(np.ceil(rate*.004))))
    guard=abs(delta)*max(fade,sfade)+2;return (mx-guard-(mn+guard))*.25
rows=[];rng=np.random.default_rng(11)
for rate in (44100,48000,96000):
  for sh in (-2,-1,1,2):
    tr=travel(rate,sh);fc=rate/tr
    for f0 in np.round(np.arange(fc-1.0,fc+1.0001,0.04),3):
      ph=rng.uniform(0,2*np.pi,int(8000//f0)).tolist();tilt=.8
      z=V.tone(f0,rate,tilt,ph,secs=2);np.column_stack((z,-.65*z)).astype('<f4').tofile(S+'/fi.f32')
      lo,hi=round(.5*rate),len(z)-round(.2*rate);T=rate/f0
      r=subprocess.run([exe,S+'/fi.f32',S+'/fio.f32',str(rate),str(sh),eng],capture_output=True,text=True,check=True)
      tj=[abs(float(l.split()[3])-float(l.split()[2])) for l in r.stdout.split('\n') if l and l[0]=='T'][1:]
      err=[j-round(j/T)*T for j in tj];edge=[tr<=j<tr+1 for j in tj]
      y=np.fromfile(S+'/fio.f32','<f4').reshape(-1,2)[lo:hi,0].astype(float)
      fi=f0*2**(sh/12);h,res=V.interharm(y,fi,rate);hi_,ri=V.interharm(V.tone(fi,rate,tilt,ph,secs=2)[lo:hi],fi,rate)
      rows.append({'rate':rate,'shift':sh,'f0':float(f0),'fc':fc,'travel':tr,'period':T,'t_splices':len(tj),
                   'periods':sorted(set(int(round(j/T)) for j in tj)),'err_max':float(max(map(abs,err))) if err else None,
                   'edge_share':float(np.mean(edge)) if edge else None,'net_db':float(R.net_db(10*np.log10(res/h),10*np.log10(ri/hi_)))})
    s=[r for r in rows if r['rate']==rate and r['shift']==sh]
    bad=[r for r in s if r['net_db']>-50]
    print(rate,sh,'fc %.3f'%fc,'bad f0:',[(r['f0'],round(r['net_db'],1),r['periods'],round(r['edge_share'] or 0,2),round(r['err_max'] or 0,2)) for r in bad],flush=True)
json.dump(rows,open(out,'w'),indent=1)
