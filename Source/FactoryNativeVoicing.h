#pragma once
#include "FactoryPresets.h"
#include "FactoryPresetLevels.h"
#include "AmpNativeParameters.h"
#include "PedalBoardParameters.h"
#include "PostNativeParameters.h"

namespace spectralforge {
// Authored starting tones, indexed by stable Factory ID. These are knob
// positions selected for each musical role, not hardware measurements. Master
// unity, disabled effects and deliberately flat correction bands stay neutral.
struct FactoryTone {float bass,mid,treble,presence,resonance;};
inline constexpr std::array<FactoryTone,31> factoryTones{{
    {.42f,.58f,.62f,.45f,.50f}, // Clean Sustain: body without boom, clear top
    {.36f,.57f,.57f,.58f,.48f}, // Tight Rhythm: low cut, forward attack
    {.53f,.57f,.52f,.50f,.50f}, // Bass Matrix: solid dry low, defined mids
    {.40f,.65f,.55f,.50f,.48f}, // Filter Lead: vocal mids, restrained fizz
    {.39f,.58f,.48f,.44f,.48f}, // Fuzz Texture: dark top, audible mid body
    {.40f,.47f,.66f,.50f,.50f}, // Bell Clean: bright, lean, bell-like
    {.41f,.58f,.62f,.54f,.50f}, // Edge Chime: open upper mids
    {.43f,.63f,.57f,.57f,.52f}, // Classic Crunch: thick midrange
    {.46f,.59f,.51f,.48f,.53f}, // Orange Heavy: low-mid weight
    {.34f,.55f,.59f,.60f,.46f}, // Melodic Death: tight, articulate
    {.38f,.64f,.54f,.48f,.47f}, // Melodic Lead: singing, rounded top
    {.44f,.53f,.57f,.43f,.50f}, // Ambient Clean: warm direct body
    {.57f,.58f,.43f,.50f,.50f}, // Finger Round: warm low mids
    {.49f,.60f,.55f,.50f,.50f}, // Pick Punch: upper-mid definition
    {.56f,.40f,.60f,.50f,.50f}, // Slap Studio: restrained scoop
    {.48f,.56f,.54f,.50f,.50f}, // Modern Grind: mid-forward drive
    {.55f,.60f,.43f,.50f,.50f}, // Vintage Bass DI: full, dark top
    {.55f,.52f,.40f,.50f,.50f}, // Wool Bass Fuzz: weight over fizz
    {.54f,.52f,.48f,.50f,.50f}, // Bass Envelope: dry-low foundation
    {.43f,.57f,.59f,.53f,.50f}, // Clean/Crunch: clear layered mids
    {.36f,.58f,.56f,.57f,.48f}, // Tight/Wide: tighter bass in both rigs
    {.54f,.55f,.49f,.54f,.46f}, // Low Anchor: weight + upper definition
    {.55f,.53f,.57f,.50f,.50f}, // Air/Weight: round low, open high
    {.55f,.58f,.51f,.50f,.50f}, // Warm/Definition: warm but readable
    {.53f,.58f,.52f,.50f,.50f}, // Clean/Grind: intact low and bite
    {.56f,.54f,.46f,.50f,.50f}, // Low B: subdued treble
    {.51f,.59f,.57f,.50f,.50f}, // Pick Attack: focused upper mids
    {.47f,.56f,.53f,.48f,.48f}, // Spectral Texture: balanced split body
    {.42f,.55f,.61f,.52f,.50f}, // Matchless Edge: articulate chime
    {.40f,.66f,.52f,.46f,.47f}, // Dumble Lead: mid sustain, soft top
    {.54f,.53f,.55f,.50f,.50f}, // EICH: clean definition, modest lift
}};

// New factory recalls use the same engines their panels edit. This deliberately
// revoices the factory bank; loading an existing project never calls this code.
template<class Getter,class Setter>
void voiceFactoryNative(int index,Getter get,Setter set) {
    if(index<0 || index>=31)return;
    const int mode=int(get("mode")),lanes=mode==0?1:mode==1?2:3;
    for(int lane=0;lane<lanes;++lane) {
        const auto suffix=juce::String(lane+1);
        const int model=int(get("amp"+suffix)),context=ampNativeContext(mode,lane);
        auto s=defaultAmpNativeState(model);const float drive=get("drive"+suffix);
        s.outputLevelDb=get("level"+suffix);
        const auto tone=factoryTones[size_t(index)];
        const auto position=[](float x){return juce::jlimit(.2f,.8f,x);};
        const float bass=position(tone.bass+get("bass"+suffix)/48.f),mid=position(tone.mid+(get("lowmid"+suffix)+get("highmid"+suffix))/96.f);
        const float treble=position(tone.treble+get("treble"+suffix)/48.f),presence=tone.presence;
        const auto k=[&](const char* key,float value){const int c=ampNativeControlIndex(model,key);jassert(c>=0);if(c>=0)s.values[size_t(c)]=value;};
        const auto eq=[&](const char* b,const char* m,const char* t){k(b,bass);k(m,mid);k(t,treble);};
        const float highGain=juce::jlimit(.4f,.72f,.35f+.5f*drive);
        switch(model) {
        case 0:k("hw.normal.volume",.42f+.3f*drive);eq("hw.normal.bass","hw.normal.middle","hw.normal.treble");break;
        case 1:k("hw.high_treble",.25f+.65f*drive);eq("hw.bass","hw.middle","hw.treble");k("hw.presence",presence);break;
        case 2:s.channel=1;k("hw.lead.pre_gain",highGain);eq("hw.low","hw.mid","hw.high");k("hw.presence",presence);k("hw.resonance",tone.resonance);break;
        case 3:s.channel=2;k("hw.ch3.gain",highGain);k("hw.ch3.mode",2);eq("hw.ch3.bass","hw.ch3.mid","hw.ch3.treble");k("hw.ch3.presence",presence);break;
        case 4:s.channel=2;k("hw.lead.gain",highGain);k("hw.lead.drive",.56f);eq("hw.lead.bass","hw.lead.middle","hw.lead.treble");k("hw.lead.presence",presence);k("hw.reverb",0);break;
        case 5:k("hw.ch1.volume",.4f+.5f*drive);eq("hw.ch1.bass","hw.ch1.midrange","hw.ch1.treble");break;
        case 6:k("hw.volume",.45f);k("hw.boost",drive*.6f);eq("hw.bass","hw.lo_mid","hw.treble");k("hw.hi_mid",position(tone.mid+get("highmid"+suffix)/48.f));break;
        case 7:k("hw.b7k.distortion",1);k("hw.b7k.drive",.3f+.5f*drive);k("hw.b7k.blend",.55f);eq("hw.b7k.bass","hw.b7k.lo_mids","hw.b7k.treble");k("hw.b7k.hi_mids",position(tone.mid+get("highmid"+suffix)/48.f));break;
        case 8:s.channel=drive<.1f?0:1;k(s.channel==0?"hw.normal.volume":"hw.top_boost.volume",.35f+.65f*drive);k("hw.top_boost.bass",bass);k("hw.top_boost.treble",treble);k("hw.tone_cut",index==5?.36f:.43f);k("hw.reverb_level",0);k("hw.tremolo_depth",0);break;
        case 9:s.channel=1;k("hw.dirty.gain",highGain);eq("hw.dirty.bass","hw.dirty.middle","hw.dirty.treble");k("hw.reverb",0);break;
        case 10:s.channel=0;k("hw.vintage.volume",.42f+.45f*drive);eq("hw.vintage.bass","hw.vintage.mid","hw.vintage.treble");break;
        case 11:k("hw.input",.5f);k("hw.high_pass",0);k("hw.voicing",index==14?.20f:.08f);k("hw.lo_mid_frequency",index==14?.28f:.40f);k("hw.hi_mid_frequency",index==14?.67f:.52f);eq("hw.bass","hw.lo_mid","hw.treble");k("hw.hi_mid",position(tone.mid+get("highmid"+suffix)/48.f));break;
        case 12:s.channel=0;k("hw.ch1.volume",.3f+.65f*drive);k("hw.ch1.bass",bass);k("hw.ch1.treble",treble);k("hw.cut",.40f);break;
        case 13:s.channel=1;k("hw.volume",.5f);k("hw.od.drive",highGain);k("hw.od.ratio",.5f);eq("hw.bass","hw.middle","hw.treble");k("hw.presence",presence);break;
        case 14:k("hw.gain",.46f);k("hw.taste",.46f);k("hw.lo",bass);k("hw.lo_mid",mid);k("hw.hi_mid",position(tone.mid+get("highmid"+suffix)/48.f));k("hw.hi",treble);break;
        default:break;
        }
        sanitiseAmpNativeState(s);
        set(ampNativeEnabledID(context),1);set(ampNativeModelID(context),float(model));
        set(ampNativeChannelID(context,model),float(s.channel));set(ampNativeRouteID(context,model),float(s.inputRoute));
        set(ampNativeInputTrimID(context),0);set(ampNativeOutputLevelID(context),s.outputLevelDb);
        for(size_t c=0;c<ampNativePanel(model).controls.size();++c)set(ampNativeControlID(context,model,int(c)),s.values[c]);
    }
    set("boardEnabled",1);set("boardLowTap",2);
    constexpr const char* models[]{"compmodel","filtermodel","fuzzmodel","boostmodel","drivemodel"};
    constexpr const char* enabled[]{"precompon","filteron","fuzzon","booston","preon"};
    constexpr int first[]{6,11,16,21,1};
    for(int owner=0;owner<5;++owner) {
        const int model=first[owner]+int(get(models[owner]));auto p=defaultPedalInstance(model);
        auto& v=p.controls;
        const auto level=[](float db){return juce::jlimit(0.f,1.f,.5f*std::pow(10.f,db/40.f));};
        if(owner==0){if(model==6){v[4]=.22f+.35f*get("precomp");v[2]=level(get("precomplevel"));}else if(model==7){v[1]=get("precomp");v[0]=level(get("precomplevel"));}else if(model==8){v[0]=get("precomp");v[2]=level(get("precomplevel"));}else if(model==9){v[0]=.22f+.35f*get("precomp");v[1]=level(get("precomplevel"));v[2]=.15f;v[3]=.15f;}else{v[0]=1.f/3.f;v[2]=get("precomp");v[5]=level(get("precomplevel"));}}
        if(owner==0) {
            if(model==6){v[0]=.42f;v[1]=index==14?.35f:.72f;v[3]=0;}
            else if(model==8){v[1]=index==12?.44f:index==28?.56f:.52f;v[3]=1;}
            else if(model==9){v[4]=.16f;v[5]=.44f;}
            else if(model==10){v[3]=.36f;v[4]=2;v[6]=0;}
        }
        if(owner==1){if(model==11){v[4]=get("filtersense");v[3]=(get("filterq")-.5f)/5.5f;}else if(model==14){v[0]=(1-get("filtermix"))*.5f;v[1]=get("filtermix")*.5f;v[3]=(get("filterq")-.5f)/5.5f;v[4]=get("filtersense");}else if(model==15)v[2]=get("filtersense");else{v[0]=get("filtersense");v[1]=(get("filterq")-.5f)/5.5f;}}
        if(owner==2){v[0]=level(get("fuzzlevel"));v[model==16?2:model<=18?1:3]=get("fuzzdrive")/36.f;if(model==16||model==19)v[1]=get("fuzztone");if(model==19)v[2]=.12f;}
        if(owner==3){v[0]=get("boostgain")/24.f;if(model==21){v[1]=.5f;v[2]=.5f+get("boosttreble")/24.f;v[3]=.5f+get("boostbass")/24.f;}}
        if(owner==4){const float tone=juce::jlimit(0.f,1.f,std::log(get("pretone")/700.f)/std::log(18.f));v[0]=get("predrive");v[1]=model==3?1-tone:tone;v[2]=level(get("prelevel"));if(model==4){v[0]=level(get("prelevel"));v[1]=.6f;v[2]=.5f;v[6]=get("predrive");}else if(model==5){v[0]=.55f;v[3]=get("predrive");}}
        set(pedalModelID(owner),float(model));set(pedalBypassID(owner,model),get(enabled[owner])>.5f?0.f:1.f);set(pedalOrderID(owner),float(owner));
        for(int c=0;c<pedalModel(model).controlCount;++c){const auto& spec=pedalModel(model).controls[size_t(c)];set(pedalControlID(owner,model,c),juce::jlimit(spec.minimum,spec.maximum,v[size_t(c)]));}
    }
    if(get("preorder")>.5f){set(pedalOrderID(0),1);set(pedalOrderID(1),0);}
    if(get("gainorder")>.5f){set(pedalOrderID(3),4);set(pedalOrderID(4),3);}
    // Factory rack voicings are explicit native banks, including bypassed ones.
    // Spatial effects retain their single engine and existing authored settings.
    constexpr const char* postModels[]{"busmodel","preampmodel","eqmodel"};
    constexpr const char* postOn[]{"buscompon","preampon","eqon"};
    for(int section=0;section<3;++section) {
        const int model=int(get(postModels[section]));const auto& spec=postNativeModel(section,model);
        auto state=defaultPostNativeState();auto bank=state.sections[section].banks[model];
        const auto k=[&](const char* key,float value){for(int c=0;c<spec.controlCount;++c)if(juce::String(key)==spec.controls[c].id){bank.values[c]=juce::jlimit(spec.controls[c].minimum,spec.controls[c].maximum,value);return;}jassertfalse;};
        if(section==0) {
            if(model==0){k("threshold",get("busthreshold"));k("makeup",get("busmakeup"));k("ratio",get("busratio")<=2?0:get("busratio")<=4?1:2);k("attack",5);k("release",0);}
            if(model==1){k("input",-6);k("output",get("busmakeup"));k("attack",.2f);k("release",.55f);}
            if(model==2){k("peak_reduction",25);k("gain",get("busmakeup"));}
        } else if(section==1) {
            if(model==0){const float drive=5*std::round(get("preampdrive")/5);k("gain",2+drive/5);bank.levelDb=-drive;}
            if(model==1){const float drive=2*std::round(get("preampdrive")/2);k("boost",drive/2);bank.levelDb=-drive;}
            if(model==2){k("input",1);k("gain",2);k("trim",get("preampdrive"));bank.levelDb=-get("preampdrive");}
        } else {
            if(model==0){k("lf_gain",get("eqlow"));k("lf_frequency",80);k("hmf_frequency",get("eqmidhz"));k("hmf_gain",get("eqmid"));k("hmf_q",get("eqq"));k("hf_gain",get("eqhigh"));}
            if(model==1){k("lf_gain",get("eqlow"));k("lf_frequency",2);k("mf_gain",get("eqmid"));k("mf_frequency",2);k("hf_gain",get("eqhigh"));}
            if(model==2){k("lf_frequency",2);k("lf_boost",std::max(0.f,get("eqlow")));k("lf_atten",std::max(0.f,-get("eqlow")));k("hf_boost",std::max(0.f,get("eqhigh")));k("hf_atten",std::max(0.f,-get("eqhigh")));}
        }
        set(postNativeModeID(section),1);set(postNativeModelID(section),float(model));set(postNativeBypassID(section,model),get(postOn[section])>.5f?0.f:1.f);
        set(postNativeTrimID(section,model),bank.trimDb);set(postNativeLevelID(section,model),bank.levelDb);
        for(int c=0;c<spec.controlCount;++c)set(postNativeControlID(section,model,c),bank.values[c]);
    }
}
inline bool factoryPerformanceParameter(const juce::String& id) {
    return id=="input" || id=="inputmode" || id=="tempo" || id=="temposync" || id=="metronome"
        || id=="tuneron" || id=="tunermute" || id=="tunerref";
}
inline juce::ValueTree factoryNativeSnapshot(juce::AudioProcessorValueTreeState& state,int index) {
    if(index<0 || index>=factoryPresetCount)return {};
    auto snapshot=state.copyState();
    snapshot.removeChild(snapshot.getChildWithName("GUITAR_SIGNATURE"),nullptr);
    for(auto* raw:state.processor.getParameters())if(auto* p=dynamic_cast<juce::RangedAudioParameter*>(raw)) {
        if(factoryPerformanceParameter(p->paramID))continue;
        auto node=snapshot.getChildWithProperty("id",p->paramID);
        if(!node.isValid()){node=juce::ValueTree("PARAM");node.setProperty("id",p->paramID,nullptr);snapshot.appendChild(node,nullptr);}
        node.setProperty("value",p->convertFrom0to1(p->getDefaultValue()),nullptr);
    }
    const auto set=[&](const juce::String& id,float value){auto* p=state.getParameter(id);jassert(p);if(p)snapshot.getChildWithProperty("id",id).setProperty("value",p->convertFrom0to1(p->convertTo0to1(value)),nullptr);};
    const auto get=[&](const juce::String& id){const auto node=snapshot.getChildWithProperty("id",id);jassert(node.isValid());return float(node.getProperty("value"));};
    applyFactoryPreset(index,set);voiceFactoryNative(index,get,set);set("output",factoryOutputDb[size_t(index)]);
    return snapshot;
}
} // namespace spectralforge
