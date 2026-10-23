import subprocess,numpy as np,sys
X='/tmp/claude-0/lpc/timing3/work/timing_mix-bass_B0-48000.f32'
bins={'old':'/tmp/claude-0/tn-old/TransposeProbe','new':'/tmp/claude-0/tn/TransposeProbe'}
for shift in ['-2','2']:
 for block in ['32','64']:
  dl=int(block)/48000*1e6
  for e in sys.argv[1].split(','):
   res={}
   for label,R in bins.items():
    miss=0;means=[];peaks=[];spl=[]
    for r in range(int(sys.argv[2])):
      subprocess.run([R,X,'ab.f32','48000',block,shift,e,'ab.csv','cpu'],check=True,capture_output=True)
      c=np.loadtxt('ab.csv',delimiter=',',skiprows=1)[:,2];miss+=int((c>dl).sum());means.append(c.mean());peaks.append(np.percentile(c,99.9))
    res[label]=(miss,np.mean(means),np.median(peaks))
   print(f'shift {shift} block {block} {e:18} old: miss {res["old"][0]:3} mean {res["old"][1]:6.1f}us p99.9 {res["old"][2]:7.1f}us | new: miss {res["new"][0]:3} mean {res["new"][1]:6.1f}us p99.9 {res["new"][2]:7.1f}us',flush=True)
