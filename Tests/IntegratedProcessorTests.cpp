#include "PluginProcessor.h"
#include "GuitarSignaturePresets.h"
#include "FactoryNativeVoicing.h"
#include "PresetOrder.h"
#include <iostream>
#include <stdexcept>
#include <iomanip>
#include <limits>
#include <map>
#include <set>

namespace {
void require(bool ok,const char* message) {if(!ok)throw std::runtime_error(message);}
void set(ChimeraProcessor& p,const juce::String& id,float value) {
    auto* parameter=p.parameters().getParameter(id);require(parameter!=nullptr,"Unknown preset/test parameter");
    parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
}
std::vector<float> render(ChimeraProcessor& p,bool bass=false,int blocks=120) {
    p.prepareToPlay(48000,128);juce::AudioBuffer<float> b(2,128);juce::MidiBuffer midi;std::vector<float> result;
    for(int block=0;block<blocks;++block) {
        for(int n=0;n<128;++n) {
            const double t=double(block*128+n)/48000;
            // Six plucks spanning low B/E through upper strings. A harmonic-rich
            // deterministic fixture checks gain staging, not real-DI acceptance.
            constexpr double bassNotes[]{30.8677,41.2034,55.,73.4162,30.8677,98.};
            constexpr double guitarNotes[]{82.4069,110.,146.8324,196.,82.4069,246.9417};
            const double noteTime=std::fmod(t,.2),fundamental=(bass?bassNotes:guitarNotes)[int(t/.2)%6];
            const float envelope=float(std::min(1.,noteTime/.003)*(.08+.92*std::exp(-noteTime*12)));
            float x=0;for(int harmonic=1;harmonic<=8;++harmonic)
                x+=envelope*.16f/float(harmonic)*std::sin(float(juce::MathConstants<double>::twoPi*fundamental*harmonic*noteTime));
            b.setSample(0,n,x);b.setSample(1,n,-.7f*x);
        }
        p.processBlock(b,midi);
        for(int c=0;c<2;++c)for(int n=0;n<128;++n) {const auto x=b.getSample(c,n);require(std::isfinite(x) && std::abs(x)<4,"Signature output invalid/unbounded");result.push_back(x);}
    }
    p.releaseResources();return result;
}
// General factory recalls preserve the actual host/APVTS performance values.
// A decimal request is not necessarily exactly representable after JUCE's
// interval snap (e.g. input=3 may be 2.99999928 with fused multiply-add).
void performanceRecall(ChimeraProcessor& p,int index) {
    using namespace spectralforge;
    const std::map<juce::String,float> requested{{"input",3},{"inputmode",1},{"tempo",143},
        {"temposync",1},{"metronome",1},{"tuneron",1},{"tunermute",0},{"tunerref",442}};
    struct Value {float raw,normalised;};
    std::map<juce::String,Value> before;
    for(const auto& [id,value]:requested) {
        set(p,id,value);auto* parameter=p.parameters().getParameter(id);
        const float actual=p.parameters().getRawParameterValue(id)->load();
        const auto& range=parameter->getNormalisableRange();
        const float tolerance=2*std::numeric_limits<float>::epsilon()*std::max(1.f,range.end-range.start);
        require(std::abs(actual-value)<=tolerance,"Performance fixture did not reach the requested setting");
        before[id]={actual,parameter->getValue()};
    }
    const auto snapshot=factoryNativeSnapshot(p.parameters(),index);
    for(int repeat=0;repeat<2;++repeat) {
        p.loadFactoryPreset(index);
        for(const auto& [id,value]:before) {
            const float afterRaw=p.parameters().getRawParameterValue(id)->load();
            const float afterNormalised=p.parameters().getParameter(id)->getValue();
            if(id=="input" || id=="tempo") {
                const auto precision=std::cout.precision();
                std::cout<<std::setprecision(std::numeric_limits<float>::max_digits10)
                    <<"PERFORMANCE_RECALL preset="<<index<<" repeat="<<repeat<<" id="<<id
                    <<" requested="<<requested.at(id)<<" before_raw="<<value.raw
                    <<" before_normalized="<<value.normalised
                    <<" snapshot_raw="<<float(snapshot.getChildWithProperty("id",id)["value"])
                    <<" after_raw="<<afterRaw<<" after_normalized="<<afterNormalised<<'\n';
                std::cout.precision(precision);
            }
            // No tolerance here: compare the actual values before recall, not
            // integer literals. Even a one-ULP recall drift must be diagnosed.
            require(afterRaw==value.raw && afterNormalised==value.normalised,
                    "Factory recall changed an actual raw/normalized performance value");
        }
    }
}
void factoryBank(bool measureOnly) {
    using namespace spectralforge;
    std::array<bool,selectablePresetCount> seen{};
    for(const int index:presetDisplayOrder()) {
        require(index>=0&&index<selectablePresetCount&&!seen[size_t(index)],"Preset display order is not a permutation");seen[size_t(index)]=true;
        require(adjacentPreset(adjacentPreset(index,1),-1)==index,"Preset navigation disagrees with category order");
        const auto a=std::make_unique<ChimeraProcessor>(),b=std::make_unique<ChimeraProcessor>();
        for(auto* raw:a->getParameters())if(auto* p=dynamic_cast<juce::RangedAudioParameter*>(raw))
            if(!factoryPerformanceParameter(p->paramID))p->setValueNotifyingHost(.81f);
        std::map<juce::String,float> previous;
        for(auto* raw:a->getParameters())if(auto* p=dynamic_cast<juce::RangedAudioParameter*>(raw))
            previous[p->paramID]=p->convertTo0to1(a->parameters().getRawParameterValue(p->paramID)->load());
        a->loadFactoryPreset(index);b->loadFactoryPreset(index);
        // Diagnostic mode can render the current header-defined voicing while
        // iterating with a cached DSP library. The normal regression always
        // exercises the actual production loadFactoryPreset entry point.
        if(measureOnly)for(auto* processor:{a.get(),b.get()})
            processor->parameters().replaceState(isGuitarSignature(index)
                ?guitarSignatureSnapshot(processor->parameters(),index-factoryPresetCount)
                :factoryNativeSnapshot(processor->parameters(),index));
        std::set<juce::String> inactive;
        if(index<factoryPresetCount)for(int owner=0;owner<pedalBoardCapacity;++owner) {
            const int selected=a->pedalBoardState().instances[size_t(owner)].model;
            for(int model=0;model<pedalModelCount;++model)if(model!=selected) {
                inactive.insert(pedalBypassID(owner,model));
                for(int c=0;c<pedalModel(model).controlCount;++c)inactive.insert(pedalControlID(owner,model,c));
            }
        }
        for(int i=0;i<a->getParameters().size();++i) {
            auto* p=dynamic_cast<juce::RangedAudioParameter*>(a->getParameters()[i]);
            require(p!=nullptr,"Factory test expected a ranged parameter");
            const bool preserved=inactive.count(p->paramID)!=0;
            const float expected=preserved?previous.at(p->paramID):b->getParameters()[i]->getValue();
            if(std::abs(p->getValue()-expected)>=1e-6f) {
                std::cerr<<"FACTORY_STATE preset="<<index<<" id="<<p->paramID<<" inactive="<<preserved
                    <<" actual="<<p->getValue()<<" expected="<<expected<<'\n';
                require(false,preserved?"Factory recall erased an inactive PRE bank":"Factory recall inherited a prior sound bank");
            }
        }
        if(index<31) {
            const int mode=int(a->parameters().getRawParameterValue("mode")->load());
            for(int lane=0;lane<(mode==0?1:mode==1?2:3);++lane)require(a->parameters().getRawParameterValue(ampNativeEnabledID(ampNativeContext(mode,lane)))->load()>.5f,"Factory amp still uses a different engine from its panel");
            for(int section=0;section<3;++section)require(a->parameters().getRawParameterValue(postNativeModeID(section))->load()>.5f,"Factory POST still uses a different engine from its panel");
            require(a->parameters().getRawParameterValue("boardEnabled")->load()>.5f,"Factory PRE still uses a different engine from its panel");
        }
        const bool bass=index<factoryPresetCount&&juce::String(factoryPresets[size_t(index)].instrument).contains("Bass");
        const auto audio=render(*a,bass,450),clean=render(*b,bass,450);double energy=0;float peak=0;
        require(audio.size()==clean.size(),"Factory audio fixture size differs");
        for(size_t i=0;i<audio.size();++i)require(std::abs(audio[i]-clean[i])<1e-6f,"Inactive PRE bank changed factory audio");
        for(float x:audio){energy+=double(x)*x;peak=std::max(peak,std::abs(x));}
        const double rmsDb=10*std::log10(energy/audio.size()),peakDb=20*std::log10(peak);
        const auto* name=isGuitarSignature(index)?guitarSignatures[size_t(index-factoryPresetCount)].name:factoryPresets[size_t(index)].name;
        std::cout<<"PRESET_LEVEL,"<<index<<","<<name<<","<<rmsDb<<","<<peakDb<<","<<a->parameters().getRawParameterValue("output")->load()<<'\n';
        if(measureOnly&&(index==1||index==14||index==31))std::cout<<"PRESET_DIAGNOSTIC "<<index<<" "<<a->diagnosticReport()<<'\n';
        if(!measureOnly){require(peak<.34f,"Factory preset lacks 9 dB nominal peak headroom");require(rmsDb>-30,"Factory preset is unexpectedly quiet on the synthetic pluck fixture");}
        set(*b,"input",6);const auto hot=render(*b,bass,450);float hotPeak=0;
        for(float x:hot)hotPeak=std::max(hotPeak,std::abs(x));
        std::cout<<"PRESET_HOT,"<<index<<","<<20*std::log10(hotPeak)<<'\n';
        require(hotPeak<.95f,"Factory preset clips the +6 dB input pluck fixture");
        if(index<factoryPresetCount)performanceRecall(*a,index);
    }
    std::cout<<"PASS factory recall and category navigation: "<<selectablePresetCount<<" presets (synthetic fixture only)\n";
}
void signatures(const juce::File& directory) {
    require(directory.createDirectory().wasOk(),"Cannot create signature evidence directory");
    for(int song=0;song<4;++song) {
        const auto a=std::make_unique<ChimeraProcessor>(),b=std::make_unique<ChimeraProcessor>();
        // Dirty every parameter bank before recall, including unrelated models.
        for(auto* raw:a->getParameters())raw->setValueNotifyingHost(.81f);
        a->loadFactoryPreset(spectralforge::factoryPresetCount+song);
        b->loadFactoryPreset(spectralforge::factoryPresetCount+song);
        for(int i=0;i<a->getParameters().size();++i) {
            const auto av=a->getParameters()[i]->getValue(),bv=b->getParameters()[i]->getValue();
            if(std::abs(av-bv)>1e-6f) {std::cerr<<"DIRTY "<<a->getParameters()[i]->getName(100)<<" a="<<av<<" b="<<bv<<'\n';require(false,"Signature inherited a dirty parameter bank");}
        }
        const auto snapshot=spectralforge::guitarSignatureSnapshot(a->parameters(),song);
        int params=0;for(auto child:snapshot)if(child.hasProperty("id") && child.hasProperty("value"))++params;
        require(params==a->getParameters().size(),"Full signature snapshot omitted APVTS parameters");
        require(snapshot.getChildWithName("GUITAR_SIGNATURE").getProperty("id").toString()==spectralforge::guitarSignatures[size_t(song)].id,"Signature identity missing");
        auto xml=snapshot.createXml();require(xml->writeTo(directory.getChildFile(juce::String(song)+".xml")),"Snapshot export failed");
        juce::MemoryBlock state;a->getStateInformation(state);b->setStateInformation(state.getData(),int(state.getSize()));
        for(int i=0;i<a->getParameters().size();++i)require(std::abs(a->getParameters()[i]->getValue()-b->getParameters()[i]->getValue())<1e-6f,"Signature binary project restore differs");
        const auto first=render(*a),restored=render(*b);double energy=0;float delta=0;
        for(size_t i=0;i<first.size();++i) {delta=std::max(delta,std::abs(first[i]-restored[i]));energy+=double(first[i])*first[i];}
        require(delta<1e-6f && energy>1e-5,"Signature saved-state synthetic audio roundtrip failed");
        a->copyComparison();a->selectComparison(1);a->selectComparison(0);
        require(a->parameters().state.getChildWithName("GUITAR_SIGNATURE").getProperty("id").toString()==spectralforge::guitarSignatures[size_t(song)].id,"Signature metadata lost in A/B");
        std::cout<<"PASS full-state "<<spectralforge::guitarSignatures[size_t(song)].name<<" parameters="<<params<<" audio_delta="<<delta<<" energy="<<energy<<" synthetic only\n";
    }
}
void gainAndGR() {
    for(bool board:{false,true})for(int mode:{0,1,2}) {
        const auto p=std::make_unique<ChimeraProcessor>();
        set(*p,"gateon",0);set(*p,"input",0);set(*p,"output",0);set(*p,"mode",float(mode));set(*p,"boardEnabled",board?1.f:0.f);set(*p,"oversampling",0);set(*p,"lowampmix",1);
        for(int lane=1;lane<=3;++lane){set(*p,"ampon"+juce::String(lane),0);set(*p,"cab"+juce::String(lane),0);}
        p->prepareToPlay(48000,128);juce::AudioBuffer<float> audio(2,128);juce::MidiBuffer midi;double input=0,output=0;
        for(int block=0;block<300;++block) {
            for(int n=0;n<128;++n) {const float x=.2f*std::sin(float(block*128+n)*.07f);audio.setSample(0,n,x);audio.setSample(1,n,-.7f*x);if(block>100)input+=double(x)*x;}
            p->processBlock(audio,midi);if(block>100)for(int n=0;n<128;++n)output+=std::pow(audio.getSample(0,n),2);
        }
        const auto db=10*std::log10(output/input);require(std::abs(db)<.05,"Unity routing unexpectedly loses level");
        require(p->getLatencySamples()<p->pitchLatency(),"Zero-shift/bypass retains pitch-window latency");p->releaseResources();
        std::cout<<"MEASURE unity mode="<<mode<<" board="<<board<<" delta_db="<<db<<" zero_shift_reported="<<p->getLatencySamples()<<'\n';
    }
    for(int model=6;model<=10;++model) {
        const auto p=std::make_unique<ChimeraProcessor>();set(*p,"gateon",0);set(*p,"output",0);p->setPedalModel(0,model);
        for(int c=0;c<spectralforge::pedalModel(model).controlCount;++c)set(*p,spectralforge::pedalControlID(0,model,c),spectralforge::pedalModel(model).controls[size_t(c)].maximum);
        p->prepareToPlay(48000,128);juce::AudioBuffer<float> audio(2,128);juce::MidiBuffer midi;
        for(int block=0;block<100;++block){for(int c=0;c<2;++c)for(int n=0;n<128;++n)audio.setSample(c,n,.5f);p->processBlock(audio,midi);}
        require(p->pedalReduction()>.1f && std::isfinite(p->pedalReduction()),"Compressor DSP GR did not reach processor meter");
        set(*p,spectralforge::pedalBypassID(0,model),1);for(int i=0;i<20;++i){audio.clear();p->processBlock(audio,midi);}
        require(p->pedalReduction()==0,"Bypassed compressor retains stale GR");p->releaseResources();
        std::cout<<"PASS processor pedal GR model="<<model<<" active and bypass\n";
    }
}
}
#include "PresetGainTests.h"
int main(int argc,char** argv) {
    juce::ScopedJuceInitialiser_GUI init;
    try {
        if(argc>1&&juce::String(argv[1])=="--measure-gain"){presetGainTests::run(true);return 0;}
        const bool measureOnly=argc>2&&juce::String(argv[2])=="--measure-presets";factoryBank(measureOnly);
        if(!measureOnly){signatures(juce::File(argc>1?argv[1]:"/tmp/chimera-signatures"));gainAndGR();presetGainTests::run(false);}
    }
    catch(const std::exception& error){std::cerr<<"FAIL "<<error.what()<<'\n';return 1;}
}
