#include <juce_cryptography/juce_cryptography.h>
#include "PluginProcessor.h"
#include "GuitarSignaturePresets.h"
#include "OriginalPresets.h"
#include "NiflheimrPresets.h"
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <vector>

// "Settings restored" is not "sound preserved": every recall path must render
// the same audio as the sound it recalls. Each render starts from a fresh
// prepareToPlay with an identical fixture, so any difference is recall state.
namespace {
void require(bool ok,const juce::String& message) {if(!ok)throw std::runtime_error(message.toStdString());}
void set(ChimeraProcessor& p,const juce::String& id,float value)
{
    auto* parameter=p.parameters().getParameter(id);require(parameter!=nullptr,"Missing parameter "+id);
    parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
}

constexpr double rate=48000;constexpr int block=256,blocks=120; // 0.64 s

// Drop A palm-mute style fixture: A1 plucks with harmonics, a 2:1 accent and
// low deterministic noise, identical on both input channels.
float fixture(int n)
{
    const int period=int(rate*.16),local=n%period;const double t=double(local)/rate;
    const double accent=(n/period)%2==0 ? .35 : .18,envelope=accent*std::exp(-t*9);
    double x=0;for(int h=1;h<=6;++h)x+=std::sin(juce::MathConstants<double>::twoPi*55*h*t)/h;
    uint32_t s=uint32_t(n)*1664525u+1013904223u;s^=s>>13;
    return float(envelope*x+1e-4*(double(s&0xffff)/32768.-1));
}

struct Render {std::vector<float> audio;double energy{};};
Render render(ChimeraProcessor& p)
{
    p.prepareToPlay(rate,block);
    juce::AudioBuffer<float> buffer(2,block);juce::MidiBuffer midi;Render r;r.audio.reserve(size_t(2*block*blocks));
    for(int b=0;b<blocks;++b) {
        for(int i=0;i<block;++i) {const float x=fixture(b*block+i);buffer.setSample(0,i,x);buffer.setSample(1,i,x);}
        p.processBlock(buffer,midi);
        for(int c=0;c<2;++c)for(int i=0;i<block;++i) {
            const float y=buffer.getSample(c,i);require(std::isfinite(y),"Non-finite recall render");
            r.audio.push_back(y);r.energy+=double(y)*y;
        }
    }
    p.releaseResources();
    return r;
}

// Difference energy relative to the reference, in dB (-300 = bit identical).
double differenceDb(const Render& a,const Render& b)
{
    require(a.audio.size()==b.audio.size(),"Recall render length mismatch");
    double d=0;for(size_t i=0;i<a.audio.size();++i){const double e=double(a.audio[i])-b.audio[i];d+=e*e;}
    return d<=0 ? -300. : 10*std::log10(d/std::max(a.energy,1e-30));
}
float maxDifference(const Render& a,const Render& b)
{
    float m=0;for(size_t i=0;i<a.audio.size();++i)m=std::max(m,std::abs(a.audio[i]-b.audio[i]));return m;
}

// Bit identity is expected; the bound only absorbs FP contraction differences
// between call sites. A real recall fault (one parameter, a stale cabinet or
// smoothing start) sits tens of dB above it.
constexpr double sameSoundDb=-120;
void requireSame(const Render& reference,const Render& recalled,const juce::String& what)
{
    const double db=differenceDb(reference,recalled);
    std::cout<<"MEASURE "<<what<<" difference_db="<<db<<" max_abs="<<maxDifference(reference,recalled)<<'\n';
    require(db<=sameSoundDb,what+" changed the sound ("+juce::String(db,1)+" dB difference)");
}
void requireAudible(const Render& r,const juce::String& what)
{
    require(r.energy>1e-3,what+" rendered silence; the recall comparison would be vacuous");
}

// Presets from each family that the selector can load.
std::vector<int> presetSample()
{
    using namespace spectralforge;
    std::vector<int> presets{0,factoryPresetCount/2,factoryPresetCount-1};
    if(factoryPresetCount<originalPresetStart)presets.push_back(factoryPresetCount); // guitar signature
    if(originalPresetCount>0)presets.push_back(originalPresetStart);
    if(niflheimrPresetCount>0)presets.push_back(niflheimrPresetStart);
    return presets;
}
// A user edit on top of a preset, so recall covers values no factory snapshot
// contains. Factory recall deliberately keeps performance settings such as
// input gain (factoryPerformanceParameter); only full-state recall restores them.
void editSound(ChimeraProcessor& p)
{
    set(p,"output",-9.f);set(p,"doubleron",1);set(p,"doublertime",11.f);
}
void edit(ChimeraProcessor& p) {editSound(p);set(p,"input",3.5f);}

void presetRecall(const std::vector<int>& presets)
{
    for(size_t k=0;k<presets.size();++k) {
        const int preset=presets[k],other=presets[(k+1)%presets.size()];
        auto fresh=std::make_unique<ChimeraProcessor>();fresh->loadFactoryPreset(preset);
        const auto reference=render(*fresh);requireAudible(reference,"Preset "+juce::String(preset));
        // Load over a different, already-rendered and edited sound.
        auto used=std::make_unique<ChimeraProcessor>();used->loadFactoryPreset(other);editSound(*used);(void)render(*used);
        used->loadFactoryPreset(preset);
        requireSame(reference,render(*used),"preset "+juce::String(preset)+" loaded over "+juce::String(other));
        // Recalling the same preset twice is also stable.
        used->loadFactoryPreset(preset);
        requireSame(reference,render(*used),"preset "+juce::String(preset)+" reloaded");
    }
    std::cout<<"PASS factory preset recall renders the same audio as a fresh load ("<<presets.size()<<" presets)\n";
}

void comparisonAndProjectRecall(int presetA,int presetB)
{
    auto p=std::make_unique<ChimeraProcessor>();
    require(p->comparisonSlot()==0,"New instance does not start on A");
    p->loadFactoryPreset(presetA);edit(*p);
    const auto a=render(*p);requireAudible(a,"Sound A");
    p->selectComparison(1);
    requireSame(a,render(*p),"empty B initialised from A");
    p->loadFactoryPreset(presetB);
    const auto b=render(*p);requireAudible(b,"Sound B");
    require(differenceDb(a,b)>-20,"A and B fixtures sound alike; choose presets that differ");

    p->selectComparison(0);requireSame(a,render(*p),"A/B recall of A");
    p->selectComparison(1);requireSame(b,render(*p),"A/B recall of B");
    p->selectComparison(0);p->selectComparison(1);requireSame(b,render(*p),"repeated A/B toggling");
    std::cout<<"PASS A/B recall renders the stored sound\n";

    // Copy A/B: the copied slot sounds like the active one.
    {
        auto c=std::make_unique<ChimeraProcessor>();c->loadFactoryPreset(presetA);edit(*c);
        c->selectComparison(1);c->loadFactoryPreset(presetB);c->copyComparison();
        c->selectComparison(0);requireSame(b,render(*c),"A after copying B to A");
    }
    std::cout<<"PASS A/B copy renders the copied sound\n";

    // Project save/load: the active slot and the stored slot keep their sound.
    juce::MemoryBlock saved;p->getStateInformation(saved);
    auto restored=std::make_unique<ChimeraProcessor>();restored->setStateInformation(saved.getData(),int(saved.getSize()));
    require(restored->comparisonSlot()==1,"Project restore lost the active A/B slot");
    requireSame(b,render(*restored),"project restore of active B");
    restored->selectComparison(0);requireSame(a,render(*restored),"project restore of stored A");

    // Restore into an instance that already plays something else.
    auto used=std::make_unique<ChimeraProcessor>();used->loadFactoryPreset(presetA);(void)render(*used);
    used->setStateInformation(saved.getData(),int(saved.getSize()));
    requireSame(b,render(*used),"project restore over a playing instance");

    // A second save of the restored project is equivalent to the first.
    juce::MemoryBlock resaved;restored->getStateInformation(resaved);
    auto second=std::make_unique<ChimeraProcessor>();second->setStateInformation(resaved.getData(),int(resaved.getSize()));
    requireSame(a,render(*second),"second-generation project restore");
    std::cout<<"PASS project save/restore renders both A/B sounds\n";
}
}

int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    try {
        const auto presets=presetSample();
        presetRecall(presets);
        comparisonAndProjectRecall(presets.front(),presets.back());
    }catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
    return 0;
}
