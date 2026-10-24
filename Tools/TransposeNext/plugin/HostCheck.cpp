// Headless VST3 host check for the research Transpose plug-in (T6 DAW prep).
// Loads the built .vst3 like a DAW does and compares its output with the
// engine called directly (the TransposeProbe configuration, block 128).
// usage: TransposeResearchHostCheck plugin.vst3 in.f32 rate shift baseline|v2|v3|v4 out.json
#include <juce_audio_utils/juce_audio_utils.h>
#include "ResearchEngine.h"
#include <fstream>
#include <random>
#include <bit>

using namespace transposeResearchPlugin;
using Signal=std::vector<float>; // stereo interleaved

namespace {
Signal readF32(const std::string& path) {
    std::ifstream f(path,std::ios::binary|std::ios::ate);
    if(!f)throw std::runtime_error("input missing");
    const auto bytes=f.tellg();if(bytes<=0 || bytes%8)throw std::runtime_error("expected stereo interleaved f32");
    Signal x(size_t(bytes)/4);f.seekg(0);f.read(reinterpret_cast<char*>(x.data()),bytes);return x;
}
size_t frames(const Signal& x){return x.size()/2;}

Signal direct(const Signal& x,double rate,int shift,Engine e) {
    // Same floating-point mode as the plug-in (and every DAW): denormals flushed.
    // Without it, v2 differs from the plug-in only at ~1e-36.
    juce::ScopedNoDenormals noDenormals;
    ResearchEngine dsp;dsp.prepare(rate,128,2);dsp.setSemitones(shift);
    juce::AudioBuffer<float> in(2,128),out(2,128);Signal y(x.size());
    for(size_t off=0;off<frames(x);off+=128) {
        const int n=int(std::min<size_t>(128,frames(x)-off));
        in.setSize(2,n,false,false,true);out.setSize(2,n,false,false,true);
        for(int i=0;i<n;++i)for(int c=0;c<2;++c)in.setSample(c,i,x[2*(off+size_t(i))+size_t(c)]);
        dsp.process(e,in,out);
        for(int i=0;i<n;++i)for(int c=0;c<2;++c)y[2*(off+size_t(i))+size_t(c)]=out.getSample(c,i);
    }
    return y;
}

juce::AudioProcessorParameter* param(juce::AudioPluginInstance& p,const juce::String& name) {
    for(auto* q:p.getParameters())if(q->getName(64)==name)return q;
    throw std::runtime_error(("parameter missing: "+name).toStdString());
}
void setParams(juce::AudioPluginInstance& p,int shift,Engine e) {
    param(p,"Shift (semitones)")->setValue(float(shift+2)/4.f);
    param(p,"Engine")->setValue(float(e==Engine::v4?2:int(e))/2.f);
    param(p,"Low upshift floor (v4)")->setValue(e==Engine::v4?1.f:0.f);
}

// Blocks come from `next()`; `onBlock` may change parameters between blocks.
template<class Next,class OnBlock>
Signal render(juce::AudioPluginInstance& p,const Signal& x,double rate,int maxBlock,Next next,OnBlock onBlock) {
    p.prepareToPlay(rate,maxBlock);
    juce::AudioBuffer<float> b(2,maxBlock);juce::MidiBuffer midi;Signal y(x.size());
    size_t off=0;int index=0;
    while(off<frames(x)) {
        const int n=int(std::min<size_t>(size_t(next()),frames(x)-off));
        onBlock(index++,off);
        b.setSize(2,n,false,false,true);
        for(int i=0;i<n;++i)for(int c=0;c<2;++c)b.setSample(c,i,x[2*(off+size_t(i))+size_t(c)]);
        p.processBlock(b,midi);
        for(int i=0;i<n;++i)for(int c=0;c<2;++c)y[2*(off+size_t(i))+size_t(c)]=b.getSample(c,i);
        off+=size_t(n);
    }
    return y;
}
template<class Next> Signal render(juce::AudioPluginInstance& p,const Signal& x,double rate,int maxBlock,Next next) {
    return render(p,x,rate,maxBlock,next,[](int,size_t){});
}
double maxDiff(const Signal& a,const Signal& b) {
    if(a.size()!=b.size())throw std::runtime_error("output length mismatch");
    double d=0;for(size_t i=0;i<a.size();++i) {
        if(!std::isfinite(a[i]) || !std::isfinite(b[i]))throw std::runtime_error("nonfinite output");
        if(a[i]==b[i] && std::bit_cast<uint32_t>(a[i])!=std::bit_cast<uint32_t>(b[i]))
            throw std::runtime_error("signed-zero bit mismatch");
        d=std::max(d,double(std::abs(a[i]-b[i])));
    }return d;
}
bool finite(const Signal& y,double& peak) {
    peak=0;for(float v:y){if(!std::isfinite(v))return false;peak=std::max(peak,double(std::abs(v)));}return true;
}
void pump(){juce::MessageManager::getInstance()->runDispatchLoopUntil(50);}
}

int main(int argc,char** argv) {
    juce::ScopedJuceInitialiser_GUI gui;
    try {
        if(argc!=7)throw std::runtime_error("usage: TransposeResearchHostCheck plugin.vst3 in.f32 rate shift baseline|v2|v3|v4 out.json");
        const juce::File plugin(juce::File::getCurrentWorkingDirectory().getChildFile(argv[1]));
        const Signal x=readF32(argv[2]);const double rate=std::stod(argv[3]);const int shift=std::stoi(argv[4]);
        const std::string en=argv[5];if(en!="baseline" && en!="v2" && en!="v3" && en!="v4")throw std::runtime_error("engine must be baseline, v2, v3 or v4");
        const Engine e=en=="baseline"?Engine::baseline:en=="v2"?Engine::v2:en=="v3"?Engine::v3:Engine::v4;
        juce::VST3PluginFormat format;juce::OwnedArray<juce::PluginDescription> found;
        format.findAllTypesForFile(found,plugin.getFullPathName());
        if(found.isEmpty())throw std::runtime_error("no VST3 plug-in found in "+plugin.getFullPathName().toStdString());
        auto create=[&]{
            juce::String error;auto p=format.createInstanceFromDescription(*found[0],rate,128,error);
            if(!p)throw std::runtime_error(("instantiation failed: "+error).toStdString());
            p->enableAllBuses();setParams(*p,shift,e);return p;
        };
        auto obj=std::make_unique<juce::DynamicObject>();bool ok=true;
        auto check=[&](const char* name,bool pass,juce::var detail){
            auto c=std::make_unique<juce::DynamicObject>();c->setProperty("pass",pass);c->setProperty("detail",detail);
            obj->setProperty(name,juce::var(c.release()));ok&=pass;
        };
        auto report=[&](const char* name,juce::var detail){obj->setProperty(name,detail);};
        const Signal ref=direct(x,rate,shift,e);
        auto p=create();
        check("legacy_engine_normalization_preserved",param(*p,"Engine")->getNumSteps()==3
              && param(*p,"Engine")->getValue()==float(e==Engine::v4?2:int(e))/2.f,
              "unchanged 0 / 0.5 / 1 mapping for product / v2 / v3; v4 uses a new opt-in bool");
        report("plugin",found[0]->name+" "+found[0]->version+" ("+found[0]->pluginFormatName+")");
        report("io",juce::String(p->getTotalNumInputChannels())+"in/"+juce::String(p->getTotalNumOutputChannels())+"out");

        // 1. Fixed 128: the plug-in wrapper must not change a single sample.
        const Signal fixed=render(*p,x,rate,128,[]{return 128;});pump();
        check("fixed_block_128_bit_identical",maxDiff(fixed,ref)==0,maxDiff(fixed,ref));
        ResearchEngine probe;probe.prepare(rate,128,2);
        check("reported_latency",p->getLatencySamples()==probe.latency(e),
              juce::String(p->getLatencySamples())+" samples (expected "+juce::String(probe.latency(e))+")");

        // 2. DAW-style variable blocks (1..512, seeded) and large fixed host blocks.
        std::mt19937 rng(20261024);std::uniform_int_distribution<int> pick(1,512);
        const Signal varied=render(*p,x,rate,512,[&]{return pick(rng);});
        check("variable_blocks_1_512_bit_identical",maxDiff(varied,ref)==0,maxDiff(varied,ref));
        // (A host may not exceed the prepared maximum: VST3 maxSamplesPerBlock.)
        auto large=std::make_unique<juce::DynamicObject>();
        for(int b:{1024,2048,4096}) {
            const Signal y=render(*p,x,rate,b,[b]{return b;});large->setProperty(juce::String(b),maxDiff(y,ref));
        }
        report("large_block_max_abs_diff",juce::var(large.release()));  // reported, known block dependence

        // 3. Re-prepare at another rate and back: no stale state.
        render(*p,x,rate==48000?96000.:48000.,128,[]{return 128;});p->releaseResources();
        const Signal again=render(*p,x,rate,128,[]{return 128;});
        check("reprepare_bit_identical",maxDiff(again,ref)==0,maxDiff(again,ref));

        // 4. State round trip into a fresh instance.
        juce::MemoryBlock state;p->getStateInformation(state);
        juce::String error;auto q=format.createInstanceFromDescription(*found[0],rate,128,error);
        if(!q)throw std::runtime_error("second instance failed");
        q->enableAllBuses();q->setStateInformation(state.getData(),int(state.getSize()));pump();
        const Signal restored=render(*q,x,rate,128,[]{return 128;});
        check("state_round_trip_bit_identical",maxDiff(restored,ref)==0,maxDiff(restored,ref));

        // A pre-v4 session has no lowUpshiftFloor parameter. Restore that
        // exact shape into a fresh instance; its old engine must stay selected.
        auto legacyXml=juce::AudioProcessor::getXmlFromBinary(state.getData(),int(state.getSize()));
        if(!legacyXml)throw std::runtime_error("invalid state XML");
        auto* component=legacyXml->getChildByName("IComponent");
        juce::MemoryBlock componentState;
        if(component==nullptr || !componentState.fromBase64Encoding(component->getAllSubText()))
            throw std::runtime_error("missing VST3 component state");
        auto processorXml=juce::AudioProcessor::getXmlFromBinary(componentState.getData(),int(componentState.getSize()));
        if(!processorXml || !processorXml->hasTagName("TransposeResearch"))throw std::runtime_error("invalid processor state");
        int removed=0;
        for(auto* child=processorXml->getFirstChildElement();child!=nullptr;) {
            auto* next=child->getNextElement();
            if(child->getStringAttribute("id")=="lowUpshiftFloor"){processorXml->removeChildElement(child,true);++removed;}
            child=next;
        }
        if(removed!=1)throw std::runtime_error("legacy fixture did not remove exactly one opt-in parameter");
        // Keep JUCE's VST3 private-state trailer byte-for-byte.
        const auto xmlBytes=size_t(juce::ByteOrder::littleEndianInt(static_cast<const char*>(componentState.getData())+4))+9;
        if(xmlBytes>componentState.getSize())throw std::runtime_error("invalid processor XML size");
        juce::MemoryBlock legacyComponent;juce::AudioProcessor::copyXmlToBinary(*processorXml,legacyComponent);
        legacyComponent.append(static_cast<const char*>(componentState.getData())+xmlBytes,componentState.getSize()-xmlBytes);
        component->deleteAllChildElements();component->addTextElement(legacyComponent.toBase64Encoding());
        juce::MemoryBlock legacyState;juce::AudioProcessor::copyXmlToBinary(*legacyXml,legacyState);
        auto legacy=format.createInstanceFromDescription(*found[0],rate,128,error);
        if(!legacy)throw std::runtime_error("legacy state instance failed");
        legacy->enableAllBuses();legacy->setStateInformation(legacyState.getData(),int(legacyState.getSize()));pump();
        const Engine legacyEngine=e==Engine::v4?Engine::v3:e;
        const Signal legacyRef=direct(x,rate,shift,legacyEngine);
        const Signal legacyRestored=render(*legacy,x,rate,128,[]{return 128;});
        check("legacy_state_without_v4_option",maxDiff(legacyRestored,legacyRef)==0
              && param(*legacy,"Low upshift floor (v4)")->getValue()==0.f,maxDiff(legacyRestored,legacyRef));

        // 5. Automation: shift changes every 50 ms and an engine switch, random blocks.
        const int every=int(rate*.05);size_t nextChange=0;int step=0;
        std::uniform_int_distribution<int> pick2(16,512);
        const Signal automated=render(*q,x,rate,512,[&]{return pick2(rng);},[&](int,size_t off){
            if(off>=nextChange){
                const int s=std::array<int,5>{-2,-1,1,2,0}[size_t(step%5)];
                param(*q,"Shift (semitones)")->setValue(float(s+2)/4.f);
                if(step==7)param(*q,"Engine")->setValue(e==Engine::baseline?.5f:0.f);
                ++step;nextChange=off+size_t(every);
            }
        });
        double peak=0,inPeak=0;finite(x,inPeak);
        const bool fin=finite(automated,peak);
        check("automation_finite_bounded",fin && peak<=4*inPeak+1e-6,
              juce::String("peak ")+juce::String(peak,5)+" vs input "+juce::String(inPeak,5)+(fin?"":" NONFINITE"));
        pump();
        obj->setProperty("all_hard_checks_pass",ok);
        std::ofstream(argv[6])<<juce::JSON::toString(juce::var(obj.release()))<<'\n';
        return ok?0:2;
    } catch(const std::exception& ex){std::cerr<<ex.what()<<'\n';return 1;}
}
