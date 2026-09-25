#pragma once
#include "FXChain.h"
#include "ModelCatalog.h"
#include "PerformanceUtilities.h"
namespace studioTests {
inline void require(bool ok,const char* message) {if(!ok)throw std::runtime_error(message);}
inline std::vector<float> effects(int selected,int variant=0)
{
    spectralforge::PreFXChain pre;spectralforge::PostFXChain post;pre.prepare({48000,127,2});post.prepare({48000,127,2});
    spectralforge::FXState fx;fx.preCompOn=selected==0;fx.filterOn=selected==1;fx.fuzzOn=selected==2;fx.boostOn=selected==3;fx.driveOn=selected==4;
    fx.busCompOn=selected==5;fx.preampOn=selected==6;fx.eqOn=selected==7;fx.chorusOn=selected==8;fx.delayOn=selected==9;fx.reverbOn=selected==10;fx.eqLow=6;fx.eqMid=-6;fx.eqHigh=6;
    const std::array<int,11> families{3,4,5,6,0,7,8,9,10,1,2};if(selected>=0)fx.models[(size_t)families[(size_t)selected]]=variant;
    juce::AudioBuffer<float> buffer(2,127);std::vector<float> output;
    for(int b=0;b<400;++b) {
        for(int n=0;n<127;++n) {const double t=(b*127+n)/48000.0;const float x=float(.3*std::sin(juce::MathConstants<double>::twoPi*83*t)+.13*std::sin(juce::MathConstants<double>::twoPi*1301*t));buffer.setSample(0,n,x);buffer.setSample(1,n,-x*.6f);}
        pre.process(buffer,false,-60,80,20,false,0,fx);post.process(buffer,fx);
        if(b>150) for(int n=0;n<127;++n) {const float x=buffer.getSample(0,n);require(std::isfinite(x) && std::abs(x)<10,"Studio FX output is unstable");output.push_back(x);}
    }
    return output;
}
inline std::vector<float> dual(bool cross,float blend,int classic=-1)
{
    spectralforge::Engine engine;engine.prepare({48000,127,1});std::array<spectralforge::LaneState,3> lanes{};lanes[0].amp=0;lanes[1].amp=3;for(auto& lane:lanes)lane.cab=false;
    if(classic==1)lanes[0]=lanes[1];juce::AudioBuffer<float> b(1,127);std::vector<float> result;
    for(int block=0;block<360;++block) {
        for(int n=0;n<127;++n)b.setSample(0,n,.1f*float(std::sin(juce::MathConstants<double>::twoPi*220*(block*127+n)/48000)));
        engine.process(b,classic>=0 ? spectralforge::RoutingMode::classic : spectralforge::RoutingMode::dual,350,1200,lanes,nullptr,cross,blend);
        if(block>120)result.insert(result.end(),b.getReadPointer(0),b.getReadPointer(0)+127);
    }
    return result;
}
inline std::vector<float> dualSum(int blockSize,bool reference)
{
    spectralforge::DualCrossover xo;xo.prepare({48000,(juce::uint32)blockSize,1});
    juce::dsp::LinkwitzRileyFilter<float> allpass;allpass.prepare({48000,(juce::uint32)blockSize,1});allpass.setType(juce::dsp::LinkwitzRileyFilterType::allpass);
    juce::SmoothedValue<float,juce::ValueSmoothingTypes::Multiplicative> cutoff;cutoff.reset(48000,.05);cutoff.setCurrentAndTargetValue(350);
    std::array<juce::AudioBuffer<float>,3> bands;for(auto& b:bands)b.setSize(1,blockSize);juce::AudioBuffer<float> input(1,blockSize);std::vector<float> result;unsigned seed=19838;
    for(int offset=0;offset<48000;) {
        const float hz=(offset/4000)%2 ? 4000.f : 60.f;cutoff.setTargetValue(hz);const int count=juce::jmin(blockSize,4000-offset%4000,48000-offset);input.setSize(1,count,false,false,true);
        for(int n=0;n<count;++n){seed=seed*1664525u+1013904223u;input.setSample(0,n,(float(seed>>8)/8388608.f-1)*.1f);}
        if(reference)for(int n=0;n<count;++n){const float hz=cutoff.getNextValue();if((offset+n)%16==0)allpass.setCutoffFrequency(hz);result.push_back(allpass.processSample(0,input.getSample(0,n)));}
        else{xo.split(input,bands,hz);for(int n=0;n<count;++n)result.push_back(bands[0].getSample(0,n)+bands[1].getSample(0,n));}
        offset+=count;
    }
    return result;
}
inline void contracts()
{
    // Program-dependent detectors must give the same envelope in small live
    // blocks and large offline-render blocks. Include a release after a burst.
    for(int character=0;character<5;++character) {
        const auto render=[&](int blockSize) {
            spectralforge::DynamicsModule compressor;compressor.prepare({48000,(juce::uint32)blockSize,2});
            juce::AudioBuffer<float> buffer(2,blockSize);std::vector<float> result;
            for(int offset=0;offset<24000;) {
                const int count=std::min(blockSize,24000-offset);buffer.setSize(2,count,false,false,true);
                for(int n=0;n<count;++n) {const int t=offset+n;const float x=(t<6000 ? .7f : .05f)*std::sin(juce::MathConstants<float>::twoPi*125*t/48000);buffer.setSample(0,n,x);buffer.setSample(1,n,-x*.4f);}
                compressor.process(buffer,true,-25,5,12,100,0,character);
                for(int n=0;n<count;++n) {require(std::abs(buffer.getSample(1,n)+.4f*buffer.getSample(0,n))<1e-6,"Compressor is not stereo linked");result.push_back(buffer.getSample(0,n));}
                offset+=count;
            }
            return result;
        };
        const auto small=render(17),large=render(511);float error=0;
        for(size_t n=0;n<small.size();++n)error=std::max(error,std::abs(small[n]-large[n]));
        std::cout<<"MEASURE compressor "<<character<<" block-size null "<<error<<"\n";
        require(error<1e-6,"Program-dependent compressor recovery changes with host block size");
    }
    const auto driveRender=[](int blockSize) {
        spectralforge::DriveModule drive;drive.prepare({48000,(juce::uint32)blockSize,2});spectralforge::FXState fx;fx.driveOn=true;
        juce::AudioBuffer<float> buffer(2,blockSize);std::vector<float> result;
        for(int offset=0;offset<30000;) {
            fx.models[0]=offset/6000;fx.drive=fx.models[0]%2 ? .8f : .35f;fx.tone=fx.models[0]%2 ? 1300.f : 9000.f;
            const int count=juce::jmin(blockSize,6000-offset%6000,30000-offset);buffer.setSize(2,count,false,false,true);
            for(int n=0;n<count;++n) {const int t=offset+n;buffer.setSample(0,n,.45f*std::sin(juce::MathConstants<float>::twoPi*125*t/48000));buffer.setSample(1,n,0);}
            drive.process(buffer,fx);
            for(int n=0;n<count;++n) {require(std::abs(buffer.getSample(1,n))<1e-7,"Drive leaks signal into the silent stereo channel");result.push_back(buffer.getSample(0,n));}
            offset+=count;
        }
        return result;
    };
    const auto small=driveRender(17),large=driveRender(511);float driveError=0;
    for(size_t n=0;n<small.size();++n)driveError=std::max(driveError,std::abs(small[n]-large[n]));
    std::cout<<"MEASURE automated drive block-size null "<<driveError<<"\n";
    require(driveError<3e-6,"Drive model/parameter smoothing depends on host block size");
    for(int model=0;model<5;++model) {
        spectralforge::DriveModule drive;drive.prepare({48000,240,1});spectralforge::FXState fx;fx.driveOn=true;fx.drive=.8f;fx.models[0]=model;
        juce::AudioBuffer<float> buffer(1,240);double dc=0;
        for(int block=0;block<200;++block) {
            for(int n=0;n<240;++n) {const int t=block*240+n;buffer.setSample(0,n,.4f*std::sin(juce::MathConstants<float>::twoPi*60*t/48000)+.1f*std::sin(juce::MathConstants<float>::twoPi*420*t/48000));}
            drive.process(buffer,fx);if(block>=100)for(int n=0;n<240;++n)dc+=buffer.getSample(0,n);
        }
        dc/=24000;std::cout<<"MEASURE drive "<<model<<" DC mean "<<dc<<"\n";
        require(std::abs(dc)<1e-4,"Asymmetric drive retains signal-generated DC");
    }
    // All modules were active before bypass: history/tails must still disappear
    // from the dry path, with identical fixed latency at every supported rate.
    for(double rate:{44100.0,48000.0,96000.0,192000.0}) {
        spectralforge::PreFXChain pre;spectralforge::PostFXChain post;pre.prepare({rate,127,2});post.prepare({rate,127,2});
        spectralforge::FXState fx;fx.models={4,2,2,4,4,4,4,2,2,2,5};
        juce::AudioBuffer<float> buffer(2,127);float maximum=0;
        const int warm=juce::roundToInt(rate*.1),settled=juce::roundToInt(rate*.18),total=juce::roundToInt(rate*.22),delay=pre.latency(false)+post.latency();
        for(int offset=0;offset<total;offset+=127) {
            const bool enabled=offset<warm;
            fx.preCompOn=fx.filterOn=fx.fuzzOn=fx.boostOn=fx.driveOn=fx.busCompOn=fx.preampOn=fx.eqOn=fx.chorusOn=fx.delayOn=fx.reverbOn=enabled;
            for(int n=0;n<127;++n) {const float x=.2f*float(std::sin(juce::MathConstants<double>::twoPi*137*(offset+n)/rate));buffer.setSample(0,n,x);buffer.setSample(1,n,-.4f*x);}
            pre.process(buffer,false,-60,80,20,false,0,fx);post.process(buffer,fx);
            if(offset>=settled)for(int n=0;n<127;++n) {const float expected=.2f*float(std::sin(juce::MathConstants<double>::twoPi*137*(offset+n-delay)/rate));maximum=std::max(maximum,std::abs(expected-buffer.getSample(0,n)));maximum=std::max(maximum,std::abs(-.4f*expected-buffer.getSample(1,n)));}
        }
        std::cout<<"MEASURE FX return-to-bypass "<<rate<<" Hz: dry null "<<maximum<<"; PRE GR "<<pre.compressor.reduction()<<"; POST GR "<<post.compressorReduction()<<"\n";
        require(maximum<1e-6,"Bypassed FX retain coloration or wet tails after bypass settles");
        require(std::abs(pre.compressor.reduction())<1e-6 && std::abs(post.compressorReduction())<1e-6,"Bypassed compressor reports gain reduction which is not being applied");
        require(std::abs(post.stagePeaks.back()-buffer.getMagnitude(0,buffer.getNumSamples()))<1e-6,"Rack meter does not report actual stage output");
        post.reset();for(float peak:post.stagePeaks)require(peak==0,"Rack meters retain a stale level after reset");
    }
    std::cout<<"PASS: sample-clock smoothing, stereo separation, DC removal, bypass return and real rack meters\n";
}
inline void colourAutomation()
{
    for(bool fuzz:{false,true})for(int model=0;model<(fuzz ? 5 : 3);++model) {
        const auto render=[&](int blockSize,bool automate) {
            spectralforge::ColourModule effect;effect.prepare({48000,(juce::uint32)blockSize,2});
            juce::AudioBuffer<float> buffer(2,blockSize);std::vector<float> result;
            for(int offset=0;offset<18000;) {
                const int count=juce::jmin(blockSize,6000-offset%6000,18000-offset);buffer.setSize(2,count,false,false,true);
                for(int n=0;n<count;++n) {const float x=.3f*std::sin(juce::MathConstants<float>::twoPi*3500*(offset+n)/48000);buffer.setSample(0,n,x);buffer.setSample(1,n,x);}
                effect.process(buffer,true,6,automate && offset>=6000 && offset<12000 ? 1.f : 0.f,0,fuzz,model);
                for(int n=0;n<count;++n) {require(std::abs(buffer.getSample(0,n)-buffer.getSample(1,n))<1e-6,"Colour tone automation changes the stereo image");result.push_back(buffer.getSample(0,n));}
                offset+=count;
            }
            return result;
        };
        const auto small=render(17,true),large=render(511,true),unchanged=render(511,false);float blockError=0,initialChange=0,settledChange=0,returnError=0;
        for(size_t n=0;n<small.size();++n) {
            blockError=std::max(blockError,std::abs(small[n]-large[n]));
            if(n>=6000 && n<6016)initialChange=std::max(initialChange,std::abs(small[n]-unchanged[n]));
            if(n>=8000 && n<10000)settledChange=std::max(settledChange,std::abs(small[n]-unchanged[n]));
            if(n>=17000)returnError=std::max(returnError,std::abs(small[n]-unchanged[n]));
        }
        std::cout<<"MEASURE "<<(fuzz ? "fuzz " : "preamp ")<<model<<" tone automation: initial/settled "<<initialChange<<"/"<<settledChange<<"; block null "<<blockError<<"; return null "<<returnError<<"\n";
        require(settledChange>1e-3,"Colour tone control is not connected");
        require(initialChange<settledChange*.12f,"Colour tone changes abruptly at the automation boundary");
        require(blockError<3e-6,"Colour tone automation depends on host block size");
        require(returnError<1e-5,"Colour tone does not return to its original response after automation");
    }
    std::cout<<"PASS: all eight fuzz/preamp voices preserve stereo and smooth tone automation across host block sizes\n";
}
inline void gainOrderContracts()
{
    // Boost after clipping must offer output lift, whereas boost before it
    // drives the clipper harder. Both must leave the clean Matrix tap intact.
    for (double rate : {44100., 96000.}) {
        std::array<std::vector<float>, 2> reference;
        for (int blockSize : {17, 511}) {
            spectralforge::PreFXChain before, after;
            before.prepare({rate, (juce::uint32)blockSize, 2});
            after.prepare({rate, (juce::uint32)blockSize, 2});
            spectralforge::FXState a;
            a.fuzzOn=a.boostOn=a.driveOn=true;a.models[5]=1;
            a.boostGain=12;a.boostBass=0;a.boostTreble=0;a.drive=.8f;
            auto b=a;b.boostAfterDrive=true;
            std::array<std::vector<float>, 2> output;
            juce::AudioBuffer<float> left(2,blockSize),right(2,blockSize);
            double cleanError=0;
            for(int offset=0;offset<16384;offset+=blockSize) {
                const int count=juce::jmin(blockSize,16384-offset);
                left.setSize(2,count,false,false,true);right.setSize(2,count,false,false,true);
                for(int n=0;n<count;++n) {
                    const double t=(offset+n)/rate;
                    const float x=float(.18*std::sin(juce::MathConstants<double>::twoPi*220*t)+.045*std::sin(juce::MathConstants<double>::twoPi*660*t));
                    left.setSample(0,n,x);left.setSample(1,n,0);
                }
                right.makeCopyOf(left,true);
                before.process(left,false,-60,80,20,false,0,a);
                after.process(right,false,-60,80,20,false,0,b);
                for(int n=0;n<count;++n) {
                    cleanError=juce::jmax(cleanError,std::abs(double(before.cleanOutput().getSample(0,n)-after.cleanOutput().getSample(0,n))));
                    for(auto* buffer : {&left,&right}) {
                        require(std::isfinite(buffer->getSample(0,n)) && std::abs(buffer->getSample(0,n))<8.f,"Gain-order output is unstable");
                        require(std::abs(buffer->getSample(1,n))<1e-7f,"Gain-order processing leaks across stereo channels");
                    }
                    output[0].push_back(left.getSample(0,n));output[1].push_back(right.getSample(0,n));
                }
            }
            require(cleanError<1e-7,"Gain order changed Matrix clean DI");
            require(before.latency(false)==after.latency(false) && before.latency(true)==after.latency(true),"Gain order changed latency");
            double beforeEnergy=0,afterEnergy=0,difference=0;
            for(size_t n=4096;n<output[0].size();++n) {
                beforeEnergy+=output[0][n]*output[0][n];afterEnergy+=output[1][n]*output[1][n];
                difference+=std::pow(output[0][n]-output[1][n],2);
            }
            require(std::sqrt(afterEnergy/beforeEnergy)>1.5,"Post-drive boost does not provide the expected headroom-dependent level lift");
            require(difference>1,"Gain-order choice does not change active processing");
            if(blockSize==17) reference=output;
            else for(size_t order=0;order<2;++order) {
                double maximum=0;size_t peakAt=0;
                for(size_t n=0;n<output[order].size();++n) if(std::abs(output[order][n]-reference[order][n])>maximum) {
                    maximum=std::abs(output[order][n]-reference[order][n]);peakAt=n;
                }
                std::cout<<"MEASURE gain order "<<order<<" at "<<rate<<" Hz: block null "<<maximum<<" at "<<peakAt<<"\n";
                require(maximum<3e-5,"Gain-order result depends on host block size");
            }
        }
        for(bool boostAfterDrive : {false,true}) {
            spectralforge::PreFXChain bypass;bypass.prepare({rate,127,2});
            spectralforge::FXState fx;fx.boostAfterDrive=boostAfterDrive;
            juce::AudioBuffer<float> impulse(2,127);impulse.clear();impulse.setSample(0,0,1);
            bypass.process(impulse,false,-60,80,20,false,0,fx);
            for(int n=0;n<127;++n) require(std::abs(impulse.getSample(0,n)-(n==bypass.latency(false)?1.f:0.f))<1e-7,"Gain-order bypass is not latency-aligned dry audio");
        }
    }
    std::cout<<"PASS: both gain orders change saturation/level, preserve clean DI and latency, stereo isolation and 17/511-sample block independence\n";
}
inline void run()
{
    contracts();
    colourAutomation();
    gainOrderContracts();
    // Detector order must alter an expressive filter without moving the Matrix
    // clean tap through fuzz/boost/drive or changing algorithmic delay.
    for(bool first:{false,true}) {
        spectralforge::PreFXChain clean,driven,opposite;
        for(auto* chain:{&clean,&driven,&opposite})chain->prepare({48000,127,2});
        spectralforge::FXState fx;fx.envelopeFirst=first;fx.preCompOn=fx.filterOn=true;fx.preComp=.8f;
        auto wet=fx;wet.fuzzOn=wet.boostOn=wet.driveOn=true;wet.models[0]=4;wet.models[5]=4;
        auto reversed=fx;reversed.envelopeFirst=!first;
        double tapError=0,orderError=0;juce::AudioBuffer<float> a(2,127),b(2,127),c(2,127);
        for(int block=0;block<180;++block) {
            for(int n=0;n<127;++n) {const int t=block*127+n;const float attack=std::exp(-float(t%4800)/1100);const float x=attack*.6f*std::sin(juce::MathConstants<float>::twoPi*110*t/48000);a.setSample(0,n,x);a.setSample(1,n,-x*.5f);}
            b.makeCopyOf(a);c.makeCopyOf(a);clean.process(a,false,-60,80,20,false,0,fx);driven.process(b,false,-60,80,20,false,0,wet);opposite.process(c,false,-60,80,20,false,0,reversed);
            for(int n=0;n<127;++n) {tapError=std::max(tapError,double(std::abs(clean.cleanOutput().getSample(0,n)-driven.cleanOutput().getSample(0,n))));orderError+=std::pow(a.getSample(0,n)-c.getSample(0,n),2);}
        }
        require(tapError<1e-6,"Pedal order leaked saturation into Matrix clean DI");require(orderError>1e-4,"Pedal order does not change envelope dynamics");require(clean.latency(false)==opposite.latency(false),"Pedal order changed latency");
    }
    std::cout<<"PASS: both detector orders affect dynamics, preserve Matrix clean tap and fixed latency\n";
    {spectralforge::PreFXChain pre;spectralforge::PostFXChain post;pre.prepare({48000,127,2});post.prepare({48000,127,2});spectralforge::FXState state;juce::AudioBuffer<float> impulse(2,127);impulse.clear();impulse.setSample(0,0,1);impulse.setSample(1,0,-.5f);
     pre.process(impulse,false,-60,80,20,false,0,state);post.process(impulse,state);const int delay=pre.latency(false)+post.latency();
     for(int n=0;n<127;++n){require(std::abs(impulse.getSample(0,n)-(n==delay?1.f:0.f))<1e-6,"Bypassed pedal/rack startup is not an exact latency-aligned dry path");require(std::abs(impulse.getSample(0,n)+2*impulse.getSample(1,n))<1e-6,"Bypassed FX changed stereo polarity");}
     std::cout<<"MEASURE all FX bypass at startup: exact dry impulse at "<<delay<<" samples\n";
    }

    const auto off=effects(-1);for(int i=0;i<11;++i) {
        const auto on=effects(i);double residual=0;for(size_t n=0;n<off.size();++n)residual+=std::pow(off[n]-on[n],2);residual=std::sqrt(residual/off.size());
        std::cout<<"MEASURE effect module "<<i<<": enabled/bypass residual RMS "<<residual<<"\n";require(residual>1e-4,"A studio FX module is not connected");
    }
    const std::array<int,11> families{3,4,5,6,0,7,8,9,10,1,2};
    for(int module=0;module<11;++module){const int family=families[(size_t)module];std::vector<std::vector<float>> renders;
        for(int model=0;model<spectralforge::modelFamilies[(size_t)family].count;++model)renders.push_back(effects(module,model));
        double minimum=1e9;for(size_t a=0;a<renders.size();++a)for(size_t b=a+1;b<renders.size();++b){double difference=0;for(size_t n=0;n<renders[a].size();++n)difference+=std::pow(renders[a][n]-renders[b][n],2);minimum=std::min(minimum,std::sqrt(difference/renders[a].size()));}
        std::cout<<"MEASURE "<<spectralforge::modelFamilies[(size_t)family].category<<" model pairs: minimum residual "<<minimum<<"\n";require(minimum>1e-5,"Two selectable models produce the same audio");
    }
    for(double rate:{44100.0,96000.0}) {
        spectralforge::PreFXChain pre;spectralforge::PostFXChain post;pre.prepare({rate,511,2});post.prepare({rate,511,2});spectralforge::FXState fx;
        fx.preCompOn=fx.filterOn=fx.fuzzOn=fx.boostOn=fx.driveOn=fx.busCompOn=fx.preampOn=fx.eqOn=fx.chorusOn=fx.delayOn=fx.reverbOn=true;
        fx.feedback=.85f;fx.room=1;fx.chorusDepth=1;juce::AudioBuffer<float> buffer(2,511);
        for(int block=0;block<180;++block){for(size_t i=0;i<fx.models.size();++i)fx.models[i]=(block/13)%spectralforge::modelFamilies[i].count;
            for(int n=0;n<511;++n){const float x=block<100 ? .2f*float(std::sin(juce::MathConstants<double>::twoPi*110*(block*511+n)/rate)) : 0.f;buffer.setSample(0,n,x);buffer.setSample(1,n,-.7f*x);}
            pre.process(buffer,false,-60,80,20,false,0,fx);post.process(buffer,fx);
            for(int c=0;c<2;++c)for(int n=0;n<511;++n)require(std::isfinite(buffer.getSample(c,n)) && std::abs(buffer.getSample(c,n))<20,"Model switching or feedback tails are unstable");
        }
    }
    std::cout<<"PASS: 46 distinct selectable models; stereo model changes and tails at 44.1/96 kHz\n";
    const auto left=dual(false,0),right=dual(false,1),a=dual(false,0,0),b=dual(false,1,1),blend=dual(false,.25f);
    double endpoint=0,blendError=0;for(size_t n=0;n<a.size();++n){endpoint=juce::jmax(endpoint,std::abs(double(left[n]-a[n])),std::abs(double(right[n]-b[n])));blendError=juce::jmax(blendError,std::abs(double(blend[n]-(.75f*a[n]+.25f*b[n]))));}
    require(endpoint<1e-6 && blendError<1e-6,"Dual blend endpoints or balance are incorrect");
    const auto moving=dualSum(127,false),other=dualSum(511,false),reference=dualSum(127,true);double sum=0,blocks=0;
    for(size_t n=0;n<moving.size();++n){sum=juce::jmax(sum,std::abs(double(moving[n]-reference[n])));blocks=juce::jmax(blocks,std::abs(double(moving[n]-other[n])));}
    std::cout<<"MEASURE Dual blend null "<<blendError<<", crossover/allpass null "<<sum<<", block-size null "<<blocks<<"\n";require(sum<1e-5 && blocks<1e-6,"Dual crossover sum/phase is inconsistent");
    spectralforge::PerformanceUtilities utilities;utilities.prepare({48000,127,2});juce::AudioBuffer<float> buffer(2,127);buffer.clear();utilities.process(buffer,120,false,6,false,false);require(buffer.getMagnitude(0,127)==0,"Performance utilities generate sound while off");
    buffer.clear();utilities.process(buffer,120,false,6,true,true);require(buffer.getMagnitude(0,127)>.01f,"Metronome is not audible");
    std::cout<<"PASS: eleven independent FX modules, Dual blend/crossover and metronome\n";
}
}
