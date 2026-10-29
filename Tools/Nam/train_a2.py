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
from quality_profile import PROFILE, fidelity
from training_contract import verify_audio_manifest, training_identity
from a2_recipe import (RECIPE, A2TrainingLoss, training_output_gains,
    rescale_output_head, steps_per_virtual_epoch, virtual_epoch_learning_rate)

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

def centered_prediction(model,x):
    result=predict(model,x)
    with torch.inference_mode():dc=model(torch.zeros(1,model.receptive_field+1),pad_start=False).numpy()[0,...,-1]
    return result-np.asarray(dc)[...,None]

def export_consistent_prediction(model, x):
    """Differentiate through the same fixed zero-history correction as export.

    Never remove a signal-dependent mean: asymmetry, transients and native
    decay remain in the target. Only the model's zero-input steady state is
    subtracted. This is an explicit refinement, not the untouched A2 recipe.
    """
    dc = model(x.new_zeros(1, model.receptive_field), pad_start=False)
    return model(x, pad_start=False) - dc

def main():
    ap=argparse.ArgumentParser();ap.add_argument('--data',type=Path,required=True);ap.add_argument('--out',type=Path,required=True)
    ap.add_argument('--trainer',type=Path,required=True);ap.add_argument('--steps',type=int,default=5000)
    ap.add_argument('--batch',type=int,default=2);ap.add_argument('--threads',type=int,default=4);ap.add_argument('--frames',type=int,default=2048)
    ap.add_argument('--lr',type=float,default=.004);ap.add_argument('--lr-half-life',type=float,default=2000)
    ap.add_argument('--lr-origin',type=int,default=0)
    ap.add_argument('--lr-warmup',type=int,default=0,help='Steps of linear LR warmup from zero; useful after a warm start resets Adam')
    ap.add_argument('--recipe',choices=['legacy','official-a2'],default='legacy')
    ap.add_argument('--loss-normalization',choices=['window','channel'],default='window')
    # Five 8,192-sample excerpts cover 0.85 s of validation; on 2026-10-25 they kept a
    # state 24% worse than the final step for Nidhoggr. Full validation is the default.
    ap.add_argument('--validation-selection',choices=['window','full'],default='full')
    ap.add_argument('--warm-start',type=Path,help='Initialize from a prior run with the same architecture and frozen native source; reset optimizer and re-score validation')
    ap.add_argument('--keep-passed',action='store_true',help='With --warm-start: a checkpoint is eligible only if it keeps every profile limit the starting model passes on validation')
    ap.add_argument('--warm-start-final',action='store_true',help='Warm-start from the prior run\'s final weights instead of its selected states')
    ap.add_argument('--loud-fraction',type=float,default=0.,help='Fraction of training crops containing input at or above -6 dBFS peak, where error concentrates')
    ap.add_argument('--tail-fraction',type=float,default=0.,help='Fraction of training windows drawn from the final captured decay')
    ap.add_argument('--center-training-output',action='store_true',help='Train the same zero-history-corrected response that is exported')
    ap.add_argument('--amplitude-weight',type=float,default=0.,help='Legacy loss: weight each sample by 1+A*|target|/window peak, so loud passages count more')
    ap.add_argument('--overshoot-weight',type=float,default=0.,help='Legacy loss: penalize output beyond the window\'s target peak (attack overshoot)')
    ap.add_argument('--peak-weight',default='',help='Legacy loss, per channel as Name=weight,...: penalize each crop\'s output peak '
      'beyond its target peak, as a squared fraction of the target peak. The profile\'s peak level limit is set by a few attack samples, '
      'which a per-sample penalty averages away')
    ap.add_argument('--tail-all-parts',action='store_true',help='Draw tail crops from the final decay of every combined capture part, '
      'not only the end of the whole training signal')
    ap.add_argument('--target-limits',default='',help='Per channel as Name=limit+limit,...: steps that pass these profile limits '
      '(full_esr, active_median, active_p95, active_worst, quiet, level) are preferred, ahead of level and ESR')
    ap.add_argument('--train-channels',default='',help='Comma-separated channels whose loss is used; the others get no gradient '
      'and keep their starting models. Default: all')
    ap.add_argument('--block-norm-samples',type=int,default=0,help='Legacy loss: also normalize error by the reference energy of each '
      'block of this many samples, floored at --block-norm-floor-db below the channel\'s mean energy, so quiet decays inside loud crops count')
    ap.add_argument('--block-norm-weight',type=float,default=.5)
    ap.add_argument('--block-norm-floor-db',type=float,default=50.)
    ap.add_argument('--diagnostic-extra-dilations',help='Comma-separated dilations appended (kernel 6) to the A2 layer array. '
      'Diagnostic only: the result is not an A2 model, cannot load in the target player and is exported as Diagnostic-*.nam')
    ap.add_argument('--resume',action='store_true');args=ap.parse_args()
    if args.resume and args.warm_start:raise ValueError('Choose resume or warm-start')
    if not 0<=args.tail_fraction<=.25:raise ValueError('Tail fraction must be between 0 and .25')
    if not 0<=args.loud_fraction<=.75 or args.tail_fraction+args.loud_fraction>.8:raise ValueError('Loud fraction must be in [0, .75] and leave random crops')
    if args.warm_start_final and not args.warm_start:raise ValueError('--warm-start-final needs --warm-start')
    if args.keep_passed and not (args.warm_start or args.resume):raise ValueError('--keep-passed needs --warm-start')
    if args.amplitude_weight<0 or args.overshoot_weight<0:raise ValueError('Loss weights must be nonnegative')
    names=[v[0] for v in VOICES];peak_weights=[0.]*len(VOICES)
    for item in filter(None,args.peak_weight.split(',')):
        name,_,w=item.partition('=')
        if name not in names or float(w)<0:raise ValueError(f'Bad --peak-weight entry: {item}')
        peak_weights[names.index(name)]=float(w)
    trained=[n.strip() for n in args.train_channels.split(',') if n.strip()] or names
    if not set(trained)<=set(names):raise ValueError('Unknown --train-channels entry')
    channel_mask=[1. if n in trained else 0. for n in names]
    if args.block_norm_samples and (args.block_norm_samples<16 or args.frames%args.block_norm_samples):raise ValueError('--block-norm-samples must divide --frames')
    target_limits=[[] for _ in VOICES]
    for item in filter(None,args.target_limits.split(',')):
        name,_,limits=item.partition('=');limits=limits.split('+')
        if name not in names or not set(limits)<={'full_esr','active_median','active_p95','active_worst','quiet','level'}:
            raise ValueError(f'Bad --target-limits entry: {item}')
        target_limits[names.index(name)]=sorted(set(limits))
    if args.recipe!='legacy' and (args.amplitude_weight or args.overshoot_weight or any(peak_weights)):raise ValueError('Loss weights apply to the legacy recipe only')
    if min(args.steps,args.batch,args.frames,args.threads,args.lr,args.lr_half_life)<=0:raise ValueError('Positive training budget and optimizer settings required')
    manifest=verify_audio_manifest(args.data)
    torch.set_num_threads(args.threads);torch.manual_seed(6405);rng=np.random.default_rng(6405)
    template=json.loads((args.trainer/'nam/train/_resources/config_model_packed.json').read_text())['net']['config']['submodels'][-1]['config']
    extra=[int(d) for d in args.diagnostic_extra_dilations.split(',')] if args.diagnostic_extra_dilations else []
    if any(d<=0 for d in extra):raise ValueError('Diagnostic dilations must be positive')
    if extra:
        template=copy.deepcopy(template);layers=template['layers_configs'][0]
        layers['dilations']+=extra;layers['kernel_sizes']+=[6]*len(extra)
    prefix='Diagnostic-' if extra else 'Nastrond-'
    cfg={'sample_rate':48000,'submodels':[{'name':v[0],'config':copy.deepcopy(template)} for v in VOICES]}
    model=PackedWaveNet.init_from_config(copy.deepcopy(cfg));enable_grouped(model)
    rf=model.receptive_field
    # Memory-mapped: hour-scale training sets exceed RAM; crops read only what they use.
    x=np.load(args.data/'train/input.npy',mmap_mode='r');ys=[np.load(args.data/'train'/f'{v[0]}.npy',mmap_mode='r') for v in VOICES]
    gains=training_output_gains(np.stack(ys)) if args.recipe=='official-a2' else np.full(len(VOICES),.1)
    gain_tensor=torch.tensor(gains,dtype=torch.float32)[None,:,None]
    peak_weight_tensor=torch.tensor(peak_weights,dtype=torch.float32)[None,:]
    mask_tensor=torch.tensor(channel_mask,dtype=torch.float32)[None,:]
    def masked_mean(term):
        # term is [batch, channel]; untrained channels contribute nothing.
        return (term*mask_tensor).sum()/(mask_tensor.sum()*term.shape[0])
    # Notes are scaled to exactly -6 or -3 dBFS peak; -6.1 keeps the -6 dBFS ones.
    loud_samples=np.flatnonzero(np.abs(x[rf:len(x)-args.frames])>=10**(-6.1/20))+rf
    # End of every capture part: each common.signal ends with 0.5 s of silence after the last note's decay.
    part_ends=np.cumsum([r['train_samples'] for r in manifest.get('combined_from',[])]) if args.tail_all_parts else np.array([len(x)])
    if args.tail_all_parts and (not len(part_ends) or part_ends[-1]!=len(x)):raise ValueError('--tail-all-parts needs a combined dataset')
    if args.loud_fraction and not len(loud_samples):raise ValueError('No input at or above -6 dBFS for --loud-fraction')
    epoch_steps=steps_per_virtual_epoch(len(x),rf,args.frames,args.batch)
    args.out.mkdir(parents=True,exist_ok=True)
    source=manifest['source']
    train_cfg={'dataset_sha256':digest(args.data/'manifest.json'),'config':cfg,'sample_rate':48000,
      'trainer_commit':'0072676419459f5d39e36f5b9fd4172f28d62cbf','target_player_commit':'ef6f178ae1ac6412b55fec6f058d86e640e5aaaf',
      'training_target_gain':gains.tolist() if args.recipe=='official-a2' else .1,'export':'undo training gain in head_scale; fixed silence-bias correction; independent A2-Full WaveNets',
      'source':source,'receptive_field':rf,'torch':torch.__version__,'python':platform.python_version(),'batch':args.batch,'frames':args.frames,'seed':6405,'threads':args.threads,
      'learning_rate':{'initial':args.lr,'half_life_steps':args.lr_half_life,'origin_step':args.lr_origin,'warmup_steps':args.lr_warmup},
      'loss_normalization':args.loss_normalization,'validation_selection':args.validation_selection,
      'recipe':args.recipe,'recipe_details':RECIPE if args.recipe=='official-a2' else {'loss':'normalized MSE + .1 normalized DC','clip_gradient_norm':5.},
      'sampling':'seeded random crops; matched across comparison arms, not upstream shuffled nonoverlapping epochs',
      'virtual_epoch_steps':epoch_steps,
      'quality_profile':PROFILE,'tail_fraction':args.tail_fraction,
      'identity_version':2,'center_training_output':args.center_training_output,
      'selection':'level-gated: a checkpoint within the profile RMS/peak level limits always beats one outside them; ESR ranks within each group',
      'diagnostic_extra_dilations':extra or None,
      'keep_passed':args.keep_passed,'amplitude_weight':args.amplitude_weight,'overshoot_weight':args.overshoot_weight,'peak_weight':dict(zip(names,peak_weights)),'target_limits':dict(zip(names,target_limits)),'tail_all_parts':args.tail_all_parts,'train_channels':trained,
      'block_norm':{'samples':args.block_norm_samples,'weight':args.block_norm_weight,'floor_db':args.block_norm_floor_db} if args.block_norm_samples else None,'loud_fraction':args.loud_fraction,
      'warm_start':None if args.warm_start is None else {'run':str(args.warm_start),'checkpoint_sha256':digest(args.warm_start/'checkpoint.pt')}}
    if args.recipe=='official-a2':
        train_cfg['learning_rate']={'initial':args.lr,'gamma':.994,'virtual_epoch_steps':epoch_steps,'schedule':'step at completed virtual epoch; random crop adaptation'}
        train_cfg['loss_normalization']='train-only output RMS -18dBFS; summed per-channel unnormalized MSE + .0005 MRSTFT'
    identity=training_identity(train_cfg)
    train_cfg['identity']=identity
    vx=np.load(args.data/'validation/input.npy');vy=np.stack([np.load(args.data/'validation'/f'{v[0]}.npy') for v in VOICES])
    def mean_square(y,chunk=1<<24):
        return sum(float(np.sum(np.square(y[i:i+chunk],dtype=np.float64))) for i in range(0,len(y),chunk))/len(y)
    channel_power=torch.tensor([mean_square(y)*g**2 for y,g in zip(ys,gains)],dtype=torch.float32)[None].clamp_min(.0001)
    recipe_loss=A2TrainingLoss() if args.recipe=='official-a2' else None
    def learning_rate(step):
        if args.recipe=='official-a2':return virtual_epoch_learning_rate(step,epoch_steps,initial=args.lr)
        warm=min(1.,step/args.lr_warmup) if args.lr_warmup else 1.
        return warm*args.lr*(.5**(max(0,step-args.lr_origin)/args.lr_half_life))
    opt=torch.optim.Adam(model.parameters(),lr=.004,weight_decay=3.17e-7);start=0;best=[float('inf')]*5;weights=[None]*5;best_steps=[0]*5
    best_level_ok=[False]*5
    keep_required=[[] for _ in VOICES]
    def passed_limits(prediction,i):
        """Each profile limit on full validation, judged separately."""
        f=fidelity(prediction,vy[i],VOICES[i][0],vx)
        def within(key,limit):return f[key] is not None and f[key]<=PROFILE[limit]
        return {'full_esr':f['full_esr_pass'],'active_median':within('active_median','active_window_esr_median_max'),
                'active_p95':within('active_p95','active_window_esr_p95_max'),'active_worst':within('active_worst','active_window_esr_worst_max'),
                'quiet':f['quiet_pass'],'level':f['level_pass']}
    def level_ok(prediction,i):
        """Export-consistent level within the profile."""
        row=scores(prediction,vy[i])[0]
        return abs(row['rms_error_db'])<=PROFILE['rms_error_max_db'] and abs(row['peak_error_db'])<=PROFILE['peak_error_max_db']
    def keeps_passed(prediction,i):
        """With --keep-passed, every limit the starting model passed is still passed.
        A hard gate, separate from level, so a start that fails level still keeps the rest."""
        if not keep_required[i]:return True
        limits=passed_limits(prediction,i);return all(limits[k] for k in keep_required[i])
    best_target_ok=[True]*5
    def target_ok(prediction,i):
        if not target_limits[i]:return True
        limits=passed_limits(prediction,i);return all(limits[k] for k in target_limits[i])
    def better(i,ok,score,target=True):
        # Target limits rank first, then level, then ESR.
        if target!=best_target_ok[i]:return target
        return (ok and not best_level_ok[i]) or (ok==best_level_ok[i] and score<best[i])
    selection_windows=[rf,6*48000,12*48000,18*48000,22*48000]
    def excerpt_predictions(net):
        """Excerpts with the zero-input output removed, as export removes it.
        Raw output can carry a large drifting DC (-0.87 for Surtr at 30k steps
        with centered training) that the exported model never plays."""
        with torch.inference_mode():
            dc=net(torch.zeros(1,rf+1),pad_start=False).numpy()[0,...,-1]
            return [net(torch.from_numpy(vx[s-rf+1:s+8192].copy())[None],pad_start=False).numpy()[0]-np.asarray(dc)[...,None]
                    for s in selection_windows]
    def selection_esr(sub,i):
        """One channel's selection score, computed exactly as during training."""
        if args.validation_selection=='full':return scores(centered_prediction(sub,vx)/gains[i],vy[i])[0]['esr']
        return float(np.mean([scores(p/gains[i],vy[i,s:s+8192],skip=0)[0]['esr']
                              for s,p in zip(selection_windows,excerpt_predictions(sub))]))
    if args.resume:
        ck=torch.load(args.out/'checkpoint.pt',map_location='cpu',weights_only=False)
        if ck['identity']!=identity:raise RuntimeError('Resume identity mismatch')
        if ck.get('recipe','legacy')!=args.recipe:raise RuntimeError('Resume recipe mismatch')
        prior_config=json.loads((args.out/'training-config.json').read_text())
        if prior_config['identity']!=ck['identity']:raise RuntimeError('Resume checkpoint/config identity mismatch')
        train_cfg['warm_start']=prior_config.get('warm_start')
        model.load_state_dict(ck['model']);opt.load_state_dict(ck['optimizer']);start=ck['step'];best=ck['best'];weights=ck['best_weights'];best_steps=ck['best_steps']
        best_level_ok=ck['best_level_ok'];keep_required=ck.get('keep_required',keep_required)
        # Re-score saved states with this code's metric; a checkpoint written by
        # an older selection metric must not be compared with new scores.
        for i,state in enumerate(weights):
            sub=model.extract_submodel(i).eval();sub.load_state_dict(state)
            resumed=centered_prediction(sub,vx)/gains[i]
            best[i]=selection_esr(sub,i);best_level_ok[i]=level_ok(resumed,i);best_target_ok[i]=target_ok(resumed,i)
        model.load_state_dict(ck['model'])
        rng.bit_generator.state=ck['rng'];torch.set_rng_state(ck['torch_rng'])
        if ck.get('validation_selection','window')!=args.validation_selection:
            if args.validation_selection!='full':raise RuntimeError('Changing full selection back to excerpts is not supported')
            # Re-score saved states on the new selection metric before comparing.
            # Never compare excerpt ESR values with full-signal ESR values.
            for i,state in enumerate(weights):
                sub=model.extract_submodel(i).eval();sub.load_state_dict(state)
                best[i]=scores(centered_prediction(sub,vx)/gains[i],vy[i])[0]['esr']
    elif (args.out/'checkpoint.pt').exists():raise RuntimeError('Use --resume or a fresh output directory')
    elif args.warm_start:
        prior=json.loads((args.warm_start/'training-config.json').read_text())
        # The native source must match; the training excerpt (train_seed) may differ.
        native=lambda src:{k:v for k,v in src.items() if k!='train_seed'}
        if prior['config']!=cfg or native(prior['source'])!=native(source):
            raise RuntimeError('Warm-start architecture or capture source mismatch')
        prior_gains=np.broadcast_to(np.asarray(prior['training_target_gain'],dtype=np.float64),gains.shape)
        ck=torch.load(args.warm_start/'checkpoint.pt',map_location='cpu',weights_only=False)
        if ck['identity']!=prior['identity']:raise RuntimeError('Warm-start checkpoint/config identity mismatch')
        raw_deltas=[]
        if args.warm_start_final:
            final=PackedWaveNet.init_from_config(copy.deepcopy(cfg));final.load_state_dict(ck['model'])
            start_states=[final.extract_submodel(i).state_dict() for i in range(5)]
        else:start_states=ck['best_weights']
        for i,state in enumerate(start_states):
            sub=model.extract_submodel(i).eval();sub.load_state_dict(state)
            with torch.inference_mode():before=sub(torch.from_numpy(x[:32768].copy()))/prior_gains[i]
            rescale_output_head(sub,prior_gains[i],gains[i])
            with torch.inference_mode():after=sub(torch.from_numpy(x[:32768].copy()))/gains[i]
            delta=float((before-after).abs().max());raw_deltas.append(delta)
            if not np.isfinite(delta) or delta>1e-5:raise RuntimeError('Warm-start output reparameterization changed physical prediction')
            model.import_submodel(i,sub)
            # Score the starting state with the same metric later steps are selected
            # by; full-signal and excerpt ESR values are not comparable.
            best[i]=selection_esr(sub,i)
            start_prediction=centered_prediction(sub,vx)/gains[i]
            if args.keep_passed:keep_required[i]=sorted(k for k,v in passed_limits(start_prediction,i).items() if v)
            best_level_ok[i]=level_ok(start_prediction,i);best_target_ok[i]=target_ok(start_prediction,i)
            weights[i]=copy.deepcopy(sub.state_dict())
        # Zero denotes the warm-start candidate, not a newly trained checkpoint.
        best_steps=[0]*5
        write_json(args.out/'initialization.json',{'source_checkpoint_sha256':digest(args.warm_start/'checkpoint.pt'),
          'source_checkpoint_step':ck['step'],'source_selected_steps':ck['best_steps'],'optimizer':'reset identically in comparison arms',
          'raw_prediction_max_abs_delta':dict(zip([v[0] for v in VOICES],raw_deltas)),
          'initial_validation_esr':dict(zip([v[0] for v in VOICES],best)),'keep_required':dict(zip([v[0] for v in VOICES],keep_required)),'initial_validation_metric':args.validation_selection,'gains':gains.tolist(),'seed':6405})
    # Failed identity checks must not overwrite a recoverable run's configuration.
    write_json(args.out/'training-config.json',train_cfg)
    for group in opt.param_groups:group['lr']=learning_rate(start)
    stages_path=args.out/'training-stages.json'
    stages=json.loads(stages_path.read_text()) if stages_path.exists() else []
    if args.steps>start:
        stages.append({'start':start,'end':args.steps,'batch':args.batch,'frames':args.frames,'learning_rate':train_cfg['learning_rate'],
          'loss_normalization':train_cfg['loss_normalization'],'validation_selection':args.validation_selection,'tail_fraction':args.tail_fraction,'recipe':args.recipe})
        write_json(stages_path,stages)
    t=time.monotonic();losses=[];crop_digest=hashlib.sha256()
    print(json.dumps({'stage':'start','rf':rf,'channels':[v[0] for v in VOICES],'start_step':start}),flush=True)
    for step in range(start+1,args.steps+1):
        model.train();st=rng.integers(rf,len(x)-args.frames,args.batch)
        for j in range(args.batch):
            draw=rng.random()
            if draw<args.tail_fraction:
                end=int(part_ends[rng.integers(len(part_ends))])
                st[j]=rng.integers(max(rf,end-round(.8*48000)),end-args.frames)
            elif draw<args.tail_fraction+args.loud_fraction:
                # A crop whose output window contains one of the loud input samples.
                k=loud_samples[rng.integers(len(loud_samples))]
                st[j]=int(np.clip(k-rng.integers(args.frames),rf,len(x)-args.frames-1))
        crop_digest.update(st.astype('<i8').tobytes())
        xx=torch.from_numpy(np.stack([x[s-rf+1:s+args.frames] for s in st]))
        yy=torch.from_numpy(np.stack([np.stack([y[s:s+args.frames] for y in ys]) for s in st]))*gain_tensor
        pred=export_consistent_prediction(model,xx) if args.center_training_output else model(xx,pad_start=False)
        if recipe_loss is not None:loss=recipe_loss(pred,yy)
        else:
            power=yy.square().mean(dim=-1).clamp_min(.0001) if args.loss_normalization=='window' else channel_power
            peak=yy.abs().amax(dim=-1,keepdim=True).clamp_min(1e-6)
            weight=1+args.amplitude_weight*yy.abs()/peak
            loss=masked_mean(((pred-yy).square()*weight).mean(dim=-1)/weight.mean(dim=-1)/power)
            loss+=.1*masked_mean((pred.mean(dim=-1)-yy.mean(dim=-1)).square()/power)
            if args.overshoot_weight:
                loss+=args.overshoot_weight*masked_mean(torch.relu(pred.abs()-peak).square().mean(dim=-1)/power)
            if args.block_norm_samples:
                shape=(*yy.shape[:2],-1,args.block_norm_samples)
                block_power=yy.reshape(shape).square().mean(dim=-1).clamp_min(channel_power[...,None]*10**(-args.block_norm_floor_db/10))
                loss+=args.block_norm_weight*masked_mean(((pred-yy).reshape(shape).square().mean(dim=-1)/block_power).mean(dim=-1))
            if any(peak_weights):
                crop_peak=peak.squeeze(-1)
                loss+=masked_mean(peak_weight_tensor*(torch.relu(pred.abs().amax(dim=-1)-crop_peak)/crop_peak).square())
        if not torch.isfinite(loss):raise RuntimeError('Nonfinite training loss')
        opt.zero_grad();loss.backward()
        if args.recipe=='legacy':torch.nn.utils.clip_grad_norm_(model.parameters(),5.)
        # Set this step's LR before stepping. Setting it afterwards made the first
        # step use the constructor's 0.004 whatever --lr said, jolting warm starts.
        for group in opt.param_groups:group['lr']=learning_rate(step)
        opt.step();model.apply_mask();losses.append(float(loss.detach()))
        if step%100==0:
            print(json.dumps({'step':step,'loss':float(np.mean(losses[-100:])),'elapsed_seconds':round(time.monotonic()-t,2),
              'learning_rate':learning_rate(step),'last_loss_components':recipe_loss.last_components if recipe_loss is not None else None}),flush=True)
        if step%500==0 or step==args.steps:
            # Selection never touches either independent test input.
            vals=[]
            if args.validation_selection=='full':
                prediction=centered_prediction(model,vx)/gains[:,None]
                means=[row['esr'] for row in scores(prediction,vy)]
            else:
                for s,p in zip(selection_windows,excerpt_predictions(model)):
                    vals.append(scores(p/gains[:,None],vy[:,s:s+8192],skip=0))
                means=[float(np.mean([r[i]['esr'] for r in vals])) for i in range(5)]
            if args.validation_selection!='full':prediction=centered_prediction(model,vx)/gains[:,None]
            levels=[level_ok(prediction[i],i) for i in range(5)]
            kept=[keeps_passed(prediction[i],i) for i in range(5)]
            targets=[target_ok(prediction[i],i) for i in range(5)]
            for i,score in enumerate(means):
                if kept[i] and better(i,levels[i],score,targets[i]):
                    best[i]=score;best_level_ok[i]=levels[i];best_target_ok[i]=targets[i];best_steps[i]=step;weights[i]=copy.deepcopy(model.extract_submodel(i).state_dict())
            profile_metrics={v[0]:fidelity(prediction[i],vy[i],v[0],vx) for i,v in enumerate(VOICES)} if args.validation_selection=='full' else None
            write_json(args.out/f'validation-step-{step}.json',{'esr_by_channel':dict(zip([v[0] for v in VOICES],means)),'level_ok_by_channel':dict(zip([v[0] for v in VOICES],levels)),'keeps_passed_by_channel':dict(zip([v[0] for v in VOICES],kept)),'target_ok_by_channel':dict(zip([v[0] for v in VOICES],targets)),'windows':vals,'best_steps':best_steps,'best_level_ok':best_level_ok,'tone3000_fidelity':profile_metrics,
              'crop_schedule_sha256':crop_digest.hexdigest(),'crop_schedule_start_step':start,'identity':identity,'checkpoint_step':step,'recipe':args.recipe})
            checkpoint_temporary=args.out/'checkpoint.pt.tmp'
            torch.save({'identity':identity,'model':model.state_dict(),'optimizer':opt.state_dict(),'step':step,'best':best,'best_weights':weights,'best_steps':best_steps,'best_level_ok':best_level_ok,'keep_required':keep_required,'rng':rng.bit_generator.state,'torch_rng':torch.get_rng_state(),
              'validation_selection':args.validation_selection,'recipe':args.recipe},checkpoint_temporary)
            checkpoint_temporary.replace(args.out/'checkpoint.pt')
            print(json.dumps({'step':step,'validation_esr':dict(zip([v[0] for v in VOICES],means)),'best_steps':best_steps}),flush=True)
    results={}
    for i,(name,character) in enumerate(VOICES):
        sub=model.extract_submodel(i).eval();sub.load_state_dict(weights[i]);sub._net._head_scale/=gains[i]
        # A fixed final bias correction enforces the known zero-input steady state.
        with torch.inference_mode():dc=float(sub(torch.zeros(rf+512),pad_start=False)[-1])
        with torch.no_grad():sub._net._layer_arrays[-1]._head_rechannel.bias.sub_(dc/sub._net._head_scale)
        sub.export(args.out,basename=prefix+name,include_snapshot=False,other_metadata={
          'name':'Náströnd '+name,'modeled_by':'RavenForge Luthier Intelligence','gear_type':'amp',
          'character':character,'source_commit':source['source_commit'],'status':'DIAGNOSTIC_NOT_A2_DO_NOT_SHIP' if extra else 'TEST_BUILD_PENDING_ACCEPTANCE',
          'channel':i,'cabinet_included':False,'head_bias_correction_output_units':dc})
        path=args.out/f'{prefix}{name}.nam';obj=json.loads(path.read_text());restored=init_from_nam(obj).eval()
        with torch.inference_mode():delta=float((sub(torch.from_numpy(vx[:32768]))-restored(torch.from_numpy(vx[:32768]))).abs().max())
        if not np.isfinite(delta) or delta>1e-6:raise RuntimeError('NAM roundtrip changed output')
        prediction=predict(restored,vx);stats=scores(prediction,vy[i])[0]
        windows=[]
        for s in range(rf,len(vx)-4096,4096):
            if np.mean(vy[i,s:s+4096]**2)>1e-6:windows.append(scores(prediction[s:s+4096],vy[i,s:s+4096],skip=0)[0]['esr'])
        stats.update({'selected_step':best_steps[i],'validation_window_esr_median':float(np.median(windows)),
          'validation_window_esr_p95':float(np.quantile(windows,.95)),'validation_window_esr_worst':float(max(windows)),
          'official_roundtrip_max_abs':delta,'sha256':digest(path),'head_bias_correction':dc})
        stats['tone3000_fidelity']=fidelity(prediction,vy[i],name,vx)
        results[name]=stats;np.save(args.out/f'{name}-validation.npy',prediction)
    write_json(args.out/'validation.json',{'channels':results,'thresholds':{'median_esr':.005,'p95_esr':.01,'worst_esr':.02},
      'quality_profile':PROFILE,'input':'unseen synthetic validation excitation','test':'independent comparison is a separate step','real_DI':'not supplied',
      'status':'TEST_BUILD_PENDING_ACCEPTANCE','checkpoint_step':args.steps,'identity':identity,'recipe':args.recipe,
      'crop_schedule_sha256':crop_digest.hexdigest(),'crop_schedule_start_step':start})
    print(json.dumps({'stage':'exported','channels':results}),flush=True)

if __name__=='__main__':main()
