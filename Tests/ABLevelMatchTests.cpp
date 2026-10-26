#include <juce_cryptography/juce_cryptography.h>
#include "PluginProcessor.h"
#include "ABLevelMatch.h"
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <vector>

// A/B level match: a monitoring cut that turns the louder A/B sound down to the
// quieter one. These tests cover the meter, the match, what it must never do
// (boost, touch the output while off, survive in a project) and the processor
// wiring.
namespace {
using spectralforge::ABLevelMatch;
void require(bool ok,const juce::String& message) {if(!ok)throw std::runtime_error(message.toStdString());}
constexpr double rate=48000;constexpr int block=256;

double rmsDb(const juce::AudioBuffer<float>& b) {
    double e=0;for(int c=0;c<b.getNumChannels();++c)for(int i=0;i<b.getNumSamples();++i)e+=double(b.getSample(c,i))*b.getSample(c,i);
    return 10*std::log10(e/(b.getNumChannels()*b.getNumSamples())+1e-30);
}
struct Sine {
    double frequency,phase{};
    void fill(juce::AudioBuffer<float>& b,float amplitude) {
        for(int i=0;i<b.getNumSamples();++i) {
            const float x=amplitude*float(std::sin(phase));phase+=juce::MathConstants<double>::twoPi*frequency/rate;
            for(int c=0;c<b.getNumChannels();++c)b.setSample(c,i,x);
        }
    }
};
// Plays one slot for some seconds; returns the RMS of the last half second.
double play(ABLevelMatch& m,int slot,float amplitude,double seconds,Sine& sine,int channels=2) {
    juce::AudioBuffer<float> b(channels,block);double tail=0;int tailBlocks=0;const int blocks=int(seconds*rate/block);
    for(int k=0;k<blocks;++k) {
        sine.fill(b,amplitude);m.process(b,slot);
        if(k>=blocks-int(.5*rate/block)){tail+=std::pow(10.,rmsDb(b)/10);++tailBlocks;}
    }
    return 10*std::log10(tail/std::max(1,tailBlocks));
}

void meter() {
    // BS.1770: a 997 Hz sine at full scale on one channel reads -3.01 LUFS.
    ABLevelMatch m;m.prepare(rate,1);Sine sine{997};
    play(m,0,1.f,4,sine,1);
    require(std::abs(m.loudness(0)-(-3.01f))<.1f,"K-weighted loudness is off: "+juce::String(m.loudness(0)));
    require(std::isnan(m.loudness(1)),"An unheard slot reported a loudness");
    // Low frequencies are weighted down (RLB high-pass), the presence region up.
    ABLevelMatch low;low.prepare(rate,1);Sine lowSine{30};play(low,0,1.f,4,lowSine,1);
    ABLevelMatch high;high.prepare(rate,1);Sine highSine{4000};play(high,0,1.f,4,highSine,1);
    require(low.loudness(0)<m.loudness(0)-1.f && high.loudness(0)>m.loudness(0)+2.f,"K-weighting shape is wrong");
    std::cout<<"MEASURE lufs 997Hz="<<m.loudness(0)<<" 30Hz="<<low.loudness(0)<<" 4kHz="<<high.loudness(0)<<'\n';
    std::cout<<"PASS meter: BS.1770 level, unheard slot unknown, K-weighting shape\n";
}

void match() {
    ABLevelMatch m;m.prepare(rate,2);m.setEnabled(true);Sine sine{220};
    const float quiet=.05f,loud=.2f; // 12 dB apart
    const double a=play(m,0,quiet,3,sine);
    require(m.appliedDb()==0.f,"Matched before the other sound was heard");
    play(m,1,loud,3,sine);
    const double b=play(m,1,loud,1,sine);
    require(std::abs(b-a)<.2,"The louder sound was not matched: "+juce::String(b-a,2)+" dB");
    require(std::abs(m.appliedDb()+12.f)<.2f,"Applied cut does not match the level difference");
    // The quieter sound plays untouched (never boosted).
    juce::AudioBuffer<float> x(2,block),y(2,block);
    play(m,0,quiet,1,sine);Sine copy=sine;sine.fill(x,quiet);copy.fill(y,quiet);m.process(y,0);
    for(int i=0;i<block;++i)require(x.getSample(0,i)==y.getSample(0,i),"The quieter sound was changed");
    require(m.appliedDb()==0.f,"The quieter sound was boosted or cut");
    std::cout<<"MEASURE match a="<<a<<" b="<<b<<" dB\n";

    // Off: bit-identical output, measurement continues.
    m.setEnabled(false);play(m,1,loud,.2,sine);
    Sine c1=sine,c2=sine;c1.fill(x,loud);c2.fill(y,loud);m.process(y,1);
    for(int i=0;i<block;++i)require(x.getSample(1,i)==y.getSample(1,i),"Output changed while level match was off");
    m.setEnabled(true);play(m,1,loud,.2,sine);
    require(std::abs(m.appliedDb()+12.f)<.3f,"Re-enabling did not use the kept measurement");

    // Silence does not move the estimates (beyond the filter's few-block decay tail).
    const float before=m.loudness(1);play(m,1,0.f,3,sine);
    require(std::abs(m.loudness(1)-before)<.05f,"Silence changed a slot's loudness: "+juce::String(m.loudness(1)-before,3)+" dB");

    // The cut is bounded.
    ABLevelMatch far;far.prepare(rate,2);far.setEnabled(true);Sine s2{220};
    play(far,0,.01f,3,s2);play(far,1,1.f,3,s2); // 40 dB apart, both above the gate
    require(std::abs(far.appliedDb()+float(ABLevelMatch::maximumCutDb))<.1f,"The cut exceeded its bound");

    // Copying A to B: they sound the same, no cut. Forgetting: no cut until re-heard.
    m.copied(0);play(m,1,quiet,.2,sine); // B now holds A's (quiet) sound
    require(std::abs(m.loudness(1)-m.loudness(0))<.5f && m.appliedDb()>-1.f,"Copy did not equalise the slots");
    m.forget();play(m,1,loud,.1,sine);
    require(std::isnan(m.loudness(0)) && m.appliedDb()>-.5f,"Forget kept a measurement or a cut");
    std::cout<<"PASS match: louder sound cut to the quieter one, never boosted, off is bit-identical, silence gated, bounded cut, copy and forget\n";
}

void set(ChimeraProcessor& p,const juce::String& id,float value) {
    auto* q=p.parameters().getParameter(id);require(q!=nullptr,"Missing parameter "+id);q->setValueNotifyingHost(q->convertTo0to1(value));
}
double render(ChimeraProcessor& p,double seconds,Sine& sine,std::vector<float>* keep=nullptr) {
    juce::AudioBuffer<float> b(2,block);juce::MidiBuffer midi;double tail=0;int tailBlocks=0;const int blocks=int(seconds*rate/block);
    for(int k=0;k<blocks;++k) {
        sine.fill(b,.05f);p.processBlock(b,midi);
        if(keep)for(int c=0;c<2;++c)for(int i=0;i<block;++i)keep->push_back(b.getSample(c,i));
        if(k>=blocks-int(.5*rate/block)){tail+=std::pow(10.,rmsDb(b)/10);++tailBlocks;}
    }
    return 10*std::log10(tail/std::max(1,tailBlocks));
}
void processor() {
    auto p=std::make_unique<ChimeraProcessor>();p->loadFactoryPreset(0);
    p->prepareToPlay(rate,block);Sine sine{110};
    // B is the same sound 6 dB louder at the output trim.
    p->selectComparison(1);set(*p,"output",-1.f);p->selectComparison(0);set(*p,"output",-7.f);
    p->abLevelMatch().setEnabled(true);
    const double a=render(*p,3,sine);
    p->selectComparison(1);render(*p,3,sine);const double b=render(*p,1,sine);
    require(std::abs(b-a)<.5,"Processor A/B level match failed: "+juce::String(b-a,2)+" dB");
    require(p->abLevelMatch().appliedDb()<-4.f,"Processor did not report the applied cut");
    std::cout<<"MEASURE processor a="<<a<<" b="<<b<<" cut="<<p->abLevelMatch().appliedDb()<<" dB\n";

    // Never saved: the project carries neither the switch nor the cut.
    juce::MemoryBlock saved;p->getStateInformation(saved);
    auto restored=std::make_unique<ChimeraProcessor>();restored->setStateInformation(saved.getData(),int(saved.getSize()));
    require(!restored->abLevelMatch().enabled(),"A restored project switched level match on");
    // Loading a project forgets the old measurements.
    p->setStateInformation(saved.getData(),int(saved.getSize()));
    p->prepareToPlay(rate,block);render(*p,.1,sine);
    require(std::isnan(p->abLevelMatch().loudness(0)) && std::isnan(p->abLevelMatch().loudness(1)),"Loading a project kept old A/B measurements");
    p->releaseResources();

    // Off (the default): bit-identical to an instance that never had it.
    auto one=std::make_unique<ChimeraProcessor>(),two=std::make_unique<ChimeraProcessor>();
    for(auto* q:{one.get(),two.get()}){q->loadFactoryPreset(0);q->prepareToPlay(rate,block);}
    two->abLevelMatch().setEnabled(true);two->abLevelMatch().setEnabled(false);
    std::vector<float> x,y;Sine s1{110},s2{110};render(*one,1,s1,&x);render(*two,1,s2,&y);
    require(x==y,"Level match changed the output while off");
    std::cout<<"PASS processor: A/B level match on the real output, not saved, forgotten on project load, off is bit-identical\n";
}
}

int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    try {meter();match();processor();}
    catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
    return 0;
}
