"""R1a POST panel contracts. No production parameter remapping or DSP binding.
Continuous values are panel positions, never invented physical transfer curves.
"""
from copy import deepcopy

REVIEWED = 'primary_panel_reviewed'
PARTIAL = 'partial_primary_review'


def knob(key, label, group='PANEL', note=''):
    return dict(id='hw.'+key, label=label, kind='knob', minimum=0, maximum=1,
                initial=.5, step=.01, unit='position', group=group,
                range_basis='prototype_position_not_hardware_taper', dsp_binding=None, note=note)


def choice(key, label, options, group='PANEL', initial=0):
    return dict(id='hw.'+key, label=label, kind='choice', options=options, initial=initial,
                group=group, range_basis='discrete_labels', dsp_binding=None)


def switch(key, label, group='PANEL', initial=0):
    return dict(id='hw.'+key, label=label, kind='toggle', initial=initial,
                group=group, range_basis='binary_state', dsp_binding=None)


def panel(reference, controls, sources, note, omitted, review=REVIEWED):
    return dict(reference=reference, controls=controls, sources=sources, note=note,
                omitted=omitted, review_status=review, review_date='2026-09-28',
                production_dsp_connected=False, capture_approved=False,
                circuit_response_verified=False, meter_signal_available=False)


NEVE = 'https://www.ams-neve.com/outboard/1073-range/1073-mic-preamp-equaliser/'


def neve_eq():
    return [knob('hf_gain','HF GAIN · 12 kHz','HF'),
            knob('mf_gain','MF GAIN','MF'),choice('mf_frequency','MF FREQ',['360 Hz','700 Hz','1.6 kHz','3.2 kHz','4.8 kHz','7.2 kHz'],'MF'),
            knob('lf_gain','LF GAIN','LF'),choice('lf_frequency','LF FREQ',['35 Hz','60 Hz','110 Hz','220 Hz'],'LF'),
            choice('high_pass','HIGH PASS',['OFF','50 Hz','80 Hz','160 Hz','300 Hz'],'FILTER'),
            switch('eq_in','EQL','FILTER',initial=1)]


def rack_panels():
    p = {}
    p['legacy.bus.0'] = panel('SSL XLogic G Series Compressor · 2005 Rev 0A',[
        knob('threshold','THRESHOLD'),knob('makeup','MAKE-UP'),
        choice('attack','ATTACK',['0.1 ms','0.3 ms','1 ms','3 ms','10 ms','30 ms']),
        choice('release','RELEASE',['0.1 s','0.3 s','0.6 s','1.2 s','AUTO']),
        choice('ratio','RATIO',['2:1','4:1','10:1']),switch('compressor_in','COMPRESSOR IN',initial=1),
        switch('external_sidechain','EXTERNAL SIDE-CHAIN','ROUTING'),
        knob('fade_rate','RATE','AUTO FADE'),switch('auto_fade','AUTO FADE','AUTO FADE')],
        ['https://www.solid-state-logic.co.jp/docs/XLogic_G-Comp.pdf'],
        'XLogic 원본의 단계형 Attack/Release/Ratio와 연속 Threshold/Make-up을 분리합니다. BUS+나 플러그인 전용 Dry/Wet·SC HPF를 추가하지 않습니다.',
        'Sidechain input, stereo detector, autofade timing and meter audio are not connected.')
    p['legacy.bus.1'] = panel('Universal Audio 1176LN · D/E-style hardware reissue',[
        knob('input','INPUT'),knob('output','OUTPUT'),
        knob('attack','ATTACK',note='Clockwise is faster; 0–1 is a position, not milliseconds.'),
        knob('release','RELEASE',note='Clockwise is faster; 0–1 is a position, not milliseconds.'),
        switch('attack_off','ATTACK OFF'),
        choice('ratio','RATIO',['4:1','8:1','12:1','20:1','ALL BUTTONS']),
        choice('meter','METER',['GR','+4','+8','OFF'],'METER')],
        ['https://media.uaudio.com/assetlibrary/1/1/1176ln_manual.pdf'],
        'Input과 Output, 압축을 끄는 Attack OFF를 독립 보존합니다. All Buttons는 비율 숫자로 환산하지 않습니다. 미터 OFF는 실물 전원 기능의 상태 대상입니다.',
        'Other multi-button combinations, stereo-link accessory, electrical power and meter calibration are not modelled.')
    p['legacy.bus.2'] = panel('Teletronix LA-2A · Universal Audio hardware reissue',[
        knob('gain','GAIN'),knob('peak_reduction','PEAK REDUCTION'),
        choice('mode','MODE',['COMPRESS','LIMIT']),choice('meter','METER',['+4','GR','+10'],'METER',initial=1),
        knob('r37','R37 · HF EMPHASIS','REAR / CALIBRATION',note='Clockwise is flat sidechain; not a front-panel attack control.')],
        ['https://media.uaudio.com/assetlibrary/l/a/la-2a_manual.pdf'],
        'Gain과 Peak Reduction은 독립입니다. 원본에 없는 Attack·Release·연속 Ratio를 표시하지 않습니다. R37은 후면 보정 영역으로 분리합니다.',
        'R3 stereo calibration, meter zero and electrical power omitted; program-dependent optical response remains unimplemented.')
    gain_options = [f'LINE {v:+d} dB' for v in range(-10,21,5)] + ['OFF'] + [f'MIC {v:+d} dB' for v in range(20,81,5)]
    p['legacy.preamp.0'] = panel('Neve 1073 Classic · preamplifier section',[
        choice('gain','MIC / LINE GAIN',gain_options),switch('phase','PHASE')],
        [NEVE],
        '단일 Gain 선택기에 Mic/Line의 단계와 OFF를 저장합니다. EQ는 별도 N73 EQ 패널에 있으며 임의 Drive·Bass·Treble로 교체하지 않습니다.',
        'Rack output fader, phantom power and console/rack impedance switching are separate hardware configuration. This descriptor is only the preamp section.')
    p['legacy.preamp.1'] = panel('Avalon V5 · mono DI / RE / mic preamp',[
        choice('input','INPUT',['LINE','INST / DI','MIC HI-Z','MIC LO-Z','MIC +48 V']),
        choice('boost','BOOST',[str(i) for i in range(1,22)]),
        choice('tone','TONE',[str(i) for i in range(1,11)]),
        switch('tone_in','TONE IN'),switch('high_cut','HIGH CUT'),switch('phase','PHASE'),switch('pad','PAD')],
        ['https://avalondesign.com/manuals/V5-Manual.pdf','https://www.avalondesign.com/v55-1'],
        'V5 원본 PDF의 검색 색인에서 21단 Boost와 10단 Tone Bank를 확인했습니다. 제조사 웹 페이지에 V55 설명이 섞여 있어 V5 전체 패널 최종 대조는 보류합니다. 임의 3밴드 EQ는 없습니다.',
        'Full original-panel image review and switch naming/routing remain pending. Tone curves, mode-dependent gain and reamp path are not implemented.',PARTIAL)
    p['legacy.preamp.2'] = panel('Focusrite ISA One · main and instrument preamps',[
        choice('input','INPUT',['MIC','LINE','INST']),choice('gain','GAIN',['0','10','20','30']),
        switch('gain_range','30–60'),knob('trim','TRIM'),
        choice('impedance','Z IN',['LOW · 600 Ω','ISA 110 · 1.4 kΩ','MED · 2.4 kΩ','HIGH · 6.8 kΩ']),
        switch('phantom','+48 V'),switch('phase','PHASE'),switch('high_pass','HPF · 75 Hz'),switch('insert','INSERT'),
        knob('instrument_gain','GAIN','INSTRUMENT'),choice('instrument_z','Z IN',['LOW','HIGH'],'INSTRUMENT'),
        switch('post_insert','POST INSERT','METER')],
        ['https://userguides.focusrite.com/hc/en-gb/articles/17525637995538-ISA-One-Controls-and-Features',
         'https://userguides.focusrite.com/hc/en-gb/articles/17525660671634-ISA-One-performance-and-specifications'],
        'ISA One으로 리비전을 고정했습니다. Gain 4단·30–60 전환·Trim·악기용 Gain/Z를 분리하며 입력에 따른 값 해석은 아직 오디오에 연결하지 않습니다.',
        'Headphone/cue monitor, optional ADC clock, physical phantom/impedance, meter calibration and external insert audio are not implemented.')
    ssl=[]
    for band in ('HF','HMF','LMF','LF'):
        ssl += [knob(band.lower()+'_gain',band+' GAIN',band),knob(band.lower()+'_frequency',band+' FREQ',band)]
        ssl += [knob(band.lower()+'_q',band+' Q',band)] if band in ('HMF','LMF') else [switch(band.lower()+'_bell',band+' BELL',band)]
    ssl += [switch('black','BLK','EQ'),switch('eq_in','IN','EQ',initial=1)]
    p['legacy.eq.0'] = panel('SSL E Series EQ · 500 Series / Brown-02 and Black-242',ssl,
        ['https://www.solidstatelogic.com/assets/uploads/downloads/SSL_500_Series_E_EQ_Module_User_Guide.pdf'],
        '4밴드, HF/LF Bell 전환, 두 중역의 Q, Brown/Black 전환을 보존합니다. 채널 스트립 필터를 500 EQ의 원본 조절부로 추가하지 않습니다.',
        'Frequency/gain/Q tapers and Black/Brown response curves are unmeasured; hardware module does not include console HPF/LPF.')
    p['legacy.eq.1'] = panel('Neve 1073 Classic · complete EQ section',neve_eq(),[NEVE],
        '기존 이름 N73 Shelves와 달리 원본의 중역 및 HPF도 포함한 EQ 대상입니다. HF 12 kHz는 고정이며 별도 주파수 노브를 만들지 않습니다.',
        'Input amplifier belongs to the separate N73 preamp record; frequency response/tapers and bypass interaction are unimplemented.')
    p['legacy.eq.2'] = panel('Pulse Techniques Pultec EQP-1A · tube program EQ',[
        choice('lf_frequency','LOW FREQ',['20 Hz','30 Hz','60 Hz','100 Hz'],'LOW'),
        knob('lf_boost','BOOST','LOW'),knob('lf_atten','ATTEN','LOW'),
        knob('bandwidth','BANDWIDTH','HIGH BOOST'),knob('hf_boost','BOOST','HIGH BOOST'),
        choice('hf_frequency','HIGH FREQ',['3 kHz','4 kHz','5 kHz','8 kHz','10 kHz','12 kHz','16 kHz'],'HIGH BOOST'),
        knob('hf_atten','ATTEN','HIGH ATTEN'),choice('hf_atten_frequency','ATTEN SEL',['5 kHz','10 kHz','20 kHz'],'HIGH ATTEN'),
        switch('eq_in','IN','EQ',initial=1)],
        ['https://pulsetechniques.com/products/tube-equalizers/eqp-1a/'],
        '저역 Boost/Atten을 동시에 저장하며 고역 Boost 주파수와 Atten 주파수를 분리합니다. 일반 Bass/Treble 노브로 통합하지 않습니다.',
        'Full panel-switch review, boost/atten interaction, bandwidth taper and tube amplifier response need further validation.',PARTIAL)
    p.update(post_expansion_panels())
    return p


# 1.7 POST expansion: three models appended to each section (indices 3-5).
# Panels follow published controls; manufacturer pages were not reachable from
# the build environment, so each records the secondary sources consulted.
def post_expansion_panels():
    p={}
    p['planned.post.bus.3'] = panel('Eventide Omnipressor 2830*Au reissue',[
        choice('input_cal','INPUT CAL',['0 dB','-10 dB','-20 dB','-30 dB'],'INPUT'),
        knob('function','FUNCTION','DYNAMICS',note='Gate / expand / neutral at 12 o\'clock / compress / reverse.'),
        knob('threshold','THRESHOLD','DYNAMICS'),knob('attack','ATTACK','TIME'),knob('release','RELEASE','TIME'),
        knob('atten_limit','ATTEN LIMIT','LIMITS'),knob('gain_limit','GAIN LIMIT','LIMITS'),
        switch('bass_cut','BASS CUT','DYNAMICS'),knob('output','OUTPUT','OUTPUT'),
        choice('meter','METER',['INPUT','GAIN','OUTPUT'],'OUTPUT',initial=1),switch('dynamics_in','IN','OUTPUT',initial=1)],
        ['https://www.musicradar.com/reviews/tech/eventide-omnipressor-533112','https://vintageking.com/blog/hear-the-gear-eventide-omnipressor/'],
        'Function 하나로 게이트·익스팬드·압축·역전(dynamic reversal)을 연속 조절합니다. Atten/Gain Limit이 Function보다 우선합니다. Meter는 소리에 영향이 없습니다.',
        'Exact Function law, detector and Output range are authored; manufacturer manual not reachable from the build environment.',PARTIAL)
    p['planned.post.bus.4'] = panel('API 2500 stereo bus compressor',[
        knob('threshold','THRESHOLD','COMPRESSOR'),
        choice('attack','ATTACK',['0.03 ms','0.1 ms','0.3 ms','1 ms','3 ms','10 ms','30 ms'],'COMPRESSOR',initial=3),
        choice('ratio','RATIO',['1.5:1','2:1','3:1','4:1','6:1','10:1','INF:1'],'COMPRESSOR',initial=3),
        choice('release','RELEASE',['0.05 s','0.1 s','0.2 s','0.5 s','1 s','2 s','VAR'],'COMPRESSOR',initial=3),
        knob('release_var','VARIABLE RELEASE','COMPRESSOR'),
        choice('knee','KNEE',['HARD','MED','SOFT'],'TONE',initial=1),choice('thrust','THRUST',['NORM','MED','LOUD'],'TONE'),
        choice('type','TYPE',['NEW','OLD'],'TONE'),knob('link','L/R LINK','LINK'),knob('makeup','OUTPUT GAIN','OUTPUT'),
        switch('compressor_in','IN','OUTPUT',initial=1)],
        ['https://apiaudio.com/product/2500-stereo-compressor','https://www.manualslib.com/manual/3555411/Api-2500Plus.html?page=10'],
        'Thrust(검출기 하이패스), Knee, New(feed-forward)/Old(feedback), 연속 L/R Link를 분리합니다. 2500+ 표기의 Attack/Ratio/Release 값을 사용합니다.',
        'Link filters, exact Thrust curves and the threshold calibration are authored; 2500 vs 2500+ differences are not modelled.',PARTIAL)
    p['planned.post.bus.5'] = panel('AMS Neve 33609/N diode-bridge compressor/limiter',[
        knob('comp_threshold','COMP THRESHOLD','COMPRESSOR'),
        choice('comp_ratio','RATIO',['1.5:1','2:1','3:1','4:1','6:1'],'COMPRESSOR',initial=1),
        choice('comp_recovery','COMP RECOVERY',['100 ms','400 ms','800 ms','1.5 s','A1','A2'],'COMPRESSOR',initial=1),
        choice('comp_attack','COMP ATTACK',['FAST','SLOW'],'COMPRESSOR',initial=1),
        knob('gain','GAIN','COMPRESSOR'),switch('comp_in','COMP IN','COMPRESSOR',initial=1),
        knob('limit_threshold','LIMIT THRESHOLD','LIMITER'),
        choice('limit_recovery','LIMIT RECOVERY',['100 ms','200 ms','800 ms','A1','A2'],'LIMITER',initial=1),
        choice('limit_attack','LIMIT ATTACK',['FAST','SLOW'],'LIMITER'),switch('limit_in','LIMIT IN','LIMITER')],
        ['https://www.ams-neve.com/wp-content/uploads/2022/01/33609N1.0usermanual.pdf','https://vintageking.com/neve-33609-n-stereo-compressor'],
        '압축기와 리미터를 별도 문턱·회복 시간·켜짐으로 둡니다. 비율 5단, 회복 4단+A1/A2. 다이오드 브리지 색채는 감쇠량에 비례하는 저차 왜곡으로 표현합니다.',
        'Threshold scales, A1/A2 programme curves and diode-bridge distortion are authored; the official manual was not reachable.',PARTIAL)
    p['planned.post.preamp.3'] = panel('API 512c discrete mic/line preamp',[
        choice('input','INPUT',['MIC','LINE / INST'],'INPUT',initial=1),knob('gain','GAIN','INPUT'),
        switch('pad','PAD -20 dB','INPUT'),switch('phase','PHASE','INPUT'),switch('phantom','+48 V','INPUT')],
        ['https://apiaudio.com/product/512c-mic-pre','https://www.kmraudio.com/api-512c-mono-mic-line-pre.php'],
        'Mic 65 dB / Line·Inst 45~50 dB(자료마다 다름), -20 dB 패드, 위상, +48 V. +48 V는 녹음된 신호에는 물리적 효과가 없습니다.',
        '2520 op-amp and transformer behaviour are authored; +48 V is physical only.',PARTIAL)
    p['planned.post.preamp.4'] = panel('Chandler Limited TG2 preamp / DI',[
        choice('input','INPUT',['MIC','DI','LINE'],'INPUT',initial=1),
        choice('coarse_gain','COARSE GAIN',['+20','+25','+30','+35','+40','+45','+50','+55','+60'],'GAIN',initial=4),
        knob('fine_gain','FINE GAIN','GAIN'),knob('output','OUTPUT','OUTPUT'),
        choice('load','LOAD',['300 Ω','1200 Ω'],'INPUT',initial=1),switch('phase','PHASE','INPUT'),switch('phantom','+48 V','INPUT')],
        ['https://barryrudolph.com/utilities/chandlertg2.html','https://www.soundonsound.com/reviews/chandler-limited-tg2-500'],
        'Coarse(+20~+60 dB, 5 dB 단계)과 Fine(±10 dB), Output을 분리합니다. 랙형 TG2의 앞면 DI 입력을 포함합니다.',
        'Load needs a source-impedance model (physical only); transformer saturation and output stage are authored.',PARTIAL)
    p['planned.post.preamp.5'] = panel('Grace Design m101 preamp / DI',[
        choice('input','INPUT',['MIC','HI-Z INST'],'INPUT',initial=1),
        choice('gain','GAIN',['1','2','3','4','5','6','7','8','9','10','11','12'],'GAIN',initial=4,),
        knob('trim','TRIM','GAIN'),switch('high_pass','HPF 75 Hz','FILTER'),
        switch('ribbon','RIBBON','INPUT'),switch('phantom','+48 V','INPUT')],
        ['https://www.mcgill.ca/library/files/library/m101_manual_reva.pdf','https://proaudiodesign.com/products/grace-design-m101'],
        '게인 12단(MIC 10~65 dB, INST -10~45 dB, 5 dB 단계), Trim 0~+10 dB, 75 Hz 12 dB/oct HPF. Ribbon 모드는 입력 임피던스·팬텀 전환이므로 녹음된 신호에는 물리적 효과가 없습니다.',
        'Ribbon mode and +48 V are physical only; the amplifier is modelled as very low colouration (authored).',PARTIAL)
    p['planned.post.eq.3'] = panel('API 550A three-band discrete EQ',[
        choice('lf_frequency','LF FREQ',['30 Hz','40 Hz','50 Hz','100 Hz','200 Hz','300 Hz','400 Hz'],'LF',initial=3),
        choice('lf_gain','LF GAIN',['-12','-9','-6','-4','-2','0','+2','+4','+6','+9','+12'],'LF',initial=5),
        switch('lf_shelf','LF SHELF','LF'),
        choice('mf_frequency','MF FREQ',['200 Hz','400 Hz','600 Hz','800 Hz','1.5 kHz','3 kHz','5 kHz'],'MF',initial=4),
        choice('mf_gain','MF GAIN',['-12','-9','-6','-4','-2','0','+2','+4','+6','+9','+12'],'MF',initial=5),
        choice('hf_frequency','HF FREQ',['2.5 kHz','5 kHz','7 kHz','10 kHz','12.5 kHz','15 kHz','20 kHz'],'HF',initial=3),
        choice('hf_gain','HF GAIN',['-12','-9','-6','-4','-2','0','+2','+4','+6','+9','+12'],'HF',initial=5),
        switch('hf_shelf','HF SHELF','HF'),switch('filter','FILTER 50 Hz–15 kHz','FILTER'),switch('eq_in','IN','EQ',initial=1)],
        ['https://www.bhphotovideo.com/lit_files/981197.pdf','https://www.turramusic.com.au/products/api-550a'],
        '3밴드 계단식 이득(±2/4/6/9/12 dB), LF/HF 피크·셸프 전환, 50 Hz~15 kHz 밴드패스 필터. 주파수 목록은 하드웨어 사양서 값입니다(자료마다 다름).',
        'Proportional-Q law is authored; frequency lists differ between sources and follow the hardware spec sheet.',PARTIAL)
    p['planned.post.eq.4'] = panel('Maag Audio EQ4 with AIR BAND',[
        knob('sub','SUB 10 Hz','BANDS'),knob('b40','40 Hz','BANDS'),knob('b160','160 Hz','BANDS'),knob('b650','650 Hz','BANDS'),
        knob('b2500','2.5 kHz','BANDS'),knob('air_gain','AIR GAIN','AIR BAND'),
        choice('air_frequency','AIR FREQ',['OFF','2.5 kHz','5 kHz','10 kHz','15 kHz','20 kHz','40 kHz'],'AIR BAND',initial=3),
        switch('eq_in','IN','EQ',initial=1)],
        ['https://www.soundonsound.com/reviews/maag-audio-eq4','https://reverb.com/p/maag-audio-eq4'],
        'Sub 10 Hz, 40/160/650 Hz 벨, 2.5 kHz 셸프(Q≈0.4, 약 3옥타브 폭), 최대 +15 dB / -4.5 dB. Air Band는 최대 +20 dB, 주파수 OFF~40 kHz.',
        '40 kHz AIR is applied at the processing rate\'s upper limit; band shapes are authored from the published Q.',PARTIAL)
    p['planned.post.eq.5'] = panel('Chandler Limited Curve Bender',[
        knob('treble_gain','TREBLE','TREBLE'),
        choice('treble_frequency','TREBLE FREQ',['5 kHz','7 kHz','10 kHz','12 kHz','15 kHz','20 kHz'],'TREBLE',initial=2),
        switch('treble_bell','TREBLE BELL','TREBLE'),
        knob('presence1_gain','PRESENCE 1','PRESENCE 1'),
        choice('presence1_frequency','PRES 1 FREQ',['0.8 kHz','1.2 kHz','1.8 kHz','2.8 kHz','3.6 kHz','4.2 kHz','6.5 kHz','8.1 kHz'],'PRESENCE 1',initial=3),
        knob('presence2_gain','PRESENCE 2','PRESENCE 2'),
        choice('presence2_frequency','PRES 2 FREQ',['0.3 kHz','0.4 kHz','0.5 kHz','0.8 kHz','1.2 kHz','1.8 kHz','2.8 kHz','3.6 kHz'],'PRESENCE 2',initial=2),
        knob('bass_gain','BASS','BASS'),
        choice('bass_frequency','BASS FREQ',['35 Hz','50 Hz','70 Hz','91 Hz','150 Hz','200 Hz','300 Hz'],'BASS',initial=2),
        switch('bass_bell','BASS BELL','BASS'),
        choice('high_pass','HPF',['OFF','20 Hz','30 Hz','40 Hz','50 Hz','60 Hz','80 Hz','100 Hz','160 Hz','200 Hz','320 Hz'],'FILTER'),
        choice('low_pass','LPF',['OFF','20 kHz','15 kHz','12 kHz','10 kHz','8 kHz','6 kHz','4 kHz'],'FILTER'),
        switch('eq_in','IN','EQ',initial=1)],
        ['https://chandlerlimited.com/wp-content/uploads/2012/12/curve-bender-half-db-manual.pdf','https://softube.com/user-manuals/chandler-limited-curve-bender'],
        '4밴드 세미 파라메트릭(Bass/Treble 셸프·벨 전환), HPF/LPF. Bass·Presence 주파수와 HPF 값은 공개 매뉴얼 값, Treble·LPF 위치는 Chimera의 선택입니다.',
        'Treble and LPF positions, band Q and the Multiply switch are not verified; Multiply is omitted.',PARTIAL)
    return p


def upgrade_racks(models):
    panels=rack_panels()
    for m in models:
        if m['id'] not in panels: continue
        p=deepcopy(panels[m['id']])
        m['controls']=p.pop('controls')
        m['rack_panel']=p
        m['controls_status']='native_rack_panel_unimplemented_dsp' if p['review_status']==REVIEWED else 'provisional_rack_panel_unimplemented_dsp'
        m['sources']=list(dict.fromkeys(m['sources']+p['sources']))
    return models


def validate_rack_panels(models):
    expected=rack_panels()
    actual={m['id']:m for m in models if 'rack_panel' in m}
    if set(actual)!=set(expected):raise ValueError('Missing or unexpected rack panel')
    for mid,m in actual.items():
        p=m['rack_panel']
        if m['location']!='post' or not m['controls']:raise ValueError('Rack panel placement')
        if p['review_status'] not in (REVIEWED,PARTIAL) or not p['sources']:raise ValueError('Rack evidence missing')
        for flag in ('production_dsp_connected','capture_approved','circuit_response_verified','meter_signal_available'):
            if p[flag] is not False:raise ValueError('Unsupported rack verification claim')
        if not all(c['id'].startswith('hw.') and c['dsp_binding'] is None for c in m['controls']):raise ValueError('Rack binding collision')
    return True
