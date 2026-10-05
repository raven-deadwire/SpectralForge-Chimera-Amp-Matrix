#!/usr/bin/env python3
"""Export calibrated A2 captures, actual TONE3000 presets, and measured reports."""
import argparse, json, shutil, subprocess, tempfile, uuid
from pathlib import Path
import numpy as np
import soundfile as sf
import torch
from nam.models._from_nam import init_from_nam
from train_a2 import predict, scores
from common import digest, write_json
from capture_channels import VOICES

def eq_for(name):
    # Real TONE3000 BlockEq roles: shelf/cut, four bells, shelf/cut.
    table={
      'Fenrir':[(100,-1,.7),(250,-1.5,1),(700,0,1),(1700,1.5,.9),(3500,.5,1.2),(8000,-.5,.7)],
      'Surtr':[(100,0,.7),(280,-.8,1),(850,1.5,.8),(1700,.5,1),(3500,-.5,1),(8000,-1,.7)],
      'Nidhoggr':[(100,-.5,.7),(280,-1,1),(750,-.5,1),(1800,1,.9),(3600,1,1.2),(8000,-.5,.7)],
      'Fimbulvetr':[(100,-.5,.7),(300,-2.5,.8),(700,0,1),(1500,2,.75),(3500,1,1),(8000,.5,.7)],
      'Ragnarok':[(90,1,.7),(250,-2,1),(650,-.5,1),(2000,1,.8),(3500,.5,1.2),(8000,-1,.7)]}
    types=['lowshelf','bell','bell','bell','bell','highshelf']
    return {'enabled':True,'pre':False,'bands':[{'type':t,'freqHz':f,'gainDb':g,'q':q} for t,(f,g,q) in zip(types,table[name])]}

def main():
    ap=argparse.ArgumentParser();ap.add_argument('--data',type=Path,required=True);ap.add_argument('--run',type=Path,required=True)
    ap.add_argument('--out',type=Path,required=True);ap.add_argument('--tool',type=Path,required=True);args=ap.parse_args()
    torch.set_num_threads(2)
    if args.out.exists() and any(args.out.iterdir()):raise RuntimeError('Package output must be empty')
    for d in ['NAM','TONE3000-Presets','Settings','Validation','Audio','Training']:(args.out/d).mkdir(parents=True,exist_ok=True)
    manifest=json.loads((args.data/'manifest.json').read_text());train=json.loads((args.run/'training-config.json').read_text())
    prior=json.loads((args.run/'validation.json').read_text());capture_pad=.1
    vx=np.load(args.data/'validation/input.npy');ax=np.load(args.data/'audition/input.npy');reports={}
    sf.write(args.out/'Audio'/'Synthetic-DI-input.wav',ax,48000,subtype='FLOAT')
    with tempfile.TemporaryDirectory() as temporary:
        temp=Path(temporary);raw=temp/'input.f32';rendered=temp/'output.f32';job_file=temp/'job.json'
        for ch,(name,character,_) in enumerate(VOICES):
            original=init_from_nam(json.loads((args.run/f'Nastrond-{name}.nam').read_text())).eval()
            # One identical -20 dB output pad on all captures gives TONE3000's
            # loudness metadata a useful range without altering drive/character.
            original._net._head_scale*=capture_pad
            meta={'name':'Náströnd '+name,'modeled_by':'RavenForge Luthier Intelligence','gear_type':'amp',
              'source_commit':manifest['source']['source_commit'],'character':character,'channel':ch,
              'capture_output_pad_db':-20,'cabinet_included':False,'source_kind':'software amp DSP',
              'status':'TEST_BUILD_PENDING_LISTENING_ACCEPTANCE'}
            original.export(args.out/'NAM',basename='Nastrond-'+name,include_snapshot=False,other_metadata=meta)
            path=args.out/'NAM'/f'Nastrond-{name}.nam';model=init_from_nam(json.loads(path.read_text())).eval()
            py=predict(model,vx);reference=np.load(args.data/'validation'/f'{name}.npy')*capture_pad
            stat=scores(py,reference)[0];windows=[]
            for s in range(model.receptive_field,len(vx)-4096,4096):
                if np.mean(reference[s:s+4096]**2)>1e-8:windows.append(scores(py[s:s+4096],reference[s:s+4096],0)[0]['esr'])
            stat.update({'window_esr_median':float(np.median(windows)),'window_esr_p95':float(np.quantile(windows,.95)),
              'window_esr_worst':float(max(windows)),'selected_training_step':prior['channels'][name]['selected_step']})
            # Actual TONE3000 NamEngine and exact A2 import gate, unnormalized.
            ax.tofile(raw);write_json(job_file,{'block_size':64,'normalize':False})
            runtime=json.loads(subprocess.check_output([str(args.tool),'render',str(path),str(raw),str(rendered),str(job_file)],text=True))
            cpp=np.fromfile(rendered,dtype='<f4');python=predict(model,ax);parity=scores(cpp,python)[0]
            max_abs=float(np.max(np.abs(cpp[model.receptive_field:]-python[model.receptive_field:])))
            parity['max_abs']=max_abs;parity['pass']=parity['residual_rms_dbfs']<=-80
            if not parity['pass']:raise RuntimeError(f'TONE3000 engine parity failed: {name}')
            np.zeros(48000,dtype='<f4').tofile(raw)
            subprocess.check_output([str(args.tool),'render',str(path),str(raw),str(rendered),str(job_file)],text=True)
            quiet=np.fromfile(rendered,dtype='<f4')[model.receptive_field:]
            silence_db=float(10*np.log10(max(float(np.mean(quiet**2)),1e-20)))
            # Encode and read back through TONE3000's own preset APIs.
            job={'name':'Nastrond '+name+' Character','id':str(uuid.uuid5(uuid.NAMESPACE_URL,'ravenforge/nastrond/a2/'+name)),
                 'block_size':64,'normalize':True,'eq':eq_for(name)}
            settings=args.out/'Settings'/f'{name}.json';write_json(settings,job)
            preset=args.out/'TONE3000-Presets'/f'Nastrond-{name}.t3kpreset'
            state=json.loads(subprocess.check_output([str(args.tool),'preset',str(path),str(preset),str(settings)],text=True))
            # Ensure real serializer did not coerce away our intended EQ type/values.
            got=state['blocks'][0]['eq']
            for expected,actual in zip(job['eq']['bands'],got['bands']):
                if expected['type']!=actual['type'] or any(abs(expected[k]-actual[k])>1e-5 for k in ['freqHz','gainDb','q']):
                    raise RuntimeError('Preset EQ changed while serializing')
            ax.tofile(raw)
            eq_runtime=json.loads(subprocess.check_output([str(args.tool),'render',str(path),str(raw),str(rendered),str(settings)],text=True))
            audition=np.fromfile(rendered,dtype='<f4')
            native=np.load(args.data/'audition'/f'{name}.npy')*capture_pad
            sf.write(args.out/'Audio'/f'{name}-Native-left-NAM-right.wav',np.stack([native,cpp],axis=1),48000,subtype='PCM_24')
            sf.write(args.out/'Audio'/f'{name}-TONE3000-EQ.wav',audition,48000,subtype='FLOAT')
            reports[name]={'character':character,'model_sha256':digest(path),'preset_sha256':digest(preset),
              'validation':stat,'python_tone3000_parity':parity,'silence_dbfs':silence_db,'engine':runtime,'preset':state,'eq_engine':eq_runtime}
            print(json.dumps({'channel':name,'validation_esr':stat['esr'],'parity_dbfs':parity['residual_rms_dbfs'],'silence_dbfs':silence_db}),flush=True)
    report={'channels':reports,'source':manifest['source'],'target_player_commit':train['target_player_commit'],
      'target_core_commit':'1f42f88535884450104b8711d7595019afa0495b','format':'NAM 0.7.0, bare A2-Full WaveNet; 5 independent captures',
      'reference_output_pad_db':-20,'eq':'TONE3000 6-band POST EQ in presets; not baked into NAM',
      'thresholds':{'window_esr_median':.005,'window_esr_p95':.01,'window_esr_worst':.02,'rms_error_db':.5,'peak_error_db':1,'silence_dbfs':-80},
      'final_test':'reserved; validation-based test build','real_DI_listening':'not performed: no recorded instrument DI available',
      'actual_GUI_DAW_test':'not performed; unmodified engine, EQ, A2 import predicate and preset APIs tested offline',
      'status':'TEST_BUILD_PENDING_ACCEPTANCE'}
    report['numerical_accuracy_pass']=all(r['validation']['window_esr_median']<=.005 and r['validation']['window_esr_p95']<=.01 and r['validation']['window_esr_worst']<=.02 and abs(r['validation']['rms_error_db'])<=.5 and abs(r['validation']['peak_error_db'])<=1 and r['silence_dbfs']<=-80 for r in reports.values())
    write_json(args.out/'Validation'/'validation.json',report)
    shutil.copy2(args.data/'channel-settings.json',args.out/'Settings'/'native-channel-settings.json')
    shutil.copy2(args.data/'manifest.json',args.out/'Validation'/'capture-manifest.json')
    for file in ['checkpoint.pt','training-config.json','training-stages.json']:
        if (args.run/file).exists():shutil.copy2(args.run/file,args.out/'Training'/file)
    # Text intentionally distinguishes a test build from release approval.
    lines=['# Náströnd — TONE3000 A2 테스트 패키지','',
      '채널 캐릭터를 강조한 독립 NAM 5개와 TONE3000 파라메트릭 EQ 프리셋 5개입니다. 앰프 헤드 캡처이며 캐비닛 IR은 포함하지 않습니다.','',
      '## 사용 방법','',
      '1. `NAM` 폴더의 `.nam` 파일을 TONE3000의 빈 블록으로 드래그합니다. 폴더 전체를 넣으면 Model에서 5개 채널을 선택할 수 있습니다.',
      '2. 앰프 뒤에 원하는 캐비닛 IR을 추가합니다.',
      '3. EQ까지 적용하려면 `TONE3000-Presets`의 `.t3kpreset` 파일을 TONE3000의 사용자 Presets 폴더에 복사하고 플러그인 창을 다시 엽니다.',
      '4. Your Presets에서 채널을 선택합니다. NAM 데이터와 6밴드 POST EQ가 프리셋 안에 포함되어 있어 오프라인으로 불러올 수 있습니다.','',
      '프리셋 위치: Windows `%APPDATA%/TONE3000/Presets`, macOS `~/Library/Application Support/TONE3000/Presets`, Linux `~/.config/TONE3000/Presets`.','',
      '입력·출력은 0 dB, Mix는 100%, Normalize는 켜져 있습니다. 글로벌 EQ·피치·스프레드·게이트는 꺼져 있습니다. A2-Full 단일 모델이며 별도의 Lite 모델은 포함하지 않습니다.','',
      '## 채널','',
      '| 파일 | 강조한 캐릭터 |','| --- | --- |',
      '| Fenrir | 타이트한 저역과 선명한 어택·클랭크 |',
      '| Surtr | 조밀한 포화감과 앞으로 나오는 중음 |',
      '| Nidhoggr | 강한 ROT와 거친 비대칭 그라인드 |',
      '| Fimbulvetr | 넓은 블룸과 서스테인, 먹먹함을 줄인 상중역 |',
      '| Ragnarok | 깊은 저역 타격과 단단한 반응 |','',
      'NAM에는 각 채널의 고정 앰프 세팅이 담겨 있습니다. 추가 조절은 TONE3000의 입력 게인과 파라메트릭 EQ로 합니다. Chimera의 13개 노브를 NAM 내부에서 가변 재현하는 모델은 아닙니다.','',
      '## 검증 상태','',
      'TONE3000의 A2 판별, 실제 재생 엔진, EQ 및 프리셋 저장/복원 코드로 검증했습니다. 아직 테스트 빌드이며, 실제 악기 DI 청취와 GUI/DAW 실사용 검증은 완료되지 않았습니다.','',
      '| 채널 | 전체 검증 ESR | 구간 ESR 중앙값 | 구간 ESR p95 |','| --- | ---: | ---: | ---: |']
    for name,r in reports.items():
        v=r['validation'];lines.append(f'| {name} | {v["esr"]:.6f} | {v["window_esr_median"]:.6f} | {v["window_esr_p95"]:.6f} |')
    lines+=['','ESR은 낮을수록 오차가 작습니다. “음색 유사도 퍼센트”를 뜻하지 않습니다. 합성 신호로 학습·검증했으며, 최종 테스트 신호는 아직 사용하지 않았습니다.',
      '',f'설계상 수치 기준 전체 통과: {"예" if report["numerical_accuracy_pass"] else "아니오 — 정식 릴리즈 승인 전 추가 개선 필요"}. 상세 수치와 미완료 항목은 `Validation/validation.json`에 있습니다.','',
      '`Audio/*Native-left-NAM-right.wav`는 왼쪽 원본 DSP, 오른쪽 NAM 비교입니다. `*TONE3000-EQ.wav`는 Normalize와 채널 EQ를 적용한 합성 테스트 음원입니다. 모두 캐비닛 없는 앰프 신호입니다.','',
      '캡처는 Chimera v1.2.0-beta.1의 실제 4배 오버샘플링 앰프 경로를 사용했습니다. 원본의 지연을 유지해 인과적인 학습 대상으로 만들었습니다. 모델의 출력에 동일한 -20 dB 패드를 두어 헤드룸을 확보했으며, 이 스케일은 학습 검증 기준에도 동일하게 적용했습니다. 입력 드라이브는 바꾸지 않았습니다.','',
      'TONE3000 문서: https://www.tone3000.com/guides/tone3000-plugin',
      'NAM 학습기: https://github.com/sdatkinson/neural-amp-modeler',
      'TONE3000 플레이어: https://github.com/tone-3000/tone3000-plugin','',
      'Náströnd/Chimera: RavenForge Luthier Intelligence. NAM 및 TONE3000과의 제휴·공식 인증을 뜻하지 않습니다.']
    (args.out/'README-KO.md').write_text('\n'.join(lines)+'\n')
    sums=[f'{digest(p)}  {p.relative_to(args.out).as_posix()}' for p in sorted(args.out.rglob('*')) if p.is_file()]
    (args.out/'SHA256SUMS').write_text('\n'.join(sums)+'\n')
    print(json.dumps({'package':str(args.out),'numerical_accuracy_pass':report['numerical_accuracy_pass']}),flush=True)

if __name__=='__main__':main()
