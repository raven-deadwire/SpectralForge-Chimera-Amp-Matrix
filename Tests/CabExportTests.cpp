#include <juce_cryptography/juce_cryptography.h>
#include "PluginProcessor.h"
#include "CabIRFile.h"
#include <complex>
#include <iostream>
#include <random>
#include <stdexcept>
#include <fstream>
#include <thread>

// Access the cabinets actually owned/prepared by the processor, not another
// offline copy. processBlock applies the production parameter mapping first.
struct ChimeraCabExportTestAccess {
    static spectralforge::Cab& cab(ChimeraProcessor& p,int lane) {return p.engine.cabinet(lane);}
};
namespace {
int failures=0;
double onlyRate=0;
void check(bool ok,const juce::String& why) {if(!ok){++failures;std::cerr<<"FAIL "<<why<<'\n';}}
void require(bool ok,const juce::String& why) {if(!ok)throw std::runtime_error(why.toStdString());}
void set(ChimeraProcessor& p,const juce::String& id,float v) {auto* q=p.parameters().getParameter(id);require(q!=nullptr,id);q->setValueNotifyingHost(q->convertTo0to1(v));}
juce::AudioBuffer<float> signal(int n,bool impulse) {
    juce::AudioBuffer<float> b(2,n);b.clear();std::mt19937 rng(189);std::uniform_real_distribution<float> u(-.2f,.2f);
    for(int c=0;c<2;++c)for(int i=0;i<n;++i)b.setSample(c,i,impulse?(i==0?1.f:0.f):u(rng));return b;
}
void prepare(ChimeraProcessor& p,double rate,int block) {
    p.setRateAndBufferSizeDetails(rate,block);p.prepareToPlay(rate,block);
    juce::AudioBuffer<float> silence(2,block);juce::MidiBuffer midi;
    silence.clear();p.processBlock(silence,midi);
}
juce::AudioBuffer<float> live(ChimeraProcessor& p,int lane,const juce::AudioBuffer<float>& in,int block,bool reset=true) {
    auto& cab=ChimeraCabExportTestAccess::cab(p,lane);if(reset)cab.reset();
    juce::AudioBuffer<float> out;out.makeCopyOf(in);
    for(int i=0;i<out.getNumSamples();i+=block){int count=std::min(block,out.getNumSamples()-i);float* ptr[]{out.getWritePointer(0,i),out.getWritePointer(1,i)};juce::AudioBuffer<float> part(ptr,2,count);cab.process(part);}
    return out;
}
struct Metrics {double nullDb=-300,levelDb=0,magnitudeDb=0,phaseDeg=0;int peakShift=0;};
Metrics compare(const juce::AudioBuffer<float>& ref,const juce::AudioBuffer<float>& actual,double rate,bool spectrum) {
    Metrics m;const int n=std::max(ref.getNumSamples(),actual.getNumSamples());int order=0;while((1<<order)<n)++order;
    for(int c=0;c<2;++c){double er=0,ea=0,ed=0;int pr=0,pa=0;float mr=0,ma=0;
        std::vector<std::complex<float>> r(1<<order),a(1<<order),fr(1<<order),fa(1<<order);
        for(int i=0;i<n;++i){float x=i<ref.getNumSamples()?ref.getSample(std::min(c,ref.getNumChannels()-1),i):0;float y=i<actual.getNumSamples()?actual.getSample(std::min(c,actual.getNumChannels()-1),i):0;
            er+=double(x)*x;ea+=double(y)*y;ed+=double(x-y)*(x-y);r[i]=x;a[i]=y;
            if(std::abs(x)>mr){mr=std::abs(x);pr=i;}if(std::abs(y)>ma){ma=std::abs(y);pa=i;}}
        m.nullDb=std::max(m.nullDb,10*std::log10(std::max(ed,1e-30)/(er<1e-18?1.:er)));
        m.levelDb=std::max(m.levelDb,std::abs(10*std::log10(std::max(ea,1e-30)/std::max(er,1e-30))));
        m.peakShift=std::max(m.peakShift,std::abs(pa-pr));
        if(spectrum && er>1e-18){juce::dsp::FFT fft(order);fft.perform(r.data(),fr.data(),false);fft.perform(a.data(),fa.data(),false);float peak=0;for(auto x:fr)peak=std::max(peak,std::abs(x));
            for(int k=1;k<(1<<order)/2;++k){double hz=rate*k/(1<<order);if(hz<20 || hz>rate*.45 || std::abs(fr[k])<peak*.001f)continue;
                const auto ratio=fa[k]/fr[k];m.magnitudeDb=std::max(m.magnitudeDb,std::abs(20*std::log10(std::max(double(std::abs(ratio)),1e-30))));m.phaseDeg=std::max(m.phaseDeg,std::abs(double(std::arg(ratio)))*180/juce::MathConstants<double>::pi);}}
    }return m;
}
void report(const juce::String& name,const Metrics& m,bool spectral=true) {
    std::cout<<"MEASURE "<<name<<" null_db="<<m.nullDb<<" level_db="<<m.levelDb<<" magnitude_db="<<m.magnitudeDb<<" phase_deg="<<m.phaseDeg<<" delay_samples="<<m.peakShift<<std::endl;
    check(m.nullDb<-75 && m.levelDb<.005 && (!spectral || (m.magnitudeDb<.1 && m.phaseDeg<.5 && m.peakShift==0)),name);
}
juce::AudioBuffer<float> read(const juce::File& file,double rate,int channels) {
    juce::WavAudioFormat format;std::unique_ptr<juce::AudioFormatReader> r(format.createReaderFor(file.createInputStream().release(),true));require(bool(r),"read WAV");
    check(r->sampleRate==rate && r->bitsPerSample==32 && r->usesFloatingPointData && (channels<0 || int(r->numChannels)==channels),"WAV format/rate/channel count");
    juce::AudioBuffer<float> b(int(r->numChannels),int(r->lengthInSamples));require(r->read(&b,0,b.getNumSamples(),0,true,true),"read samples");return b;
}
void fixture(const juce::File& f,double rate,bool stereo,bool longTail=false) {
    juce::AudioBuffer<float> b(stereo?2:1,longTail?int(rate):2048);b.clear();
    for(int c=0;c<b.getNumChannels();++c){b.setSample(c,13+c*31,c?-.3f:.7f);b.setSample(c,103+c*43,c?.17f:-.23f);b.setSample(c,b.getNumSamples()-1,c?-.09f:.12f);}
    juce::WavAudioFormat format;std::unique_ptr<juce::AudioFormatWriter> w(format.createWriterFor(f.createOutputStream().release(),rate,(unsigned)b.getNumChannels(),32,{},0));require(w && w->writeFromAudioSampleBuffer(b,0,b.getNumSamples()),"fixture");
}
void configure(ChimeraProcessor& p,int lane,int kind,const juce::File& mono,const juce::File& stereo,const juce::File& longIR) {
    p.loadFactoryPreset(0);const auto n=juce::String(lane+1);set(p,"mode",2);
    for(int i=0;i<3;++i){set(p,"ampon"+juce::String(i+1),0);set(p,spectralforge::originalCabID(i,"Aon"),0);set(p,spectralforge::originalCabID(i,"Bon"),0);}
    set(p,"cab"+n,1);set(p,"cabtype"+n,1);set(p,"cabBtype"+n,2);
    set(p,"cablow"+n,143);set(p,"cabhigh"+n,6100);set(p,"cabBlow"+n,337);set(p,"cabBhigh"+n,3200);
    set(p,"cabAgain"+n,-4);set(p,"cabAdelay"+n,1.37f);set(p,"cabAinvert"+n,1);
    set(p,"cabBgain"+n,-7);set(p,"cabBdelay"+n,3.73f);set(p,"cabBinvert"+n,0);
    set(p,"cabblend"+n,kind==1?0.f:kind==2?1.f:.37f);
    if(kind==0 || kind==8){set(p,spectralforge::originalCabID(lane,"Aon"),1);set(p,spectralforge::originalCabID(lane,"Bon"),1);}
    if(kind==3 || kind==4 || kind==6)require(p.loadIR(lane,kind==3?mono:kind==4?stereo:longIR).wasOk(),"user IR import");
    if(kind==4)require(p.loadMicIR(lane,1,stereo).wasOk(),"stereo B import");
    if(kind==5)set(p,"cab"+n,0);
    if(kind==7){set(p,"cabBtype"+n,1);set(p,"cabblend"+n,.5f);set(p,"cabBgain"+n,-4);set(p,"cabBdelay"+n,1.37f);set(p,"cabBlow"+n,143);set(p,"cabBhigh"+n,6100);}
}
void matrix(const juce::File& dir) {
    const auto mono=dir.getChildFile("source-mono.wav"),stereo=dir.getChildFile("source-stereo.wav"),longIR=dir.getChildFile("source-long.wav");fixture(mono,48000,false);fixture(stereo,48000,true);fixture(longIR,48000,true,true);
    int cases=0;
    for(double rate:{44100.,48000.,96000.})for(int kind=0;kind<9;++kind){if(onlyRate!=0 && rate!=onlyRate)continue;const int lane=kind%3,block=kind%3==0?64:kind%3==1?257:1024;
        const auto label=juce::String(rate,0)+"/case"+juce::String(kind)+"/lane"+juce::String(lane)+"/block"+juce::String(block);
        try {
        auto p=std::make_unique<ChimeraProcessor>();configure(*p,lane,kind,mono,stereo,longIR);prepare(*p,rate,block);
        const auto impulse=signal(int(rate*2),true),noise=signal(8192,false);
        const auto reference=live(*p,lane,impulse,block);const auto offline=p->renderCabinet(lane,impulse,rate);
        report(label+"/live-offline",compare(reference,offline,rate,true));
        const auto liveNoise=live(*p,lane,noise,block);report(label+"/noise",compare(liveNoise,p->renderCabinet(lane,noise,rate),rate,false),false);
        const auto wav=dir.getChildFile("export-"+juce::String(cases)+".wav");require(p->exportCabinetIR(lane,wav).wasOk(),"export");
        const auto exported=read(wav,rate,(kind==0 || kind==8)?-1:kind==4 || kind==6?2:1);report(label+"/live-wav",compare(reference,exported,rate,true));
        auto q=std::make_unique<ChimeraProcessor>();require(q->loadIR(lane,wav).wasOk(),"reimport "+label);set(*q,"mode",2);prepare(*q,rate,block);
        report(label+"/reload",compare(reference,live(*q,lane,impulse,block),rate,true));
        report(label+"/reload-noise",compare(liveNoise,live(*q,lane,noise,block),rate,false),false);
        // Project-embedded data must survive after the exported disk file vanishes.
        if(kind==4){juce::MemoryBlock state;q->getStateInformation(state);q->releaseResources();wav.deleteFile();auto restored=std::make_unique<ChimeraProcessor>();restored->setStateInformation(state.getData(),int(state.getSize()));prepare(*restored,rate,block);report(label+"/restore",compare(reference,live(*restored,lane,impulse,block),rate,true));restored->releaseResources();}
        // Export between two processed segments must not reset/alter live history.
        auto full=live(*p,lane,noise,block);auto& cab=ChimeraCabExportTestAccess::cab(*p,lane);cab.reset();juce::AudioBuffer<float> interrupted;interrupted.makeCopyOf(noise);
        for(int start:{0,4096}){float* ptr[]{interrupted.getWritePointer(0,start),interrupted.getWritePointer(1,start)};juce::AudioBuffer<float> segment(ptr,2,4096);auto part=live(*p,lane,segment,block,false);for(int c=0;c<2;++c)interrupted.copyFrom(c,start,part,c,0,4096);if(start==0)p->cabinetImpulseResponse(lane,rate);}
        report(label+"/history",compare(full,interrupted,rate,false),false);
        p->releaseResources();q->releaseResources();++cases;
        } catch(const std::exception& e) {++failures;std::cerr<<"FAIL "<<label<<" "<<e.what()<<std::endl;}
    }std::cout<<"CASES "<<cases<<std::endl;
}
void fileSafety(const juce::File& dir) {
    auto p=std::make_unique<ChimeraProcessor>();set(*p,"cab1",0);
    const auto target=dir.getChildFile("replace.wav");require(target.replaceWithText("old file must survive failed save"),"sentinel");
    juce::MemoryBlock bytes("partial",7);
    for(bool flushFailure:{false,true}) {
        const auto failed=spectralforge::cabIRFile::writeAtomically(target,bytes,[flushFailure](const juce::File& temp,const juce::MemoryBlock& data) {
            require(temp.replaceWithData(data.getData(),flushFailure?data.getSize():2),"partial fixture");
            return juce::Result::fail(flushFailure?"injected flush failure":"injected short write");
        });
        check(failed.failed() && target.loadFileAsString()=="old file must survive failed save","write/flush failure protects old file");
    }
    check(spectralforge::cabIRFile::needsAdditionalOverwriteConfirmation(dir.getChildFile("replace.txt")),"changed-extension overwrite needs confirmation");
    check(!spectralforge::cabIRFile::needsAdditionalOverwriteConfirmation(target),"native-confirmed name is unchanged");
    require(p->exportCabinetIR(0,target).wasOk(),"overwrite success");read(target,48000,1);
    const auto blocked=dir.getChildFile("blocked.wav");require(blocked.createDirectory().wasOk(),"blocked directory");const auto sentinel=blocked.getChildFile("keep.txt");sentinel.replaceWithText("preserve");
    check(p->exportCabinetIR(0,blocked).failed() && sentinel.loadFileAsString()=="preserve","directory target protected");
    const auto parent=dir.getChildFile("parent-file");parent.replaceWithText("preserve");check(p->exportCabinetIR(0,parent.getChildFile("bad.wav")).failed() && parent.loadFileAsString()=="preserve","invalid parent protected");
    check(dir.findChildFiles(juce::File::findFiles,true,"*_temp*").isEmpty(),"temporary files cleaned");
    std::cout<<"PASS save replacement, failed target/parent and temporary cleanup"<<std::endl;
}
}
int main(int argc,char** argv){juce::ScopedJuceInitialiser_GUI init;try{auto dir=juce::File(argc>1?juce::String(argv[1]):"/tmp/chimera-cab-regression");require(dir.createDirectory().wasOk(),"output directory");if(argc>2)onlyRate=juce::String(argv[2]).getDoubleValue();matrix(dir);fileSafety(dir);}catch(const std::exception& e){++failures;std::cerr<<"FAIL exception "<<e.what()<<std::endl;}std::cout<<"FAILURES "<<failures<<std::endl;return failures?1:0;}
