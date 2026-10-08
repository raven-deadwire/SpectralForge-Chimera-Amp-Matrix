#include <juce_cryptography/juce_cryptography.h>
#include "PluginProcessor.h"
#include "GateUITests.h"
#include "Fixtures/ReleasedParameterContract.h"
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
void require(bool ok,const char* message) {if(!ok)throw std::runtime_error(message);}
void set(ChimeraProcessor& p,const juce::String& id,float value)
{
    auto* parameter=p.parameters().getParameter(id);require(parameter!=nullptr,"Missing parameter");
    parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
}
float get(ChimeraProcessor& p,const juce::String& id) {return p.parameters().getRawParameterValue(id)->load();}
// Only new-session defaults approved for the 1.1.2 calibration patch may change.
// Keep the historical fixture intact; saved values, IDs and ranges stay frozen.
struct Default112 { const char* suffix; double oldValue,newValue; };
constexpr Default112 defaults112[] {
    {"m2_hw_lead_pre_gain",.5,.68}, {"m3_hw_ch2_gain",.5,.65},
    {"m3_hw_ch3_gain",.5,.68}, {"m3_hw_ch2_mode",0,.5}, {"m3_hw_ch3_mode",0,1},
    {"m4_hw_lead_gain",.5,.72}, {"m4_hw_lead_drive",.5,.65},
    {"m9_hw_dirty_gain",.5,.66}, {"m15_ch3_gain",.5,.78}, {"m15_ch4_gain",.5,.68},
    {"m15_channel",0,2./15.}, {"m17_hw_ch3_gain",.5,.78}, {"m17_hw_ch4_gain",.5,.72},
    {"m20_hw_ep_gain",.5,.72}, {"m20_hw_kk_gain1",.5,.68}, {"m20_hw_kk_gain2",.5,.62},
    {"m21_hw_overdrive_preamp",.5,.72}, {"m22_hw_lead_gain",.5,.70}
};
const Default112* approvedDefault112(const juce::String& id)
{
    static constexpr Default112 legacyZuta {"",0,2./3.};
    for(int bank=1;bank<=3;++bank) {
        if(id=="ampchannel"+juce::String(bank)+"_m15")return &legacyZuta;
    }
    for(int bank=0;bank<6;++bank)for(const auto& entry:defaults112)
        if(id=="nativeAmp_c"+juce::String(bank)+"_"+entry.suffix)return &entry;
    return nullptr;
}
void parameterContract(ChimeraProcessor& p)
{
    juce::String manifest;int count=0;
    for(auto* raw:p.getParameters()) {
        auto* parameter=dynamic_cast<juce::RangedAudioParameter*>(raw);require(parameter!=nullptr,"Parameter has no ID/range");
        if (parameter->getParameterIndex() >= 3686) break;
        require(std::isfinite(parameter->getDefaultValue()) && std::isfinite(parameter->convertFrom0to1(0.f))
                && std::isfinite(parameter->convertFrom0to1(.5f)) && std::isfinite(parameter->convertFrom0to1(1.f)),"Non-finite released parameter mapping");
        manifest+=juce::String(count++)+"|"+parameter->paramID+"|"+juce::String(parameter->getVersionHint())+"|"
            +juce::String(parameter->getDefaultValue(),9)+"|"+juce::String(parameter->getNumSteps())+"|"
            +juce::String(parameter->convertFrom0to1(0.f),9)+"|"+juce::String(parameter->convertFrom0to1(.5f),9)+"|"
            +juce::String(parameter->convertFrom0to1(1.f),9)+"|"+juce::String(int(parameter->isAutomatable()))+"\n";
    }
    const auto digest=juce::SHA256(manifest.toRawUTF8(),manifest.getNumBytesAsUTF8()).toHexString();
    juce::String frozen;
    for(const auto* chunk:releasedParameterContractChunks)frozen+=chunk;
    require(juce::SHA256(frozen.toRawUTF8(),frozen.getNumBytesAsUTF8()).toHexString()
            =="1de9d010b4267088d80a6922105b047ae0c9e142e3b931c000129d6befa3523f","Frozen released contract fixture changed");
    const auto actualRows=juce::StringArray::fromLines(manifest),expectedRows=juce::StringArray::fromLines(frozen);
    require(actualRows.size()==expectedRows.size(),"Released parameter count changed");
    int numericDifferences=0,approvedDefaults=0;
    for(int row=0;row<expectedRows.size();++row) {
        if(expectedRows[row].isEmpty()){require(actualRows[row].isEmpty(),"Unexpected contract row");continue;}
        const auto actual=juce::StringArray::fromTokens(actualRows[row],"|",{});
        const auto expected=juce::StringArray::fromTokens(expectedRows[row],"|",{});
        require(actual.size()==9 && expected.size()==9,"Malformed parameter contract row");
        for(int field=0;field<9;++field) {
            if(field==3 || (field>=5 && field<=7)) {
                // JUCE skew conversion uses exp/log. ARM and x86 libm/FMA may
                // round the same float mapping differently; preserve numeric
                // semantics without relying on nine-decimal string identity.
                const double a=actual[field].getDoubleValue();
                double e=expected[field].getDoubleValue();
                if(field==3)if(const auto* change=approvedDefault112(expected[1])) {
                    require(std::abs(e-change->oldValue)<1.e-6,"Approved default no longer matches its released value");
                    e=change->newValue;++approvedDefaults;
                }
                // Near zero, cancellation/FMA error scales with the domain,
                // not the result (e.g. ARM bass midpoint -2.68e-7 vs x86 0).
                // Normalized defaults have a unit domain; range samples use
                // their frozen endpoints/span. No host structure is relaxed.
                const double lo=expected[5].getDoubleValue(),hi=expected[7].getDoubleValue();
                const double scale=field==3 ? 1. : std::max({std::abs(lo),std::abs(hi),std::abs(hi-lo)});
                const double tolerance=5e-10+4*double(std::numeric_limits<float>::epsilon())*scale;
                if(std::abs(a-e)>tolerance) {
                    std::cerr<<"Contract mismatch "<<expected[1]<<" field="<<field<<" expected="<<juce::String(e,9)<<" actual="<<actual[field]<<'\n';
                    require(false,"Released default/range numeric contract changed");
                }
                if(a!=e){++numericDifferences;if(numericDifferences<=8)std::cout<<"ROUNDING "<<expected[1]<<" field="<<field<<" expected="<<juce::String(e,9)<<" actual="<<actual[field]<<'\n';}
            } else require(actual[field]==expected[field],"Released ID/order/version/steps/automation contract changed");
        }
    }
    require(approvedDefaults==6*int(std::size(defaults112))+3,"Approved default coverage changed");
    auto* range=p.parameters().getParameter("gateRangeDb");
    // Five-channel Original appends 342 parameters after the frozen 3981.
    // Niflheimr appends six contexts with two selectors and five 14-control banks.\n    // CAB then appends ten version-6 controls for each of the three lanes.
    constexpr int cabParameterCount=3*10;\n    constexpr int expectedParameterCount=4323+6*(2+5*14)+cabParameterCount;
    require(range && range->getParameterIndex()==3890 && count==3686 && p.getParameters().size()==expectedParameterCount,"Gate Range ordinal or complete appended channel parameter count changed");
    require(range->getVersionHint()==2 && range->isAutomatable(),"Range AU version hint/automation changed");
    require(range->getText(1,0)=="Full" && range->getValueForText("Full")==1.f,"Host Full text does not round-trip");
    require(std::abs(range->convertFrom0to1(range->getValueForText("24 dB"))-24)<.01f,"Host dB text does not parse");
    require(get(p,"gateRangeDb")==96,"New-session default is not legacy Full");
    std::cout<<"PASS legacy parameter contract: "<<count<<" entries SHA256 "<<digest<<" approved_112_defaults="<<approvedDefaults<<" numeric_rounding_differences="<<numericDifferences<<"; append-only gateRangeDb\n";
}
void stateAndAutomation(ChimeraProcessor& p)
{
    const auto rStorage=std::make_unique<ChimeraProcessor>();auto& r=*rStorage;
    const std::array<const char*,6> ids{"gateon","gatethreshold","gaterelease","gatehold","gateAfterRig","gateRangeDb"};
    const std::array<float,6> values{1,-51,137,43,1,24};
    for(size_t i=0;i<ids.size();++i)set(p,ids[i],values[i]);
    // Existing projects keep CH1 and their exact old gain even though new sessions
    // now start on CH3 and use stronger high-gain defaults.
    set(p,"ampchannel1_m15",0);set(p,"nativeAmp_c0_m15_channel",0);
    set(p,"nativeAmp_c0_m2_hw_lead_pre_gain",.5f);
    juce::MemoryBlock saved;p.getStateInformation(saved);r.setStateInformation(saved.getData(),int(saved.getSize()));
    require(get(r,"ampchannel1_m15")==0 && get(r,"nativeAmp_c0_m15_channel")==0
            && get(r,"nativeAmp_c0_m2_hw_lead_pre_gain")==.5f,"New defaults overwrote saved amp settings");
    for(size_t i=0;i<ids.size();++i)require(std::abs(get(r,ids[i])-values[i])<.011f,"Gate state did not round-trip");
    auto xml=juce::AudioProcessor::getXmlFromBinary(saved.getData(),int(saved.getSize()));
    auto old=juce::ValueTree::fromXml(*xml);old.removeChild(old.getChildWithProperty("id","gateRangeDb"),nullptr);
    for(auto slot:old.getChildWithName("COMPARISONS"))slot.removeChild(slot.getChildWithProperty("id","gateRangeDb"),nullptr);
    old.setProperty("schemaVersion",8,nullptr);juce::MemoryBlock legacy;
    juce::AudioProcessor::copyXmlToBinary(*old.createXml(),legacy);
    set(r,"gateRangeDb",0);r.setStateInformation(legacy.getData(),int(legacy.getSize()));
    require(get(r,"gateRangeDb")==96,"Missing legacy Range inherited a dirty session value");
    for(size_t i=0;i<ids.size()-1;++i)require(std::abs(get(r,ids[i])-values[i])<.011f,"Legacy gate settings changed during restore");
    r.selectComparison(1);require(get(r,"gateRangeDb")==96,"Legacy A/B slot failed Range migration");
    set(r,"gateRangeDb",12);r.selectComparison(0);require(get(r,"gateRangeDb")==96,"A/B lost Full");
    r.selectComparison(1);require(get(r,"gateRangeDb")==12,"A/B lost limited Range");
    r.getStateInformation(saved);p.setStateInformation(saved.getData(),int(saved.getSize()));
    require(get(p,"gateRangeDb")==12,"Active A/B Range did not survive project recall");
    p.selectComparison(0);require(get(p,"gateRangeDb")==96,"Inactive A/B Range did not survive project recall");
    set(r,"gateRangeDb",0);r.loadFactoryPreset(0);require(get(r,"gateRangeDb")==96,"Factory recall retained previous Range");
    struct Listener:juce::AudioProcessorParameter::Listener {
        int values{},gestures{};
        void parameterValueChanged(int,float) override {++values;}
        void parameterGestureChanged(int,bool) override {++gestures;}
    } listener;
    auto* range=r.parameters().getParameter("gateRangeDb");range->addListener(&listener);
    range->beginChangeGesture();range->setValueNotifyingHost(.25f);range->endChangeGesture();
    range->removeListener(&listener);
    require(listener.values>0 && listener.gestures==2 && get(r,"gateRangeDb")==24,"Range host notification/gesture mapping failed");
    // Real processor MIDI mapping and binary recall, without claiming a DAW host test.
    r.prepareToPlay(48000,128);juce::AudioBuffer<float> audio(2,128);audio.clear();juce::MidiBuffer midi;
    r.learnMidi("gateRangeDb");midi.addEvent(juce::MidiMessage::controllerEvent(1,17,127),0);r.processBlock(audio,midi);
    require(get(r,"gateRangeDb")==96,"MIDI cannot select Full endpoint");
    r.learnMidi("gaterelease");midi.clear();midi.addEvent(juce::MidiMessage::controllerEvent(1,18,32),0);audio.clear();r.processBlock(audio,midi);
    r.getStateInformation(saved);p.setStateInformation(saved.getData(),int(saved.getSize()));p.prepareToPlay(48000,128);
    midi.clear();midi.addEvent(juce::MidiMessage::controllerEvent(1,17,0),0);midi.addEvent(juce::MidiMessage::controllerEvent(1,18,127),1);audio.clear();p.processBlock(audio,midi);
    require(get(p,"gateRangeDb")==0 && get(p,"gaterelease")==500,"Saved MIDI IDs no longer resolve to Range/release");
    p.releaseResources();r.releaseResources();
    std::cout<<"PASS binary state, legacy missing-field migration, A/B, factory reset, host gestures and MIDI automation recall\n";
}
void configure(ChimeraProcessor& p,int mode,bool board,bool highGain,bool native)
{
    set(p,"mode",float(mode));set(p,"input",0);set(p,"output",0);set(p,"oversampling",0);
    set(p,"gateon",1);set(p,"gatethreshold",-40);set(p,"gaterelease",5);set(p,"gatehold",0);
    set(p,"boardEnabled",board?1.f:0.f);
    for(int i=0;i<5;++i)set(p,spectralforge::pedalModelID(i),0);
    for(int c=0;c<spectralforge::ampNativeContextCount;++c)set(p,spectralforge::ampNativeEnabledID(c),native?1.f:0.f);
    for(int i=0;i<3;++i) {
        const auto suffix=juce::String(i+1);set(p,"cab"+suffix,0);set(p,"cabtype"+suffix,0);
        set(p,"ampon"+suffix,highGain?1.f:0.f);set(p,"drive"+suffix,1.f);
        if(native)p.setAmpModel(i,3);else set(p,"amp"+suffix,3);
    }
    set(p,"lowampmix",1);set(p,"lowcomp",0);
}
double noiseEnergy(ChimeraProcessor& p,bool post,float range)
{
    set(p,"gateAfterRig",post?1.f:0.f);set(p,"gateRangeDb",range);p.prepareToPlay(48000,128);
    juce::AudioBuffer<float> audio(2,128);juce::MidiBuffer midi;double energy=0;
    for(int block=0;block<300;++block) {
        for(int i=0;i<128;++i) {
            const float hiss=1e-5f*std::sin(float((block*128+i)*1.713));audio.setSample(0,i,hiss);audio.setSample(1,i,-.7f*hiss);
        }
        p.processBlock(audio,midi);
        for(int c=0;c<2;++c)for(int i=0;i<128;++i) {const float x=audio.getSample(c,i);require(std::isfinite(x),"Non-finite high-gain render");if(block>220)energy+=double(x)*x;}
    }
    if(range==24)require(std::abs(juce::Decibels::gainToDecibels(p.gateMeter())+24)<.03f,"Gate meter not reporting current applied Range");
    p.releaseResources();return energy;
}
void routing()
{
    for(int mode:{0,1,2})for(bool board:{false,true}) {
        const auto storage=std::make_unique<ChimeraProcessor>();auto& p=*storage;
        configure(p,mode,board,false,false);
        const double unity=noiseEnergy(p,false,0);
        require(unity>1e-14,"Routing unity fixture is silent");
        for(bool post:{false,true}) {
            const double limited=noiseEnergy(p,post,24),full=noiseEnergy(p,post,96);
            require(std::abs(10*std::log10(limited/unity)+24)<.05,"Range missing from pre/post or board/compatibility path");
            require(full/unity<1e-14,"Full does not close actual processor routing");
        }
        for(bool native:{false,true}) {
            configure(p,mode,board,true,native);
            const double open=noiseEnergy(p,true,0),limited=noiseEnergy(p,true,24),full=noiseEnergy(p,true,96);
            require(open>1e-14 && std::abs(10*std::log10(limited/open)+24)<.06,"High-gain post attenuation altered detector or amp input");
            require(full/open<1e-14,"High-gain Full attenuation failed");
            std::cout<<"MEASURE high-gain mode="<<mode<<" board="<<board<<" native="<<native
                <<" range24_db="<<10*std::log10(limited/open)<<" full_db="<<10*std::log10(std::max(full/open,1e-30))<<" (floor -300 dB)\n";
        }
        std::cout<<"PASS actual processor synthetic hiss: mode "<<mode<<" board "<<board<<"; pre/post, Full/24 dB, compatibility/native high gain\n";
    }
}
void postTails()
{
    for(bool reverb:{false,true}) {
        const auto storage=std::make_unique<ChimeraProcessor>();auto& p=*storage;configure(p,0,true,false,false);
        set(p,"gateAfterRig",1);set(p,"gateRangeDb",96);
        if(reverb) {set(p,"reverbon",1);set(p,"reverbsize",.8f);set(p,"reverbmix",.5f);}
        else {set(p,"delayon",1);set(p,"delaytime",160);set(p,"delayfeedback",.55f);set(p,"delaymix",.5f);}
        p.prepareToPlay(48000,128);juce::AudioBuffer<float> audio(2,128);juce::MidiBuffer midi;double energy=0;
        for(int b=0;b<375;++b) {
            for(int i=0;i<128;++i)for(int c=0;c<2;++c)audio.setSample(c,i,b<40 ? .1f*std::sin(float(b*128+i)*.03f) : 0);
            p.processBlock(audio,midi);
            if(b>200) {require(p.gateMeter()<1e-8,"Tail fixture detector not closed");for(int i=0;i<128;++i)energy+=std::pow(audio.getSample(0,i),2);}
        }
        require(energy>1e-6,"Closed post-rig gate truncated POST delay/reverb tails");p.releaseResources();
    }
    std::cout<<"PASS actual processor POST delay and reverb tails remain audible with Full gate closed\n";
}
}
int main(int argc,char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    try {
        if(argc==3 && juce::String(argv[1])=="--ui") {
            const auto storage=std::make_unique<ChimeraProcessor>();const auto editorStorage=std::make_unique<ChimeraEditor>(*storage);auto& editor=*editorStorage;
            const juce::File directory{juce::String(argv[2])};require(directory.createDirectory().wasOk(),"Cannot create snapshot directory");
            checkGateUI(*storage,editor,directory);std::cout<<"PASS Gate UI at 75/100/125 percent scale\n";return 0;
        }
        const auto storage=std::make_unique<ChimeraProcessor>();parameterContract(*storage);stateAndAutomation(*storage);
        routing();postTails();
    }catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
}
