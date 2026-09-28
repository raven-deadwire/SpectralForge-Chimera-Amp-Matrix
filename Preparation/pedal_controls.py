"""Hardware-panel targets for the 25 existing PRE variants.

Definitions only: hw.* positions never drive the legacy DSP or overwrite its raw
parameters. A reviewed panel is NOT a calibrated circuit/capture certification.
Historical panels without an inspected primary source remain explicitly pending.
"""
from copy import deepcopy


def knob(key, label, note='', group='PANEL'):
    return dict(id='hw.'+key, label=label, kind='knob', minimum=0, maximum=1,
                initial=.5, step=.01, unit='position', group=group,
                range_basis='normalized_panel_position_not_measured_taper',
                dsp_binding=None, note=note)


def select(key, label, options, initial=0, note='', group='PANEL'):
    return dict(id='hw.'+key, label=label, kind='choice', options=options,
                initial=initial, group=group, range_basis='panel_switch_positions',
                dsp_binding=None, note=note)


def toggle(key, label, initial=0, note='', group='PANEL'):
    return dict(id='hw.'+key, label=label, kind='toggle', initial=initial,
                group=group, range_basis='panel_switch_state', dsp_binding=None, note=note)


def knobs(labels, notes=None, group='PANEL'):
    notes=notes or {}
    return [knob(label.lower().replace(' ','_').replace('.',''), label, notes.get(label,''), group) for label in labels]


def definition(reference, controls, sources, note='', reviewed=True, meters=None):
    return dict(reference=reference, controls=controls, sources=sources,
                review_status='primary_panel_reviewed' if reviewed else 'primary_panel_confirmation_pending',
                reviewed_on='2026-09-28', note=note,
                scope='control_names_and_configuration_only',
                circuit_response_verified=False, meters=meters or [],
                exclusions='Power, battery, physical jacks and hardware bypass are not additional tone knobs. Board bypass stays separate.')


def hardware_panels():
    d={}
    def put(family,index,*args,**kwargs): d[f'legacy.{family}.{index}']=definition(*args,**kwargs)
    put('drive',0,'Ibanez TS808',knobs(['OVER DRIVE','TONE','LEVEL']),
        ['https://www.ibanez.com/usa/products/detail/ts808_99.html'])
    put('drive',1,'Klon Centaur',knobs(['GAIN','TREBLE','OUTPUT']),[],
        'GAIN/TREBLE/OUTPUT target; original primary panel/manual confirmation remains open. No independent Blend knob.',reviewed=False)
    put('drive',2,'Pro Co RAT 2',knobs(['DISTORTION','FILTER','VOLUME'],{'FILTER':'Target: clockwise darker. Transfer function and primary manual remain to be checked.'}),[],
        'RAT 2 is the proposed revision, not evidence identifying the old generic Rodent algorithm.',reviewed=False)
    put('drive',3,'Tech 21 SansAmp Bass Driver DI v2',
        knobs(['LEVEL','BLEND','TREBLE','MID','BASS','PRESENCE','DRIVE'],{'BLEND':'Independent dry/SansAmp mixture; not fixed at one factory value.'})+
        [select('mid_shift','MID SHIFT',['500 Hz','1 kHz']),select('bass_shift','BASS SHIFT',['40 Hz','80 Hz'])],
        ['https://www.tech21nyc.com/products/sansamp-2/bassdriver-di/',
         'https://www.tech21nyc.com/wp-content/uploads/2023/11/BSDR_v2_OM4.pdf'],
        'Explicit v2 target. v1 has no MID or frequency-shift switches. XLR/quarter-inch pads and phantom/ground wiring excluded; no fictional CAB bypass.')
    put('drive',4,'Darkglass Microtubes B3K v2',
        knobs(['BLEND','TONE','LEVEL','DRIVE'],{'LEVEL':'Distorted path level; clean level is not the same control.'})+
        [toggle('grunt','GRUNT'),toggle('mid_boost','MID BOOST')],
        ['https://www.darkglass.com/en-int/products/microtubes-b3k'],
        'v2 has TONE and MID BOOST; do not add the older revision ATTACK selector. Model/capture revision still needs matching.')
    put('comp',0,'MXR M87 Bass Compressor',
        [knob('release','RELEASE','매뉴얼의 시계방향 설명이 상충합니다. 실제 테이퍼를 임의로 정하지 않았습니다.'),
         knob('attack','ATTACK','공식 범위 20–800 μs. 이 위치 값에 시간 곡선을 임의로 대응시키지 않았습니다.'),
         knob('output','OUTPUT'),select('ratio','RATIO',['4:1','8:1','12:1','20:1']),knob('input','INPUT')],
        ['https://www.jimdunlop.com/content/manuals/M87.pdf'],
        'Five original controls, not Amount/Attack/Level. The name Studio VCA is a legacy Chimera label, not a circuit identity certification.',
        meters=['10-segment gain-reduction display; no signal is simulated in this preview'])
    put('comp',1,'MXR Dyna Comp M102',knobs(['OUTPUT','SENSITIVITY']),
        ['https://www.jimdunlop.com/content/manuals/M102.pdf'],
        'Two controls. No hardware Attack/Release knob; legacy attack is retained only in the old-value store.')
    put('comp',2,'Diamond Compressor CPR-1',
        knobs(['COMP','EQ','VOL'],{'EQ':'Tilt balance, center flat; not a simple treble-only control.'})+
        [toggle('eq_in','EQ IN',1,group='INTERNAL'),toggle('hi_cut','4.8 kHz HI-CUT',group='INTERNAL')],
        ['https://www.diamondpedals.com/pages/legacy-support',
         'https://cdn.shopify.com/s/files/1/0716/1387/4479/files/Manual-Diamond_Compressor.pdf?v=1678928483'],
        'CPR-1 selected explicitly as the original three-knob target. Current COMP/EQ is a different panel (COMP/LEVEL/MIDS/TILT/ATTACK) and is not silently mixed in. Old DSP provenance remains unspecified.',
        meters=['Bi-colour compression indicator; inactive without audio'])
    put('comp',3,'Origin Effects Cali76 FET Compressor (2024)',
        knobs(['IN','OUT','DRY','RATIO','ATTACK','RELEASE'],{'DRY':'Adds original dry level; not a constant-power wet/dry crossfade.',
              'RATIO':'Continuously variable 4:1–20:1; unlike M87 it is not a four-position switch.'}),
        ['https://origineffects.com/product/cali76-fet-compressor/'],
        '2024 six-knob FET version is the explicit target, not every Cali76/1176 revision.',
        meters=['10-segment gain-reduction display; inactive without audio'])
    mu=[knob('dual_input','DUAL INPUT',group='GLOBAL'),select('link','STEREO LINK',['SEP','LINK'],1,group='GLOBAL')]
    for ch in ('L','R'):
        mu += [knob(ch.lower()+'.threshold','THRESHOLD',group=ch),knob(ch.lower()+'.attack','ATTACK',group=ch),
               select(ch.lower()+'.recovery','RECOVERY',['8 s','4 s','0.6 s','0.4 s','0.2 s'],2,group=ch),
               knob(ch.lower()+'.output','OUTPUT',group=ch),select(ch.lower()+'.mode','MODE',['COMPRESS','LIMIT'],group=ch)]
    put('comp',4,'Manley Stereo Variable Mu — original linked-input panel',mu,
        ['https://www.manley.com/products/pro-audio/dynamics/variable-mu',
         'https://mlabs2017.squarespace.com/s/Manley-Stereo-Variable-Mu-Limiter-Compressor-Session-Recall-Sheet.pdf'],
        'A rack reference adapted to one PRE slot, not a physical pedal. Older continuous DUAL INPUT layout selected; newer stepped input/HP-SC revision is not combined. Link detector semantics and channel bypass DSP remain pending.',
        meters=['Left and right gain-reduction meters; inactive without audio'])
    put('filter',0,'EHX Q-Tron — original big-box panel',
        [select('mode','MODE',['LP','BP','HP','MIX']),select('sweep','SWEEP',['UP','DOWN']),
         select('range','RANGE',['HI','LO'],1),knob('peak','PEAK'),knob('gain','GAIN'),toggle('boost','BOOST')],
        ['https://www.ehx.com/products/q-tron/',
         'https://b2155914.assetcdn.net/2155914/wp-content/uploads/2023/05/Picture21-1.jpg'],
        'Manufacturer photo confirms MIX as fourth MODE position and SWEEP label. BOOST is on/off despite its rotary physical shape; not a continuous Mix knob.')
    for index,mode,drive in ((1,1,0),(2,0,1)):
        put('filter',index,'Musitronics Mu-Tron III — vintage target',
            knobs(['GAIN','PEAK'])+[select('mode','MODE',['LP','BP','HP'],mode),
            select('range','RANGE',['LO','HI']),select('drive','DRIVE',['UP','DOWN'],drive)],
            ['https://mu-tron.com/'],
            'Original five-control target still requires vintage primary manual confirmation. Current compact Mu-Tron III adds LEVEL; do not mix revisions. Tron Band and Reverse Sweep share a hardware family and differ in starting MODE/DRIVE.',reviewed=False)
    put('filter',3,'MXR M82 Bass Envelope Filter',
        knobs(['DRY','FX','DECAY','Q','SENS.'],{'DRY':'Independent dry level, not inverse FX mix.',
              'FX':'Independent effect level.', 'DECAY':'Decay STOP FREQUENCY: 76 Hz–1.3 kHz. 시간 조절이 아닙니다.'}),
        ['https://www.jimdunlop.com/content/manuals/M82.pdf'])
    aw=knobs(['DECAY','MANUAL','SENS'])+[select('mode','MODE',['UP','DOWN','SHARP','HUMAN','TEMPO'])]
    aw[1]['mode_labels']={'selector':'hw.mode','labels':{'3':'VOWEL 2'}}
    aw[2]['mode_labels']={'selector':'hw.mode','labels':{'3':'VOWEL 1'}}
    put('filter',4,'BOSS AW-3',aw,
        ['https://www.boss.info/global/products/aw-3/',
         'https://static.roland.com/assets/images/products/main/aw-3_top_main.jpg'],
        'Four physical controls. HUMAN relabels MANUAL to VOWEL 2 and SENS to VOWEL 1 on the same stored knobs. Mode timing/vowel mapping and EXP/GUITAR/BASS routing are not implemented by this preview.')
    put('fuzz',0,'EHX Big Muff Pi — NYC',knobs(['VOLUME','TONE','SUSTAIN']),
        ['https://www.ehx.com/products/big-muff-pi/'])
    put('fuzz',1,'Dunlop Fuzz Face JDF2',knobs(['VOLUME','FUZZ']),
        ['https://www.jimdunlop.com/content/manuals/JDF2.pdf'],
        'No Tone control. JDF2 germanium is the explicit panel target; pickup loading is not simulated.')
    put('fuzz',2,'Sola Sound Tone Bender Professional MKII — target',knobs(['LEVEL','ATTACK']),
        ['https://macaris.co.uk/product-category/colorsound/'],
        'MKII two-knob target is provisional until the original panel is verified. Do not conflate MKIII/IV Tone controls or pretend ATTACK is a compressor time control.',reviewed=False)
    put('fuzz',3,'ZVEX Woolly Mammoth',knobs(['VOL','EQ','PINCH','WOOL']),
        ['https://www.zvex.com/guitar-pedals/woolly-mammoth-guitar-effects-pedal',
         'https://static1.squarespace.com/static/555e332ce4b0577e788c3a16/t/56329249e4b0b94d8f649311/1446154825093/ZVEX%2BWoolly%2BMammoth%2BInstructions.pdf'],
        'PINCH pulse-width/gating and WOOL fuzz must not be collapsed into a generic Tone/Drive pair.')
    put('fuzz',4,'ZVEX Fuzz Factory — five-knob',knobs(['VOLUME','GATE','COMP','DRIVE','STAB']),
        ['https://www.zvex.com/guitar-pedals/fuzz-factory-guitar-effects-pedal',
         'https://images.squarespace-cdn.com/content/v1/555e332ce4b0577e788c3a16/1530208977456-YTYPEWLPZF90YRMOO22R/ZVEX_Fuzz_Factory.jpg'],
        'Manufacturer photo confirms all five labels. GATE/COMP/STAB interaction and oscillation remain DSP acceptance work; no generic Tone control.')
    put('boost',0,'Xotic RC Booster — original four-knob',knobs(['GAIN','VOLUME','TREBLE','BASS']),
        ['https://xotic.us/manuals/pdf/RC_Booster_manual.pdf'],
        'Original RC, not RC Booster V2 dual-gain. Separate Gain and Volume; active two-band EQ.')
    put('boost',1,'Dallas Rangemaster — vintage target',knobs(['SET']),[],
        'Single SET/output-level target. Vintage primary photo/manual still pending. Hardware on/off is represented by board bypass; no Bass/Treble controls.',reviewed=False)
    put('boost',2,'MXR Micro Amp M133',knobs(['GAIN']),
        ['https://www.jimdunlop.com/mxr-micro-amp/'],
        'M133 one knob, not Micro Amp+ with added EQ.')
    put('boost',3,'Xotic EP Booster',knobs(['GAIN'])+
        [toggle('gain_dip','+3 dB GAIN',1,group='INTERNAL'),
         toggle('bright_dip','BRIGHT / FLAT EQ',1,note='ON은 매뉴얼에 기재된 평탄한 EQ 설정입니다.',group='INTERNAL')],
        ['https://xotic.us/effects/ep-booster/', 'https://xotic.us/effects/ep-booster/pdf/ep_booster_manual.pdf'],
        'One top-panel knob; two DIP switches under INTERNAL. Factory switch states are documented; normalized knob seeds are not factory defaults.')
    put('boost',4,'EHX LPB-1 — Nano',knobs(['BOOST']),
        ['https://www.ehx.com/products/lpb-1/'],
        'One BOOST knob. Do not borrow EQ controls from LPB-3.')
    notes_ko={'legacy.drive.0': '원본 OVER DRIVE·TONE·LEVEL로 구성했습니다. 기존 Tone의 Hz 값을 원본 Tone의 노브 위치로 환산하지 않습니다.', 'legacy.drive.1': 'GAIN·TREBLE·OUTPUT 대상 정의입니다. 원본 1차 자료 확인은 남아 있으며 별도 Blend 노브를 추가하지 않았습니다.', 'legacy.drive.2': 'RAT 2를 대상 리비전으로 명시했습니다. FILTER·VOLUME을 공통 Tone·Level로 대체하지 않으며, 원본 매뉴얼 확인은 남아 있습니다.', 'legacy.drive.3': 'v2의 7개 노브와 두 주파수 전환을 구분했습니다. v1과 혼용하지 않으며 물리 출력 패드·팬텀/접지 제어는 이번 패널에서 제외했습니다.', 'legacy.drive.4': 'v2의 4개 노브·GRUNT·MID BOOST를 정의했습니다. 구형 ATTACK 스위치를 함께 넣지 않았고 기존 DSP의 리비전 일치는 별도 검증입니다.', 'legacy.comp.0': 'INPUT·OUTPUT·ATTACK·RELEASE와 4단 RATIO입니다. Studio VCA는 기존 키메라 이름이며 M87의 회로 방식을 인증하는 표기가 아닙니다.', 'legacy.comp.1': 'OUTPUT·SENSITIVITY 두 노브입니다. 원본에 없는 Attack은 전용 조작부에서 제거하고 옛 상태 값으로만 보존했습니다.', 'legacy.comp.2': '원형 CPR-1의 COMP·EQ·VOL과 내부 EQ/Hi-cut 스위치입니다. 현행 COMP/EQ의 MIDS·TILT·ATTACK을 섞지 않았습니다. 기존 DSP가 어느 리비전을 참고했는지는 별도 확인이 필요합니다.', 'legacy.comp.3': '2024 Cali76 FET의 IN·OUT·DRY·RATIO·ATTACK·RELEASE입니다. DRY는 원음 레벨을 추가하는 기능이며 단순한 반대 방향 Wet/Dry 크로스페이드로 가정하지 않습니다.', 'legacy.comp.4': '물리 페달이 아니라 스테레오 랙 레퍼런스를 한 PRE 슬롯에서 다루는 정의입니다. 구형 공통 연속 INPUT 패널과 좌우 제어를 선택했고, 현행 단계형 INPUT·HP SC와 혼용하지 않았습니다.', 'legacy.filter.0': '제조사 사진에서 MIX를 포함한 4단 MODE와 SWEEP·RANGE·BOOST를 확인했습니다. BOOST는 회전식 외형이지만 ON/OFF이며 연속적인 Mix 노브가 아닙니다.', 'legacy.filter.1': '빈티지 Mu-Tron III의 5개 조작부를 대상으로 한 임시 정의입니다. 현행 제품의 LEVEL을 섞지 않았으며 빈티지 원본 매뉴얼 확인은 남아 있습니다.', 'legacy.filter.2': 'Tron Band와 같은 하드웨어 계열이며 MODE·DRIVE의 시작 설정을 다르게 둡니다. 빈티지 원본 매뉴얼의 최종 확인은 남아 있습니다.', 'legacy.filter.3': 'DRY·FX를 독립 레벨로 분리했습니다. DECAY는 릴리스 시간이 아니라 감쇄 시 멈추는 주파수를 조절하는 기능입니다.', 'legacy.filter.4': '물리 조작부는 4개입니다. HUMAN을 고르면 같은 MANUAL·SENS가 VOWEL 2·VOWEL 1로 표시됩니다. 모드별 추적·모음 반응과 외부 입력/EXP는 아직 DSP에 연결되지 않았습니다.', 'legacy.fuzz.0': 'NYC Big Muff Pi의 VOLUME·TONE·SUSTAIN으로 표시합니다. 원래 Sustain을 공통 Drive로 바꾸지 않았습니다.', 'legacy.fuzz.1': 'JDF2의 VOLUME·FUZZ 두 노브입니다. 원본에 없는 Tone을 제거했으며 픽업 부하 반응은 시제품에서 구현하지 않습니다.', 'legacy.fuzz.2': 'Professional MKII의 LEVEL·ATTACK을 대상 정의로 두었습니다. 원본 패널 확인은 남아 있으며 MKIII/IV의 Tone을 섞거나 Attack을 컴프 시간으로 취급하지 않습니다.', 'legacy.fuzz.3': 'VOL·EQ·PINCH·WOOL입니다. PINCH의 게이팅/파형 반응을 공통 Tone·Drive에 합치지 않습니다.', 'legacy.fuzz.4': '제조사 사진의 VOLUME·GATE·COMP·DRIVE·STAB 다섯 노브입니다. Tone은 제거했으며 노브 간 상호작용과 발진은 별도 DSP 검증 대상입니다.', 'legacy.boost.0': '원형 RC Booster의 GAIN·VOLUME·TREBLE·BASS입니다. RC Booster V2의 추가 게인 채널을 섞지 않았습니다.', 'legacy.boost.1': '단일 SET 노브를 대상으로 합니다. 원본 1차 자료 확인은 남아 있으며 Bass·Treble 노브는 제거했습니다.', 'legacy.boost.2': 'M133의 단일 GAIN 노브입니다. EQ가 있는 Micro Amp+와 구분했습니다.', 'legacy.boost.3': '전면 GAIN 하나와 내부 DIP 두 개를 구분했습니다. DIP의 출고 상태와 달리 연속 노브의 50%는 시제품 초기 위치일 뿐입니다.', 'legacy.boost.4': '단일 BOOST 노브입니다. LPB-3의 EQ 조절부를 가져오지 않았습니다.'}
    for mid,note in notes_ko.items(): d[mid]["note_ko"]=note
    return d


def upgrade_pedals(models):
    definitions=hardware_panels()
    for model in models:
        if model['id'] not in definitions: continue
        panel=deepcopy(definitions[model['id']])
        model['legacy_controls']=deepcopy(model['controls'])
        model['controls']=panel.pop('controls')
        model['hardware_panel']=panel
        model['controls_status']='original_panel_dsp_pending' if panel['review_status']=='primary_panel_reviewed' else 'original_panel_provisional_dsp_pending'
        model['control_schema']='hardware-panel-v2'
        model['sources']=list(dict.fromkeys(model['sources']+panel['sources']))
        model['legacy_mapping']='Separate raw store: no automatic equivalence between legacy and hw.* parameters.'
    return models
