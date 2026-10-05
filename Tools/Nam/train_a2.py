#!/usr/bin/env python3
"""Train five independent, full-size official NAM A2 models in one packed batch.

Packing is a CPU training optimization only. Export extracts five separate
WaveNets through upstream's official API; there is no channel interpolation.
"""
import argparse, copy, hashlib, json, platform, time, types
from pathlib import Path
import numpy as np
import torch
from nam.models.wavenet import WaveNet, PackedWaveNet
from nam.models.wavenet._packed_conv import PackedConv1dBase
from nam.models._from_nam import init_from_nam
from common import digest, write_json
from capture_channels import VOICES

def enable_grouped(model):
    def grouped(self,x):
        w=torch.cat([self.weight[o:o+n,i:i+k,:] for (o,n),(i,k) in zip(self.output_segments,self.input_segments)],dim=0)
        return torch.nn.functional.conv1d(x,w,self.bias,self.stride,self.padding,self.dilation,len(self.output_segments))
    for layer in model.modules():
        if isinstance(layer,PackedConv1dBase) and not layer.shared_input:
            if len(set(n for _,n in layer.input_segments))!=1 or len(set(n for _,n in layer.output_segments))!=1:
                raise ValueError('Grouped optimization requires equal-sized independent submodels')
            layer.forward=types.MethodType(grouped,layer)

def predict(model,x):
    rf=model.receptive_field;xx=np.pad(x,(rf-1,0));out=[];model.eval()
    with torch.inference_mode():
        for i in range(0,len(x),16384):
            n=min(16384,len(x)-i)
            y=model(torch.from_numpy(xx[i:i+rf-1+n].copy())[None],pad_start=False)
            out.append(y.numpy()[0])
    return np.concatenate(out,axis=-1)

def scores(pred,ref,skip=6347):
    pred=np.asarray(pred,dtype=np.float64)[...,skip:];ref=np.asarray(ref,dtype=np.float64)[...,skip:]
    err=pred-ref;energy=np.mean(ref**2,axis=-1);mse=np.mean(err**2,axis=-1)
    return [{'esr':float(m/max(e,1e-20)), 'rms_error_db':float(10*np.log10(max(np.mean(p*p),1e-20)/max(e,1e-20))),
      'peak_error_db':float(20*np.log10(max(np.max(np.abs(p)),1e-20)/max(np.max(np.abs(r)),1e-20))),
      'residual_rms_dbfs':float(10*np.log10(max(m,1e-20)))} for p,r,e,m in zip(np.atleast_2d(pred),np.atleast_2d(ref),np.atleast_1d(energy),np.atleast_1d(mse))]

def main():
    ap=argparse.ArgumentParser();ap.add_argument('--data',type=Path,required=True);ap.add_argument('--out',type=Path,required=True)
    ap.add_argument('--trainer',type=Path,required=True);ap.add_argument('--steps',type=int,default=5000)
    ap.add_argument('--batch',type=int,default=2);ap.add_argument('--threads',type=int,default=4);ap.add_argument('--frames',type=int,default=2048)
    ap.add_argument('--lr',type=float,default=.004);ap.add_argument('--lr-half-life',type=float,default=2000)
    ap.add_argument('--lr-origin',type=int,default=0)
    ap.add_argument('--resume',action='store_true');args=ap.parse_args()
    torch.set_num_threads(args.threads);torch.manual_seed(6405);rng=np.random.default_rng(6405)
    template=json.loads((args.trainer/'nam/train/_resources/config_model_packed.json').read_text())['net']['config']['submodels'][-1]['config']
    cfg={'sample_rate':48000,'submodels':[{'name':v[0],'config':copy.deepcopy(template)} for v in VOICES]}
    model=PackedWaveNet.init_from_config(copy.deepcopy(cfg));enable_grouped(model)
    gain=.1;rf=model.receptive_field
    identity=hashlib.sha256((digest(args.data/'manifest.json')+json.dumps(cfg,sort_keys=True)+'causal_raw_gain0.1_grouped_v1').encode()).hexdigest()
    args.out.mkdir(parents=True,exist_ok=True)
    source=json.loads((args.data/'manifest.json').read_text())['source']
    train_cfg={'dataset_sha256':digest(args.data/'manifest.json'),'identity':identity,'config':cfg,'sample_rate':48000,
      'trainer_commit':'0072676419459f5d39e36f5b9fd4172f28d62cbf','target_player_commit':'ef6f178ae1ac6412b55fec6f058d86e640e5aaaf',
      'training_target_gain':gain,'export':'undo training gain in head_scale; fixed silence-bias correction; independent A2-Full WaveNets',
      'source':source,'receptive_field':rf,'torch':torch.__version__,'python':platform.python_version(),'batch':args.batch,'frames':args.frames,'seed':6405,'threads':args.threads,
      'learning_rate':{'initial':args.lr,'half_life_steps':args.lr_half_life,'origin_step':args.lr_origin}}
    write_json(args.out/'training-config.json',train_cfg)
    x=np.load(args.data/'train/input.npy');ys=np.stack([np.load(args.data/'train'/f'{v[0]}.npy') for v in VOICES])
    vx=np.load(args.data/'validation/input.npy');vy=np.stack([np.load(args.data/'validation'/f'{v[0]}.npy') for v in VOICES])
    opt=torch.optim.Adam(model.parameters(),lr=.004,weight_decay=3.17e-7);start=0;best=[float('inf')]*5;weights=[None]*5;best_steps=[0]*5
    if args.resume:
        ck=torch.load(args.out/'checkpoint.pt',map_location='cpu',weights_only=False)
        if ck['identity']!=identity:raise RuntimeError('Resume identity mismatch')
        model.load_state_dict(ck['model']);opt.load_state_dict(ck['optimizer']);start=ck['step'];best=ck['best'];weights=ck['best_weights'];best_steps=ck['best_steps']
        rng.bit_generator.state=ck['rng'];torch.set_rng_state(ck['torch_rng'])
    elif (args.out/'checkpoint.pt').exists():raise RuntimeError('Use --resume or a fresh output directory')
    for group in opt.param_groups:group['lr']=args.lr*(.5**(max(0,start-args.lr_origin)/args.lr_half_life))
    stages_path=args.out/'training-stages.json'
    stages=json.loads(stages_path.read_text()) if stages_path.exists() else []
    if args.steps>start:
        stages.append({'start':start,'end':args.steps,'batch':args.batch,'frames':args.frames,'learning_rate':train_cfg['learning_rate']})
        write_json(stages_path,stages)
    t=time.monotonic();losses=[]
    print(json.dumps({'stage':'start','rf':rf,'channels':[v[0] for v in VOICES],'start_step':start}),flush=True)
    for step in range(start+1,args.steps+1):
        model.train();st=rng.integers(rf,len(x)-args.frames,args.batch)
        xx=torch.from_numpy(np.stack([x[s-rf+1:s+args.frames] for s in st]))
        yy=torch.from_numpy(np.stack([ys[:,s:s+args.frames] for s in st]))*gain
        pred=model(xx,pad_start=False);power=yy.square().mean(dim=-1).clamp_min(.0001)
        loss=((pred-yy).square().mean(dim=-1)/power).mean()
        loss+=.1*((pred.mean(dim=-1)-yy.mean(dim=-1)).square()/power).mean()
        if not torch.isfinite(loss):raise RuntimeError('Nonfinite training loss')
        opt.zero_grad();loss.backward();torch.nn.utils.clip_grad_norm_(model.parameters(),5.);opt.step();model.apply_mask();losses.append(float(loss.detach()))
        for group in opt.param_groups:group['lr']=args.lr*(.5**(max(0,step-args.lr_origin)/args.lr_half_life))
        if step%100==0:
            print(json.dumps({'step':step,'loss':float(np.mean(losses[-100:])),'elapsed_seconds':round(time.monotonic()-t,2)}),flush=True)
        if step%500==0 or step==args.steps:
            # Fixed diverse excerpts select checkpoints without touching the test input.
            vals=[]
            for s in [rf,6*48000,12*48000,18*48000,22*48000]:
                with torch.inference_mode():p=model(torch.from_numpy(vx[s-rf+1:s+8192].copy())[None],pad_start=False).numpy()[0]/gain
                vals.append(scores(p,vy[:,s:s+8192],skip=0))
            means=[float(np.mean([r[i]['esr'] for r in vals])) for i in range(5)]
            for i,score in enumerate(means):
                if score<best[i]:best[i]=score;best_steps[i]=step;weights[i]=copy.deepcopy(model.extract_submodel(i).state_dict())
            write_json(args.out/f'validation-step-{step}.json',{'esr_by_channel':dict(zip([v[0] for v in VOICES],means)),'windows':vals,'best_steps':best_steps})
            torch.save({'identity':identity,'model':model.state_dict(),'optimizer':opt.state_dict(),'step':step,'best':best,'best_weights':weights,'best_steps':best_steps,'rng':rng.bit_generator.state,'torch_rng':torch.get_rng_state()},args.out/'checkpoint.pt')
            print(json.dumps({'step':step,'validation_esr':dict(zip([v[0] for v in VOICES],means)),'best_steps':best_steps}),flush=True)
    results={}
    for i,(name,character,_) in enumerate(VOICES):
        sub=model.extract_submodel(i).eval();sub.load_state_dict(weights[i]);sub._net._head_scale/=gain
        # A fixed final bias correction enforces the known zero-input steady state.
        with torch.inference_mode():dc=float(sub(torch.zeros(rf+512),pad_start=False)[-1])
        with torch.no_grad():sub._net._layer_arrays[-1]._head_rechannel.bias.sub_(dc/sub._net._head_scale)
        sub.export(args.out,basename='Nastrond-'+name,include_snapshot=False,other_metadata={
          'name':'Náströnd '+name,'modeled_by':'RavenForge Luthier Intelligence','gear_type':'amp',
          'character':character,'source_commit':source['source_commit'],'status':'TEST_BUILD_PENDING_ACCEPTANCE',
          'channel':i,'cabinet_included':False,'head_bias_correction_output_units':dc})
        path=args.out/f'Nastrond-{name}.nam';obj=json.loads(path.read_text());restored=init_from_nam(obj).eval()
        with torch.inference_mode():delta=float((sub(torch.from_numpy(vx[:32768]))-restored(torch.from_numpy(vx[:32768]))).abs().max())
        if delta>1e-6:raise RuntimeError('NAM roundtrip changed output')
        prediction=predict(restored,vx);stats=scores(prediction,vy[i])[0]
        windows=[]
        for s in range(rf,len(vx)-4096,4096):
            if np.mean(vy[i,s:s+4096]**2)>1e-6:windows.append(scores(prediction[s:s+4096],vy[i,s:s+4096],skip=0)[0]['esr'])
        stats.update({'selected_step':best_steps[i],'validation_window_esr_median':float(np.median(windows)),
          'validation_window_esr_p95':float(np.quantile(windows,.95)),'validation_window_esr_worst':float(max(windows)),
          'official_roundtrip_max_abs':delta,'sha256':digest(path),'head_bias_correction':dc})
        results[name]=stats;np.save(args.out/f'{name}-validation.npy',prediction)
    write_json(args.out/'validation.json',{'channels':results,'thresholds':{'median_esr':.005,'p95_esr':.01,'worst_esr':.02},
      'input':'unseen synthetic validation excitation','test':'reserved until validation qualifies','real_DI':'not supplied',
      'status':'TEST_BUILD_PENDING_ACCEPTANCE'})
    print(json.dumps({'stage':'exported','channels':results}),flush=True)

if __name__=='__main__':main()
