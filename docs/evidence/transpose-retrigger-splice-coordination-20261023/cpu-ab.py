import subprocess,numpy as np,sys,itertools
R='/tmp/claude-0/tn/TransposeProbe';X='/tmp/claude-0/lpc/timing3/work/timing_mix-bass_B0-48000.f32'
engines=sys.argv[1].split(',');reps=int(sys.argv[2]);SHIFT=sys.argv[3] if len(sys.argv)>3 else '-2';deadline=64/48000*1e6
stats={e:{'miss':0,'spk':[],'mean':[]} for e in engines}
for r in range(reps):
  for e in (engines if r%2==0 else engines[::-1]):
    subprocess.run([R,X,'/tmp/claude-0/sim/ab.f32','48000','64',SHIFT,e,'/tmp/claude-0/sim/ab.csv','cpu'],check=True,capture_output=True)
    c=np.loadtxt('/tmp/claude-0/sim/ab.csv',delimiter=',',skiprows=1)[:,2]
    s=stats[e];s['miss']+=int((c>deadline).sum());s['spk']+=list(np.flatnonzero(c>500));s['mean'].append(c.mean())
for e,s in stats.items():print(e,'misses',s['miss'],'spikes>500us',len(s['spk']),'mean %.1f'%np.mean(s['mean']),'positions',sorted(s['spk'])[:12])
