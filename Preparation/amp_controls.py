#!/usr/bin/env python3
"""Amp panel targets, NOT circuit models or production host parameters.

All continuous hardware values are normalized knob POSITIONS, not inferred dB,
Hz, time constants, measured tapers, or factory defaults. Shared hardware controls
have one key, even when several channels use them. Incomplete primary evidence
is recorded explicitly; a working UI must not promote it to verified audio.
"""
from __future__ import annotations
from copy import deepcopy

REVIEWED = 'primary_core_reviewed'
PARTIAL = 'partial_primary_review'
PENDING = 'primary_panel_pending'


def knob(key, label, group='PANEL', channels=(), initial=.5, note=''):
    return dict(id=key, label=label, kind='knob', minimum=0, maximum=1,
                initial=initial, step=.01, unit='position', group=group,
                channels=list(channels), range_basis='normalized_position_not_hardware_taper',
                dsp_binding=None, origin='hardware_target', note=note)


def choice(key, label, options, group='PANEL', channels=(), initial=0, note=''):
    return dict(id=key, label=label, kind='choice', options=options, initial=initial,
                group=group, channels=list(channels), range_basis='discrete_labels',
                dsp_binding=None, origin='hardware_target', note=note)


def toggle(key, label, group='PANEL', channels=(), initial=0, note=''):
    return dict(id=key, label=label, kind='toggle', initial=initial, group=group,
                channels=list(channels), range_basis='binary_state', dsp_binding=None,
                origin='hardware_target', note=note)


def knobs(prefix, names, group='PANEL', channels=()):
    """names: whitespace-separated safe keys, or (key, printed label) pairs."""
    pairs = [(k, k.replace('_', ' ').upper()) for k in names.split()] if isinstance(names, str) else names
    return [knob(prefix + k, label, group, channels) for k, label in pairs]


def panel(reference, controls, sources, channels=None, initial=None, review=REVIEWED,
          note='', omitted='', scope='', routes=None):
    ch = channels or [('single', 'SINGLE')]
    return dict(reference=reference, controls=controls, sources=sources,
                channels=[dict(id=k, label=v) for k, v in ch],
                initial_channel=initial or ch[0][0], review_status=review,
                note_ko=note, omissions=omitted or 'Mains/standby, service bias and physical connector wiring are not simulated.',
                reference_scope=scope or 'Panel target only. Existing fixed NAM references do not validate all controls/channels.',
                input_routes=routes or ['INPUT'],
                route_basis='Software input-routing selector, not an added hardware knob.',
                circuit_response_verified=False, capture_approved=False,
                production_dsp_connected=False, review_date='2026-09-28')


def amp_panels():
    p = {}
    twin = []
    for ch in ('normal', 'vibrato'):
        twin += knobs(f'hw.{ch}.', 'volume treble middle bass', ch.upper(), [ch])
        twin += [toggle(f'hw.{ch}.bright', 'BRIGHT', ch.upper(), [ch])]
    twin += knobs('hw.vibrato.', 'reverb speed intensity', 'VIBRATO / FX', ['vibrato'])
    p['legacy.amp.0'] = panel("Fender '65 Twin Reverb reissue", twin,
        ['https://www.fender.com/products/65-twin-reverb'], [('normal','NORMAL'),('vibrato','VIBRATO')],
        note='채널별 Volume·3밴드·Bright를 구분. VIBRATO 채널의 Reverb/Speed/Intensity는 앰프 내장 효과 대상이며 POST와 별개. 원본에 없는 Gain/Master를 추가하지 않습니다.',
        routes=['INPUT 1', 'INPUT 2'])

    p['legacy.amp.1'] = panel('Marshall JTM45 2245 reissue',
        knobs('hw.', [('high_treble','LOUDNESS I / HIGH TREBLE'),('normal','LOUDNESS II / NORMAL'),
                     ('treble','TREBLE'),('middle','MIDDLE'),('bass','BASS'),('presence','PRESENCE')]),
        ['https://www.marshall.com/us/en/product/jtm45-2245-vintage-reissue-head'],
        note='별도 Master가 없는 두 입력 볼륨 구조. 점퍼/입력 선택은 소프트웨어 라우팅이며 회로 부하·상호작용은 미구현입니다.',
        routes=['HIGH TREBLE HIGH','HIGH TREBLE LOW','NORMAL HIGH','NORMAL LOW','JUMPED — routing target'])

    peavey = knobs('hw.', 'low mid high resonance presence', 'SHARED EQ / POWER')
    for ch in ('rhythm','lead'):
        peavey += knobs(f'hw.{ch}.', [('pre_gain','PRE GAIN'),('post_gain','POST GAIN')],ch.upper(),[ch])
    peavey += [toggle('hw.rhythm.bright','BRIGHT','RHYTHM',['rhythm']),toggle('hw.rhythm.crunch','CRUNCH','RHYTHM',['rhythm'])]
    p['legacy.amp.2'] = panel('Peavey 6505 1992 Original — not 6505 II/Plus',peavey,
        ['https://peavey.com/collections/amplifiers/products/6505-1992-original-guitar-amp-head'],
        [('rhythm','RHYTHM'),('lead','LEAD')],initial='lead',
        note='RHYTHM/LEAD의 Pre/Post Gain은 독립. Low/Mid/High·Resonance·Presence는 실제처럼 공유합니다. 기존 Lead 캡처로 Rhythm 전체를 검증했다고 하지 않습니다.',routes=['HIGH GAIN INPUT','LOW GAIN INPUT'])

    rect = []
    for ch, modes in [('ch1',['CLEAN','PUSHED']),('ch2',['RAW','VINTAGE','MODERN']),('ch3',['RAW','VINTAGE','MODERN'])]:
        rect += knobs(f'hw.{ch}.','gain treble mid bass presence master',ch.upper(),[ch])
        rect += [choice(f'hw.{ch}.mode','MODE',modes,ch.upper(),[ch])]
    rect += knobs('hw.','output solo','SHARED OUTPUT')
    rect += [choice('hw.power_feel','POWER FEEL',['BOLD','SPONGY'],'POWER')]
    p['legacy.amp.3'] = panel('Mesa Dual Rectifier — three-channel core / Multi-Watt revision pending',rect,
        ['https://mesa-boogie.imgix.net/media/User%20Manuals/3chRecto.pdf'],
        [('ch1','CH1'),('ch2','CH2'),('ch3','CH3')],initial='ch3',review=PARTIAL,
        note='3채널 핵심 패널은 제조사 매뉴얼 기준. 기존 NAM은 Multi-Watt 자료이므로 정확한 세대 대조가 남아 있습니다. 미확인 전력/정류 스위치를 임의 추가하지 않았습니다.',
        omitted='Exact Multi-Watt 50/100 W and rectifier assignments, loop-bypass/output interaction and rear selectors require the matching revision manual.')

    mark = knobs('hw.rhythm.', 'bass middle','RHYTHM SHARED',['r1','r2'])
    for ch in ('r1','r2'):
        mark += knobs(f'hw.{ch}.','gain treble presence master',ch.upper(),[ch])
    mark += [toggle('hw.r1.bright','GAIN PULL BRIGHT','R1',['r1']),toggle('hw.r2.fat','GAIN PULL FAT','R2',['r2']),toggle('hw.r2.presence_shift','PRESENCE PULL SHIFT','R2',['r2'])]
    mark += knobs('hw.lead.','gain treble bass middle drive presence master','LEAD',['lead'])
    mark += [toggle('hw.lead.fat','GAIN PULL FAT','LEAD',['lead']),toggle('hw.lead.bright','DRIVE PULL BRIGHT','LEAD',['lead']),toggle('hw.lead.presence_shift','PRESENCE PULL SHIFT','LEAD',['lead'])]
    mark += [knob('hw.geq.'+hz,hz+' Hz','GRAPHIC EQ') for hz in ('80','240','750','2200','6600')]
    mark += [choice('hw.geq.mode','EQ MODE',['OFF','ON','AUTO R2'],'GRAPHIC EQ'),knob('hw.output','OUTPUT LEVEL','SHARED OUTPUT'),toggle('hw.silent_recording','OUTPUT PULL SILENT RECORDING','SHARED OUTPUT')]
    mark += [choice('hw.power','POWER',['FULL','TWEED'],'POWER'),choice('hw.triode','OUTPUT TUBES',['TRIODE','PENTODE'],'POWER'),choice('hw.class','POWER CLASS',['SIMUL-CLASS','CLASS A'],'POWER'),choice('hw.lead.voicing','LEAD VOICING',['HARMONICS','MID GAIN'],'POWER',['lead']),knob('hw.reverb','REVERB','REAR / FX')]
    p['legacy.amp.4'] = panel('Mesa Mark IV — manual panel / Lead reference',mark,
        ['https://mesa-boogie.imgix.net/media/User%20Manuals/Mark%204.pdf'],
        [('r1','RHYTHM 1'),('r2','RHYTHM 2'),('lead','LEAD')],initial='lead',
        note='R1/R2 Bass·Middle 공유, 별도 Lead Gain/Drive, 5밴드 Graphic EQ와 Pull 기능을 분리. 기존 비교는 Lead 고정 설정이므로 다른 채널/스위치의 소리 검증은 별도입니다.',
        omitted='External switching/loop assignment and recording jack paths are not connected; Mark IV revision and each power mode require capture matching.')

    vr = knobs('hw.ch1.','volume treble midrange bass','CHANNEL ONE',['ch1'])
    vr += [toggle('hw.ch1.ultra_hi','ULTRA-HI','CHANNEL ONE',['ch1']),choice('hw.ch1.mid_frequency','MID FREQUENCY',['1 · 220 Hz','2 · 800 Hz','3 · 3 kHz'],'CHANNEL ONE',['ch1']),choice('hw.ch1.low_mode','LOW MODE',['BASS CUT','OFF','ULTRA-LO'],'CHANNEL ONE',['ch1'],initial=1)]
    vr += knobs('hw.ch2.','volume treble bass','CHANNEL TWO',['ch2'])
    vr += [toggle('hw.ch2.ultra_hi','ULTRA-HI','CHANNEL TWO',['ch2']),toggle('hw.ch2.ultra_lo','ULTRA-LO','CHANNEL TWO',['ch2'])]
    p['legacy.amp.5'] = panel('Ampeg SVT-VR',vr,
        ['https://ampeg.com/data/6/0a020a3f585165fb4f5ea3277/application/pdf/'],
        [('ch1','CHANNEL ONE'),('ch2','CHANNEL TWO')],
        note='CH1은 중역과 3단 주파수 선택, CH2는 Treble/Bass 구성입니다. SVT-CL의 5단 미드나 Gain/Master를 섞지 않습니다.',routes=['NORMAL INPUT','BRIGHT INPUT'])

    gk = knobs('hw.','volume treble hi_mid lo_mid bass boost','INPUT / EQ')
    gk += [toggle('hw.pad','INPUT -10 dB','INPUT / EQ'),toggle('hw.low_cut','LOW CUT','VOICING'),toggle('hw.mid_cut','MID CUT','VOICING'),toggle('hw.high_boost','HIGH BOOST','VOICING')]
    gk += knobs('hw.','low_master high_master crossover','BI-AMP TARGET')
    gk += [toggle('hw.biamp','BI-AMP','BI-AMP TARGET')]
    p['legacy.amp.6'] = panel('Gallien-Krueger 800RB — bi-amp original',gk,
        ['https://www.manualslib.com/manual/253056/Gallien-Krueger-800rb.html?page=2','https://www.manualslib.com/manual/253056/Gallien-Krueger-800rb.html?page=3'],review=PARTIAL,
        note='제조사 원문 매뉴얼의 재게시본에서 4밴드 EQ·Boost/Master·-10 dB 입력·크로스오버와 Lo Cut/Mid Cut/Hi Boost를 확인. 전체 전면 배치·스위치 대조는 추가 확인 대상입니다. 일반적인 3밴드 앰프로 바꾸지 않습니다.',
        omitted='Exact voicing switch labels/ranges and separate HF/LF power-output routing still require full original-panel inspection.')

    hybrid = knobs('hw.b7k.','master blend level drive bass lo_mids hi_mids treble','B7K ULTRA')
    hybrid += [choice('hw.b7k.attack','ATTACK',['CUT','FLAT','BOOST'],'B7K ULTRA',initial=1),choice('hw.b7k.grunt','GRUNT',['CUT','FLAT','BOOST'],'B7K ULTRA',initial=1),choice('hw.b7k.lo_frequency','LO MID FREQ',['250 Hz','500 Hz','1 kHz'],'B7K ULTRA'),choice('hw.b7k.hi_frequency','HI MID FREQ',['750 Hz','1.5 kHz','3 kHz'],'B7K ULTRA'),toggle('hw.b7k.distortion','DISTORTION','B7K ULTRA')]
    hybrid += knobs('hw.db751.','gain bass mid treble master','DB751 TARGET')
    hybrid += [toggle('hw.db751.deep','DEEP','DB751 TARGET'),toggle('hw.db751.bright','BRIGHT','DB751 TARGET')]
    p['legacy.amp.7'] = panel('Darkglass B7K Ultra + Aguilar DB751 — reference chain',hybrid,
        ['https://www.darkglass.com/products/b7uv2a'],review=PARTIAL,
        note='단일 헤드가 아닌 B7K Ultra→DB751 레퍼런스 체인입니다. 각 장비를 다른 그룹으로 표시하지만 현재 DSP가 두 실물 모델을 직렬 실행한다는 뜻은 아닙니다. B7K v2 패널과 원래 캡처 리비전 대조, DB751 원문 확인이 남았습니다.',
        omitted='DB751 panel verification; exact B7K capture revision; physical AUX/headphones/DI and B7K cabinet IR are excluded from this head panel.')

    vox = knobs('hw.normal.','volume','NORMAL',['normal']) + knobs('hw.top_boost.','volume treble bass','TOP BOOST',['top_boost'])
    vox += knobs('hw.','tone_cut master_volume','MASTER') + knobs('hw.','reverb_tone reverb_level tremolo_speed tremolo_depth','ONBOARD FX')
    p['legacy.amp.8'] = panel('VOX AC30CH Custom Head',vox,['https://voxamps.com/product/ac30-custom-head/'],
        [('normal','NORMAL'),('top_boost','TOP BOOST')],initial='top_boost',
        note='Normal은 Volume만, Top Boost는 Volume·Treble·Bass. Master Tone Cut을 독립 구현 대상으로 둡니다. 공통 Middle을 추가하지 않습니다.',
        omitted='Reactive attenuator and external footswitch behavior require revision-specific checks; onboard FX audio is not implemented.',routes=['HIGH INPUT','LOW INPUT'])

    orange = knobs('hw.clean.','volume treble bass','CLEAN',['clean']) + knobs('hw.dirty.','gain treble middle bass volume','DIRTY',['dirty'])
    orange += knobs('hw.','reverb attenuator','GLOBAL') + [choice('hw.power','POWER',['FULL','HALF'],'POWER')]
    p['legacy.amp.9'] = panel('Orange Rockerverb 50 MKIII — target panel',orange,[],
        [('clean','CLEAN'),('dirty','DIRTY')],initial='dirty',review=PENDING,
        note='Clean/Dirty의 다른 EQ 구성과 Attenuator를 분리한 대상 정의입니다. 이번 조회에서 공식 제품 페이지 접근이 차단돼 전면·스위치 원문 최종 대조는 대기 상태입니다.',
        omitted='Full/Half operation, attenuator bypass and onboard reverb behavior require original manual review.')

    bassman = knobs('hw.vintage.','volume bass mid treble','VINTAGE',['vintage'])
    bassman += knobs('hw.overdrive.','gain blend volume bass mid_frequency mid_level treble','OVERDRIVE',['overdrive'])
    for ch in ('vintage','overdrive'):
        bassman += [toggle(f'hw.{ch}.deep','BASS PULL DEEP',ch.upper(),[ch]),toggle(f'hw.{ch}.bright','TREBLE PULL BRIGHT',ch.upper(),[ch])]
    bassman += [knob('hw.master','MASTER','GLOBAL'),toggle('hw.mute','MASTER PULL MUTE','GLOBAL')]
    p['legacy.amp.10'] = panel('Fender Super Bassman',bassman,
        ['https://www.fender.com/products/super-bassman','https://www.fender.com/cdn/shop/files/2249000000_fen_amp_fal_1_nr.png?v=1742864859&width=1445'],
        [('vintage','VINTAGE'),('overdrive','OVERDRIVE')],initial='overdrive',
        note='Vintage Mid와 OD의 Mid Frequency/Mid Level을 구분. 제조사 사진의 두 중역 노브를 기준으로 웹 설명의 중복된 Mid를 세 번째 노브로 만들지 않았습니다. 기존 레퍼런스는 DI 출력입니다.',routes=['INPUT 1','INPUT 2 · -6 dB'])

    subway = knobs('hw.','input gain high_pass voicing bass lo_mid_frequency lo_mid hi_mid_frequency hi_mid treble master','PANEL')
    # The actual D-800+ has INPUT gain, not separate generic Drive + Gain.
    subway = [c for c in subway if c['id']!='hw.gain']
    subway[0]['label']='INPUT'
    subway += [toggle('hw.deep','DEEP'),toggle('hw.bright','BRIGHT'),toggle('hw.mute','MUTE'),choice('hw.input_type','INPUT TYPE',['PASSIVE','ACTIVE'])]
    p['legacy.amp.11'] = panel('Mesa Subway D-800+ — not D-800',subway,
        ['https://www.tone3000.com/tones/mesa-subway-d-800-di-flat-41431'],review=PENDING,
        note='2017 D-800+라는 장비/DI 범위는 캡처 제작자가 명시. + 모델의 HPF·Voicing·가변 중역을 구분한 대상 정의이며 제조사 전체 패널 최종 확인은 대기입니다. D-800 자료로 대체하지 않습니다.',
        omitted='Exact HPF/mid frequency endpoints, switch labels and physical DI controls need the D-800+ manual.')

    match = knobs('hw.ch1.','volume bass treble','CH1 / 12AX7',['ch1'])
    match += [knob('hw.ch2.volume','VOLUME','CH2 / EF86',['ch2']),choice('hw.ch2.tone','TONE',['1','2','3','4','5','6'],'CH2 / EF86',['ch2'])]
    match += [knob('hw.cut','CUT','MASTER'),knob('hw.master','MASTER VOLUME','MASTER'),toggle('hw.master_bypass','MASTER BYPASS','MASTER'),choice('hw.power','POWER',['HI','LO'],'POWER')]
    p['legacy.amp.12'] = panel('Matchless C-30 / DC-30 — original panel target',match,
        ['https://www.matchlessamplifiers.com/amplifiers-and-cabinets/c-30'],[('ch1','CH1 / 12AX7'),('ch2','CH2 / EF86')],initial='ch2',
        note='CH1 Bass/Treble와 CH2 6단 Tone을 구분하고 Cut·우회 가능한 Master를 별도로 둡니다. 기존 NAM은 Ceriatone 클론이며 원본 Matchless 실물 검증으로 승격하지 않습니다.',
        omitted='Rear phase switch/loop routing and power-switch response not connected.',routes=['HIGH INPUT','LOW INPUT'])

    ods = knobs('hw.','volume treble middle bass master presence','SHARED')
    ods += [toggle('hw.bright','BRIGHT','SHARED'),choice('hw.voicing','VOICING',['ROCK','JAZZ'],'SHARED')]
    ods += knobs('hw.od.','drive ratio','OVERDRIVE',['od'])
    ods += [toggle('hw.pab','PREAMP BOOST','SHARED')]
    p['legacy.amp.13'] = panel('ODS #102-style clone — exact hardware/panel pending',ods,[],
        [('clean','CLEAN'),('od','OVERDRIVE')],initial='od',review=PENDING,
        note='원본 Dumble가 아닌 기존 #102 스타일 클론 레퍼런스의 대상 정의입니다. 서로 다른 ODS/HRM/클론 패널을 섞지 않도록 미확인 Deep/Mid Boost 스위치는 추가하지 않았습니다. 제작자 패널 자료를 얻기 전 확정 원본으로 표기하지 않습니다.',
        omitted='Exact #102 clone manufacturer, Deep/Mid Boost labeling, OD ratio/level naming, PAB wiring and switches remain unresolved.')

    eich = knobs('hw.', [('gain','GAIN'),('taste','TASTE'),('lo','LO'),('lo_mid','LO MID'),('hi_mid','HI MID'),('hi','HI'),('master','MASTER')])
    eich += [toggle('hw.mute','MUTE')]
    p['legacy.amp.14'] = panel('EICH T900',eich,['https://www.eich-amps.com/t900'],
        note='Taste와 4밴드 EQ를 분리. 조절부 정의를 갖췄다는 것이 정확한 T900 NAM 확보 또는 실제 Taste 곡선 검증을 뜻하지 않습니다.')

    zuta = []
    for ch, buttons in [(1,['low_cut','gain_switch','gate','bright']),(2,['vintage','gain_switch','gate','bright']),
                        (3,['vintage','gain_switch','gate','bright']),(4,['low_boost','tight','gate','mid_boost'])]:
        cid=f'ch{ch}'
        # Preserve already-published Preparation ZUTA keys; hardware origin is explicit.
        zuta += knobs(cid+'.','gain volume low mid high',cid.upper(),[cid])
        zuta += [toggle(cid+'.'+b,b.replace('_',' ').upper(),cid.upper(),[cid]) for b in buttons]
    zuta += knobs('', 'presence depth master gate_threshold solo_level','GLOBAL')
    zuta += [toggle('presence_frequency_shift','PRESENCE FREQUENCY SHIFT','GLOBAL')]
    p['planned.amp.zuta-gbg120'] = panel('ZUTA GBG120',zuta,['https://zutagroup.com/products/gbg120-tube-amp-by-zuta'],
        [(f'ch{i}',f'CH{i}') for i in range(1,5)],
        note='채널별 5노브·고유 버튼과 공통 Presence/Depth 등을 분리. Gate Threshold의 방향과 각 채널 반응은 별도 DSP 검증 대상입니다.',
        omitted='GBG diode modes, tube-pair switching, series/parallel FX loops and external MIDI/solo switching are not connected.')

    engl = knobs('hw.clean.', [('gain','CLEAN GAIN')],'CLEAN',['clean']) + knobs('hw.lead.', [('gain','LEAD GAIN'),('volume','LEAD VOLUME')],'LEAD',['lead'])
    engl += knobs('hw.','bass middle treble','SHARED EQ') + knobs('hw.',[('presence','LEAD PRESENCE')],'LEAD',['lead'])
    engl += knobs('hw.','master reverb','GLOBAL') + [toggle('hw.gain_boost','GAIN BOOST','GLOBAL'),toggle('hw.mvb','MASTER VOLUME BOOST','GLOBAL'),choice('hw.power_soak','POWER SOAK',['FULL','5 W','1 W','SPEAKER OFF'],'POWER')]
    p['planned.amp.engl'] = panel('ENGL Ironball E606 20W — proposed exact model',engl,
        ['https://www.engl-amps.com/shop/heads/ironball-e606/'],[('clean','CLEAN'),('lead','LEAD')],initial='lead',
        note='ENGL 추가는 필수, Ironball E606은 자료 기반 우선안입니다. Clean/Lead와 Gain Boost를 구분하고 Savage 120으로 재명명하지 않습니다. 공유 EQ를 채널별로 복제하지 않습니다.')

    vh = []
    for i in range(1,5):
        ch=f'ch{i}';vh+=knobs(f'hw.{ch}.','gain treble middle bass volume',ch.upper(),[ch])
    vh += knobs('hw.','master presence deep','GLOBAL')
    p['planned.amp.diezel-vh4'] = panel('Diezel VH4 — core channel controls',vh,
        ['https://www.diezelamplification.com/vh4/'],[(f'ch{i}',v) for i,v in enumerate(['CH1 / CLEAN','CH2 / CRUNCH','CH3 / MEGA','CH4 / LEAD'],1)],initial='ch3',review=PARTIAL,
        note='4채널 독립 Gain/EQ/Volume과 공통 Master/Presence/Deep. 확인한 공식 설명은 핵심 제어 범위이며 밝기 스위치·병렬 루프 등 전체 패널 최종 대조는 남아 있습니다. 프리앰프/Amp+Cab 자료를 헤드로 취급하지 않습니다.',
        omitted='Channel bright switches, parallel-loop Mix, insert switching and exact VH4/VH4S revision require full matching manual.')

    cl = knobs('hw.','gain bass midrange treble master')
    cl += [choice('hw.mid_frequency','FREQUENCY',['1','2','3','4','5']),toggle('hw.ultra_hi','ULTRA HI'),toggle('hw.ultra_lo','ULTRA LO')]
    p['planned.amp.svt-cl'] = panel('Ampeg SVT-CL — stock classic target',cl,
        ['https://ampeg.com/products/classic/heads.html','https://ampeg.com/support/manuals/svt_cl'],review=PARTIAL,
        note='VR의 2채널·3단 중역 선택과 구분한 CL 전용 Gain/Master·5단 중역 대상입니다. 공식 페이지의 5단 선택은 확인했지만 숫자별 주파수는 매뉴얼 파일 대조 전 임의 기재하지 않습니다.',
        omitted='Exact frequency labels, pad and remaining front/rear controls require readable SVT-CL manual; stock calibrated head reference still pending.')

    fortin = knobs('hw.ep.','girth grind gain','EVIL PUMPKIN',['ep']) + knobs('hw.kk.',[('gain1','GAIN 1'),('gain2','GAIN 2')],'KILLER KALI',['kk'])
    fortin += knobs('hw.gain_eq.','bass middle sweep treble','GAIN CHANNELS SHARED',['ep','kk'])
    fortin += knobs('hw.clean.','volume bass middle treble','CLEAN',['clean'])
    fortin += knobs('hw.', [('depth','DEPTH'),('presence','PRESENCE'),('master1','MASTER 1'),('master2','MASTER 2')],'GLOBAL')
    fortin += [toggle('hw.master2_select','MASTER 2 SELECT','GLOBAL')]
    p['planned.amp.fortin'] = panel('Fortin Evil Pumpkin — 2023 panel',fortin,
        ['https://fortinamps.com/products/evil-pumpkin%C2%AE-3-channel-midi-100w-free-hydra-midi-pedal','https://fortinamps.com/cdn/shop/files/1_1024x1024.jpg?v=1694692165'],
        [('ep','EVIL PUMPKIN'),('kk','KILLER KALI'),('clean','CLEAN')],review=PARTIAL,
        note='제조사 전면 사진에서 Girth/Grind/Gain, KK Gain1/Gain2, 공유 Sweep EQ와 Clean 패널을 구분했습니다. 각 채널에 같은 6노브를 복제하지 않습니다. 매뉴얼은 파일 크기 제한으로 전체 기능 대조가 남았습니다.',
        omitted='Loop switch/level, MIDI write behavior and exact Girth/Grind/Sweep interactions require complete manual/capture validation.')

    slo = knobs('hw.normal.', [('preamp','PREAMP'),('master','MASTER')],'NORMAL',['normal']) + knobs('hw.overdrive.',[('preamp','PREAMP'),('master','MASTER')],'OVERDRIVE',['overdrive'])
    slo += [toggle('hw.normal.bright','BRIGHT','NORMAL',['normal']),choice('hw.normal.mode','NORMAL MODE',['CLEAN','CRUNCH'],'NORMAL',['normal'])]
    slo += knobs('hw.','bass middle treble presence depth','SHARED EQ / POWER')
    p['planned.amp.soldano'] = panel('Soldano SLO-100 Classic panel target / LTD capture revision pending',slo,
        ['https://www.soldano.com/products/classic/amplifiers/slo-100-classic/'],[('normal','NORMAL'),('overdrive','OVERDRIVE')],initial='overdrive',review=PARTIAL,
        note='공식 현행 Classic의 독립 Preamp/Master와 공유 EQ·Presence·Depth 대상 정의입니다. 기존 LTD OD 캡처와 세대·Depth 유무를 대조해야 하며 다른 실물 리비전으로 자동 확정하지 않습니다.',
        omitted='Capture LTD revision, Depth availability, FX-loop bypass and channel behavior need matching data.')

    uber = knobs('hw.clean.','gain bass middle treble volume','CLEAN',['clean']) + knobs('hw.lead.','gain bass middle treble presence volume','LEAD',['lead']) + knobs('hw.','master','GLOBAL')
    p['planned.amp.bogner'] = panel('Bogner Uberschall Rev Blue — target panel',uber,
        ['https://www.tone3000.com/tones/bogner-uberschall-79751'],[('clean','CLEAN'),('lead','LEAD')],initial='lead',review=PENDING,
        note='캡처 제작자가 Rev Blue와 Clean/Lead를 명시. 전면 제어의 제조사 원문 최종 대조는 미완료이며 Twin Jet/Ultra 또는 Ecstasy의 제어를 혼용하지 않습니다.',
        omitted='Full original Rev Blue panel/Presence layout and switching manual must be verified before production binding.')

    sunn = knobs('hw.','normal brilliant bass midrange treble master')
    p['planned.amp.sunn'] = panel('SUNN Model T — 1970s target, revision unresolved',sunn,[],review=PENDING,
        note='SUNN 추가는 필수이며 Model T는 우선 조사안입니다. Normal/Brilliant 입력 볼륨과 톤·Master 대상 정의만 준비했습니다. 원형 세대·패널 확인 전 확정 원본 조작부라고 표시하지 않습니다. 리이슈/JFET 페달/클론으로 대체하지 않습니다.',
        omitted='Exact generation, original panel and input-jumper wiring require primary material. No invented Presence, modes or power controls added.',
        routes=['NORMAL — target','BRILLIANT — target','JUMPED — target'])
    # Append E670FE; never relabel the released Ironball panel at model 16.
    se = []
    for ch, label in [('clean','CLEAN'),('crunch','CRUNCH'),('lead1','LEAD I'),('lead2','LEAD II')]:
        se += knobs(f'hw.{ch}.','gain treble volume',label,[ch])
    se += knobs('hw.clean_eq.','bass middle','CLEAN / CRUNCH EQ',['clean','crunch'])
    se += knobs('hw.lead_eq.','bass middle','LEAD EQ',['lead1','lead2'])
    for key, label, channels in [
        ('gain_boost','GAIN BOOST',['clean','crunch']),
        ('mid_shift','MID SHIFT',['clean','crunch']),
        ('bright','BRIGHT',['clean','crunch']),
        ('hi_gain','HI GAIN',['lead1','lead2']),
        ('contour','CONTOUR',['lead1','lead2']),
        ('mid_edge','MID EDGE',['lead1','lead2'])]:
        se += [toggle('hw.'+key,label,'VOICING',channels)]
    se += [choice('hw.character','CHARACTER',['MODERN','CLASSIC'],'VOICING',['clean','crunch','lead1','lead2'])]
    se += knobs('hw.','presence_a presence_b','POWER')
    se += [choice('hw.presence_select','PRESENCE',['A','B'],'POWER')]
    se += knobs('hw.','master_a master_b','POWER')
    se += [choice('hw.master_select','MASTER',['A','B'],'POWER'),toggle('hw.depth_boost','DEPTH BOOST','POWER')]
    se += [toggle('hw.mega_lo_punch','MEGA LO PUNCH','VOICING',['clean','crunch','lead1','lead2'])]
    se += [toggle('hw.tube_eq','T.D. EQ','TUBE DRIVER',['driver'])]
    p['planned.amp.engl-e670fe'] = panel('ENGL E670FE Special Edition Founders Edition',se,
        ['https://www.engl-amps.com/wp-content/uploads/2024/01/E670FE-OM-2-FE-Special-Edition.pdf'],
        [('clean','CLEAN'),('crunch','CRUNCH'),('lead1','LEAD I'),('lead2','LEAD II'),('driver','TUBE DRIVER')],
        initial='lead1',review=PARTIAL,
        note='E670FE core controls from the official manual. Tube Driver replaces the four main paths; no invented Driver gain. All coefficients are authored, not capture-calibrated.',
        omitted='Internal reverb/noise gate and external loop/MIDI hardware are delegated to Chimera FX/global controls. Tube Driver EQ uses an authored passive-style insertion response; physical tone-control ownership/taper remains unverified. No selectable output-tube type or E670/Savage/Powerball equivalence is claimed.',
        scope='C Documentary for named functions; T.D. EQ insertion and all sonic coefficients remain pending exact E670FE validation.')
    # Retain the released 1.1.2 default-gain calibration on regeneration.
    defaults = {'legacy.amp.2': {'hw.lead.pre_gain': 0.68}, 'legacy.amp.3': {'hw.ch2.gain': 0.65, 'hw.ch2.mode': 1, 'hw.ch3.gain': 0.68, 'hw.ch3.mode': 2}, 'legacy.amp.4': {'hw.lead.gain': 0.72, 'hw.lead.drive': 0.65}, 'legacy.amp.9': {'hw.dirty.gain': 0.66}, 'planned.amp.zuta-gbg120': {'ch3.gain': 0.78, 'ch4.gain': 0.68}, 'planned.amp.diezel-vh4': {'hw.ch3.gain': 0.78, 'hw.ch4.gain': 0.72}, 'planned.amp.fortin': {'hw.ep.gain': 0.72, 'hw.kk.gain1': 0.68, 'hw.kk.gain2': 0.62}, 'planned.amp.soldano': {'hw.overdrive.preamp': 0.72}, 'planned.amp.bogner': {'hw.lead.gain': 0.7}, 'planned.amp.engl-e670fe': {'hw.lead1.gain': 0.72, 'hw.lead2.gain': 0.68, 'hw.hi_gain': 1}}
    for key, values in defaults.items():
        for control in p[key]["controls"]:
            if control["id"] in values:
                control["initial"] = values[control["id"]]
    p["planned.amp.zuta-gbg120"]["initial_channel"] = p["planned.amp.zuta-gbg120"]["channels"][2]["id"]
    return p


def upgrade_amplifiers(models):
    panels = amp_panels()
    for model in models:
        if model['location'] != 'rig':
            continue
        target = deepcopy(panels[model['id']])
        model['controls'] = target.pop('controls')
        model['amp_panel'] = target
        model.pop('native_target', None)
        model.pop('omissions', None)
        model['controls_status'] = target['review_status'] + '_unimplemented_dsp'
        model['control_sources'] = target['sources']
        model['audio_in_prototype'] = False
    return models


def validate_amp_panels(models):
    for m in models:
        if m['location'] != 'rig':
            continue
        p = m['amp_panel']
        channels = [c['id'] for c in p['channels']]
        if not channels or len(channels) != len(set(channels)) or p['initial_channel'] not in channels:
            raise ValueError('Bad amplifier channels')
        if p['review_status'] not in (REVIEWED, PARTIAL, PENDING):
            raise ValueError('Unknown amplifier review status')
        if p['review_status'] == REVIEWED and not p['sources']:
            raise ValueError('Panel review has no source')
        if not p['input_routes'] or len(set(p['input_routes'])) != len(p['input_routes']):
            raise ValueError('Bad amplifier input routes')
        if p['production_dsp_connected'] or p['circuit_response_verified'] or p['capture_approved']:
            raise ValueError('Preparation must not claim audio approval')
        for c in m['controls']:
            if any(ch not in channels for ch in c['channels']):
                raise ValueError('Invalid control channel visibility')
    return True
