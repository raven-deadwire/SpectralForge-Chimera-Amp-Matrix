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
void factoryBank(bool measureOnly,bool originalOnly=false) {
    using namespace spectralforge;
    std::array<bool,selectablePresetCount> seen{};
    for(const int index:presetDisplayOrder()) {
        if(originalOnly&&!isOriginalPreset(index))continue;
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
            processor->parameters().replaceState(isNiflheimrPreset(index)?niflheimrPresetSnapshot(processor->parameters(),index-niflheimrPresetStart):isOriginalPreset(index)?originalPresetSnapshot(processor->parameters(),index-originalPresetStart):isGuitarSignature(index)
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
        require(std::abs(a->parameters().getRawParameterValue("output")->load())<1e-5f,"Factory recall must start at OUTPUT 0 dB");
        const bool bass=isNiflheimrPreset(index)||(index<factoryPresetCount&&juce::String(factoryPresets[size_t(index)].instrument).contains("Bass"));
        const auto audio=render(*a,bass,450),clean=render(*b,bass,450);double energy=0;float peak=0;
        require(audio.size()==clean.size(),"Factory audio fixture size differs");
        for(size_t i=0;i<audio.size();++i)require(std::abs(audio[i]-clean[i])<1e-6f,"Inactive PRE bank changed factory audio");
        for(float x:audio){energy+=double(x)*x;peak=std::max(peak,std::abs(x));}
        const double rmsDb=10*std::log10(energy/audio.size()),peakDb=20*std::log10(peak);
        const auto* name=selectablePresetName(index);
        std::cout<<"PRESET_LEVEL,"<<index<<","<<name<<","<<rmsDb<<","<<peakDb<<","<<a->parameters().getRawParameterValue("output")->load()<<'\n';
        if(measureOnly&&(index==1||index==14||index==31))std::cout<<"PRESET_DIAGNOSTIC "<<index<<" "<<a->diagnosticReport()<<'\n';
        if(!measureOnly){require(peak<.95f,"Factory preset clips at unity OUTPUT");require(rmsDb>-30,"Factory preset is unexpectedly quiet on the synthetic pluck fixture");}
        set(*b,"input",6);const auto hot=render(*b,bass,450);float hotPeak=0;
        for(float x:hot)hotPeak=std::max(hotPeak,std::abs(x));
        std::cout<<"PRESET_HOT,"<<index<<","<<20*std::log10(hotPeak)<<'\n';
        if(!measureOnly)require(hotPeak<.95f,"Factory preset clips the +6 dB input pluck fixture");
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
void originalProduction() {
    using namespace spectralforge;
    // The product dispatch must preserve the measured Original core, without
    // accidentally adding a legacy/native reference EQ or a second drive stage.
    double worst=0;
    for(double rate:{44100.,48000.,192000.,384000.})for(int preset=0;preset<originalPresetCount;++preset) {
        auto state=defaultAmpNativeState(firstOriginalAmpModel);
        const auto voice=original::channelState(preset);state.channel=preset;state.originalModern=voice.modern;
        for(size_t c=0;c<original::controlCount;++c)state.values[c]=voice.values[c];
        AmpNativeDSP product;original::OriginalAmpDSP reference;
        product.prepare(rate);product.set(state);product.reset();reference.prepare(rate);reference.set(voice);reference.reset();
        for(int n=0;n<12000;++n)for(int channel=0;channel<2;++channel) {
            const float x=channel?.013f*std::sin(float(n*.039)):.027f*std::sin(float(n*.071));
            worst=std::max(worst,double(std::abs(product.tick(x,channel)-reference.tick(x,channel))));
        }
    }
    require(worst<1e-7,"Production Original dispatch changes the calibrated core");
    const auto a=std::make_unique<ChimeraProcessor>(),b=std::make_unique<ChimeraProcessor>();
    for(int context=0;context<ampNativeContextCount;++context) {
        const int mode=context==0?0:context<3?1:2,lane=context==0?0:context<3?context-1:context-3;
        set(*a,"mode",float(mode));a->setAmpModel(lane,firstOriginalAmpModel);
        for(size_t c=0;c<original::controlCount;++c) {
            const auto& spec=original::controls[c];const float value=spec.minimum+(spec.maximum-spec.minimum)*(.2f+.1f*context);
            set(*a,ampNativeControlID(context,firstOriginalAmpModel,int(c)),value);
        }
    }
    juce::MemoryBlock bytes;a->getStateInformation(bytes);b->setStateInformation(bytes.getData(),int(bytes.getSize()));
    for(int mode=0;mode<3;++mode){set(*b,"mode",float(mode));for(int lane=0;lane<mode+1;++lane)require(b->selectedAmpModel(lane)==firstOriginalAmpModel,"Original model missing from a saved routing context");}
    for(int context=0;context<ampNativeContextCount;++context)for(size_t c=0;c<original::controlCount;++c) {
        const auto id=ampNativeControlID(context,firstOriginalAmpModel,int(c));
        require(a->parameters().getRawParameterValue(id)->load()==b->parameters().getRawParameterValue(id)->load(),"Original inactive context control changed on restore");
    }
    for(int preset=0;preset<originalPresetCount;++preset) {
        a->loadFactoryPreset(originalPresetStart+preset);a->getStateInformation(bytes);b->setStateInformation(bytes.getData(),int(bytes.getSize()));
        const auto x=render(*a),y=render(*b);double energy=0;float delta=0;
        for(size_t n=0;n<x.size();++n){delta=std::max(delta,std::abs(x[n]-y[n]));energy+=x[n]*x[n];}
        require(delta<1e-6f&&energy>1e-5,"Original preset project roundtrip changed audio");
        const int mode=int(a->parameters().getRawParameterValue("mode")->load());
        const auto gainId=ampNativeControlID(ampNativeContext(mode,0),firstOriginalAmpModel,0,a->selectedAmpChannel(0));
        const float authoredGain=a->parameters().getRawParameterValue(gainId)->load();
        a->copyComparison();a->selectComparison(1);set(*a,gainId,.1f);a->selectComparison(0);
        require(std::abs(a->parameters().getRawParameterValue(gainId)->load()-authoredGain)<.001f,"Original A/B lost the authored gain");
        const auto board=a->pedalBoardState();
        require(board.enabled && board.instances[0].model!=0 && !board.instances[0].bypass,"Original rig is missing its authored PRE chain");
        require(a->parameters().getRawParameterValue(postNativeBypassID(2,preset==1?1:preset==3?2:0))->load()<.5f,"Original rig is missing its native POST EQ");
        require(juce::String(selectablePresetName(originalPresetStart+preset))!=juce::String::fromUTF8(original::channelNames[preset]),"Old channel-named factory example remains selectable");
        const auto knob=[&](original::Control c){return a->parameters().getRawParameterValue(ampNativeControlID(ampNativeContext(mode,0),firstOriginalAmpModel,int(c),a->selectedAmpChannel(0)))->load();};
        if(preset==0)require(knob(original::Control::bloom)>=.749f,"Thall Rhythm BLOOM must start at 7.5 or higher");
        if(preset==1)require(knob(original::Control::rot)>=.749f,"Molten Lead ROT must start at 7.5 or higher");
        if(preset==4)require(std::abs(a->parameters().getRawParameterValue("lowcomp")->load()-.5f)<.001f && std::abs(a->parameters().getRawParameterValue("lowampmix")->load()-.75f)<.001f,"Slam Impact LOW DI COMP / DI-AMP mix changed");
        if(preset==2)require(mode==1 && a->selectedAmpChannel(0)==2 && a->selectedAmpChannel(1)==4 && a->parameters().getRawParameterValue("dualtype")->load()==1,"Grind rig lost its Nidhoggr/Ragnarok crossover Dual routing");
        if(preset==4)require(mode==2 && a->selectedAmpChannel(0)==0 && a->selectedAmpChannel(1)==3 && a->selectedAmpChannel(2)==4 && board.lowTap==1,"Slam rig lost split PRE or its LOW/MID/HIGH channel roles");
    }
    // A pre-1.2 project must get declared defaults, never the last edited bank.
    auto old=b->parameters().copyState();
    for(int i=old.getNumChildren()-1;i>=0;--i){const auto id=old.getChild(i).getProperty("id").toString();if(id.startsWith("originalAmp_")||(id.startsWith("nativeAmp_") && id.contains("_m24_")))old.removeChild(i,nullptr);}
    // New rigs can end in Matrix: make every context a valid pre-Original selection.
    for(int c=0;c<ampNativeContextCount;++c)old.getChildWithProperty("id",ampNativeModelID(c)).setProperty("value",2,nullptr);
    juce::AudioProcessor::copyXmlToBinary(*old.createXml(),bytes);b->setStateInformation(bytes.getData(),int(bytes.getSize()));
    require(b->selectedAmpModel(0)==2,"Old project selection changed during Original migration");
    for(int c=0;c<ampNativeContextCount;++c)require(std::abs(b->parameters().getRawParameterValue(ampNativeControlID(c,firstOriginalAmpModel,0))->load()-.5f)<.001f && b->parameters().getRawParameterValue(originalResponseID(c,0))->load()>.5f,"Pre-Original project inherited a stale Original voice");
    std::cout<<"PASS Original product dispatch residual="<<worst<<"; six contexts, five preset audio roundtrips, A/B and pre-1.2 migration\n";
}
void originalChannels() {
    using namespace spectralforge;
    const auto a=std::make_unique<ChimeraProcessor>(),b=std::make_unique<ChimeraProcessor>();
    AmpNativeParameterCache cache;cache.bind(a->parameters());
    for(int context=0;context<ampNativeContextCount;++context) {
        const int mode=context==0?0:context<3?1:2,lane=context==0?0:context<3?context-1:context-3;
        set(*a,"mode",float(mode));a->setAmpModel(lane,firstOriginalAmpModel);
        for(int channel=0;channel<original::channelCount;++channel) {
            a->setAmpChannel(lane,channel);
            for(size_t control=0;control<original::controlCount;++control) {
                const auto& spec=original::controls[control];
                const float value=spec.minimum+(spec.maximum-spec.minimum)*(.12f+.07f*channel+.04f*context);
                set(*a,ampNativeControlID(context,firstOriginalAmpModel,int(control),channel),value);
            }
            const auto state=cache.read(context);
            require(state.channel==channel && state.originalModern,"Channel did not reach the real DSP cache");
            const auto id=ampNativeControlID(context,firstOriginalAmpModel,0,channel);
            require(std::abs(state.values[0]-a->parameters().getRawParameterValue(id)->load())<1.e-6,"Channel cache reads another channel's knob memory");
        }
        const auto before=a->parameters().copyState();
        for(int channel=0;channel<original::channelCount;++channel)a->setAmpChannel(lane,channel);
        for(auto node:before) {
            const auto id=node.getProperty("id").toString();
            if(id.isEmpty()||id==ampNativeChannelID(context,firstOriginalAmpModel))continue;
            require(std::abs(a->parameters().getRawParameterValue(id)->load()-float(node.getProperty("value")))<1.e-6,"Channel switch rewrote a knob, PRE, CAB or POST state");
        }
    }
    juce::MemoryBlock bytes;a->getStateInformation(bytes);b->setStateInformation(bytes.getData(),int(bytes.getSize()));
    for(int context=0;context<ampNativeContextCount;++context)for(int channel=0;channel<original::channelCount;++channel)for(size_t c=0;c<original::controlCount;++c) {
        const auto id=ampNativeControlID(context,firstOriginalAmpModel,int(c),channel);
        require(a->parameters().getRawParameterValue(id)->load()==b->parameters().getRawParameterValue(id)->load(),"Inactive Original channel lost a knob on binary restore");
    }
    set(*a,"mode",0);a->setAmpChannel(0,1);a->copyComparison();a->selectComparison(1);a->setAmpChannel(0,4);a->selectComparison(0);
    require(a->selectedAmpChannel(0)==1,"A/B lost selected Original channel");a->selectComparison(1);require(a->selectedAmpChannel(0)==4,"A/B overwrote the other Original channel");
    // Reconstruct an incoming v1 snapshot. Missing response flags must NEVER
    // enable the new taper over the owner's existing maxed-out controls.
    auto old=a->parameters().copyState();old.setProperty("schemaVersion",8,nullptr);
    for(int i=old.getNumChildren()-1;i>=0;--i) {
        const auto id=old.getChild(i).getProperty("id").toString();
        if(id.startsWith("originalAmp_")&&id.contains("_ch"))old.removeChild(i,nullptr);
    }
    for(int context=0;context<ampNativeContextCount;++context)old.getChildWithProperty("id",ampNativeChannelID(context,firstOriginalAmpModel)).setProperty("value",0,nullptr);
    juce::AudioProcessor::copyXmlToBinary(*old.createXml(),bytes);b->setStateInformation(bytes.getData(),int(bytes.getSize()));
    AmpNativeParameterCache restored;restored.bind(b->parameters());
    for(int context=0;context<ampNativeContextCount;++context) {
        const auto state=restored.read(context);require(!state.originalModern,"Old v1 snapshot was silently revoiced");
        original::State legacy;for(size_t c=0;c<original::controlCount;++c)legacy.values[c]=state.values[c];
        AmpNativeDSP product;original::OriginalAmpDSP reference;product.prepare(48000);product.set(state);product.reset();reference.prepare(48000);reference.set(legacy);reference.reset();
        for(int n=0;n<12000;++n){const float x=.017f*std::sin(float(n*.063));require(std::abs(product.tick(x,0)-reference.tick(x,0))<1.e-7,"Legacy migration changed the audio transfer");}
        require(b->parameters().getRawParameterValue(originalResponseID(context,1))->load()>.5f,"Old recall contaminated a new inactive channel");
    }
    std::cout<<"PASS 30 independent Original channel banks / 390 knob values, channel-only switching, DSP cache, binary recall, A/B and legacy audio preservation\n";
}
void ownerOriginalReference(const juce::File& source,const juce::File& directory) {
    using namespace spectralforge;juce::MemoryBlock bytes;require(source.loadFileAsData(bytes),"Cannot read owner preset");directory.createDirectory();
    for(int channel=-1;channel<original::channelCount;++channel)for(int upper=0;upper<(channel<0?1:2);++upper) {
        const auto p=std::make_unique<ChimeraProcessor>();p->setStateInformation(bytes.getData(),int(bytes.getSize()));
        require(int(p->parameters().getRawParameterValue("mode")->load())==1,"Owner reference must use Dual");
        if(channel>=0)for(int context=1;context<=2;++context) {
            set(*p,ampNativeChannelID(context,firstOriginalAmpModel),float(channel));set(*p,originalResponseID(context,channel),1);
            auto voice=original::channelState(channel);
            voice[original::Control::midFrequency]=p->parameters().getRawParameterValue(ampNativeControlID(context,firstOriginalAmpModel,int(original::Control::midFrequency)))->load();
            if(upper)voice[original::Control::gain]=1;
            for(size_t c=0;c<original::controlCount;++c)set(*p,ampNativeControlID(context,firstOriginalAmpModel,int(c),channel),voice.values[c]);
        }
        const auto audio=render(*p,false,450);const auto name=channel<0?juce::String("owner_v1"):juce::String(original::channelKeys[channel])+(upper?"_gain_max":"_midpoint");
        require(directory.getChildFile(name+".f32").replaceWithData(audio.data(),audio.size()*sizeof(float)),"Cannot write owner comparison render");
        double energy=0;float peak=0;for(const float v:audio){energy+=double(v)*v;peak=std::max(peak,std::abs(v));}
        std::cout<<"OWNER_FULL_RIG,"<<name<<","<<10*std::log10(energy/audio.size())<<","<<20*std::log10(peak)<<"\n";
    }
}
// 48 kHz K-weighted stereo power, without R128 integration/gating. The short
// fixture uses every sample; an independent FFmpeg R128 probe is also recorded.
// Coefficients: https://ffmpeg.org/doxygen/4.1/f__ebur128_8c.html (PRE/RLB).
double channelWeightedPower(const std::vector<float>& audio) {
    using Filter=juce::dsp::IIR::Filter<double>;
    using Coefficients=juce::dsp::IIR::Coefficients<double>;
    double energy=0;
    for(size_t channel=0;channel<2;++channel) {
        Filter shelf(new Coefficients(1.53512485958697,-2.69169618940638,1.19839281085285,1.,-1.69065929318241,.73248077421585));
        Filter highpass(new Coefficients(1.,-2.,1.,1.,-1.99004745483398,.99007225036621));
        for(size_t block=0;block<audio.size();block+=256)for(size_t n=0;n<128;++n) {
            const double y=highpass.processSample(shelf.processSample(audio[block+channel*128+n]));energy+=y*y;
        }
    }
    return -.691+10*std::log10(energy/(audio.size()/2));
}
// Compare the same cabinet with bare and hot PRE/POST paths. Unweighted RMS
// alone hid the owner's remaining Fenrir advantage under driven conditions.
void originalChannelLevelProbe(const juce::File& directory,bool enforceBalance=true) {
    using namespace spectralforge;directory.createDirectory();
    for(bool driven:{false,true}) {
      double minimum=100,maximum=-100;
      for(int channel=0;channel<original::channelCount;++channel) {
        const auto p=std::make_unique<ChimeraProcessor>();p->loadFactoryPreset(originalPresetStart);
        set(*p,"output",0);set(*p,"gateon",0);
        if(!driven) {
            set(*p,"boardEnabled",0);
            for(int section=0;section<3;++section)for(int model=0;model<3;++model)set(*p,postNativeBypassID(section,model),1);
        }
        set(*p,ampNativeChannelID(0,firstOriginalAmpModel),float(channel));
        const auto voice=original::channelState(channel);
        for(size_t c=0;c<original::controlCount;++c)set(*p,ampNativeControlID(0,firstOriginalAmpModel,int(c),channel),voice.values[c]);
        const auto audio=render(*p,false,450);double energy=0;float peak=0;
        for(float v:audio){energy+=double(v)*v;peak=std::max(peak,std::abs(v));}
        const auto name=juce::String(driven?"driven_":"cab_")+original::channelKeys[channel];
        require(directory.getChildFile(name+".f32").replaceWithData(audio.data(),audio.size()*sizeof(float)),"Cannot write channel level render");
        const double rmsDb=10*std::log10(energy/audio.size()),weighted=channelWeightedPower(audio);
        minimum=std::min(minimum,weighted);maximum=std::max(maximum,weighted);
        if(enforceBalance)require(peak<1,"Default channel overloads its cabinet path");
        std::cout<<"CHANNEL_LEVEL,"<<name<<","<<rmsDb<<","<<20*std::log10(peak)<<","<<weighted<<"\n";
      }
      if(enforceBalance)require(maximum-minimum<(driven?1.6:2.5),"K-weighted channel spread exceeds the bare/driven cabinet target");
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
#include "NiflheimrPresetTests.h"
int main(int argc,char** argv) {
    juce::ScopedJuceInitialiser_GUI init;
    try {
        if(argc==3&&juce::String(argv[1])=="--niflheimr-presets"){niflheimrPresetTests::run(juce::File(argv[2]));return 0;}
        if(argc==3&&juce::String(argv[1])=="--original-channel-levels"){originalChannelLevelProbe(juce::File(argv[2]),false);return 0;}
        if(argc==4&&juce::String(argv[1])=="--owner-original-reference"){ownerOriginalReference(juce::File(argv[2]),juce::File(argv[3]));return 0;}
        if(argc>1&&juce::String(argv[1])=="--original-production-only"){originalProduction();originalChannels();return 0;}
        if(argc>1&&juce::String(argv[1])=="--measure-gain"){presetGainTests::run(true);return 0;}
        const bool originalOnly=argc>2&&juce::String(argv[2])=="--measure-original";
        const bool measureOnly=originalOnly||(argc>2&&juce::String(argv[2])=="--measure-presets");factoryBank(measureOnly,originalOnly);
        if(!measureOnly){originalProduction();originalChannels();originalChannelLevelProbe(juce::File(argc>1?argv[1]:"/tmp/chimera-signatures").getChildFile("channel-levels"));signatures(juce::File(argc>1?argv[1]:"/tmp/chimera-signatures"));gainAndGR();presetGainTests::run(false);niflheimrPresetTests::run(juce::File(argc>1?argv[1]:"/tmp/chimera-signatures").getChildFile("niflheimr-presets"));}
    }
    catch(const std::exception& error){std::cerr<<"FAIL "<<error.what()<<'\n';return 1;}
}
