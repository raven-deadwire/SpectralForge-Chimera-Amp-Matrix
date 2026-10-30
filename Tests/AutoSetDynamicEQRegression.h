#pragma once

// Synthetic controls, not recordings or listening evidence.
std::vector<float> dynamicSustain(double rate, double f0, bool distorted, bool changingLevel) {
    std::vector<float> x(size_t(rate*6));
    for(size_t n=0;n<x.size();++n) {
        const double t=double(n)/rate;
        double y=0;
        for(int h=1;h<=12;++h)y+=std::sin(2*juce::MathConstants<double>::pi*f0*h*t)/h;
        if(distorted)y=std::tanh(4*y);
        const double envelope=changingLevel?(std::fmod(t,1.)<.2?1.:.25):std::exp(-t*.35);
        x[n]=float(.2*y*envelope*std::min(1.,t/.005));
    }
    return x;
}

void dynamicRiskProbe() {
    for(double rate:{44100.,48000.,96000.}) {
        for(double f0:{27.5,30.8677,55.,82.4069})for(bool distorted:{false,true})for(bool changing:{false,true}) {
            const auto r=analyseDynamicEQ(dynamicSustain(rate,f0,distorted,changing),rate,f0<40,Style::metal);
            std::cout<<"DYNAMIC_PROBE "<<rate<<" f0="<<f0<<" distorted="<<distorted<<" changing="<<changing;
            for(const auto& n:r.nodes)std::cout<<" node="<<n.frequency<<":"<<n.rangeDb;
            std::cout<<'\n';
        }
        for(float f0:{27.5f,30.8677f,55.f,82.41f}) {
            std::mt19937 rng{43};
            const auto r=analyseDynamicEQ(playedNotes(rate,6,rng,0,0,f0),rate,f0<40,Style::metal);
            std::cout<<"DYNAMIC_SCALE "<<rate<<" f0="<<f0;
            for(const auto& n:r.nodes)std::cout<<" node="<<n.frequency<<":"<<n.rangeDb;
            std::cout<<'\n';
        }
    }
}

void dynamicOwnershipRegression() {
    for(int mode=0;mode<4;++mode) {
        auto p=std::make_unique<ChimeraProcessor>();p->setRateAndBufferSizeDetails(48000,256);p->prepareToPlay(48000,256);
        Controller c(*p);Controller::Options options{1,false,Style::rock,false,false,false,1.f,
            Controller::lockInput|Controller::lockGate|Controller::lockCompressor,true,false};
        c.setOptions(options);c.start();std::mt19937 rng{43};double now=0;
        feedCapture(*p,c,playedNotes(48000,6.1,rng,250,12),now);
        check(c.current().phase==Controller::Phase::done && get(*p,spectralforge::dynamicSlotID(0,"target"))>0,
              "Dynamic ownership control placed no node");
        const int target=juce::roundToInt(get(*p,spectralforge::dynamicSlotID(0,"target")));
        if(mode>=2) {
            const auto id=mode==2?spectralforge::eqID(0,target-1,"frequency"):spectralforge::dynamicSlotID(0,"threshold");
            auto* q=p->parameters().getParameter(id);const auto previous=q->getValue();
            q->beginChangeGesture();q->setValueNotifyingHost(q->convertTo0to1(mode==2?300.f:-35.f));
            q->setValueNotifyingHost(previous);q->endChangeGesture();
        }
        const auto applied=values(*p);
        if(mode<2)options.locks|=mode==0?Controller::lockTone:Controller::lockDynamicEQ;
        c.setOptions(options);c.undo();
        check(values(*p)==applied,"Lock or newer edit only preserved half of an automatic node during UNDO");
        p->releaseResources();
    }
    std::cout<<"PASS dynamic ownership: newly selected Tone/Dynamic locks preserve complete node and EQ bypass on UNDO\n";
}

void dynamicSignalRegression() {
    int protectedCases=0;
    for(double rate:{44100.,48000.,96000.}) {
        for(double f0:{27.5,30.8677,36.7081,55.,82.4069})for(bool distorted:{false,true})for(bool changing:{false,true}) {
            const auto signal=dynamicSustain(rate,f0,distorted,changing);
            const auto result=analyseDynamicEQ(signal,rate,f0<40,Style::metal);
            check(result.playingDetected && result.nodes.empty(),"Ordinary low fundamental/harmonics, sustain or shared dynamics received a Dynamic EQ node");
            ++protectedCases;
        }
        for(float f0:{27.5f,30.8677f,55.f,82.41f}) {
            std::mt19937 rng{43};
            check(analyseDynamicEQ(playedNotes(rate,6,rng,0,0,f0),rate,f0<40).nodes.empty(),"Even Drop A/B0 scale got a node");
        }
    }
    std::cout<<"PASS dynamic signal protection: "<<protectedCases<<" sustain/common-envelope cases and 12 extended-range scales\n";
}

void dynamicRenderRegression() {
    for(double rate:{44100.,48000.,96000.})for(float strength:{.25f,.5f,.75f,1.f}) {
        auto p=std::make_unique<ChimeraProcessor>();p->setRateAndBufferSizeDetails(rate,256);p->prepareToPlay(rate,256);
        Controller c(*p);c.setOptions({1,false,Style::rock,false,false,false,strength,Controller::lockInput|Controller::lockGate|Controller::lockCompressor,true,false});
        std::mt19937 rng{43};const auto signal=playedNotes(rate,6.1,rng,250,12);double now=0;c.start();feedCapture(*p,c,signal,now);
        check(c.current().phase==Controller::Phase::done && !c.lastDynamicAnalysis().nodes.empty(),"Positive ring control placed no node");
        auto& eq=p->graphicalEQ(0);eq.reset();float peak=0;double inputEnergy=0,outputEnergy=0;
        juce::AudioBuffer<float> block(1,256);
        for(size_t offset=0;offset<signal.size();offset+=256) {
            const int count=int(std::min(size_t(256),signal.size()-offset));block.setSize(1,count,false,false,true);
            block.copyFrom(0,0,signal.data()+offset,count);eq.process(block);
            for(int n=0;n<count;++n){inputEnergy+=double(signal[offset+size_t(n)])*signal[offset+size_t(n)];outputEnergy+=double(block.getSample(0,n))*block.getSample(0,n);}
            for(int b=0;b<12;++b)peak=std::max(peak,eq.dynamicReduction(b));
        }
        check(peak>.05f && peak<=9.f*strength+.02f && outputEnergy<inputEnergy,"Assigned node failed to cut in the real GraphicalEQ or exceeded strength cap");
        block.setSize(1,256,false,false,true);
        for(int k=0;k<int(rate*2/256);++k){block.clear();eq.process(block);}
        for(int b=0;b<12;++b)check(eq.dynamicReduction(b)<.01f,"Dynamic EQ did not release after the ring");
        // Even with a 250 Hz node present, a low A0 at the same healthy input
        // level must not lose weight through the skirt of its bell.
        eq.reset();inputEnergy=outputEnergy=0;
        for(int offset=0;offset<int(rate*2);offset+=256) {
            const int count=std::min(256,int(rate*2)-offset);block.setSize(1,count,false,false,true);
            for(int n=0;n<count;++n)block.setSample(0,n,float(.2*std::sin(2*juce::MathConstants<double>::pi*27.5*(offset+n)/rate)));
            if(offset>int(rate*.25))for(int n=0;n<count;++n)inputEnergy+=double(block.getSample(0,n))*block.getSample(0,n);
            eq.process(block);
            if(offset>int(rate*.25))for(int n=0;n<count;++n)outputEnergy+=double(block.getSample(0,n))*block.getSample(0,n);
        }
        const double delta=10*std::log10(outputEnergy/inputEnergy);
        check(delta>=-.5,"Drop A fundamental lost >0.5 dB through the assigned node");
        std::cout<<"DYNAMIC_RENDER rate="<<rate<<" strength="<<strength<<" peakGR="<<peak<<" A0delta="<<delta<<'\n';
        c.undo();p->releaseResources();
    }
}

void dynamicEditPlacementRegression() {
    for(int mode=0;mode<3;++mode) {
        auto p=std::make_unique<ChimeraProcessor>();p->setRateAndBufferSizeDetails(48000,256);p->prepareToPlay(48000,256);
        Controller c(*p);c.setOptions({1,false,Style::rock,false,false,false,1.f,Controller::lockInput|Controller::lockGate|Controller::lockCompressor,true,false});
        c.start();const auto before=values(*p);
        const auto id=mode==0?spectralforge::eqID(0,0,"frequency"):mode==1?spectralforge::dynamicSlotID(0,"threshold"):spectralforge::eqID(0,-1,"bypass");
        auto* q=p->parameters().getParameter(id);const auto value=q->getValue();q->beginChangeGesture();
        q->setValueNotifyingHost(value>.5f?.25f:.75f);q->setValueNotifyingHost(value);q->endChangeGesture();
        std::mt19937 rng{43};double now=0;feedCapture(*p,c,playedNotes(48000,6.1,rng,250,12),now);
        check(values(*p)==before,"Phase 6 placed a partial node around a newer EQ/slot/bypass edit");
        c.undo();check(values(*p)==before,"Undo changed a manually preserved EQ");p->releaseResources();
    }
    // A host callback can edit a band synchronously during Auto Set's writes.
    auto p=std::make_unique<ChimeraProcessor>();p->setRateAndBufferSizeDetails(48000,256);p->prepareToPlay(48000,256);
    Controller c(*p);c.setOptions({1,false,Style::rock,false,false,false,1.f,Controller::lockInput|Controller::lockGate|Controller::lockCompressor,true,false});
    struct Edit final:juce::AudioProcessorListener {
        ChimeraProcessor& processor;bool fired=false;explicit Edit(ChimeraProcessor& p):processor(p){}
        void audioProcessorChanged(juce::AudioProcessor*,const ChangeDetails&) override {}
        void audioProcessorParameterChanged(juce::AudioProcessor*,int,float) override {}
        void audioProcessorParameterChangeGestureBegin(juce::AudioProcessor*,int) override {
            if(fired)return;fired=true;auto* q=processor.parameters().getParameter(spectralforge::eqID(0,0,"frequency"));q->setValueNotifyingHost(q->convertTo0to1(333.f));
        }
    } edit(*p);
    c.start();p->addListener(&edit);std::mt19937 rng{43};double now=0;feedCapture(*p,c,playedNotes(48000,6.1,rng,250,12),now);p->removeListener(&edit);
    check(edit.fired && near(get(*p,spectralforge::eqID(0,0,"frequency")),333,.02f),"Reentrant host edit was lost");
    check(get(*p,spectralforge::eqID(0,0,"enabled"))<.5f && get(*p,spectralforge::eqID(0,-1,"bypass"))>.5f
          && get(*p,spectralforge::dynamicSlotID(0,"target"))==0,"Reentrant edit left a partial node active");
    c.undo();check(near(get(*p,spectralforge::eqID(0,0,"frequency")),333,.02f),"Undo lost the reentrant edit");p->releaseResources();
    std::cout<<"PASS dynamic placement ownership: EQ/slot/bypass A-B-A and reentrant host edit\n";
}

void dynamicUserCurveRegression() {
    auto p=std::make_unique<ChimeraProcessor>();p->setRateAndBufferSizeDetails(48000,256);p->prepareToPlay(48000,256);
    const auto set=[&](const juce::String& id,float value){auto* q=p->parameters().getParameter(id);q->setValueNotifyingHost(q->convertTo0to1(value));};
    set(spectralforge::eqID(0,-1,"bypass"),0);
    for(int bank:{0,1})for(int b:{3,5}) {set(spectralforge::eqID(bank,b,"enabled"),1);set(spectralforge::eqID(bank,b,"frequency"),b==3?400:630);set(spectralforge::eqID(bank,b,"gain"),3);}
    set(spectralforge::dynamicSlotID(0,"target"),13);set(spectralforge::dynamicSlotID(1,"target"),6);
    const auto before=values(*p);Controller c(*p);c.setOptions({1,false,Style::rock,false,false,false,1.f,Controller::lockInput|Controller::lockGate|Controller::lockCompressor,true,false});
    c.start();std::mt19937 rng{43};double now=0;feedCapture(*p,c,playedNotes(48000,6.1,rng,250,12),now);
    check(get(*p,spectralforge::dynamicSlotID(2,"target"))>0,"Free slot was not used beside preserved user nodes");
    for(auto* raw:p->getParameters())if(auto* q=dynamic_cast<juce::RangedAudioParameter*>(raw)) {
        const auto id=q->paramID;
        if(id.startsWith("finalEQ_") || id.startsWith("toneEQ_b4_") || id.startsWith("toneEQ_b6_")
           || id.startsWith("dynamicEQ1_") || id.startsWith("dynamicEQ2_"))
            check(q->getValue()==before[size_t(q->getParameterIndex())],"Phase 6 changed an active static/user dynamic/Final EQ setting");
    }
    c.undo();const auto after=values(*p);for(size_t i=0;i<before.size();++i)check(near(before[i],after[i],1e-6f),"Undo failed to restore user curve");
    p->releaseResources();std::cout<<"PASS active user static Tone curve, existing Tone/Final dynamic slots, free-slot placement and Undo\n";
}
