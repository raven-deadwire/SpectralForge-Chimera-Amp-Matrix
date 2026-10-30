#pragma once
// Included by AutoSetTests.cpp: actual Capture, Controller, APVTS and Final EQ.
// The independent libebur128 comparison lives in a separate executable.
namespace levelRegression {
using Frames=std::vector<OutputFrame>;
void set(ChimeraProcessor& p,const juce::String& id,float value) {
    auto* parameter=p.parameters().getParameter(id);parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
}
constexpr unsigned upstream=Controller::lockInput|Controller::lockGate|Controller::lockTone|Controller::lockCompressor
    |Controller::lockMultiband|Controller::lockDeEsser|Controller::lockLimiter|Controller::lockDynamicEQ;
Controller::Options options(float strength=1,unsigned extra=0,bool bass=false) {
    return {bass?2:1,false,Style::rock,false,false,false,strength,upstream|extra,false,true};
}
Frames signal(double rate,int kind,int channels=2) {
    Frames out(size_t(rate*6));std::mt19937 rng(7107);
    if(kind==0 || kind==4) {
        auto x=targetNoise(false,rate,6,rng);shelve(x,6000,8,rate);scale(x,.25f);
        for(size_t n=0;n<out.size();++n)out[n]={x[n],channels==2?-x[n]:0.f};
    } else for(size_t n=0;n<out.size();++n) {
        const double t=double(n)/rate,tau=2*juce::MathConstants<double>::pi*t;
        float x=0;
        if(kind==1)x=float(.08*std::sin(997*tau));
        if(kind==2)x=float(.8*std::sin(220*tau)*std::exp(-std::fmod(t,.5)*90));
        if(kind==3) {
            x=float((.75*std::sin(27.5*tau)+.09*std::sin(55*tau))*std::exp(-std::fmod(t,.5)*18));
            if(n%size_t(rate*.5)==0)x=.94f;
        }
        out[n]={x,channels==2?-x:0.f};
    }
    if(kind==4)for(size_t n=size_t(rate*.25);n<out.size();n+=size_t(rate*.5))out[n]={.94f,channels==2?-.94f:0.f};
    return out;
}
Frames finalRender(ChimeraProcessor& p,const Frames& input,double rate,int channels=2) {
    spectralforge::GraphicalEQ eq;eq.bind(p.parameters(),1);eq.prepare(rate);
    Frames out(input.size());const float gain=std::pow(10.f,get(p,"output")/20.f);
    for(size_t start=0;start<input.size();start+=256) {
        const int count=int(std::min(size_t(256),input.size()-start));
        juce::AudioBuffer<float> block(channels,count);
        for(int c=0;c<channels;++c)for(int n=0;n<count;++n)block.setSample(c,n,input[start+size_t(n)][size_t(c)]);
        eq.process(block);
        for(int n=0;n<count;++n)for(int c=0;c<channels;++c)out[start+size_t(n)][size_t(c)]=block.getSample(c,n)*gain;
    }
    return out;
}
void feed(ChimeraProcessor& p,Controller& controller,const Frames& frames,double rate,int channels=2,
          const std::function<void()>& midRun={}) {
    controller.start();double now=1000;bool edited=false;
    for(size_t start=0;controller.busy() && start<frames.size();start+=251) {
        const int count=int(std::min(size_t(251),frames.size()-start));
        std::array<std::array<float,251>,2> data{};
        for(int n=0;n<count;++n)for(int c=0;c<channels;++c)data[size_t(c)][size_t(n)]=frames[start+size_t(n)][size_t(c)];
        const float* pointers[]{data[0].data(),data[1].data()};
        p.autoSetCapture().push(Tap::master,pointers,channels,count);
        if(midRun && !edited && start>frames.size()/2){midRun();edited=true;}
        now+=1000.*count/rate;controller.tick(now);
    }
    check(controller.current().phase==Controller::Phase::done,"Level regression did not complete");
}
void capture() {
    auto queue=std::make_unique<Capture>();queue->prepare(48000);queue->listen(Tap::master);
    const std::array<float,5> left{.1f,-.3f,.9f,.4f,-.2f},right{-.1f,.3f,-.9f,-.4f,.2f};
    const float* pointers[]{left.data(),right.data()};
    counting=true;allocations=0;queue->push(Tap::master,pointers,2,5);counting=false;
    check(allocations==0,"Stereo master capture allocated on audio thread");
    Frames got;queue->drainOutput(got);check(got.size()==5,"Master capture lost frames");
    for(size_t n=0;n<5;++n)check(got[n][0]==left[n] && got[n][1]==right[n],"Master capture averaged the channels");
    got.clear();queue->push(Tap::master,pointers,1,5);queue->drainOutput(got);
    for(size_t n=0;n<5;++n)check(got[n][0]==left[n] && got[n][1]==0,"Mono capture duplicated or reused right channel");
    // Ring wraps and drains at irregular block sizes, without channel skew.
    for(int k=0;k<30000;++k){queue->push(Tap::master,pointers,2,5);got.clear();queue->drainOutput(got);
        check(got.size()==5 && got[2][0]==.9f && got[2][1]==-.9f,"Master ring wrap skewed channels");}
}
void processorBuses() {
    for(double rate:{44100.,48000.,96000.})for(const auto io:{std::pair{1,1},std::pair{1,2},std::pair{2,2}}) {
        auto p=std::make_unique<ChimeraProcessor>();auto layout=p->getBusesLayout();
        layout.inputBuses.set(0,io.first==1?juce::AudioChannelSet::mono():juce::AudioChannelSet::stereo());
        layout.outputBuses.set(0,io.second==1?juce::AudioChannelSet::mono():juce::AudioChannelSet::stereo());
        if(io.first!=io.second) {
            check(!p->setBusesLayout(layout),"Unexpected change to unsupported mono-to-stereo bus contract");continue;
        }
        check(p->setBusesLayout(layout),"Processor rejected supported mono/stereo layout");
        set(*p,"liveLimiterOn",0);p->setRateAndBufferSizeDetails(rate,256);p->prepareToPlay(rate,256);
        p->autoSetCapture().listen(Tap::master);double energy=0;
        for(int start=0;start<int(rate);start+=256) {
            const int size=std::min(256,int(rate)-start);juce::AudioBuffer<float> buffer(io.second,size);buffer.clear();
            for(int c=0;c<io.first;++c)for(int n=0;n<size;++n)
                buffer.setSample(c,n,float(.05*(c==0?1:.5)*std::sin(2*juce::MathConstants<double>::pi*997*(start+n)/rate)));
            juce::MidiBuffer midi;p->processBlock(buffer,midi);Frames actual;p->autoSetCapture().drainOutput(actual);
            check(actual.size()==size_t(size),"Real processor master tap lost frames");
            for(const auto frame:actual)energy+=double(frame[0])*frame[0]+double(frame[1])*frame[1];
            for(int n=0;n<size;++n)for(int c=0;c<2;++c)
                check(actual[size_t(n)][size_t(c)]==(c<io.second?buffer.getSample(c,n):0.f),"Master capture differs from real processor output with limiter off");
        }
        check(energy>1e-6,"Processor bus validation was silent");p->releaseResources();
    }
    std::cout<<"PASS phase 7 processor buses: 6 mono/stereo captures, 3 mono-to-stereo rejection contracts\n";
}
void correctionBoundaries() {
    for(double rate:{44100.,48000.,96000.})for(float strength:{.25f,.5f,.75f,1.f}) {
        auto audio=signal(rate,1);for(auto& f:audio)for(auto& x:f)x*=.03f;
        // A peak requiring less than the 1 dB deadband must still be reduced.
        audio[size_t(rate)]={.94f,-.94f};
        OutputResult r;r.playingDetected=true;planOutputCorrection(r,audio,rate,{},strength,0);
        check(r.peakLimited && r.trimChangeDb<0 && r.predictedPeakDb<=-1.000f,"Deadband/strength defeated peak safety");
        // Existing hot output, trim lower limit and a lock must be reported honestly.
        planOutputCorrection(r,audio,rate,{},strength,-36);
        check(r.trimChangeDb==0 && r.peakUnresolved,"Trim floor falsely reported safe peaks");
        planOutputCorrection(r,audio,rate,{},strength,0,true);
        check(r.trimChangeDb==0 && r.peakUnresolved,"Output lock falsely reported safe peaks");
        auto extreme=audio;extreme[size_t(rate)]={5.f,-5.f};
        planOutputCorrection(r,extreme,rate,{},strength,0);
        check(r.trimChangeDb==-12 && r.peakUnresolved,"12 dB cap was exceeded or hidden");
        // One short attack has a valid meter reading but insufficient evidence
        // for automatic loudness boost. Its peak protection still applies.
        Frames attack(size_t(rate*6));for(size_t n=size_t(rate);n<size_t(rate*1.02);++n)attack[n]={.94f,-.94f};
        planOutputCorrection(r,attack,rate,{},strength,0);
        check(!r.loudnessMeasured && r.trimChangeDb<0 && !r.peakUnresolved,"Short attack was boosted or left unsafe");
    }
}
void renderedMatrix() {
    int count=0;
    for(double rate:{44100.,48000.,96000.})for(int channels:{1,2})for(int kind:{0,1,2,3,4}) {
        auto p=std::make_unique<ChimeraProcessor>();p->setRateAndBufferSizeDetails(rate,256);p->prepareToPlay(rate,256);
        set(*p,"output",0);
        Controller c(*p);c.setOptions(options(1,0,kind==3));
        const auto rawSignal=signal(rate,kind,channels);const auto before=values(*p);
        feed(*p,c,finalRender(*p,rawSignal,rate,channels),rate,channels);
        const auto result=finalRender(*p,rawSignal,rate,channels);const auto& r=c.lastLevelAnalysis();
        const auto loud=integratedLoudness(result,rate);
        check(loud && r.loudnessMeasured,"Steady/picked rendered audio was not measured");
        check(near(*loud,r.predictedLufs,.03f),"Applied Final EQ/trim loudness differs from prediction");
        check(near(outputSamplePeak(result),r.predictedPeakDb,.03f),"Applied Final EQ/trim peak differs from prediction");
        check(outputSamplePeak(result)<=-1.f+.01f || r.peakUnresolved,"Actual Final EQ output exceeded unreported peak ceiling");
        if(kind==1)check(near(*loud,-18.f,.26f),"Unconstrained sustained tone missed -18 LUFS");
        if(kind==2 || kind==3 || kind==4)check(r.peakLimited && *loud<-18,"High crest signal did not prioritize peaks");
        if(rate==48000 && channels==2)std::cout<<"MEASURE phase7 signal="<<kind<<" before="<<r.loudnessLufs
            <<" after="<<*loud<<" LUFS peak="<<outputSamplePeak(result)<<" dBFS trim="<<r.trimChangeDb
            <<" limited="<<r.peakLimited<<" unresolved="<<r.peakUnresolved<<'\n';
        c.undo();check(values(*p)==before,"Rendered case failed exact Undo");p->releaseResources();++count;
    }
    std::cout<<"PASS phase 7 rendered matrix: "<<count<<" cases, real Final EQ, mono/antiphase, tone/picking/peaks/bass\n";
}
void preservation() {
    int count=0;
    for(float strength:{.25f,.5f,.75f,1.f})for(int mode=0;mode<7;++mode) {
        auto p=std::make_unique<ChimeraProcessor>();p->setRateAndBufferSizeDetails(48000,256);p->prepareToPlay(48000,256);set(*p,"output",-2.37f);
        // Seed a non-default user curve. Mode 2 bypasses it; mode 3 fills all
        // bands; mode 4 offers just one band while reserving another dynamically.
        set(*p,spectralforge::eqID(1,0,"enabled"),1);set(*p,spectralforge::eqID(1,0,"frequency"),713);
        set(*p,spectralforge::eqID(1,0,"gain"),1.21f);set(*p,spectralforge::eqID(1,0,"q"),.913f);
        set(*p,spectralforge::eqID(1,-1,"bypass"),mode==2?1:0);
        if(mode==3 || mode==4)for(int b=1;b<12;++b)set(*p,spectralforge::eqID(1,b,"enabled"),mode==3 || b<10?1:0);
        if(mode==4)set(*p,spectralforge::dynamicSlotID(0,"target"),23); // reserved disabled band 11
        if(mode==6) {
            set(*p,spectralforge::eqID(1,5,"enabled"),1);
            set(*p,spectralforge::dynamicSlotID(0,"target"),18);
            set(*p,spectralforge::dynamicSlotID(0,"threshold"),-45);
            set(*p,spectralforge::dynamicSlotID(0,"range"),6);
            set(*p,spectralforge::dynamicSlotID(0,"ratio"),4);
        }
        const auto before=values(*p);Controller c(*p);
        c.setOptions(options(strength,mode==1?Controller::lockFinalEQ:mode==5?Controller::lockOutput:0));
        auto input=signal(48000,0);
        if(mode==4)for(size_t channel=0;channel<2;++channel) {
            std::vector<float> x;for(const auto f:input)x.push_back(f[channel]);bell(x,315,8,1,48000);
            for(size_t n=0;n<input.size();++n)input[n][channel]=x[n];
        }
        feed(*p,c,finalRender(*p,input,48000),48000);
        const auto& r=c.lastLevelAnalysis();const auto out=finalRender(*p,input,48000);
        check(near(*integratedLoudness(out,48000),r.predictedLufs,.03f),"Lock/free-band/strength compensation used unapplied EQ");
        for(int b=0;b<12;++b) {
            const auto* enabled=p->parameters().getParameter(spectralforge::eqID(1,b,"enabled"));
            if(before[size_t(enabled->getParameterIndex())]>.5f)for(const char* field:{"enabled","type","frequency","gain","q"}) {
                const auto* parameter=p->parameters().getParameter(spectralforge::eqID(1,b,field));
                check(parameter->getValue()==before[size_t(parameter->getParameterIndex())],"An existing Final EQ field changed during apply");
            }
        }
        check(near(get(*p,spectralforge::eqID(1,0,"frequency")),713,.01f) && near(get(*p,spectralforge::eqID(1,0,"gain")),1.21f,.001f),"User Final EQ changed");
        if(mode==1 || mode==2 || mode==3)check(r.eqLoudnessDeltaDb==0 && r.eqPeakDeltaDb==0,"Skipped EQ still affected trim");
        if(mode==5)check(get(*p,"output")==-2.37f || near(get(*p,"output"),-2.37f,.00001f),"Output lock changed trim");
        if(mode==4)check(r.activeMoves()>=2 && get(*p,spectralforge::eqID(1,10,"enabled"))<.5f,"Partial placement/reserved Dynamic EQ fixture failed");
        if(mode==6)for(int b=1;b<5;++b)check(get(*p,spectralforge::eqID(1,b,"enabled"))<.5f,"New static EQ preceded a time-varying Final EQ band");
        c.undo();check(values(*p)==before,"Exact Undo did not restore seeded curve/trim");p->releaseResources();++count;
    }
    std::cout<<"PASS phase 7 preservation: "<<count<<" non-default curve, bypass, locks, slots and strength cases\n";
}
void repeatedAndEdits() {
    auto p=std::make_unique<ChimeraProcessor>();p->setRateAndBufferSizeDetails(48000,256);p->prepareToPlay(48000,256);
    Controller c(*p);c.setOptions(options());const auto input=signal(48000,0);
    for(int run=0;run<4;++run) {
        const auto before=values(*p);feed(*p,c,finalRender(*p,input,48000),48000);const auto after=values(*p);
        const auto out=finalRender(*p,input,48000);check(outputSamplePeak(out)<=-1+.01f,"Repeated Auto Set exceeded ceiling");
        c.undo();check(values(*p)==before,"Repeat Undo did not restore immediate pre-run state");
        for(int i=0;i<p->getParameters().size();++i)p->getParameters()[i]->setValueNotifyingHost(after[size_t(i)]);
    }
    // A-B-A edits while listening invalidate the measurement even if the final
    // parameter value equals the snapshot.
    const auto before=values(*p);
    feed(*p,c,finalRender(*p,input,48000),48000,2,[&]{const float old=get(*p,"output");set(*p,"output",old+1);set(*p,"output",old);});
    check(values(*p)==before && c.current().steps.back().result.contains("edited while listening"),"A-B-A edit did not invalidate capture");
    c.undo();check(values(*p)==before,"Undo reclaimed a manual edit");
    // Fresh run, then a newer output edit: Undo must leave that edit intact.
    feed(*p,c,finalRender(*p,input,48000),48000);set(*p,"output",-7.23f);c.undo();
    check(near(get(*p,"output"),-7.23f,.00001f),"Undo overwrote newer output value");
    p->releaseResources();
    std::cout<<"PASS phase 7 repeat and edit history: per-run Undo, A-B-A capture invalidation, newer output retained\n";
}
void finalNodeEdits() {
    auto p=std::make_unique<ChimeraProcessor>();p->setRateAndBufferSizeDetails(48000,256);p->prepareToPlay(48000,256);
    Controller c(*p);c.setOptions(options());const auto input=signal(48000,0);
    feed(*p,c,finalRender(*p,input,48000),48000);
    int band=-1;for(int b=0;b<12;++b)if(get(*p,spectralforge::eqID(1,b,"enabled"))>.5f){band=b;break;}
    check(band>=0,"Final node edit fixture placed no band");
    set(*p,spectralforge::eqID(1,band,"gain"),-1.23f);
    const float frequency=get(*p,spectralforge::eqID(1,band,"frequency"));c.undo();
    check(get(*p,spectralforge::eqID(1,band,"enabled"))>.5f && get(*p,spectralforge::eqID(1,-1,"bypass"))<.5f
          && near(get(*p,spectralforge::eqID(1,band,"gain")),-1.23f,.00001f)
          && get(*p,spectralforge::eqID(1,band,"frequency"))==frequency,"Undo broke a manually edited Final EQ node");
    p->releaseResources();
    // A synchronous host callback during placement must preserve its edit,
    // roll back incomplete automated bands, and leave output trim alone.
    auto q=std::make_unique<ChimeraProcessor>();q->setRateAndBufferSizeDetails(48000,256);q->prepareToPlay(48000,256);
    struct HostEdit final:juce::AudioProcessorParameter::Listener {
        ChimeraProcessor& processor;bool fired=false;
        explicit HostEdit(ChimeraProcessor& p):processor(p){}
        void parameterValueChanged(int,float) override {if(!fired){fired=true;set(processor,"output",-9.13f);}}
        void parameterGestureChanged(int,bool) override {}
    } edit(*q);
    auto* parameter=q->parameters().getParameter(spectralforge::eqID(1,0,"enabled"));parameter->addListener(&edit);
    Controller controller(*q);controller.setOptions(options());feed(*q,controller,finalRender(*q,input,48000),48000);
    parameter->removeListener(&edit);
    check(edit.fired && near(get(*q,"output"),-9.13f,.00001f),"Placement overwrote synchronous host edit");
    for(int b=0;b<12;++b)check(get(*q,spectralforge::eqID(1,b,"enabled"))<.5f,"Partial Final EQ placement survived rollback");
    controller.undo();check(near(get(*q,"output"),-9.13f,.00001f),"Rollback/Undo reclaimed host output edit");q->releaseResources();
}
void run() {capture();processorBuses();correctionBoundaries();renderedMatrix();preservation();repeatedAndEdits();finalNodeEdits();}
}
