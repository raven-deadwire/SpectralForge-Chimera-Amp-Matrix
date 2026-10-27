#!/usr/bin/env python3
"""Generate production panel inventory from the reviewed Preparation definitions.

DSP status is an explicit production decision; panel review is not calibration.
"""
import json
import pathlib
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'Preparation'))
from pedal_controls import hardware_panels


def quote(value):
    return json.dumps(value, ensure_ascii=True)


def generate():
    definitions = hardware_panels()
    # Public names are presentation only. The reviewed references and stable
    # model/control keys remain in Preparation and serialized parameter IDs.
    public_names = (
        'Empty', 'Green Drive', 'Gold Drive', 'Rodent', 'Bass DI', 'Micro Bass',
        'Studio VCA', 'Red OTA', 'Optical', 'Studio FET', 'Variable Mu',
        'Q Sweep', 'Band Sweep', 'Reverse Sweep', 'Bass Envelope', 'Dynamic Wah',
        'Big Sustain', 'Round Fuzz', 'Vintage Fuzz', 'Wool Bass', 'Gated Fuzz',
        'Clean Lift', 'Treble Lift', 'Micro Lift', 'Echo Lift', 'Linear Power',
        'Yellow Asym', 'Obsession', 'Plus Drive', 'Dual Circuit', 'Manual Wah',
        'Graphic EQ', 'Classic Chorus', 'Spatial Chorus', 'Stone Phase',
        'Tidal Flange', 'Drift Vibrato', 'Pulse Tremolo', 'Mono Octaver', 'Spectral Octaver',
        # 1.4 PRE Expansion A (docs/PRE_EXPANSION_A_1_4.md). Model 40 onward
        # append their parameters after the released list (version hint 16).
        'Para EQ', 'Multi Comp', 'Tight Boost', 'Power Wah', 'Bass Synth', 'Heavy Threat')
    models = [('empty', 'Empty', 'EMPTY', [], True)]
    for family in ('drive', 'comp', 'filter', 'fuzz', 'boost'):
        for i in range(5):
            key = f'legacy.{family}.{i}'
            panel = definitions[key]
            models.append((key, panel['reference'], family.upper(), panel['controls'], True))
    def knob(key, label=None, lo=0, hi=1, initial=.5):
        return dict(id=key, label=label or key.upper(), minimum=lo, maximum=hi, initial=initial, step=.001, kind='knob')
    def choice(key, label, values, initial=0):
        return dict(id=key,label=label,initial=initial,options=values,kind='choice')
    models += [
        ('planned.sd1','Yellow Asym / SD-1 reference','DRIVE',[knob('drive'),knob('tone'),knob('level')],True),
        ('planned.ocd-v2','Obsession / OCD v2 reference','DRIVE',[knob('volume'),knob('drive'),knob('tone'),choice('peak','PEAK',['HP','LP'])],True),
        ('planned.distortion-plus','Plus / M104 reference','DRIVE',[knob('distortion'),knob('output')],True),
        # Circuit A/B labels preserve the original key and numeric route order:
        # A is the first control bank; B remains choice value 0.
        ('planned.jb2','Dual Circuit','DRIVE',[knob(f'{side}.{key}',f'{alias} {key.upper()}') for side,alias in (('boss','A'),('jhs','B')) for key in ('drive','tone','level')]+[choice('mode','MODE',['B','A','B / A toggle','B > A','A > B','PARALLEL']),choice('toggle','CHIMERA FOOT SWITCH',['B','A'])],True),
        ('planned.wah','Manual Wah / original DSP','WAH',[knob('position'),knob('range'),knob('q')],True),
        ('planned.pre-eq','PRE Graphic EQ / original DSP','EQUALIZER',[knob(f'band.{hz}',f'{hz} Hz',-12,12,0) for hz in ('31.25','62.5','125','250','500','1000','2000','4000','8000','16000')]+[knob('input','INPUT',-12,12,0),knob('output','OUTPUT',-12,12,0)],True),
    ]
    for i, name in enumerate(('Classic Chorus','Dimension','Stone Phase','Mistress Flange','Eddy Vibrato','Pulsar Tremolo')):
        models.append((f'pre.mod.{i}',f'PRE {name}','MODULATION',[knob('rate','RATE',.05,5,.7),knob('depth','DEPTH'),knob('mix','MIX',0,.7,.25)],True))
    for kind, names in (('mono',('dry','down1','down2')),('poly',('dry','down','up'))):
        title='Mono Octaver / tracking divider' if kind=='mono' else 'Spectral Octaver / experimental'
        models.append((f'planned.octaver-{kind}',title,'PITCH',[knob(x,initial=1 if i==0 else .5 if i==1 else 0) for i,x in enumerate(names)],True))
    models.append(('pre.paraeq','Para EQ / ParaEQ MKII reference','EQUALIZER',
        [c for band,label in (('low','LOW'),('mid','MID'),('high','HIGH')) for c in (knob(f'{band}.freq',f'{label} FREQ'),knob(f'{band}.gain',f'{label} GAIN',-15,15,0),choice(f'{band}.q',f'{label} Q',['WIDE','MEDIUM','NARROW'],1))]
        +[knob('boost','BOOST',0,30,10),choice('boost.on','BOOST ON',['OFF','ON'])],True))
    models.append(('pre.multicomp','Multi Comp / MultiComp reference','COMP',
        [knob('comp','COMP',1,5,3),knob('gain','GAIN',0,15,3),knob('sens','SENS',-25,6,-18),choice('mode','MODE',['NORMAL','MULTI-BAND','TUBESIM']),knob('balance','BALANCE',-6,6,0)],True))
    models.append(('pre.tightboost','Tight Boost / Fortin 33 reference','BOOST',[knob('boost','BOOST',0,22,11)],True))
    models.append(('pre.powerwah','Power Wah / Power Wah Boost reference','WAH',
        [knob('position','POSITION'),knob('boost','BOOST',0,40,10),choice('wah.on','WAH',['OFF','ON'],1),choice('boost.on','BOOST ON',['OFF','ON'])],True))
    models.append(('pre.basssynth','Bass Synth / MXR Bass Synth reference','PITCH',
        [knob('mix','MIX',initial=.7),knob('sub','SUB MIX',initial=.3),knob('cutoff','CUTOFF',initial=.45),knob('resonance','RESONANCE',initial=.3),
         knob('envelope','ENVELOPE',initial=.4),knob('mod','MOD',initial=0),choice('shape','SHAPE',['TRIANGLE','SAW','SQUARE'],1),choice('voice','VOICE',['OFF','ON'])],True))
    # Public name: never the alias Hammer (DEVELOPMENT_ROADMAP_2026_2027.md).
    models.append(('pre.heavythreat','Heavy Threat / Heavy Menace reference','DRIVE',
        [knob('gain','GAIN',initial=.6),knob('weight','WEIGHT'),knob('output','OUTPUT',-24,6,-12),knob('low','LOW',-12,12,0),knob('mid','MID',-12,12,0),knob('high','HIGH',-12,12,0),
         knob('gate.threshold','GATE THRESHOLD',-80,-20,-60),knob('mid.freq','MID FREQ'),choice('mode','MODE',['LITE','HEAVY','HEAVIER'],1),choice('gate.on','GATE',['OFF','ON'],1)],True))
    out = ['#pragma once', '// Generated by Tools/generate_pedal_catalog.py. Do not hand-edit panel definitions.', '#include <array>', '#include <atomic>', '#include <cmath>', '#include <juce_core/juce_core.h>', 'namespace spectralforge {', 'inline constexpr int pedalBoardCapacity=5, pedalMaxControls=12, pedalModelCount=46;', '// Models below this keep their parameters in the released layout; later ones append (PedalBoardParameters.h).', 'inline constexpr int releasedPedalModelCount=40;', 'struct PedalControlSpec { const char* id; const char* label; float minimum, maximum, initial, interval; bool connected; const char* choices; };', 'struct PedalModelSpec { const char* id; const char* name; const char* category; bool implemented; int controlCount; std::array<PedalControlSpec,pedalMaxControls> controls; };', 'inline constexpr std::array<PedalModelSpec,pedalModelCount> pedalModels{{']
    assert len(models) == len(public_names)
    for index, (key,name,category,controls,implemented) in enumerate(models):
        name = public_names[index]
        encoded=[]
        for c in controls:
            kind=c['kind']; options=c.get('options',[])
            minimum=c.get('minimum',0); maximum=c.get('maximum',len(options)-1 if options else 1)
            initial=c.get('initial',.5); interval=1 if kind in ('choice','toggle') else c.get('step',.001)
            connected=implemented
            # Variable Mu: linked stereo implementation uses the L bank. SEP/R bank
            # and AW-3 HUMAN/TEMPO are kept visible but are not falsely activated.
            if index==10 and (c['id']=='hw.link' or c['id'].startswith('hw.r.')): connected=False
            if index==15 and c['id']=='hw.mode': connected=False
            choices='|'.join(options) if options else ('OFF|ON' if kind=='toggle' else '')
            encoded.append('{'+','.join([quote(c['id']),quote(c['label']),str(float(minimum))+'f',str(float(maximum))+'f',str(float(initial))+'f',str(float(interval))+'f',str(connected).lower(),quote(choices)])+'}')
        out.append(' {'+','.join([quote(key),quote(name),quote(category),str(implemented).lower(),str(len(controls)),'{{'+','.join(encoded)+'}}'])+'},')
    out += ['}};', 'inline const PedalModelSpec& pedalModel(int model) { return pedalModels[(size_t)juce::jlimit(0,pedalModelCount-1,model)]; }', 'inline juce::String pedalControlID(int owner,int model,int control) { return "board"+juce::String(owner)+"_m"+juce::String(model)+"_"+juce::String(pedalModel(model).controls[(size_t)control].id).replaceCharacter(\'.\',\'_\'); }', 'inline juce::String pedalModelID(int owner) { return "board"+juce::String(owner)+"Model"; }', 'inline juce::String pedalBypassID(int owner,int model) { return "board"+juce::String(owner)+"_m"+juce::String(model)+"_bypass"; }', 'inline juce::String pedalOrderID(int position) { return "boardOrder"+juce::String(position); }', 'struct PedalInstanceState { int model{}; bool bypass{}; std::array<float,pedalMaxControls> controls{}; };', 'struct PedalBoardState { bool enabled{}; int lowTap{2}; std::array<int,pedalBoardCapacity> order{0,1,2,3,4}; std::array<PedalInstanceState,pedalBoardCapacity> instances{}; };', 'inline PedalInstanceState defaultPedalInstance(int model) { PedalInstanceState s; s.model=juce::jlimit(0,pedalModelCount-1,model); const auto& m=pedalModel(s.model); for(int i=0;i<m.controlCount;++i)s.controls[(size_t)i]=m.controls[(size_t)i].initial; return s; }', '// Fail closed: invalid/duplicate order entries are filled with unused owners.', 'inline void sanitisePedalBoard(PedalBoardState& state) { std::array<bool,5> seen{}; for(auto& id:state.order) { if(id<0 || id>=5 || seen[(size_t)id])id=-1; else seen[(size_t)id]=true; } for(auto& id:state.order)if(id<0)for(int j=0;j<5;++j)if(!seen[(size_t)j]) { id=j;seen[(size_t)j]=true;break; } state.lowTap=juce::jlimit(0,5,state.lowTap); for(auto& instance:state.instances) { if(instance.model<0 || instance.model>=pedalModelCount)instance.model=0; const auto& m=pedalModel(instance.model); for(int c=0;c<m.controlCount;++c) { auto& v=instance.controls[(size_t)c]; const auto& p=m.controls[(size_t)c]; v=std::isfinite(v)?juce::jlimit(p.minimum,p.maximum,v):p.initial; } } }', 'inline constexpr const char* pedalBoardDSPStatus="Experimental original DSP curves; hardware taper/circuit/capture matching is not verified. Variable Mu SEP/R controls and Dynamic Wah modes remain disabled. Spectral octaver adds 42.7-46.4 ms at supported rates and has limited low-chord tracking.";', '}']
    assert len(models)==46
    (ROOT/'Source/PedalBoardCatalog.h').write_text('\n'.join(out)+'\n')


if __name__=='__main__':
    generate()
