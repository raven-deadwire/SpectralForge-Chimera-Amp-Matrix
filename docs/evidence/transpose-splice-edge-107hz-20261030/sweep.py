# f0 sweep: transient-path jump error vs period, and v3-sinc net residual, per rate/shift.
import sys,json,subprocess,numpy as np
sys.path.insert(0,'/home/user/SpectralForge-Chimera-Amp-Matrix/Tools/TransposeNext')
import v3sinc_validation as V, residual_validation as R
S=sys.argv[1];exe=sys.argv[2];out=sys.argv[3];eng=sys.argv[4] if len(sys.argv)>4 else 'v3-sinc'
rng=np.random.default_rng(7)
rows=[]
for rate in (44100,48000,96000):
  for f0 in np.round(np.arange(100,115.01,0.25),3):
    ph=rng.uniform(0,2*np.pi,int(8000//f0)).tolist();tilt=.8
    z=V.tone(f0,rate,tilt,ph,secs=2);np.column_stack((z,-.65*z)).astype('<f4').tofile(S+'/sw.f32')
    lo,hi=round(.5*rate),len(z)-round(.2*rate);T=rate/f0
    for sh in (-2,-1,1,2):
      r=subprocess.run([exe,S+'/sw.f32',S+'/swo.f32',str(rate),str(sh),eng],capture_output=True,text=True,check=True)
      js=[(l.split()[0],float(l.split()[3])-float(l.split()[2])) for l in r.stdout.split('\n') if l]
      tj=[j for p,j in js if p=='T'][1:]  # skip the first (start-up) splice
      err=[abs(j)-round(abs(j)/T)*T for j in tj]
      y=np.fromfile(S+'/swo.f32','<f4').reshape(-1,2)[lo:hi,0].astype(float)
      fi=f0*2**(sh/12);h,res=V.interharm(y,fi,rate);hi_,ri=V.interharm(V.tone(fi,rate,tilt,ph,secs=2)[lo:hi],fi,rate)
      rows.append({'rate':rate,'f0':float(f0),'shift':sh,'t_splices':len(tj),'t_periods':sorted(set(int(round(abs(j)/T)) for j in tj)),
                   't_err_max':float(max(map(abs,err))) if err else None,'net_db':float(R.net_db(10*np.log10(res/h),10*np.log10(ri/hi_)))})
    print(rate,f0,[ (r['shift'],r['t_periods'],None if r['t_err_max'] is None else round(r['t_err_max'],2),round(r['net_db'],1)) for r in rows[-4:]],flush=True)
json.dump(rows,open(out,'w'),indent=1)
