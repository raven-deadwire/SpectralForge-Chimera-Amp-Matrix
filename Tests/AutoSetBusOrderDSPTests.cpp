// PR #267: independent measurement at the real POST compressor boundary.
// Synthetic deterministic input; this is not an instrument/listening test.
#include "AutoSetBus.h"
#include "FXChain.h"
#include <iostream>
#include <iomanip>
#include <random>
#include <stdexcept>
using namespace spectralforge;
using namespace spectralforge::autoset;
static void require(bool b,const char* why) { if(!b)throw std::runtime_error(why); }
static std::vector<float> playing(double rate) {
    constexpr std::array<int,11> notes{0,3,5,7,10,12,15,17,19,22,24};
    std::mt19937 random{160};std::normal_distribution<float> hiss(0,.0005f);
    std::vector<float> x(size_t(rate*busRenderSeconds));
    const size_t length=size_t(rate*.25);
    for(size_t n=0;n<x.size();++n) {
        const double f=82.41*std::pow(2.,notes[(n/length*4)%notes.size()]/12.);
        const double t=double(n%length)/rate,env=std::exp(-t*6)*std::min(1.,t/.003);
        double v=0;for(int h=1;h<=8;++h)if(f*h<rate*.45)v+=std::sin(2*juce::MathConstants<double>::pi*f*h*t)/h;
        x[n]=(float(.12*env*v)+hiss(random))*((n/length)%3==0?2.f:.8f);
    }
    return x;
}
struct Boundary {std::vector<float> input,output;};
static Boundary renderRack(const std::vector<float>& input,double rate,FXState state,const PostOrder& order,const BusSetting& setting,bool makeup) {
    auto& bank=state.postNative.sections[0].banks[0];bank.bypass=false;
    bank.values[0]=setting.threshold;bank.values[1]=makeup?setting.makeup:0;
    bank.values[2]=float(setting.attack);bank.values[3]=float(setting.release);bank.values[4]=float(setting.ratio);bank.values[5]=1;
    constexpr int block=256;
    auto rack=std::make_unique<PostFXChain>();rack->prepare({rate,block,2});
    auto analysis=std::make_unique<DynamicsAnalysis>();analysis->prepare(rate,block);
    analysis->select(DynamicsStage::postCompressor);analysis->fifo.enabled.store(true);rack->setAnalysis(analysis.get());
    Boundary out;out.input.reserve(input.size());out.output.reserve(input.size());
    juce::AudioBuffer<float> buffer(2,block);
    for(size_t offset=0;offset<input.size();offset+=block) {
        const int n=int(std::min(size_t(block),input.size()-offset));buffer.setSize(2,n,false,false,true);
        for(int c=0;c<2;++c)buffer.copyFrom(c,0,input.data()+offset,n);
        rack->process(buffer,state,order);
        EQAnalyzerFrame frame;while(analysis->fifo.pop(frame)){out.input.push_back(frame.inputL);out.output.push_back(frame.outputL);}
    }
    require(out.input.size()==input.size() && analysis->fifo.dropped.load()==0,"POST boundary capture lost samples");
    return out;
}
// Signed mean block level change, with exactly the playing/warm-up gate used
// by busGlue. Positive means over-compensation by the proposed make-up.
static float residual(const Boundary& b,double rate) {
    double sum=0;int count=0;const size_t step=size_t(rate*.02);
    for(size_t n=size_t(rate*busWarmupSeconds);n+step<=b.input.size();n+=step) {
        double x=0,y=0;for(size_t i=n;i<n+step;++i){x+=double(b.input[i])*b.input[i];y+=double(b.output[i])*b.output[i];}
        if(toDb(std::sqrt(x/step))>minimumPlayingDb){sum+=toDb(std::sqrt(y/step))-toDb(std::sqrt(x/step));++count;}
    }
    require(count>=20,"Insufficient blocks");return float(sum/count);
}
int main(int argc,char** argv) {
    try {
        const bool keptOnly=argc==2 && std::string(argv[1])=="--kept-only";
        require(argc==1 || keptOnly,"Unknown argument");
        std::cout<<"rate,style,on,strength,case,threshold,makeup,predicted_gr,actual_gr,gr_error,makeup_residual,corrected_threshold,corrected_makeup,corrected_gr,corrected_prediction_error,corrected_residual\n";
        float maxDefault=0,maxCorrected=0,maxWrong=0;int rows=0;
        if(!keptOnly)for(double rate:{44100.,48000.,96000.})for(auto style:{Style::clean,Style::rock,Style::metal})for(bool on:{false,true})for(float strength:{1.f,.5f}) {
            const auto x=playing(rate);FXState state;state.postNative=defaultPostNativeState(true);
            // An existing user's non-default attack/release/ratio must survive.
            const BusSetting current=on?BusSetting{-40,5,0,0,2}:BusSetting{-18,0,4,1,1};
            const auto predicted=analyseBus(x,rate,current,on,nativeBusRenderer(state.postNative.sections[0],x,rate),style,strength);
            for(int scenario=0;scenario<5;++scenario) {
                auto fx=state;auto order=PostOrder::initial();const char* name="default";
                if(scenario==1 || scenario==2) {
                    name=scenario==1?"eq_boost_12":"eq_cut_12";order=order.placed(postEQModule,0);
                    auto& eq=fx.postNative.sections[2].banks[0];eq.bypass=false;
                    eq.values[6]=scenario==1?12.f:-12.f;eq.values[7]=600;eq.values[8]=.5f;
                }
                if(scenario==3 || scenario==4) {
                    name=scenario==3?"preamp_plus_15":"preamp_minus_10";order=order.placed(postPreampModule,0);
                    auto& pre=fx.postNative.sections[1].banks[0];pre.bypass=false;pre.values[0]=scenario==3?5.f:0.f;
                }
                const auto actual=renderRack(x,rate,fx,order,predicted.proposed,false);
                const auto gr=busGlue(actual.input,actual.output,rate);
                const auto level=renderRack(x,rate,fx,order,predicted.proposed,true);
                // Experimental alternative: analyse the actual compressor input.
                const auto corrected=analyseBus(actual.input,rate,current,on,nativeBusRenderer(fx.postNative.sections[0],actual.input,rate),style,strength);
                const auto correctedAudio=renderRack(x,rate,fx,order,corrected.proposed,false);
                const auto correctedGR=busGlue(correctedAudio.input,correctedAudio.output,rate);
                const auto correctedLevel=renderRack(x,rate,fx,order,corrected.proposed,true);
                const float error=gr.loudDb-predicted.glueAfterDb,correctedError=correctedGR.loudDb-corrected.glueAfterDb;
                if(scenario==0)maxDefault=std::max(maxDefault,std::abs(error));
                maxWrong=std::max(maxWrong,std::abs(error));maxCorrected=std::max(maxCorrected,std::abs(correctedError));
                require(std::abs(correctedError)<.01f,"Actual-input analysis disagrees with POST DSP");
                require(corrected.reachable && std::abs(correctedGR.loudDb-corrected.glueTargetDb)<=busToleranceDb,"Corrected DSP misses style/Strength target");
                require(std::abs(residual(correctedLevel,rate))<=.26f,"Corrected make-up exceeds grid error");
                if(on)require(corrected.proposed.ratio==current.ratio && corrected.proposed.attack==current.attack && corrected.proposed.release==current.release,"Existing controls changed");
                std::cout<<std::fixed<<std::setprecision(6)<<rate<<','<<styleName(style)<<','<<on<<','<<strength<<','<<name<<','
                         <<predicted.proposed.threshold<<','<<predicted.proposed.makeup<<','<<predicted.glueAfterDb<<','<<gr.loudDb<<','<<error<<','<<residual(level,rate)<<','
                         <<corrected.proposed.threshold<<','<<corrected.proposed.makeup<<','<<correctedGR.loudDb<<','<<correctedError<<','<<residual(correctedLevel,rate)<<'\n';++rows;
            }
        }
        if(!keptOnly){require(maxDefault<.01f,"Default order diverged");require(maxWrong>2.f,"Failure fixture no longer reproduces; review it");}
        // A second run can falsely report an already-active compressor as
        // on-target: the pre-rack capture still omits the preceding preamp.
        {
            const auto x=playing(48000);FXState fx;fx.postNative=defaultPostNativeState(true);
            const auto render=nativeBusRenderer(fx.postNative.sections[0],x,48000);
            const auto first=analyseBus(x,48000,{},false,render,Style::metal);
            const auto kept=analyseBus(x,48000,first.proposed,true,render,Style::metal);
            fx.postNative.sections[1].banks[0].bypass=false;fx.postNative.sections[1].banks[0].values[0]=5;
            const auto actual=renderRack(x,48000,fx,PostOrder::initial().placed(postPreampModule,0),kept.proposed,false);
            const float gr=busGlue(actual.input,actual.output,48000).loudDb;
            require(!kept.changed() && gr-kept.glueTargetDb>5,"Already-on false-keep fixture no longer reproduces");
            std::cerr<<"MEASURE already-on false keep: predicted="<<kept.glueAfterDb<<" actual="<<gr<<" target="<<kept.glueTargetDb<<" dB\n";
        }
        if(keptOnly)std::cerr<<"PASS already-on false-keep reproduction\n";
        else std::cerr<<"PASS "<<rows<<" measurements and false-keep reproduction; max default error="<<maxDefault<<" dB, old reordered error="<<maxWrong<<" dB, actual-input prediction error="<<maxCorrected<<" dB\n";
    }catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
}
