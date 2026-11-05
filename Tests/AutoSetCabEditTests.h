#pragma once

// Included by AutoSetTests.cpp: actual processor, parameter notifications and
// controller capture. The bright 4x10 fixture exposes unsafe partial placement.
void controllerCabPlacementEdits() {
    using P=Controller::Phase;
    int cases=0,moves=0;
    const auto post=cabRegression::target(true,48000,8);
    for(int mic:{0,1})for(float strength:{.25f,.5f,.75f,1.f})for(int edited=-1;edited<3;++edited) {
        // All four public strengths cover normal/returned edits. Synchronous
        // begin/value/end host callbacks and held gestures use the 50% case.
        for(int when=0;when<(edited>=0 && strength==.5f?5:1);++when)
        for(int trigger=0;trigger<(when>=1 && when<=3?3:1);++trigger) {
            auto p=std::make_unique<ChimeraProcessor>();p->setRateAndBufferSizeDetails(48000,256);p->prepareToPlay(48000,256);
            const auto raw=[&](const juce::String& id){return get(*p,id);};
            const auto put=[&](const juce::String& id,float value){auto* q=p->parameters().getParameter(id);q->setValueNotifyingHost(q->convertTo0to1(value));};
            const juce::String letter=mic?"B":"A";
            const juce::StringArray controls{"ocab1_"+letter+"position","ocab1_"+letter+"distance","cab"+letter+"gain1"};
            put("mode",0);put("cab1",1);put("ocab1_Aon",1);put("ocab1_Bon",1);put("cabblend1",float(mic));
            put("lcab1_layout",6);put("xcab1_driver",10);put("xcab1_"+letter+"mic",5);put("lcab1_"+letter+"unit",3);
            put(controls[0],.1f);put(controls[1],20);put(controls[2],-2);
            const auto current=spectralforge::cabLayout::settings(spectralforge::OriginalCabParameters::savedKey(0,mic,raw));
            const auto before=values(*p);auto expected=before;
            Controller c(*p);c.setOptions({2,false,Style::rock,false,false,false,strength,
                Controller::lockInput|Controller::lockGate|Controller::lockCompressor,false,false,false,true,false,false,false,false,false,false});
            const auto edit=[&]{
                auto* q=p->parameters().getParameter(controls[edited]);const float v=q->getValue();
                expected[size_t(q->getParameterIndex())]=v;
                q->beginChangeGesture();put(controls[edited],raw(controls[edited])+(edited==0?.1f:1.f));
                q->setValueNotifyingHost(v);q->endChangeGesture();
            };
            struct HostEdit final : juce::AudioProcessorListener {
                int trigger{},when{};bool fired{};std::function<void()> action;
                void fire(int index,int event){if(!fired && index==trigger && when==event){fired=true;action();}}
                void audioProcessorChanged(juce::AudioProcessor*,const ChangeDetails&) override {}
                void audioProcessorParameterChanged(juce::AudioProcessor*,int index,float) override {fire(index,2);}
                void audioProcessorParameterChangeGestureBegin(juce::AudioProcessor*,int index) override {fire(index,1);}
                void audioProcessorParameterChangeGestureEnd(juce::AudioProcessor*,int index) override {fire(index,3);}
            } host;
            host.trigger=p->parameters().getParameter(controls[trigger])->getParameterIndex();host.when=when;host.action=edit;
            c.start();
            if(edited>=0 && when==0)edit();
            if(edited>=0 && when>=1 && when<=3)p->addListener(&host);
            if(edited>=0 && when==4)p->parameters().getParameter(controls[edited])->beginChangeGesture();
            double ms=juce::Time::getMillisecondCounterHiRes();
            for(size_t n=0;c.busy() && n+256<=post.size();n+=256) {
                const float* out[]{post.data()+n};p->autoSetCapture().push(Tap::cab,out,1,256);ms+=1000.*256/48000;c.tick(ms);
            }
            if(edited>=0 && when>=1 && when<=3)p->removeListener(&host);
            if(edited>=0 && when==4)p->parameters().getParameter(controls[edited])->endChangeGesture();
            check(!c.busy(),"Cab edit-history capture did not finish");
            const auto* step=stepOf(c.current(),P::cab);check(step!=nullptr,"Cab edit-history result missing");
            const auto applied=values(*p);
            if(edited<0) {
                check(cabPlacementSafe(current,{raw(controls[0]),raw(controls[1])}),"Unedited cabinet placement was unsafe");
                if(step->changed)++moves;
                else check(step->result.contains("Strength placement") && applied==before,"Unedited cabinet fixture no longer exercises a move");
            } else {
                if(when>=1 && when<=3)check(host.fired,"Cab host-edit callback was not exercised");
                check(!step->changed && step->result.contains("mic controls edited"),"Edited mic left a partial automatic placement");
                // Retain the exact value the host touched, including a newly
                // applied value; every other write belongs to this transaction.
                for(size_t k=0;k<before.size();++k)
                    check(near(applied[k],expected[k],1e-6f),"Cab edit rollback lost a host value or changed an unrelated parameter");
            }
            c.undo();const auto undone=values(*p);
            for(size_t k=0;k<before.size();++k)check(near(undone[k],edited<0?before[k]:applied[k],1e-6f),"Cab UNDO lost an edit or retained an automatic partial write");
            p->releaseResources();++cases;
        }
    }
    check(moves>=2,"Cab edit protection prevented every useful unedited placement");
    std::cout<<"PASS cabinet placement edit history: "<<cases<<" cases, "<<moves
             <<" useful moves, A/B, four strengths, position/distance/gain, A-B-A, held gestures, begin/value/end host edits and undo\n";
}
