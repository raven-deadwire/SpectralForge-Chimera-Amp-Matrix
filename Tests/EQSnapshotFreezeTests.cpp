#include "EQSpectrumAnalyzer.h"
#include "PluginProcessor.h"
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <vector>

// Construction and fixtures are outside counted sections. These counters prove
// that Freeze/thaw introduces no new allocations in the audio callback or the
// already-constructed analysis consumer; they do not claim a DAW deadline gate.
static thread_local bool countAllocations=false;
static thread_local size_t allocations=0;
void* operator new(std::size_t n) {if(countAllocations)++allocations;if(auto* p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void* operator new[](std::size_t n) {return ::operator new(n);}
void operator delete(void* p) noexcept {std::free(p);}
void operator delete[](void* p) noexcept {std::free(p);}
void operator delete(void* p,std::size_t) noexcept {std::free(p);}
void operator delete[](void* p,std::size_t) noexcept {std::free(p);}

using namespace spectralforge;
namespace {
constexpr std::array<double,4> rates{44100.,48000.,96000.,192000.};
void check(bool value,const char* message) {if(!value)throw std::runtime_error(message);}
void near(double actual,double expected,double tolerance,const char* message) {
    if(!std::isfinite(actual) || std::abs(actual-expected)>tolerance) {
        std::cerr<<message<<": actual="<<actual<<" expected="<<expected<<'\n';
        throw std::runtime_error(message);
    }
}
void fill(EQAnalyzerFIFO& fifo,int samples,int offset,int size,int bin=43,float amplitude=.1f,float gain=.5f) {
    for(int n=0;n<samples;++n) {
        const auto x=float(amplitude*std::sin(2*juce::MathConstants<double>::pi*bin*(n+offset)/size));
        fifo.push({x,gain*x,-x,-gain*x});
    }
}
void configure(EQSpectrumAnalyzer& analyzer,EQAnalyzerFIFO& fifo,int size,double rate) {
    analyzer.setFFTSize(size);analyzer.reset(fifo);analyzer.update(fifo,rate);
}
struct Snapshot {
    int size{},smoothing{};double rate{};
    std::array<std::vector<float>,4> bins;
    std::array<std::vector<float>,2> pixels;
    explicit Snapshot(const EQSpectrumAnalyzer& analyzer):size(analyzer.fftSize()),smoothing(analyzer.smoothingMode()),rate(analyzer.lastSampleRate()) {
        check(analyzer.hasData(),"Fixture has no complete real spectrum");
        for(bool output:{false,true}) {
            for(int bin=0;bin<=size/2;++bin) {
                bins[output?2:0].push_back(analyzer.rawDB(output,bin));
                bins[output?3:1].push_back(analyzer.smoothedDB(output,bin));
            }
            for(int pixel=0;pixel<181;++pixel) {
                const double centre=20*std::pow(1000.,pixel/180.);
                pixels[output?1:0].push_back(analyzer.displayDB(output,centre*.981,centre,centre*1.019));
            }
        }
    }
    void assertEqual(const EQSpectrumAnalyzer& analyzer) const {
        check(analyzer.hasData() && analyzer.fftSize()==size && analyzer.smoothingMode()==smoothing && analyzer.lastSampleRate()==rate,
              "Freeze changed frame validity or its original frequency mapping");
        for(bool output:{false,true}) {
            for(int bin=0;bin<=size/2;++bin) {
                check(analyzer.rawDB(output,bin)==bins[output?2:0][size_t(bin)],"Frozen raw IN/OUT bins changed");
                check(analyzer.smoothedDB(output,bin)==bins[output?3:1][size_t(bin)],"Frozen smoothed IN/OUT bins changed");
            }
            for(int pixel=0;pixel<181;++pixel) {
                const double centre=20*std::pow(1000.,pixel/180.);
                check(analyzer.displayDB(output,centre*.981,centre,centre*1.019)==pixels[output?1:0][size_t(pixel)],
                      "Frozen logarithmic display pixels changed");
            }
        }
    }
};

void emptyAndFreshWindow() {
    for(double rate:rates)for(int size:{2048,4096,8192}) {
        auto analyzer=std::make_unique<EQSpectrumAnalyzer>();auto fifo=std::make_unique<EQAnalyzerFIFO>();
        configure(*analyzer,*fifo,size,rate);
        fill(*fifo,size-1,0,size);analyzer->update(*fifo,rate);
        analyzer->setFrozen(true,*fifo);
        check(analyzer->isFrozen() && !analyzer->hasData() && !analyzer->canFreezeSnapshot() && analyzer->frames()==0,
              "Empty Freeze fabricated a snapshot from a partial window");
        fill(*fifo,size,0,size);analyzer->update(*fifo,rate);
        check(!analyzer->hasData() && analyzer->frames()==0 && fifo->pendingSamples()==0,
              "Empty frozen consumer accepted later audio");
        fill(*fifo,size+117,0,size); // A paused UI left complete and partial old observations queued.
        analyzer->setFrozen(false,*fifo);
        check(!analyzer->isFrozen() && !analyzer->hasData() && !analyzer->canFreezeSnapshot() &&
              analyzer->windowSamples()==0 && fifo->pendingSamples()==0,
              "Thaw retained a stale complete/overlapping/FIFO observation");
        analyzer->update(*fifo,rate);
        fill(*fifo,size-1,0,size,61,.05f,2.f);analyzer->update(*fifo,rate);
        check(!analyzer->hasData() && analyzer->frames()==0,"Thaw published before N fresh real samples");
        fill(*fifo,1,size-1,size,61,.05f,2.f);analyzer->update(*fifo,rate);
        check(analyzer->hasData() && analyzer->frames()==1 && analyzer->canFreezeSnapshot(),"N fresh real samples did not publish");
        near(analyzer->rawDB(false,61),20*std::log10(.05),.025,"Thaw input was mixed with pre-freeze samples");
        near(analyzer->rawDB(true,61),-20,.025,"Thaw output did not use the same fresh observation");
        check(analyzer->rawDB(false,43)<-110,"An old tone survived the thaw overlap/FIFO reset");
        for(bool output:{false,true})for(int bin=0;bin<=size/2;++bin)
            check(analyzer->rawDB(output,bin)==analyzer->smoothedDB(output,bin),"Thaw retained an old smoothing envelope");
        const Snapshot expected(*analyzer);analyzer->setFrozen(true,*fifo);expected.assertEqual(*analyzer);
        analyzer->setFrozen(true,*fifo);expected.assertEqual(*analyzer);
        analyzer->setFrozen(false,*fifo);analyzer->setFrozen(true,*fifo);
        check(!analyzer->hasData() && !analyzer->canFreezeSnapshot(),"Empty immediate refreeze resurrected an older snapshot");
    }
    std::cout<<"PASS 12 rate/FFT pairs: empty/partial Freeze, stale FIFO+overlap purge, first full fresh IN/OUT window and envelope\n";
}

void immutableFrameAndMapping() {
    for(double rate:rates)for(int size:{2048,4096,8192}) {
        auto analyzer=std::make_unique<EQSpectrumAnalyzer>();auto fifo=std::make_unique<EQAnalyzerFIFO>();
        configure(*analyzer,*fifo,size,rate);
        fill(*fifo,size,0,size);analyzer->update(*fifo,rate);
        fill(*fifo,size,size,size,61,.2f,.25f);analyzer->update(*fifo,rate);
        const Snapshot expected(*analyzer);const auto frames=analyzer->frames();const int smoothing=analyzer->smoothingMode();
        analyzer->setFrozen(true,*fifo);expected.assertEqual(*analyzer);
        analyzer->setFFTSize(size==8192?2048:8192);analyzer->setSmoothing((smoothing+1)%3);
        check(analyzer->smoothingMode()==smoothing,"Frozen smoothing control changed");
        expected.assertEqual(*analyzer);
        fill(*fifo,int(EQAnalyzerFIFO::capacity)+37,0,size,7,.6f,3.f);
        check(fifo->dropped.load()>0,"Frozen overflow fixture did not overflow");
        const double nextRate=rate==48000?192000:48000;
        check(!analyzer->update(*fifo,nextRate),"Frozen overflow/rate update reported a new visible frame");
        expected.assertEqual(*analyzer);
        analyzer->reset(*fifo); // Bank hide/show and LIVE reset keep the snapshot.
        check(analyzer->isFrozen(),"Visibility reset released Freeze");expected.assertEqual(*analyzer);
        analyzer->update(*fifo,std::numeric_limits<double>::quiet_NaN());expected.assertEqual(*analyzer);
        check(analyzer->frames()==frames && analyzer->lastFFTWindows()==0 && analyzer->fftMicros()==0,
              "Frozen updates transformed or published a new frame");
        fill(*fifo,size+93,0,size);analyzer->setFrozen(false,*fifo);
        fill(*fifo,size-1,0,size,97,.07f,2.f);analyzer->update(*fifo,nextRate);
        check(!analyzer->hasData(),"Rate-changing thaw published a partial frame");
        fill(*fifo,1,size-1,size,97,.07f,2.f);analyzer->update(*fifo,nextRate);
        check(analyzer->hasData() && analyzer->lastSampleRate()==nextRate,"Thaw did not accept the first fresh frame at the new rate");
        near(analyzer->rawDB(false,97),20*std::log10(.07),.025,"Rate-changing thaw retained a previous frame");
    }
    std::cout<<"PASS 12 rate/FFT pairs: raw/smoothed/pixel identity across Freeze, configuration attempts, reset, overflow and rate changes\n";
}

void malformedFramesAndRetainedMetadata() {
    const std::array<float,4> malformed{std::numeric_limits<float>::quiet_NaN(),
        std::numeric_limits<float>::infinity(),-std::numeric_limits<float>::infinity(),std::numeric_limits<float>::max()};
    for(double rate:rates)for(int size:{2048,4096,8192}) {
        auto analyzer=std::make_unique<EQSpectrumAnalyzer>();auto fifo=std::make_unique<EQAnalyzerFIFO>();
        configure(*analyzer,*fifo,size,rate);fill(*fifo,size,0,size);analyzer->update(*fifo,rate);
        const Snapshot expected(*analyzer);const auto frames=analyzer->frames();
        for(int n=0;n<size;++n)fifo->push({malformed[size_t(n%4)],.01f,-.02f,.03f});
        analyzer->update(*fifo,rate);
        check(analyzer->frames()==frames,"Malformed input was published as a normal complete frame");
        analyzer->setFrozen(true,*fifo);expected.assertEqual(*analyzer);analyzer->setFrozen(false,*fifo);
        for(int n=0;n<size;++n)fifo->push({malformed[0],.01f,-.02f,.03f});
        analyzer->update(*fifo,rate);
        check(!analyzer->hasData() && !analyzer->canFreezeSnapshot(),"Thaw accepted a sanitized NaN frame as valid audio");
        for(int n=0;n<size;++n)fifo->push({malformed[3],malformed[3],malformed[3],malformed[3]});
        analyzer->update(*fifo,rate);
        check(!analyzer->hasData() && !analyzer->canFreezeSnapshot(),"Overflowed finite FFT became a valid thaw snapshot");
        fill(*fifo,size,0,size,61,.05f,2.f);analyzer->update(*fifo,rate);
        check(analyzer->hasData(),"Valid input did not recover after rejected windows");
        near(analyzer->rawDB(false,61),20*std::log10(.05),.025,"Recovered spectrum was contaminated by invalid history");
        const Snapshot recovered(*analyzer);
        const int newSize=size==8192?2048:8192;
        analyzer->setFFTSize(newSize);analyzer->setSmoothing(2);analyzer->reset(*fifo);
        analyzer->update(*fifo,rate==48000?96000:48000);
        fill(*fifo,int(EQAnalyzerFIFO::capacity)+1,0,newSize);analyzer->update(*fifo,rate==48000?96000:48000);
        check(!analyzer->hasData() && analyzer->canFreezeSnapshot(),"Discontinuity discarded the last complete normal snapshot");
        analyzer->setFrozen(true,*fifo);recovered.assertEqual(*analyzer);
        analyzer->setFrozen(false,*fifo);
        fill(*fifo,newSize,0,newSize);analyzer->update(*fifo,std::numeric_limits<double>::infinity());
        check(!analyzer->hasData() && !analyzer->canFreezeSnapshot() && fifo->pendingSamples()==0,
              "Invalid sample rate fabricated a normal mapped frame");
    }
    std::cout<<"PASS 12 rate/FFT pairs: nonfinite samples/FFT overflow/rate rejected, valid recovery, last normal frame keeps original mapping\n";
}

void idleAndMixedBatch() {
    auto analyzer=std::make_unique<EQSpectrumAnalyzer>();auto fifo=std::make_unique<EQAnalyzerFIFO>();
    constexpr int size=4096;configure(*analyzer,*fifo,size,48000);
    fill(*fifo,size,0,size);analyzer->update(*fifo,48000);const Snapshot expected(*analyzer);
    std::this_thread::sleep_for(std::chrono::milliseconds(210));analyzer->update(*fifo,48000);
    check(analyzer->isIdle() && analyzer->rawDB(false,43)==EQSpectrumAnalyzer::floorDB &&
          analyzer->smoothedDB(false,43)<expected.bins[1][43],"Fixture did not exercise actual idle decay");
    analyzer->setFrozen(true,*fifo);expected.assertEqual(*analyzer);
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    analyzer->update(*fifo,48000);expected.assertEqual(*analyzer);

    auto reference=std::make_unique<EQSpectrumAnalyzer>();auto referenceFIFO=std::make_unique<EQAnalyzerFIFO>();
    configure(*reference,*referenceFIFO,size,48000);
    analyzer->setFrozen(false,*fifo);analyzer->update(*fifo,48000);
    for(auto* queue:{fifo.get(),referenceFIFO.get()}) {
        fill(*queue,size,0,size);fill(*queue,size,size,size,61,.2f,.25f);fill(*queue,size,2*size,size,97,.07f,2.f);
    }
    for(int n=0;n<size;++n)fifo->push({0,0,std::numeric_limits<float>::quiet_NaN(),0});
    reference->update(*referenceFIFO,48000);analyzer->update(*fifo,48000);
    const Snapshot lastGood(*reference);analyzer->setFrozen(true,*fifo);lastGood.assertEqual(*analyzer);
    std::cout<<"PASS Freeze captures the last real pre-idle frame and the latest valid window before malformed data in one FIFO batch\n";
}

void set(ChimeraProcessor& processor,const juce::String& id,float value) {
    auto* parameter=processor.parameters().getParameter(id);check(parameter!=nullptr,"Missing EQ host parameter");
    parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
}
void audioAndParameterInvariance() {
    for(double rate:rates)for(int size:{2048,4096,8192}) {
        auto reference=std::make_unique<ChimeraProcessor>(),candidate=std::make_unique<ChimeraProcessor>();
        for(auto* processor:{reference.get(),candidate.get()}) {
            for(int eq=0;eq<2;++eq) {
                set(*processor,eqID(eq,-1,"bypass"),0);set(*processor,eqID(eq,2,"enabled"),1);
                set(*processor,eqID(eq,2,"frequency"),251);set(*processor,eqID(eq,2,"gain"),eq==0?8:-5);
                set(*processor,eqID(eq,2,"q"),2.3f);
            }
            processor->prepareToPlay(rate,128);
        }
        const auto before=candidate->parameters().copyState().toXmlString();
        const int latency=candidate->getLatencySamples();
        check(latency>0 && reference->getLatencySamples()==latency,"Fixture did not initialize the real host-reported latency");
        auto tone=std::make_unique<EQSpectrumAnalyzer>(),final=std::make_unique<EQSpectrumAnalyzer>();
        configure(*tone,candidate->graphicalEQ(0).analyzer,size,rate);
        configure(*final,candidate->graphicalEQ(1).analyzer,size,rate);
        juce::AudioBuffer<float> x(2,128),y(2,128);double energy=0;allocations=0;
        for(int block=0;block<160;++block) {
            countAllocations=true;
            if(block==72)tone->setFrozen(true,candidate->graphicalEQ(0).analyzer);
            if(block==88)final->setFrozen(true,candidate->graphicalEQ(1).analyzer);
            if(block==104)tone->setFrozen(false,candidate->graphicalEQ(0).analyzer);
            if(block==120)final->setFrozen(false,candidate->graphicalEQ(1).analyzer);
            countAllocations=false;
            for(int n=0;n<128;++n) {
                const auto v=float(.05*std::sin(2*juce::MathConstants<double>::pi*251*(block*128+n)/rate));
                x.setSample(0,n,v);x.setSample(1,n,-.7f*v);
            }
            y.makeCopyOf(x,true);
            for(int eq=0;eq<2;++eq)candidate->graphicalEQ(eq).analyzer.enabled.store(!(eq==0?tone:final)->isFrozen());
            countAllocations=true;
            for(int eq=0;eq<2;++eq) {reference->graphicalEQ(eq).process(x);candidate->graphicalEQ(eq).process(y);}
            tone->update(candidate->graphicalEQ(0).analyzer,rate);final->update(candidate->graphicalEQ(1).analyzer,rate);
            countAllocations=false;
            for(int channel=0;channel<2;++channel) {
                check(std::memcmp(x.getReadPointer(channel),y.getReadPointer(channel),128*sizeof(float))==0,
                      "Freeze/capture/thaw changed processed audio bits");
                for(int n=0;n<128;++n){check(std::isfinite(y.getSample(channel,n)),"Invalid audio invariance fixture");energy+=std::abs(y.getSample(channel,n));}
            }
        }
        check(allocations==0,"Freeze or audio/analysis callback allocated after construction");
        check(energy>1 && candidate->getLatencySamples()==latency,"Freeze changed latency or audio fixture was silent");
        check(candidate->parameters().copyState().toXmlString()==before,"Snapshot Freeze wrote host parameters/project state");
        reference->releaseResources();candidate->releaseResources();
        std::cout<<"PASS audio identity rate="<<rate<<" fft="<<size<<" both active EQ banks / unchanged state+latency="<<latency<<" / zero counted allocations\n";
    }
}
}

int main() {
    try {
        juce::ScopedJuceInitialiser_GUI init;
        emptyAndFreshWindow();immutableFrameAndMapping();malformedFramesAndRetainedMetadata();idleAndMixedBatch();audioAndParameterInvariance();
        std::cout<<"PASS October 23 Snapshot Freeze regression suite; real DAW/editor acceptance remains separate\n";return 0;
    } catch(const std::exception& error) {
        countAllocations=false;std::cerr<<"FAIL "<<error.what()<<'\n';return 1;
    }
}
