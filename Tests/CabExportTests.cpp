#include <juce_cryptography/juce_cryptography.h>
#include "PluginProcessor.h"
#include "OriginalCabParameters.h"
#include <cmath>
#include <iostream>
#include <random>
#include <stdexcept>

// CAB export: a lane's cabinet rendered offline from the current parameters,
// written as WAV and read back. The exported IR must be the cabinet: the same
// samples after the file round trip, and convolving it must reproduce what
// the cabinet copy does to arbitrary audio.
namespace {
void require(bool ok,const juce::String& message) {if(!ok)throw std::runtime_error(message.toStdString());}
void set(ChimeraProcessor& p,const juce::String& id,float value) {
    auto* q=p.parameters().getParameter(id);require(q!=nullptr,"Missing parameter "+id);q->setValueNotifyingHost(q->convertTo0to1(value));
}
constexpr double rate=48000;

double energy(const juce::AudioBuffer<float>& b,int channel) {
    double e=0;for(int n=0;n<b.getNumSamples();++n)e+=double(b.getSample(channel,n))*b.getSample(channel,n);return e;
}
double differenceDb(const juce::AudioBuffer<float>& a,const juce::AudioBuffer<float>& b,int channel) {
    require(a.getNumSamples()==b.getNumSamples(),"Length mismatch");
    double d=0;for(int n=0;n<a.getNumSamples();++n){const double e=double(a.getSample(channel,n))-b.getSample(channel,n);d+=e*e;}
    return d<=0 ? -300. : 10*std::log10(d/std::max(energy(a,channel),1e-30));
}

// Captured IRs on both mics, blended, with mic B delayed and inverted: every
// part of the cabinet path that export has to reproduce.
void capturedBlend(ChimeraProcessor& p,float blend) {
    p.loadFactoryPreset(0);
    set(p,spectralforge::originalCabID(0,"Aon"),0);set(p,spectralforge::originalCabID(0,"Bon"),0);
    set(p,"cabtype1",1);set(p,"cabBtype1",2);set(p,"cabblend1",blend);
    set(p,"cabBdelay1",.35f);set(p,"cabBinvert1",1);set(p,"cabBgain1",-3);set(p,"cablow1",90);set(p,"cabhigh1",7000);
}

void impulseResponse() {
    for(const bool modeled:{true,false}) {
        auto p=std::make_unique<ChimeraProcessor>();
        if(modeled)p->loadFactoryPreset(0); else capturedBlend(*p,.5f);
        const auto ir=p->cabinetImpulseResponse(0,rate);
        const float peak=ir.getMagnitude(0,0,ir.getNumSamples());
        require(ir.getNumSamples()>=int(rate*.01) && ir.getNumSamples()<=int(rate*2) && peak>1e-3f && std::isfinite(peak),"Cabinet IR is empty or not finite");
        const auto again=p->cabinetImpulseResponse(0,rate);
        require(again.getNumSamples()==ir.getNumSamples() && differenceDb(ir,again,0)<=-300,"Cabinet IR render is not deterministic");

        // Linear and time-invariant: noise through the cabinet copy equals the
        // noise convolved with the exported IR.
        juce::AudioBuffer<float> noise(1,int(rate*.4));std::mt19937 random{11};std::uniform_real_distribution<float> u(-.3f,.3f);
        for(int n=0;n<noise.getNumSamples();++n)noise.setSample(0,n,u(random));
        const auto rendered=p->renderCabinet(0,noise,rate);
        juce::AudioBuffer<float> convolved(1,noise.getNumSamples());convolved.clear();
        const float* h=ir.getReadPointer(0);const float* x=noise.getReadPointer(0);float* y=convolved.getWritePointer(0);
        for(int n=0;n<noise.getNumSamples();++n){double acc=0;for(int k=0;k<=std::min(n,ir.getNumSamples()-1);++k)acc+=double(h[k])*x[n-k];y[n]=float(acc);}
        juce::AudioBuffer<float> renderedLeft(1,rendered.getNumSamples());renderedLeft.copyFrom(0,0,rendered,0,0,rendered.getNumSamples());
        const double db=differenceDb(renderedLeft,convolved,0);
        std::cout<<"MEASURE "<<(modeled?"modeled":"captured blend")<<" ir_samples="<<ir.getNumSamples()<<" peak="<<peak<<" convolution_difference_db="<<db<<'\n';
        require(db<-60,"Convolving the exported IR does not reproduce the cabinet ("+juce::String(db,1)+" dB)");
    }
    std::cout<<"PASS cabinet IR: modeled and captured-blend lanes, deterministic, convolution reproduces the cabinet\n";
}

void parametersReachTheExport() {
    auto a=std::make_unique<ChimeraProcessor>(),b=std::make_unique<ChimeraProcessor>();
    capturedBlend(*a,0.f);capturedBlend(*b,1.f);
    const auto irA=a->cabinetImpulseResponse(0,rate),irB=b->cabinetImpulseResponse(0,rate);
    const int n=std::min(irA.getNumSamples(),irB.getNumSamples());
    juce::AudioBuffer<float> x(1,n),y(1,n);x.copyFrom(0,0,irA,0,0,n);y.copyFrom(0,0,irB,0,0,n);
    require(differenceDb(x,y,0)>-20,"Mic blend did not change the exported IR");
    // Mic B alone, inverted against not inverted: the same IR with the sign flipped.
    auto upright=std::make_unique<ChimeraProcessor>();capturedBlend(*upright,1.f);set(*upright,"cabBinvert1",0);
    const auto irUp=upright->cabinetImpulseResponse(0,rate);
    require(irUp.getNumSamples()==irB.getNumSamples(),"Polarity changed the IR length");
    juce::AudioBuffer<float> flipped(1,irB.getNumSamples());
    for(int k=0;k<irB.getNumSamples();++k)flipped.setSample(0,k,-irB.getSample(0,k));
    juce::AudioBuffer<float> up(1,irUp.getNumSamples());up.copyFrom(0,0,irUp,0,0,irUp.getNumSamples());
    require(differenceDb(up,flipped,0)<-100,"Mic B polarity is not in the exported IR");
    // Cabinet off: the lane passes the impulse unchanged.
    auto off=std::make_unique<ChimeraProcessor>();capturedBlend(*off,.5f);set(*off,"cab1",0);
    const auto dry=off->cabinetImpulseResponse(0,rate);
    require(std::abs(dry.getSample(0,0)-1.f)<1e-6f && dry.getMagnitude(0,1,dry.getNumSamples()-1)<1e-6f,"Cabinet off did not export a plain impulse");
    std::cout<<"PASS export follows the lane: mic blend, mic B polarity, cabinet off\n";
}

void wavRoundTrip(const juce::File& directory) {
    auto p=std::make_unique<ChimeraProcessor>();capturedBlend(*p,.5f);p->prepareToPlay(rate,256);
    const auto file=directory.getChildFile("rig1-cabinet.wav");file.deleteFile();
    const auto result=p->exportCabinetIR(0,file);
    require(result.wasOk(),"Export failed: "+result.getErrorMessage());
    const auto ir=p->cabinetImpulseResponse(0,rate);
    juce::AudioFormatManager formats;formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(file));
    require(reader!=nullptr,"Exported file is not readable");
    require(reader->sampleRate==rate && reader->bitsPerSample==32 && reader->usesFloatingPointData,"Exported file is not a 48 kHz 32-bit float WAV");
    require(reader->numChannels==1,"Identical channels were not written as mono");
    juce::AudioBuffer<float> read(1,int(reader->lengthInSamples));
    require(reader->read(&read,0,read.getNumSamples(),0,true,false),"Exported file could not be read");
    require(read.getNumSamples()==ir.getNumSamples() && differenceDb(ir,read,0)<=-300,"The WAV round trip changed the IR");
    // Chimera itself accepts the file as a project IR.
    auto q=std::make_unique<ChimeraProcessor>();
    const auto loaded=q->loadIR(0,file);
    require(loaded.wasOk(),"Chimera rejected its own exported IR: "+loaded.getErrorMessage());
    p->releaseResources();
    std::cout<<"MEASURE wav samples="<<read.getNumSamples()<<" channels="<<reader->numChannels<<"\n";
    std::cout<<"PASS WAV export: 48 kHz 32-bit float, mono, bit-identical read-back, loads as a Chimera IR\n";
}
}

int main(int argc,char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    try {
        const juce::File directory=argc>1 ? juce::File(juce::String(argv[1])) : juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("chimera-cab-export");
        require(directory.createDirectory().wasOk(),"Cannot create "+directory.getFullPathName());
        impulseResponse();parametersReachTheExport();wavRoundTrip(directory);
    }catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
    return 0;
}
