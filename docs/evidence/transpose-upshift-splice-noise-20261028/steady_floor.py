import sys,json,subprocess,numpy as np
sys.path.insert(0,str(__import__('pathlib').Path(__file__).resolve().parents[3]/'Tools'/'TransposeNext'))
from scipy import signal
from weak_attack_study import FREQUENCIES
TR=sys.argv[1]; W=sys.argv[2]
def bp(y,rate): return signal.sosfiltfilt(signal.butter(3,[1000,8000],fs=rate,btype='bandpass',output='sos'),y.astype(float))
def env(y,rate):
    hi=bp(y,rate); w=round(.002*rate); return np.sqrt(signal.convolve(hi*hi,np.ones(w)/w,mode='same'))
def tone(f,rate,secs=3):
    t=np.arange(secs*rate)/rate; return sum(.12/h*np.sin(2*np.pi*f*h*t+.19*h) for h in range(1,13))
rows=[]
for rate in map(int,sys.argv[3].split(',')):
  for inst,f in FREQUENCIES.items():
    z=tone(f,rate); x=np.column_stack((z,-.65*z)).astype('<f4'); x.tofile(W+'/i.f32')
    a,b=round(.5*rate),-round(.3*rate)
    ein=env(z,rate)[a:b]
    for sh in (-2,-1,1,2):
        r=subprocess.run([TR,W+'/i.f32',W+'/o.f32',str(rate),str(sh),'v3'],check=True,capture_output=True,text=True)
        tr=json.loads(r.stdout); y=np.fromfile(W+'/o.f32','<f4').reshape(-1,2)[:,0]
        ideal=tone(f*2**(sh/12),rate)
        eo=env(y,rate); ei=env(ideal,rate)
        ts=np.array(tr['transient_splices']); bs=np.array(tr['body_splices'])
        # splice-locked: env within +-3 ms of a transient splice vs elsewhere (steady region)
        m=np.zeros(len(y),bool)
        for s in ts: m[max(0,s-round(.001*rate)):s+round(.005*rate)]=True
        mb=np.zeros(len(y),bool)
        for s in bs: mb[max(0,s-round(.001*rate)):s+round(.005*rate)]=True
        reg=np.zeros(len(y),bool); reg[a:len(y)+b]=True
        ho=bp(y,rate)[reg]; hi_=bp(ideal,rate)[reg]
        rows.append({'rate':rate,'inst':inst,'shift':sh,
          'in_floor':float(ein.max()),'ideal_floor':float(ei[a:b].max()),'out_floor':float(eo[a:b].max()),
          'out_vs_in_db':round(20*np.log10(eo[a:b].max()/ein.max()),1),
          'out_vs_ideal_db':round(20*np.log10(eo[a:b].max()/ei[a:b].max()),1),
          'rms_out_vs_ideal_db':round(10*np.log10(np.mean(ho**2)/np.mean(hi_**2)),1),
          'near_tsplice_max':float(eo[m&reg].max()) if (m&reg).any() else None,
          'away_max':float(eo[reg&~m&~mb].max()),
          'near_bsplice_max':float(eo[mb&reg&~m].max()) if (mb&reg&~m).any() else None,
          't_splices':len(ts),'b_splices':len(bs)})
        r=rows[-1]; print(rate,inst,sh,r['out_vs_in_db'],r['out_vs_ideal_db'],r['rms_out_vs_ideal_db'],
              'near_t',round(20*np.log10(r['near_tsplice_max']/r['ideal_floor']),1) if r['near_tsplice_max'] else None,
              'near_b',round(20*np.log10(r['near_bsplice_max']/r['ideal_floor']),1) if r['near_bsplice_max'] else None,
              'away',round(20*np.log10(r['away_max']/r['ideal_floor']),1),len(ts),len(bs),flush=True)
json.dump(rows,open(sys.argv[4],'w'),indent=1)
