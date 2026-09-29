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
