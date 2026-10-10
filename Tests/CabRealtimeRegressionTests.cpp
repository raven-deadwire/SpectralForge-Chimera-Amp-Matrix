#include "Cabinet.h"
#include "CabLayoutModel.h"
#include <iostream>
#include <stdexcept>

namespace {
using spectralforge::Cab;
namespace original=spectralforge::originalCab;
namespace expanded=spectralforge::cabExpansion;
namespace layouts=spectralforge::cabLayout;

void require(bool ok,const char* message) {if(!ok)throw std::runtime_error(message);}

// The reference deliberately retains JUCE's default, zero-latency uniform
// convolution. It must not inherit the production Kernel's partition choice.
struct UniformReference {
    juce::dsp::Convolution convolution;
    UniformReference(const juce::AudioBuffer<float>& samples,double rate,
                     const juce::dsp::ProcessSpec& spec,bool modeled) {
        juce::AudioBuffer<float> copy;copy.makeCopyOf(samples);
        convolution.loadImpulseResponse(std::move(copy),rate,
            juce::dsp::Convolution::Stereo::yes,juce::dsp::Convolution::Trim::no,
            modeled ? juce::dsp::Convolution::Normalise::no : juce::dsp::Convolution::Normalise::yes);
        convolution.prepare(spec);
    }
    void process(juce::AudioBuffer<float>& samples) {
        juce::dsp::AudioBlock<float> block(samples);
        juce::dsp::ProcessContextReplacing<float> context(block);convolution.process(context);
    }
};

struct Residuals {double uniform{},impulse{},silence{};};

Residuals compare(const juce::AudioBuffer<float>& response,double responseRate,uint64_t key,
                  const juce::dsp::ProcessSpec& spec) {
    juce::AudioBuffer<float> copy;copy.makeCopyOf(response);
    Cab::Kernel production(std::move(copy),responseRate,spec,key ? 0 : 3,1,key);
    UniformReference reference(response,responseRate,spec,key!=0);
    require(production.getLatency()==0,"production CAB added processing latency");
    require(reference.convolution.getLatency()==0,"uniform oracle added processing latency");
    const int channels=int(spec.numChannels),maximum=int(spec.maximumBlockSize);
    const int tail=int(std::ceil(response.getNumSamples()*spec.sampleRate/responseRate));
    const int length=317+tail+2*maximum;
    Residuals result;
    // Fixed blocks cover the actual deadline routes; irregular chunks cross
    // partition boundaries and host sub-block boundaries independently.
    for(bool irregular:{false,true}) {
        production.reset();reference.convolution.reset();
        int chunk=0;
        for(int offset=0;offset<length;) {
            const std::array<int,5> sizes{{1,17,63,maximum,3}};
            const int count=std::min(length-offset,irregular ? std::min(maximum,sizes[size_t(chunk++%5)]) : maximum);
            juce::AudioBuffer<float> actual(channels,count),expected(channels,count);
            for(int channel=0;channel<channels;++channel)for(int n=0;n<count;++n) {
                const int t=offset+n;
                // Distinct channels expose accidental stereo sharing. Ending
                // the input at sample 317 leaves the entire authored tail visible.
                const float x=t<317 ? float(.075*std::sin(t*(channel ? .113 : .071))
                    +(t%53==0 ? (channel ? -.15 : .20) : 0.)) : 0.f;
                actual.setSample(channel,n,x);expected.setSample(channel,n,x);
            }
            production.process(actual);reference.process(expected);
            for(int channel=0;channel<channels;++channel)for(int n=0;n<count;++n) {
                require(std::isfinite(actual.getSample(channel,n)),"non-finite CAB output");
                result.uniform=std::max(result.uniform,std::abs(double(actual.getSample(channel,n))-expected.getSample(channel,n)));
            }
            offset+=count;
        }
    }
    // Reset a live engine with deliberately dirty convolution history, then
    // verify silence; this catches a retained non-uniform tail after host reset.
    juce::AudioBuffer<float> dirty(channels,maximum);
    for(int channel=0;channel<channels;++channel)for(int n=0;n<maximum;++n)dirty.setSample(channel,n,.2f);
    production.process(dirty);production.reset();
    for(int offset=0;offset<tail+maximum;offset+=maximum) {
        dirty.clear();production.process(dirty);
        result.silence=std::max(result.silence,double(dirty.getMagnitude(0,maximum)));
    }
    // Absolute response check: models retain their original gain, arrival time,
    // every tail sample and channel isolation, beyond agreement with the oracle.
    if(key)for(int excited=0;excited<channels;++excited) {
        production.reset();
        for(int offset=0;offset<tail+maximum;offset+=maximum) {
            dirty.clear();if(offset==0)dirty.setSample(excited,0,1.f);production.process(dirty);
            for(int channel=0;channel<channels;++channel)for(int n=0;n<maximum;++n) {
                const float expected=channel==excited && offset+n<response.getNumSamples()
                    ? response.getSample(0,offset+n) : 0.f;
                result.impulse=std::max(result.impulse,std::abs(double(dirty.getSample(channel,n))-expected));
            }
        }
    }
    require(result.uniform<3e-6,"CAB differs from uniform convolution across chunks/tail");
    require(result.impulse<3e-6,"modeled CAB changes authored impulse/tail/channel isolation");
    require(result.silence<1e-8,"CAB reset retains convolution history");
    return result;
}

// An independent direct FIR oracle for sparse, synthetic modeled responses.
// Distinct taps expose shared mic/channel history and lost partition boundaries
// without relying on the production FFT or another partition implementation.
struct SparseResponse {
    struct Tap {int offset;float value;};
    int length{};
    std::vector<Tap> taps;
    juce::AudioBuffer<float> samples() const {
        juce::AudioBuffer<float> result(1,length);result.clear();
        for(const auto& tap:taps)result.setSample(0,tap.offset,tap.value);
        return result;
    }
};

struct DirectConvolution {
    SparseResponse response;
    std::array<std::vector<float>,2> history;
    int write{};
    explicit DirectConvolution(const SparseResponse& impulse):response(impulse) {
        for(auto& channel:history)channel.resize(size_t(response.length));
    }
    void reset() {for(auto& channel:history)std::fill(channel.begin(),channel.end(),0.f);write=0;}
    std::array<float,2> tick(const std::array<float,2>& input,int channels) {
        std::array<float,2> result{};
        for(int c=0;c<channels;++c) {
            history[size_t(c)][size_t(write)]=input[size_t(c)];
            for(const auto& tap:response.taps)
                result[size_t(c)]+=tap.value*history[size_t(c)][size_t((write-tap.offset+response.length)%response.length)];
        }
        write=(write+1)%response.length;return result;
    }
};

// Independent, sample-at-a-time reference for each mic. With no installed FIR
// it isolates the filter/history/mix/bypass path. Direct FIRs additionally test
// the six independently owned kernels and the exact 50 ms publication fade.
struct ReferenceMic {
    std::array<juce::dsp::IIR::Filter<float>,2> high,low;
    std::array<std::vector<float>,2> history;
    juce::SmoothedValue<float> gain,delay;
    std::unique_ptr<DirectConvolution> active,fading;
    int write{},fadeRemaining{},fadeLength;double rate;
    explicit ReferenceMic(double sampleRate,float db=0.f,bool invert=false,float ms=0.f)
        :fadeLength(juce::jmax(1,int(sampleRate*.050))),rate(sampleRate) {
        for(int c=0;c<2;++c) {
            history[size_t(c)].resize(size_t(int(rate*.020)+2));
        }
        setCuts(70.f,9000.f);
        gain.reset(rate,.020);gain.setCurrentAndTargetValue(juce::Decibels::decibelsToGain(db)*(invert ? -1.f : 1.f));
        delay.reset(rate,.020);delay.setCurrentAndTargetValue(float(rate*.001)*juce::jlimit(0.f,20.f,ms));
    }
    void setCuts(float lowCut,float highCut) {
        for(int c=0;c<2;++c) {
            high[size_t(c)].coefficients=juce::dsp::IIR::Coefficients<float>::makeHighPass(rate,juce::jlimit(10.f,float(rate*.44),lowCut));
            low[size_t(c)].coefficients=juce::dsp::IIR::Coefficients<float>::makeLowPass(rate,juce::jlimit(100.f,float(rate*.45),highCut));
        }
    }
    void install(const SparseResponse& response) {active=std::make_unique<DirectConvolution>(response);}
    void beginSwap(const SparseResponse& response) {
        require(fadeRemaining==0,"reference mic swap already fading");
        fading=std::move(active);install(response);fadeRemaining=fadeLength;
    }
    void reset() {
        for(int c=0;c<2;++c) {high[size_t(c)].reset();low[size_t(c)].reset();std::fill(history[size_t(c)].begin(),history[size_t(c)].end(),0.f);}
        if(active)active->reset();if(fading)fading->reset();
        write=0;
    }
    void targets(float db,bool invert,float ms) {
        gain.setTargetValue(juce::Decibels::decibelsToGain(db)*(invert ? -1.f : 1.f));
        delay.setTargetValue(float(rate*.001)*juce::jlimit(0.f,20.f,ms));
    }
    std::array<float,2> tick(const std::array<float,2>& input,int channels) {
        std::array<float,2> filtered{};
        for(int c=0;c<channels;++c)filtered[size_t(c)]=high[size_t(c)].processSample(input[size_t(c)]);
        auto convolved=active ? active->tick(filtered,channels) : filtered;
        if(fadeRemaining>0) {
            const auto previous=fading ? fading->tick(filtered,channels) : filtered;
            const float wet=1.f-float(fadeRemaining)/float(fadeLength);
            for(int c=0;c<channels;++c)convolved[size_t(c)]=wet*convolved[size_t(c)]+(1.f-wet)*previous[size_t(c)];
            if(--fadeRemaining==0)fading.reset();
        }
        const float g=gain.getNextValue(),d=delay.getNextValue(),fraction=d-int(d);
        const int size=int(history[0].size()),read=(write-int(d)+size)%size,previous=(read+size-1)%size;
        std::array<float,2> result{};
        for(int c=0;c<channels;++c) {
            auto& samples=history[size_t(c)];
            samples[size_t(write)]=low[size_t(c)].processSample(convolved[size_t(c)]);
            result[size_t(c)]=g*((1.f-fraction)*samples[size_t(read)]+fraction*samples[size_t(previous)]);
        }
        write=(write+1)%size;return result;
    }
};

double compareMicPostPath(double rate,int maximum,int channels) {
    const juce::dsp::ProcessSpec spec{rate,juce::uint32(maximum),juce::uint32(channels)};
    Cab production;production.blend=.35f;
    // Non-default restored settings must own the first sample after prepare.
    production.gainDb=-6.f;production.invert=true;production.delayMs=0.f;
    production.secondMic()->gainDb=-82.f;production.secondMic()->delayMs=.03125f;
    production.prepare(spec);
    ReferenceMic a(rate,-6.f,true,0.f),b(rate,-82.f,false,.03125f);
    juce::SmoothedValue<float> blend,enabled;
    blend.reset(rate,.020);blend.setCurrentAndTargetValue(.35f);
    enabled.reset(rate,.020);enabled.setCurrentAndTargetValue(1.f);
    struct Step {
        float delayA,delayB,gainA,gainB,mix;bool invertA,invertB,on;
        float lowA{70.f},highA{9000.f},lowB{70.f},highB{9000.f};
    };
    const std::array<Step,14> steps{{
        {0,.03125f,-6,-82,.35f,true,false,true},
        {0,0,0,0,.35f,false,false,true},
        {.125f,.375f,0,0,.35f,false,false,true},
        {20,17.9375f,-3,4,.75f,false,true,true,35,13500,410,2400},
        {.001f,.003f,.001f,-.001f,.35f,false,false,true,23,17000,870,3100},
        {.001f,.003f,-82,-84,.35f,false,true,true},
        {.001f,.003f,-82,-84,.35f,false,true,true}, // Settled, small nonzero gains and delays.
        {0,0,0,0,1,false,false,true},
        {0,0,0,0,0,false,false,true},
        {0,0,0,0,0,false,false,true}, // Fully suspend B, then wake with reset history.
        {13.373f,.517f,6,-6,.65f,true,false,true},
        {20,0,0,0,.15f,false,false,false},
        {0,0,0,0,.5f,false,false,true},
        {0,0,0,0,.5f,false,false,true}
    }};
    bool wasRunning=false;int sample=0,chunk=0;double residual=0;
    for(const auto& step:steps) {
        production.delayMs=step.delayA;production.gainDb=step.gainA;production.invert=step.invertA;
        production.secondMic()->delayMs=step.delayB;production.secondMic()->gainDb=step.gainB;production.secondMic()->invert=step.invertB;
        production.setCuts(step.lowA,step.highA);production.secondMic()->setCuts(step.lowB,step.highB);
        a.setCuts(step.lowA,step.highA);b.setCuts(step.lowB,step.highB);
        production.blend=step.mix;production.enable(step.on);enabled.setTargetValue(step.on ? 1.f : 0.f);
        // Each stage exceeds both the 20 ms smoother and multiple full history
        // wraps. At 8 kHz/max1024 a single block wraps the 162-sample ring >6 times.
        const int stageLength=int(rate*.08)+maximum;
        for(int offset=0;offset<stageLength;) {
            const std::array<int,5> sizes{{maximum,1,17,63,maximum}};
            const int count=std::min(stageLength-offset,std::min(maximum,sizes[size_t(chunk++%5)]));
            juce::AudioBuffer<float> actual(channels,count),expected(channels,count);
            for(int n=0;n<count;++n)for(int c=0;c<channels;++c) {
                const float x=float(.17*std::sin((sample+n)*(c ? .093 : .071))
                    +((sample+n)%47==0 ? (c ? -.09 : .13) : 0.));
                actual.setSample(c,n,x);expected.setSample(c,n,x);
            }
            const bool renderB=step.mix>0 || blend.getCurrentValue()>0;
            if(renderB) {if(!wasRunning)b.reset();b.targets(step.gainB,step.invertB,step.delayB);}
            wasRunning=renderB;a.targets(step.gainA,step.invertA,step.delayA);blend.setTargetValue(step.mix);
            for(int n=0;n<count;++n) {
                std::array<float,2> input{};for(int c=0;c<channels;++c)input[size_t(c)]=expected.getSample(c,n);
                const auto first=a.tick(input,channels),second=renderB ? b.tick(input,channels) : std::array<float,2>{};
                const float mix=blend.getNextValue(),wet=enabled.getNextValue();
                for(int c=0;c<channels;++c) {
                    const float processed=renderB ? (1.f-mix)*first[size_t(c)]+mix*second[size_t(c)] : first[size_t(c)];
                    expected.setSample(c,n,wet*processed+(1.f-wet)*input[size_t(c)]);
                }
            }
            production.process(actual);
            for(int c=0;c<channels;++c)for(int n=0;n<count;++n) {
                require(std::isfinite(actual.getSample(c,n)),"non-finite mic post output");
                residual=std::max(residual,std::abs(double(actual.getSample(c,n))-expected.getSample(c,n)));
            }
            sample+=count;offset+=count;
        }
    }
    require(residual<3e-6,"mic post path changes history, delay/gain, blend or bypass automation");
    return residual;
}

double compareReprepare() {
    // The same object survives host device/rate/block/channel changes. Identical
    // cut values across calls must still be recalculated for the new rate, and
    // the restored gain/polarity/delay must be correct from the first sample.
    Cab production;production.blend=.37f;
    const std::array<juce::dsp::ProcessSpec,4> specs{{{44100.,64,1},{96000.,64,2},
        {48000.,512,2},{48000.,32,1}}};
    double residual=0;
    for(size_t route=0;route<specs.size();++route) {
        const auto spec=specs[route];const int channels=int(spec.numChannels),maximum=int(spec.maximumBlockSize);
        production.gainDb=route<2 ? -6.f : route==2 ? .001f : -82.f;
        production.invert=route!=2;production.delayMs=route==2 ? .001f : 0.f;
        auto* second=production.secondMic();second->gainDb=route<3 ? -4.f : -84.f;
        second->invert=route%2!=0;second->delayMs=.375f;
        production.prepare(spec);
        ReferenceMic a(spec.sampleRate,production.gainDb,production.invert,production.delayMs);
        ReferenceMic b(spec.sampleRate,second->gainDb,second->invert,second->delayMs);
        const int length=int(spec.sampleRate*.025)+maximum;
        for(int offset=0,chunk=0;offset<length;) {
            const std::array<int,4> chunks{{maximum,1,17,3}};
            const int count=std::min(length-offset,std::min(maximum,chunks[size_t(chunk++%4)]));
            juce::AudioBuffer<float> actual(channels,count),expected(channels,count);
            for(int n=0;n<count;++n) {
                std::array<float,2> input{};
                for(int c=0;c<channels;++c) {
                    input[size_t(c)]=float(.19*std::sin((offset+n)*(c ? .093 : .071))
                        +((offset+n)%53==0 ? (c ? -.17 : .25) : 0.));
                    actual.setSample(c,n,input[size_t(c)]);
                }
                const auto first=a.tick(input,channels),last=b.tick(input,channels);
                for(int c=0;c<channels;++c)expected.setSample(c,n,.63f*first[size_t(c)]+.37f*last[size_t(c)]);
            }
            production.process(actual);
            for(int c=0;c<channels;++c)for(int n=0;n<count;++n) {
                require(std::isfinite(actual.getSample(c,n)),"non-finite mic output after host reprepare");
                residual=std::max(residual,std::abs(double(actual.getSample(c,n))-expected.getSample(c,n)));
            }
            offset+=count;
        }
    }
    require(residual<3e-6,"mic preparation reuses stale rate coefficients, gain, polarity or delay");
    return residual;
}

double compareSixMicPublications(double rate,int maximum,int channels) {
    const juce::dsp::ProcessSpec spec{rate,juce::uint32(maximum),juce::uint32(channels)};
    struct Settings {float low,high,db,delay;bool inverted;};
    const std::array<Settings,6> settings{{
        {31,14500,-3.25f,.001f,false},{730,2600,2.5f,17.9375f,true},
        {140,6100,.001f,.003f,true},{53,11900,-.001f,.517f,false},
        {260,3500,-82.f,20.f,false},{17,17300,-6.75f,.125f,true}
    }};
    const std::array<float,3> blends{{.21f,.46f,.73f}};
    std::array<Cab,3> production;
    std::array<Cab*,6> slots{};
    std::array<std::unique_ptr<ReferenceMic>,6> reference;
    std::array<SparseResponse,6> originalResponses,nextResponses;
    std::array<uint64_t,6> expectedKeys{};
    const auto modelKey=[](int slot,int revision){return uint64_t(0x101+slot+revision*0x100);};
    const auto responseFor=[](int slot,int revision) {
        const int length=517+23*slot+11*revision;
        return SparseResponse{length,{{1+slot+revision,.37f+.03f*slot},
            {83+slot*3,-.13f-.01f*revision},{193+slot*7,.07f},
            {length-1,(slot%2 ? -.04f : .03f)*(1.f+.2f*revision)}}};
    };
    const auto kernel=[&](const SparseResponse& response,uint64_t key,unsigned revision) {
        return std::make_unique<Cab::Kernel>(response.samples(),rate,spec,0,revision,key);
    };
    for(int lane=0;lane<3;++lane) {
        auto& cab=production[size_t(lane)];cab.blend=blends[size_t(lane)];
        slots[size_t(lane*2)]=&cab;slots[size_t(lane*2+1)]=cab.secondMic();
        for(int mic=0;mic<2;++mic) {
            const size_t slot=size_t(lane*2+mic);const auto& state=settings[slot];
            slots[slot]->gainDb=state.db;slots[slot]->delayMs=state.delay;slots[slot]->invert=state.inverted;
        }
        cab.prepare(spec);
    }
    for(int mic=0;mic<6;++mic) {
        const size_t slot=size_t(mic);const auto& state=settings[slot];
        originalResponses[slot]=responseFor(mic,0);nextResponses[slot]=responseFor(mic,1);
        expectedKeys[slot]=modelKey(mic,0);
        slots[slot]->setCuts(state.low,state.high);slots[slot]->requestedModel=expectedKeys[slot];
        auto prepared=kernel(originalResponses[slot],expectedKeys[slot],1);
        require(prepared->getLatency()==0,"six-mic prepared response adds processing latency");
        slots[slot]->install(std::move(prepared));
        reference[slot]=std::make_unique<ReferenceMic>(rate,state.db,state.inverted,state.delay);
        reference[slot]->setCuts(state.low,state.high);reference[slot]->install(originalResponses[slot]);
    }
    int frame=0,chunk=0;double residual=0;
    std::array<bool,3> waitingForSecond{};
    const std::array<SparseResponse,6>* queuedResponses=nullptr;
    int queuedRevision=0;
    const auto render=[&](int length,bool silence=false) {
        for(int offset=0;offset<length;) {
            const std::array<int,6> chunks{{maximum,1,17,63,3,maximum}};
            const int count=std::min(length-offset,std::min(maximum,chunks[size_t(chunk++%6)]));
            for(int lane=0;lane<3;++lane) {
                const size_t first=size_t(lane*2),second=first+1;
                // B starts on the first callback after A's unchanged 50 ms
                // fade, never halfway through the callback that finishes A.
                if(waitingForSecond[size_t(lane)] && reference[first]->fadeRemaining==0) {
                    reference[second]->beginSwap((*queuedResponses)[second]);
                    expectedKeys[second]=modelKey(int(second),queuedRevision);
                    waitingForSecond[size_t(lane)]=false;
                }
                juce::AudioBuffer<float> actual(channels,count),expected(channels,count);
                for(int n=0;n<count;++n) {
                    std::array<float,2> input{};
                    for(int c=0;c<channels;++c) {
                        const int t=frame+n;
                        input[size_t(c)]=silence ? 0.f : float(.14*std::sin(t*(.037+.013*lane+.019*c))
                            +(t%(47+lane*12)==0 ? (c ? -.11 : .19) : 0.));
                        actual.setSample(c,n,input[size_t(c)]);
                    }
                    const auto a=reference[first]->tick(input,channels),b=reference[second]->tick(input,channels);
                    for(int c=0;c<channels;++c)expected.setSample(c,n,
                        (1.f-blends[size_t(lane)])*a[size_t(c)]+blends[size_t(lane)]*b[size_t(c)]);
                }
                production[size_t(lane)].process(actual);
                require(production[size_t(lane)].transitioningMicCount()<=1,"six-mic publication overlaps A/B fades");
                for(int c=0;c<channels;++c)for(int n=0;n<count;++n) {
                    require(std::isfinite(actual.getSample(c,n)),"non-finite six-mic output");
                    residual=std::max(residual,std::abs(double(actual.getSample(c,n))-expected.getSample(c,n)));
                    if(silence)require(std::abs(actual.getSample(c,n))<1e-8f,"six-mic reset retains audio history");
                }
                for(size_t slot:{first,second}) {
                    require(slots[slot]->activeModel==expectedKeys[slot],"six-mic worker publication activates wrong response");
                    slots[slot]->collect(); // Worker-side reclamation, outside process.
                }
            }
            frame+=count;offset+=count;
        }
    };
    render(int(rate*.030)+maximum);
    // Reproduce a worker finishing an obsolete request after the final request
    // check. Every slot must keep its audible engine and retire the stale one.
    for(int mic=0;mic<6;++mic) {
        const size_t slot=size_t(mic);slots[slot]->requestedModel=modelKey(mic,1);
        slots[slot]->publish(kernel(nextResponses[slot],modelKey(mic,2),2));
    }
    render(maximum*2);
    for(auto* slot:slots)require(slot->rejectedModels==1,"stale six-mic worker response was not rejected exactly once");
    const auto publishAndCompare=[&](const std::array<SparseResponse,6>& responses,int revision) {
        queuedResponses=&responses;queuedRevision=revision;
        // All six responses are ready before one callback, as in the deadline
        // gate. Returning to a previous key must use a clean convolution state
        // while preserving the old response's running tail during its fade.
        for(int mic=0;mic<6;++mic) {
            const size_t slot=size_t(mic);slots[slot]->requestedModel=modelKey(mic,revision);
            slots[slot]->publish(kernel(responses[slot],modelKey(mic,revision),unsigned(revision+3)));
        }
        for(int lane=0;lane<3;++lane) {
            const size_t first=size_t(lane*2);reference[first]->beginSwap(responses[first]);
            expectedKeys[first]=modelKey(int(first),revision);waitingForSecond[size_t(lane)]=true;
        }
        render(int(rate*.130)+2*maximum);
        for(int lane=0;lane<3;++lane) {
            require(!waitingForSecond[size_t(lane)] && production[size_t(lane)].transitioningMicCount()==0,
                "six-mic publication did not settle within two 50 ms fades");
        }
        for(int mic=0;mic<6;++mic)require(slots[size_t(mic)]->activeModel==modelKey(mic,revision),
            "latest six-mic generation did not converge");
    };
    publishAndCompare(nextResponses,1);
    publishAndCompare(originalResponses,0);
    for(auto& cab:production)cab.reset();for(auto& mic:reference)mic->reset();
    render(int(rate*.025)+1024,true);
    for(auto& cab:production) {cab.clear();require(!cab.hasResources(),"six-mic worker resources survive clear");}
    require(residual<3e-6,"six-mic direct oracle differs across independent filters, phase, fades or reused responses");
    return residual;
}
}

int main() {try {
    const juce::ScopedNoDenormals noDenormals;
    Residuals maximum;int cases=0;
    for(double rate:{44100.,48000.,96000.}) {
        const original::Settings base{true,1,1,1,2,.4,.73,18.3};
        const auto legacy=original::key(base);
        const auto expansion=expanded::key({base,12,20,2});
        const auto array=layouts::key({{base,9,10,2},7,7});
        for(uint64_t key:{legacy,expansion,array,uint64_t{0}}) {
            juce::AudioBuffer<float> response;
            double responseRate=rate;
            if(key) {
                const auto samples=key==legacy ? original::generate(base,rate)
                    : key==expansion ? expanded::generate(key,rate) : layouts::generate(key,rate);
                response.setSize(1,int(samples.size()));response.copyFrom(0,0,samples.data(),int(samples.size()));
            } else {
                // Synthetic stereo user IR: unequal channel energy, different
                // delay and a late reflection exercise normalization/resampling
                // without bringing any third-party asset into the contract.
                responseRate=48000.;response.setSize(2,4097);response.clear();
                response.setSample(0,3,.71f);response.setSample(0,139,-.19f);response.setSample(0,4096,.07f);
                response.setSample(1,19,.31f);response.setSample(1,277,.23f);response.setSample(1,4089,-.09f);
            }
            for(int block:{17,32,64,128,256,512})for(int channels:{1,2}) {
                const auto residual=compare(response,responseRate,key,{rate,juce::uint32(block),juce::uint32(channels)});
                maximum.uniform=std::max(maximum.uniform,residual.uniform);
                maximum.impulse=std::max(maximum.impulse,residual.impulse);
                maximum.silence=std::max(maximum.silence,residual.silence);++cases;
            }
        }
    }
    double postResidual=0;int postCases=0;
    for(const auto& route:std::array<std::pair<double,int>,9>{{{8000.,1024},{44100.,64},{44100.,512},
        {48000.,32},{48000.,256},{96000.,64},{96000.,128},{96000.,256},{96000.,512}}})
        for(int channels:{1,2}) {postResidual=std::max(postResidual,compareMicPostPath(route.first,route.second,channels));++postCases;}
    const double reprepareResidual=compareReprepare();
    double sixMicResidual=0;int sixMicCases=0;
    for(const auto& route:std::array<std::array<int,3>,6>{{{44100,32,1},{44100,128,2},{48000,64,2},
        {48000,512,1},{96000,64,2},{96000,256,1}}}) {
        sixMicResidual=std::max(sixMicResidual,compareSixMicPublications(route[0],route[1],route[2]));++sixMicCases;
    }
    std::cout<<"PASS CAB realtime equivalence routes="<<cases<<" models=v1,v2,v3,userIR"
        <<" uniform_residual="<<maximum.uniform<<" impulse_residual="<<maximum.impulse
        <<" reset_residual="<<maximum.silence<<" latency=0 chunks=fixed,1,17,63,max,3"
        <<" mic_post_routes="<<postCases<<" mic_post_residual="<<postResidual
        <<" reprepare_routes=4 reprepare_residual="<<reprepareResidual
        <<" six_mic_routes="<<sixMicCases<<" six_mic_residual="<<sixMicResidual
        <<" six_mic_oracle=direct_FIR worker_cases=stale,six_ready,reuse fade_ms=50\n";
    return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
