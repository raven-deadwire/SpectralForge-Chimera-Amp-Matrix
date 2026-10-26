// Headless VST3 host check for the research Transpose plug-in (T6 DAW prep).
// Loads the built .vst3 like a DAW does and compares its output with the
// engine called directly (the TransposeProbe configuration, block 128).
// usage: TransposeResearchHostCheck plugin.vst3 in.f32 rate shift baseline|v2|v3 out.json
#include <juce_audio_utils/juce_audio_utils.h>
#include "ResearchEngine.h"
#include <fstream>
#include <random>

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
    param(p,"Engine")->setValue(float(int(e))/2.f);
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
    double d=0;for(size_t i=0;i<a.size();++i)d=std::max(d,double(std::abs(a[i]-b[i])));return d;
}
bool finite(const Signal& y,double& peak) {
    peak=0;for(float v:y){if(!std::isfinite(v))return false;peak=std::max(peak,double(std::abs(v)));}return true;
}
void pump(){juce::MessageManager::getInstance()->runDispatchLoopUntil(50);}
}

int main(int argc,char** argv) {
    juce::ScopedJuceInitialiser_GUI gui;
    try {
        if(argc!=7)throw std::runtime_error("usage: TransposeResearchHostCheck plugin.vst3 in.f32 rate shift baseline|v2|v3 out.json");
        const juce::File plugin(juce::File::getCurrentWorkingDirectory().getChildFile(argv[1]));
        const Signal x=readF32(argv[2]);const double rate=std::stod(argv[3]);const int shift=std::stoi(argv[4]);
        const std::string en=argv[5];if(en!="baseline" && en!="v2" && en!="v3")throw std::runtime_error("engine must be baseline, v2 or v3");
        const Engine e=en=="baseline"?Engine::baseline:en=="v2"?Engine::v2:Engine::v3;
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
        report("plugin",found[0]->name+" "+found[0]->version+" ("+found[0]->pluginFormatName+")");
        report("io",juce::String(p->getTotalNumInputChannels())+"in/"+juce::String(p->getTotalNumOutputChannels())+"out");

        // 1. Fixed 128: the plug-in wrapper must not change a single sample.
        const Signal fixed=render(*p,x,rate,128,[]{return 128;});pump();
        check("fixed_block_128_bit_identical",maxDiff(fixed,ref)==0,maxDiff(fixed,ref));
        ResearchEngine probe;probe.prepare(rate,128,2);
        check("reported_latency",p->getLatencySamples()==probe.latency(e,false,rate),
              juce::String(p->getLatencySamples())+" samples (expected "+juce::String(probe.latency(e,false,rate))+")");

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
        // 6. Fixed-PDC mode: the report, the output (unchanged by the mode) and
        //    host bypass delayed by exactly the reported latency.
        {
            juce::String err;auto r=format.createInstanceFromDescription(*found[0],rate,128,err);
            if(!r)throw std::runtime_error("third instance failed");
            r->enableAllBuses();setParams(*r,shift,e);param(*r,"Latency report")->setValue(1.f);
            const Signal pdcOut=render(*r,x,rate,128,[]{return 128;});pump();
            const int expect=probe.latency(e,true,rate);
            check("fixed_pdc_reported_latency",r->getLatencySamples()==expect,
                  juce::String(r->getLatencySamples())+" samples (expected "+juce::String(expect)+")");
            check("fixed_pdc_output_unchanged",maxDiff(pdcOut,ref)==0,maxDiff(pdcOut,ref));
            r->prepareToPlay(rate,128);
            juce::AudioBuffer<float> b(2,128);juce::MidiBuffer m;int found_at=-1;
            const int blocks=expect/128+3;
            for(int k=0;k<blocks && found_at<0;++k) {
                b.clear();if(k==0){b.setSample(0,5,1.f);b.setSample(1,5,1.f);}
                r->processBlockBypassed(b,m);
                for(int i=0;i<128;++i)if(std::abs(b.getSample(0,i))>.5f){found_at=k*128+i-5;break;}
            }
            check("bypass_delay_equals_reported_latency",found_at==r->getLatencySamples(),
                  juce::String(found_at)+" samples (reported "+juce::String(r->getLatencySamples())+")");
        }
        pump();
        obj->setProperty("all_hard_checks_pass",ok);
        std::ofstream(argv[6])<<juce::JSON::toString(juce::var(obj.release()))<<'\n';
        return ok?0:2;
    } catch(const std::exception& ex){std::cerr<<ex.what()<<'\n';return 1;}
}
