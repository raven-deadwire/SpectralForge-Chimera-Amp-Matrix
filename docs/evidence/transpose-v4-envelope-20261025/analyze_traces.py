"""Recreate trace comparisons/figures from envelope_study.py --trace output.
No gate metrics or DSP are modified. Run from repository root.
"""
import sys,json,gzip
from pathlib import Path
import numpy as np
from scipy import signal
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
sys.path.insert(0,'Tools/TransposeNext')
from envelope_study import envelope,dips
source=Path(sys.argv[1]);dest=Path(__file__).parent; (dest/'figures').mkdir(exist_ok=True)
engines=['corr40-split-v3','corr40-split-v4','env-floor125'];labels=['v3 / 2.0 ms','v4 / 0.5 ms','trial / 1.25 ms'];colors=['#2972b6','#d55e00','#07946b']
results=[]
for inst,kind,f,notes in [('guitar_dropA_A1','alternating_velocity',55,[12]),('bass_B0','long1414',30.86770632850775,[2,5,8,10])]:
 traces={};audio={}
 for e in engines:
  stem=inst+'-'+kind+'-'+e; audio[e]=np.load(source/(stem+'.npz'));traces[e]=np.genfromtxt(source/(stem+'.csv.gz'),delimiter=',',names=True)
 for note in notes:
  fig,axs=plt.subplots(4,1,figsize=(11,10),sharex=True,layout='constrained') if note in (2,12)else(None,None)
  base=audio[engines[0]];on=list(base['onsets']);o=int(on[note-1]);end=int(on[note]) if note<len(on)else len(base['y']);rate=int(base['rate']);nperiod=round(rate/(f*2**(2/12)))
  focus={'input':inst+'/'+kind,'note_1based':note,'rate':rate,'shift':2,'onset_sample':o,'engines':{}}
  for e,label,col in zip(engines,labels,colors):
   a=audio[e];x,y=a['x'],a['y'];d=traces[e];env=envelope(y[:,0],f*2**(2/12),rate);ind=dips(envelope(x[:,0],f,rate),on,rate)[note-1];od=dips(env,on,rate)[note-1]
   cut=y.copy();cut[end:]=0;cutenv=envelope(cut[:,0],f*2**(2/12),rate);cd=dips(cutenv,on,rate)[note-1]
   sp=np.flatnonzero((np.diff(d['splices'],prepend=0)>0)&(d['sample']>=o)&(d['sample']<o+rate*.100))
   re=np.flatnonzero((np.diff(d['resets'],prepend=0)>0)&(d['sample']>=o)&(d['sample']<o+rate*.100))
   events=[]
   for i in sp:
    length=int(d['fade_length'][i]);z=d[i:i+length];u=z['old_tap'];v=z['new_tap'];w=z['weight'];power=np.sum(((1-w)*u)**2+(w*v)**2)
    events.append({'time_ms':(int(i)-o)*1000/rate,'old_read_ms':d['old_age'][i]*1000/rate,'new_read_ms':d['next_age'][i]*1000/rate,
                   'fundamental_jump_degrees':(d['next_age'][i]-d['old_age'][i])*f/rate*360,
                   'fade_ms':length*1000/rate,'uncentered_tap_similarity':float(np.sum(u*v)/np.sqrt(np.sum(u*u)*np.sum(v*v))),
                   'fade_interference_db':float(10*np.log10(np.sum(z['body_output']**2)/power))})
   diff=np.flatnonzero(np.any(y[o:end].view('u4')!=base['y'][o:end].view('u4'),axis=1));idx=o+round(59.125*rate/1000)
   focus['engines'][e]={'output_dip':od,'input_dip':ind,'extra_dip_db':od['dip_db']-ind['dip_db'],'next_note_output_zeroed_dip':cd,
                      'future_zeroed_identical_prefix':bool(np.array_equal(y[:end].view('u4'),cut[:end].view('u4'))),
                      'trace_reconstruction_max_abs':float(np.max(abs(d['reconstructed']-d['body_output']))),
                      'trace_probe_max_abs':float(np.max(abs(d['output']-y[:,0]))),
                      'resets':[{'time_ms':(int(i)-o)*1000/rate,'old_read_ms':d['old_age'][i]*1000/rate,'new_read_ms':d['next_age'][i]*1000/rate}for i in re],
                      'splices':events,'last_different_sample_before60ms_vs_v3':None if not len(diff[diff<int(.060*rate)])else int(diff[diff<int(.060*rate)][-1]),
                      'output_40_to70ms_bit_identical_to_v3':bool(np.array_equal(y[o+int(.040*rate):o+int(.070*rate)].view('u4'),base['y'][o+int(.040*rate):o+int(.070*rate)].view('u4'))),
                      'causal_one_period_rms_at59125ms':float(np.sqrt(np.mean(y[idx-nperiod:idx,0]**2)))}
   sl=slice(max(0,o-int(.010*rate)),min(len(y),o+int(.100*rate)));tt=(d['sample'][sl]-o)*1000/rate
   matrix=np.column_stack([d[name][sl]for name in d.dtype.names]+[env[sl],cutenv[sl]])
   with gzip.open(dest/f'{inst}-note{note}-{e}.csv.gz','wt')as out:np.savetxt(out,matrix,delimiter=',',header=','.join(d.dtype.names)+',noncausal_envelope,next_note_zeroed_envelope',comments='',fmt='%.17g')
   if axs is not None:
    axs[0].plot(tt,d['old_age'][sl]*1000/rate,label=label,color=col,lw=1.2)
    axs[1].plot(tt,d['weight'][sl],color=col,lw=1.1)
    axs[2].plot(tt,y[sl,0],color=col,lw=.8,alpha=.9)
    axs[3].plot(tt,20*np.log10(np.maximum(env[sl],1e-8)),color=col,lw=1.5)
    axs[3].plot(tt,20*np.log10(np.maximum(cutenv[sl],1e-8)),color=col,lw=.8,ls='--',alpha=.6)
  results.append(focus)
  if axs is not None:
   axs[0].legend(loc='upper right');axs[0].set_ylabel('Old head age (ms)');axs[1].set_ylabel('Fade weight');axs[2].set_ylabel('Output');axs[3].set_ylabel('Fundamental envelope\n(dB re 1)');axs[3].set_xlabel('Time relative to note onset (ms)')
   axs[0].set_title(f'{inst}: note {note}, +2 semitones, 48 kHz | synthetic research')
   for ax in axs:
    ax.set_xlim(0,80);ax.grid(alpha=.18);ax.axvline(25.75 if note==12 else 26,color='gray',lw=.8,ls=':')
   axs[3].set_ylim(-100,-10);axs[3].text(.02,.06,'Solid: original noncausal metric envelope. Dashed: same past output, later notes zeroed.',transform=axs[3].transAxes,fontsize=8)
   if note==12:
    axs[2].axvspan(40,70,color='#07946b',alpha=.1);axs[2].text(.51,.82,'40–70 ms: outputs bit-identical',transform=axs[2].transAxes,fontsize=9)
    axs[3].axvline(59.125,color='#d55e00',lw=.8,ls=':')
   fig.savefig(dest/'figures'/f'{inst}-note{note}.png',dpi=150);plt.close(fig)
(dest/'mechanism-summary.json').write_text(json.dumps(results,indent=2))
print('Saved',len(results),'focus cases; all trace reconstruction errors',[q['engines'][e]['trace_reconstruction_max_abs']for q in results for e in engines])
