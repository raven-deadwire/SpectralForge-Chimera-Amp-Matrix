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

// Independent, sample-at-a-time reference for the filter-only mic path. Keeping
// an identity IR isolates the history/mix/bypass work from convolution, while
// explicit per-channel samples expose wrong copy order and ring-wrap mistakes.
struct ReferenceMic {
    std::array<juce::dsp::IIR::Filter<float>,2> high,low;
    std::array<std::vector<float>,2> history;
    juce::SmoothedValue<float> gain,delay;
    int write{};double rate;
    explicit ReferenceMic(double sampleRate):rate(sampleRate) {
        for(int c=0;c<2;++c) {
            high[size_t(c)].coefficients=juce::dsp::IIR::Coefficients<float>::makeHighPass(rate,juce::jlimit(10.f,float(rate*.44),70.f));
            low[size_t(c)].coefficients=juce::dsp::IIR::Coefficients<float>::makeLowPass(rate,juce::jlimit(100.f,float(rate*.45),9000.f));
            history[size_t(c)].resize(size_t(int(rate*.020)+2));
        }
        gain.reset(rate,.020);gain.setCurrentAndTargetValue(1.f);
        delay.reset(rate,.020);delay.setCurrentAndTargetValue(0.f);
    }
    void reset() {
        for(int c=0;c<2;++c) {high[size_t(c)].reset();low[size_t(c)].reset();std::fill(history[size_t(c)].begin(),history[size_t(c)].end(),0.f);}
        write=0;
    }
    void targets(float db,bool invert,float ms) {
        gain.setTargetValue(juce::Decibels::decibelsToGain(db)*(invert ? -1.f : 1.f));
        delay.setTargetValue(float(rate*.001)*juce::jlimit(0.f,20.f,ms));
    }
    std::array<float,2> tick(const std::array<float,2>& input,int channels) {
        const float g=gain.getNextValue(),d=delay.getNextValue(),fraction=d-int(d);
        const int size=int(history[0].size()),read=(write-int(d)+size)%size,previous=(read+size-1)%size;
        std::array<float,2> result{};
        for(int c=0;c<channels;++c) {
            auto& samples=history[size_t(c)];
            samples[size_t(write)]=low[size_t(c)].processSample(high[size_t(c)].processSample(input[size_t(c)]));
            result[size_t(c)]=g*((1.f-fraction)*samples[size_t(read)]+fraction*samples[size_t(previous)]);
        }
        write=(write+1)%size;return result;
    }
};

double compareMicPostPath(double rate,int maximum,int channels) {
    const juce::dsp::ProcessSpec spec{rate,juce::uint32(maximum),juce::uint32(channels)};
    Cab production;production.blend=.35f;production.prepare(spec);
    ReferenceMic a(rate),b(rate);
    juce::SmoothedValue<float> blend,enabled;
    blend.reset(rate,.020);blend.setCurrentAndTargetValue(.35f);
    enabled.reset(rate,.020);enabled.setCurrentAndTargetValue(1.f);
    struct Step {float delayA,delayB,gainA,gainB,mix;bool invertA,invertB,on;};
    const std::array<Step,10> steps{{
        {0,0,0,0,.35f,false,false,true},
        {.125f,.375f,0,0,.35f,false,false,true},
        {20,17.9375f,-3,4,.75f,false,true,true},
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
            for(int block:{17,64,128,256})for(int channels:{1,2}) {
                const auto residual=compare(response,responseRate,key,{rate,juce::uint32(block),juce::uint32(channels)});
                maximum.uniform=std::max(maximum.uniform,residual.uniform);
                maximum.impulse=std::max(maximum.impulse,residual.impulse);
                maximum.silence=std::max(maximum.silence,residual.silence);++cases;
            }
        }
    }
    double postResidual=0;int postCases=0;
    for(const auto& route:std::array<std::pair<double,int>,4>{{{8000.,1024},{44100.,64},{48000.,256},{96000.,64}}})
        for(int channels:{1,2}) {postResidual=std::max(postResidual,compareMicPostPath(route.first,route.second,channels));++postCases;}
    std::cout<<"PASS CAB realtime equivalence routes="<<cases<<" models=v1,v2,v3,userIR"
        <<" uniform_residual="<<maximum.uniform<<" impulse_residual="<<maximum.impulse
        <<" reset_residual="<<maximum.silence<<" latency=0 chunks=fixed,1,17,63,max,3"
        <<" mic_post_routes="<<postCases<<" mic_post_residual="<<postResidual<<'\n';
    return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
