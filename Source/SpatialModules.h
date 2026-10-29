#pragma once
#include <juce_dsp/juce_dsp.h>

namespace spectralforge {
template<size_t N> struct ModelMorph {
    std::array<juce::SmoothedValue<float>,N> weights;
    void prepare(double rate) {for(size_t i=0;i<N;++i){weights[i].reset(rate,.035);weights[i].setCurrentAndTargetValue(i==0 ? 1.f : 0.f);}}
    void select(int choice) {choice=juce::jlimit(0,int(N)-1,choice);for(size_t i=0;i<N;++i)weights[i].setTargetValue(int(i)==choice ? 1.f : 0.f);}
    std::array<float,N> next() {std::array<float,N> result;for(size_t i=0;i<N;++i)result[i]=weights[i].getNextValue();return result;}
};
// 1.8 POST motion/space voices (docs/POST_MOTION_SPACE_1_8.md). Each runs
// only while its morph weight is non-zero; a voice that falls silent clears
// its state, so re-selecting it never replays an old tail.
namespace spatial_detail {
// Eight-line feedback delay network with a Hadamard mix, per-line damping and
// a slow modulation on two lines. Storage is sized in prepare.
struct FDN {
    std::array<std::vector<float>,8> lines;std::array<size_t,8> index{},length{};std::array<float,8> damp{};
    double rate{48000},modPhase{};
    void prepare(double sampleRate,double maxSeconds) {rate=sampleRate;for(auto& l:lines)l.assign(size_t(rate*maxSeconds)+64,0.f);reset();}
    void reset() noexcept {for(auto& l:lines)std::fill(l.begin(),l.end(),0.f);index={};damp={};modPhase=0;}
    // Line lengths scale a fixed mutually-prime set (samples at 44.1 kHz).
    void setScale(double scale) noexcept {
        static constexpr std::array<int,8> base{1557,1617,1491,1422,1277,1356,1188,1116};
        for(size_t i=0;i<8;++i)length[i]=std::min(lines[i].size()-40,size_t(std::max(8.,base[i]*scale*rate/44100.)));
    }
    float tick(float input,double t60,float dampingPole,float modSamples,double modHz) noexcept {
        modPhase+=modHz/rate;modPhase-=std::floor(modPhase);
        std::array<float,8> d{};
        for(size_t i=0;i<8;++i) {
            const float wobble=i<2?modSamples*float(std::sin(juce::MathConstants<double>::twoPi*(modPhase+.37*double(i)))):0.f;
            const float position=float(index[i])-float(length[i])-wobble;
            const float wrapped=position<0?position+float(lines[i].size()):position;
            const size_t a=size_t(wrapped)%lines[i].size(),b=(a+1)%lines[i].size();const float frac=wrapped-std::floor(wrapped);
            float v=lines[i][a]+frac*(lines[i][b]-lines[i][a]);
            damp[i]+=dampingPole*(v-damp[i]);d[i]=damp[i]*float(std::pow(10.,-3.*double(length[i])/(t60*rate)));
        }
        // Fast Hadamard transform (orthogonal after the 1/sqrt(8) scale).
        for(size_t h=1;h<8;h*=2)for(size_t i=0;i<8;i+=2*h)for(size_t j=i;j<i+h;++j){const float x=d[j],y=d[j+h];d[j]=x+y;d[j+h]=x-y;}
        float out=0;
        for(size_t i=0;i<8;++i){const float v=d[i]*.35355339f;lines[i][index[i]]=v+(i%2?-input:input)*.35f;out+=(i%2?-1.f:1.f)*v;if(++index[i]>=lines[i].size())index[i]=0;}
        return out*.35f;
    }
};
inline float onePole(double hz,double rate) noexcept {return float(1-std::exp(-juce::MathConstants<double>::twoPi*hz/rate));}
} // namespace spatial_detail
class ModulationModule {
    using Delay=juce::dsp::DelayLine<float,juce::dsp::DelayLineInterpolationTypes::Linear>;
    std::array<Delay,4> delays;
    std::array<std::array<float,4>,2> phaseMemory{};
    std::array<float,2> flangeFeedback{};
    ModelMorph<9> morph;juce::SmoothedValue<float> mix,speed,depth;
    double rate{48000},phase{};
    // 1.8 voices: rotary horn/drum, four-stage vibe, ring carrier.
    Delay horn;std::array<float,2> crossLow{};double hornHz{.7},drumHz{.6},hornPhase{},drumPhase{},carrierPhase{};
    std::array<std::array<float,4>,2> vibeMemory{};bool extendedActive{};
    void clearExtended() noexcept {horn.reset();crossLow={};hornPhase=drumPhase=carrierPhase=0;vibeMemory={};}
public:
    void prepare(const juce::dsp::ProcessSpec& s) {rate=s.sampleRate;for(auto& d:delays){d.setMaximumDelayInSamples(int(rate*.08));d.prepare(s);}horn.setMaximumDelayInSamples(int(rate*.01));horn.prepare(s);morph.prepare(rate);for(auto* v:{&mix,&speed,&depth})v->reset(rate,.03);mix.setCurrentAndTargetValue(0);speed.setCurrentAndTargetValue(.7f);depth.setCurrentAndTargetValue(.3f);reset();}
    void reset() {for(auto& d:delays)d.reset();phaseMemory={};flangeFeedback={};phase=0;mix.setCurrentAndTargetValue(0);clearExtended();hornHz=drumHz=.7;}
    void process(juce::AudioBuffer<float>& b,bool on,int model,float hz,float amount,float wet) {
        morph.select(model);mix.setTargetValue(on ? wet : 0);speed.setTargetValue(hz);depth.setTargetValue(amount);
        for(int n=0;n<b.getNumSamples();++n) {
            const auto weights=morph.next();const float m=mix.getNextValue(),d=depth.getNextValue(),hz=speed.getNextValue();phase+=hz/rate;phase-=std::floor(phase);
            const bool extended=weights[6]>0.f||weights[7]>0.f||weights[8]>0.f;
            if(extended!=extendedActive){if(!extended)clearExtended();extendedActive=extended;}
            if(extended) {
                // Rotors accelerate with inertia (horn about 0.6 s, drum 2.5 s).
                hornHz+=(hz-hornHz)*(1-std::exp(-1./(rate*.6)));drumHz+=(hz*.85-drumHz)*(1-std::exp(-1./(rate*2.5)));
                hornPhase+=hornHz/rate;hornPhase-=std::floor(hornPhase);drumPhase+=drumHz/rate;drumPhase-=std::floor(drumPhase);
                const double carrierHz=40.*std::pow(2.,6.*d)*(1.+.1*std::sin(juce::MathConstants<double>::twoPi*phase));
                carrierPhase+=carrierHz/rate;carrierPhase-=std::floor(carrierPhase);
            }
            for(int c=0;c<b.getNumChannels();++c) {
                const float x=b.getSample(c,n),lfo=std::sin(float(juce::MathConstants<double>::twoPi*(phase+c*.25))),opposed=std::sin(float(juce::MathConstants<double>::twoPi*(phase+c*.5)));
                std::array<float,6> voices{};
                voices[0]=delays[0].popSample(c,float(rate)*(.009f+.004f*d*lfo));delays[0].pushSample(c,x);
                const float a=delays[1].popSample(c,float(rate)*(.018f+.005f*d*opposed));delays[1].pushSample(c,x);
                const float v=delays[2].popSample(c,float(rate)*(.006f+.004f*d*lfo));delays[2].pushSample(c,x);
                voices[1]=.6f*a+.4f*v;
                const float frequency=juce::jlimit(50.f,float(rate*.35),350.f*std::pow(10.f,d*(.5f+.5f*lfo))),t=std::tan(float(juce::MathConstants<double>::pi)*frequency/float(rate)),coefficient=(1-t)/(1+t);
                float filtered=x;
                for(auto& z:phaseMemory[(size_t)c]){const float y=z-coefficient*filtered;z=filtered+coefficient*y;filtered=y;}
                voices[2]=filtered;
                voices[3]=delays[3].popSample(c,float(rate)*(.002f+.0018f*d*lfo));delays[3].pushSample(c,x+.55f*d*flangeFeedback[(size_t)c]);flangeFeedback[(size_t)c]=voices[3];
                voices[4]=v;voices[5]=x*(1-d*(.5f+.5f*opposed));
                float result=0;for(size_t i=0;i<6;++i)result+=voices[i]*weights[i];
                if(extended) {
                    const float twoPi=juce::MathConstants<float>::twoPi,quarter=c*.25f;
                    // Spin Cabinet: 800 Hz split; the horn carries Doppler and
                    // amplitude rotation, the drum amplitude only; outputs 90 deg apart.
                    crossLow[(size_t)c]+=spatial_detail::onePole(800,rate)*(x-crossLow[(size_t)c]);
                    const float low=crossLow[(size_t)c],high=x-low;
                    const float hornAngle=twoPi*float(hornPhase+quarter),drumAngle=twoPi*float(drumPhase+quarter);
                    const float doppler=horn.popSample(c,float(rate)*(.0015f+.0005f*d*std::sin(hornAngle)));horn.pushSample(c,high);
                    const float rotary=doppler*(1-.35f*d*(.5f+.5f*std::sin(hornAngle)))+low*(1-.25f*d*(.5f+.5f*std::sin(drumAngle)));
                    // Throb Vibe: four first-order all-passes at staggered
                    // frequencies swept by a skewed lamp response.
                    const float lamp=std::pow(.5f+.5f*std::sin(twoPi*float(phase)),2.2f);
                    float vibe=x;constexpr std::array<float,4> ratios{1.f,2.4f,5.6f,13.f};
                    for(size_t k=0;k<4;++k){const float f=juce::jlimit(20.f,float(rate*.4),80.f*ratios[k]*std::pow(2.f,3.f*d*lamp)),t=std::tan(juce::MathConstants<float>::pi*f/float(rate)),a=(1-t)/(1+t);
                        auto& z=vibeMemory[(size_t)c][k];const float y=z-a*vibe;z=vibe+a*y;vibe=y;}
                    // Ring Bell: multiply by a sine carrier (40 Hz-2.56 kHz from DEPTH, wobbled 10 % at RATE).
                    const float ring=x*std::sin(twoPi*float(carrierPhase));
                    result+=rotary*weights[6]+vibe*weights[7]+ring*weights[8];
                }
                // A vibrato or rotary reaches full-wet at the top of the shared MIX range.
                const float effective=juce::jlimit(0.f,1.f,m*(1.f+weights[4]*(1.f/.7f-1.f)+weights[6]*(1.f/.7f-1.f)));
                b.setSample(c,n,x+effective*(result-x));
            }
        }
    }
};
class EchoModule {
    using Delay=juce::dsp::DelayLine<float,juce::dsp::DelayLineInterpolationTypes::Linear>;
    std::array<Delay,3> delays;std::array<std::array<float,2>,3> dark{},dc{};
    ModelMorph<6> morph;juce::SmoothedValue<float> mix,time,feedback;
    double rate{48000},phase{};
    // 1.8 voices: a tap line, a modulated delay with a coloured loop, two linked lines.
    std::array<Delay,4> extra;std::array<std::array<float,2>,4> loopLow{},loopHigh{};double bloomPhase{},bloomSlow{};bool extendedActive{};
    void clearExtended() noexcept {for(auto& d:extra)d.reset();loopLow={};loopHigh={};bloomPhase=bloomSlow=0;}
public:
    void prepare(const juce::dsp::ProcessSpec& s) {rate=s.sampleRate;for(auto& d:delays){d.setMaximumDelayInSamples(int(rate*2.2));d.prepare(s);}for(auto& d:extra){d.setMaximumDelayInSamples(int(rate*2.2));d.prepare(s);}morph.prepare(rate);for(auto* v:{&mix,&time,&feedback})v->reset(rate,.035);mix.setCurrentAndTargetValue(0);time.setCurrentAndTargetValue(float(rate*.25));feedback.setCurrentAndTargetValue(.25f);reset();}
    void reset() {for(auto& d:delays)d.reset();dark={};dc={};phase=0;mix.setCurrentAndTargetValue(0);clearExtended();}
    void process(juce::AudioBuffer<float>& b,bool on,int model,float ms,float fb,float wet) {
        morph.select(model);mix.setTargetValue(on ? wet : 0);time.setTargetValue(float(rate)*juce::jlimit(20.f,2000.f,ms)*.001f);feedback.setTargetValue(juce::jlimit(0.f,.85f,fb));
        const std::array<float,3> cutoff{18000,4200,1800};std::array<float,3> lp{};for(size_t k=0;k<3;++k)lp[k]=float(1-std::exp(-juce::MathConstants<double>::twoPi*juce::jmin(cutoff[k],float(rate*.4))/rate));
        for(int n=0;n<b.getNumSamples();++n) {
            const auto weights=morph.next();const float m=mix.getNextValue(),samples=time.getNextValue(),gain=feedback.getNextValue();phase+=.53/rate;phase-=std::floor(phase);
            const bool extended=weights[3]>0.f||weights[4]>0.f||weights[5]>0.f;
            if(extended!=extendedActive){if(!extended)clearExtended();extendedActive=extended;}
            if(extended){bloomPhase+=.6/rate;bloomPhase-=std::floor(bloomPhase);bloomSlow+=.13/rate;bloomSlow-=std::floor(bloomSlow);}
            for(int c=0;c<b.getNumChannels();++c){const float x=b.getSample(c,n);float sum=0;
                for(size_t k=0;k<3;++k) {const float wow=k==1 ? float(rate)*.0008f*std::sin(float(juce::MathConstants<double>::twoPi*(phase+c*.03))) : k==2 ? float(rate)*.00012f*std::sin(float(juce::MathConstants<double>::twoPi*phase*5)) : 0;
                    const float delayed=delays[k].popSample(c,samples+wow);dark[k][(size_t)c]+=lp[k]*(delayed-dark[k][(size_t)c]);
                    const float echo=k==0 ? delayed : dark[k][(size_t)c];
                    // Passive loss in the recirculation path keeps tails bounded.
                    delays[k].pushSample(c,(on ? x : 0)+echo*gain);sum+=weights[k]*echo;
                }
                if(extended) {
                    const float input=on ? x : 0;
                    // Tap Cloud: six taps across TIME; FEEDBACK sets the taper
                    // (minimum: taps fade up, centre: equal, maximum: fade down).
                    float taps=0,weightSum=0;const float taper=gain/.85f*2.f-1.f;
                    for(int k=1;k<=6;++k){const float up=float(k)/6.f,down=1.f-float(k-1)/6.f,g=.5f*(1-taper)*up+.5f*(1+taper)*down;
                        taps+=g*extra[0].popSample(c,samples*float(k)/6.f,k==6);weightSum+=g;}   // the last tap advances the read pointer
                    extra[0].pushSample(c,input);taps*=1.5f/juce::jmax(1.e-3f,weightSum);
                    // Bloom Delay: the time drifts (0.6 Hz and 0.13 Hz) and the
                    // loop is band-limited (120 Hz-3.5 kHz) with soft saturation.
                    const float drift=float(rate)*.0012f*(std::sin(juce::MathConstants<float>::twoPi*float(bloomPhase+c*.2))+.5f*std::sin(juce::MathConstants<float>::twoPi*float(bloomSlow)));
                    const float bloomed=extra[1].popSample(c,samples+drift);
                    auto& low=loopLow[1][(size_t)c];auto& high=loopHigh[1][(size_t)c];low+=spatial_detail::onePole(3500,rate)*(bloomed-low);high+=spatial_detail::onePole(120,rate)*(low-high);
                    extra[1].pushSample(c,input+std::tanh((low-high)*gain*1.1f));
                    // Dual Digital: lines at TIME and 3/4 TIME, each recirculating, 12 kHz top.
                    float dual=0;for(int k=2;k<4;++k){const float delayed=extra[(size_t)k].popSample(c,k==2?samples:samples*.75f);auto& top=loopLow[(size_t)k][(size_t)c];top+=spatial_detail::onePole(12000,rate)*(delayed-top);
                        extra[(size_t)k].pushSample(c,input+top*gain);dual+=.6f*top;}
                    sum+=weights[3]*taps+weights[4]*bloomed+weights[5]*dual;
                }
                b.setSample(c,n,x*(1-m)+sum*m);
            }
        }
    }
};
class SpaceModule {
    using Delay=juce::dsp::DelayLine<float,juce::dsp::DelayLineInterpolationTypes::Linear>;
    std::array<juce::dsp::Reverb,2> rooms;std::array<juce::AudioBuffer<float>,2> wet;
    Delay hallPre,spring;std::array<std::array<float,6>,2> dispersion{};std::array<float,2> springLow{};
    ModelMorph<6> morph;juce::SmoothedValue<float> mix;double rate{48000},phase{};
    // 1.8 voices: three feedback delay networks per channel and a gate.
    std::array<std::array<spatial_detail::FDN,3>,2> networks;Delay predelay;std::array<float,2> gateLevel{};
    int gateTimer{1<<30};bool gateArmed{true},extendedActive{};
    void clearExtended() noexcept {for(auto& channel:networks)for(auto& f:channel)f.reset();predelay.reset();gateLevel={};gateTimer=1<<30;gateArmed=true;}
public:
    // juce::Reverb snaps its parameter smoothers to the last target on prepare.
    // Restore the construction defaults first so every prepare starts the same
    // ramp; otherwise a re-prepared instance renders differently from a new one.
    void prepare(const juce::dsp::ProcessSpec& s) {rate=s.sampleRate;for(size_t i=0;i<2;++i){rooms[i].setParameters({});rooms[i].prepare(s);wet[i].setSize((int)s.numChannels,(int)s.maximumBlockSize);}hallPre.setMaximumDelayInSamples(int(rate*.1));spring.setMaximumDelayInSamples(int(rate*.2));hallPre.prepare(s);spring.prepare(s);
        for(auto& channel:networks){channel[0].prepare(rate,.06);channel[1].prepare(rate,.12);channel[2].prepare(rate,.04);}predelay.setMaximumDelayInSamples(int(rate*.05));predelay.prepare(s);
        morph.prepare(rate);mix.reset(rate,.03);mix.setCurrentAndTargetValue(0);reset();}
    void reset() {for(auto& r:rooms)r.reset();hallPre.reset();spring.reset();dispersion={};springLow={};phase=0;mix.setCurrentAndTargetValue(0);clearExtended();}
    void process(juce::AudioBuffer<float>& b,bool on,int model,float size,float damping,float amount) {
        morph.select(model);mix.setTargetValue(on ? amount : 0);
        for(size_t i=0;i<2;++i){juce::Reverb::Parameters p;p.roomSize=i==0 ? .25f+.45f*size : .6f+.36f*size;p.damping=damping;p.wetLevel=1;p.dryLevel=0;p.width=i==0 ? .7f : 1;p.freezeMode=0;rooms[i].setParameters(p);wet[i].makeCopyOf(b,true);if(!on)wet[i].clear();}
        for(int n=0;n<b.getNumSamples();++n){phase+=.17/rate;phase-=std::floor(phase);for(int c=0;c<b.getNumChannels();++c){const float x=wet[1].getSample(c,n),delay=float(rate)*(.022f+.014f*size+.001f*std::sin(float(juce::MathConstants<double>::twoPi*(phase+c*.3))));wet[1].setSample(c,n,hallPre.popSample(c,delay));hallPre.pushSample(c,x);}}
        for(size_t i=0;i<2;++i){juce::dsp::AudioBlock<float> block(wet[i]);juce::dsp::ProcessContextReplacing<float> context(block);rooms[i].process(context);}
        const float lp=float(1-std::exp(-juce::MathConstants<double>::twoPi*(1800+5000*(1-damping))/rate));
        // Hall Prime 1.2-6 s, Void Space 6-60 s, Gated Room a 1 s network
        // gated 100-600 ms after each attack (SIZE); DAMPING closes the loops.
        const double hallT60=1.2+4.8*size,voidT60=6.+54.*size,roomT60=1.;
        const float loopPole=spatial_detail::onePole(2000.+10000.*(1.-damping),rate);
        const int hold=int(rate*(.1+.5*size)),close=int(rate*.015);
        for(auto& channel:networks){channel[0].setScale(.8+.6*size);channel[1].setScale(1.6+1.6*size);channel[2].setScale(.5);}
        const float gateThreshold=juce::Decibels::decibelsToGain(-40.f),gatePole=float(1-std::exp(-1./(rate*.03)));
        for(int n=0;n<b.getNumSamples();++n){const auto weights=morph.next();const float m=mix.getNextValue();
            const bool extended=weights[3]>0.f||weights[4]>0.f||weights[5]>0.f;
            if(extended!=extendedActive){if(!extended)clearExtended();extendedActive=extended;}
            float gate=1.f;
            if(extended) {
                float level=0;for(int c=0;c<b.getNumChannels();++c){const float x=std::abs(b.getSample(c,n));auto& g=gateLevel[(size_t)c];g=x>g?x:g+gatePole*(x-g);level=juce::jmax(level,g);}
                if(level<gateThreshold*.5f)gateArmed=true;
                if(gateArmed && level>gateThreshold){gateArmed=false;gateTimer=0;}
                if(gateTimer<(1<<30))++gateTimer;
                gate=gateTimer<hold?1.f:juce::jmax(0.f,1.f-float(gateTimer-hold)/float(close));
            }
            for(int c=0;c<b.getNumChannels();++c){const float x=b.getSample(c,n);float s=spring.popSample(c,float(rate)*(.031f+.039f*size+c*.0061f));
            for(size_t k=0;k<6;++k){auto& z=dispersion[(size_t)c][k];const float a=.32f+.07f*float(k),y=z-a*s;z=s+a*y;s=y;}
            springLow[(size_t)c]+=lp*(s-springLow[(size_t)c]);s=springLow[(size_t)c];spring.pushSample(c,(on ? x : 0)+s*(.35f+.5f*size));
            float sum=weights[0]*wet[0].getSample(c,n)+weights[1]*wet[1].getSample(c,n)+weights[2]*s;
            if(extended) {
                const float input=on ? x : 0;auto& fdn=networks[(size_t)c];
                const float delayed=predelay.popSample(c,float(rate)*.02f);predelay.pushSample(c,input);
                const float hall=weights[3]>0.f?fdn[0].tick(delayed,hallT60,loopPole,8.f,.3):0.f;
                const float space=weights[4]>0.f?fdn[1].tick(delayed,voidT60,loopPole,25.f,.17):0.f;
                const float room=weights[5]>0.f?fdn[2].tick(input,roomT60,loopPole,0.f,0.)*gate:0.f;
                sum+=weights[3]*hall+weights[4]*space+weights[5]*room;
            }
            b.setSample(c,n,x*(1-m)+sum*m);
        }}
    }
};
}
