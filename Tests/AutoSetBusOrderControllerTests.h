#pragma once
// Included after AutoSetBusTests.h. Never feed reordered audio into the
// unqualified bus-only model: preserve the complete user's bus setting.
void controllerBusOrderSafety() {
    using namespace spectralforge;using P=Controller::Phase;
    std::mt19937 rng{161};const auto x=dynamicPlaying(48000,10,rng);
    const Controller::Options options{1,false,Style::metal,false,false,false,.5f,
        Controller::lockInput|Controller::lockGate|Controller::lockCompressor,
        false,false,false,false,false,false,false,false,false,false,true,false,false};
    const auto set=[](ChimeraProcessor& p,const juce::String& id,float value){auto* q=p.parameters().getParameter(id);q->setValueNotifyingHost(q->convertTo0to1(value));};
    int cases=0;
    // Both active and bypassed predecessors are intentionally unsupported.
    for(int predecessor:{postPreampModule,postEQModule})for(bool enabled:{false,true})for(bool on:{false,true}) {
        const auto result=runBus(options,x,[&](ChimeraProcessor& p){
            const auto order=PostOrder::initial().placed(predecessor,0);
            for(int n=0;n<postModuleCount;++n)set(p,orderSlotID(OrderRegion::post,n),float(order.slots[size_t(n)]));
            set(p,postNativeBypassID(predecessor,0),enabled?0.f:1.f);
            set(p,postNativeBypassID(0,0),on?0.f:1.f);set(p,postNativeControlID(0,0,0),-40);
        });
        const auto* step=stepOf(result.status,P::bus);
        check(step && !step->changed && step->result.contains("POST dynamics must be first"),"Unsupported POST order applied bus correction");
        check(result.values==result.before && result.undone==result.before,"Unsupported POST order changed controls or undo");++cases;
    }
    // Returned order/model/key/trim/control edits must invalidate the capture.
    juce::StringArray dependencies{orderSlotID(OrderRegion::post,0),postNativeModeID(0),postNativeModelID(0),postNativeModelExtensionID(0),
        postNativeTrimID(0,0),postNativeLevelID(0,0),postNativeBypassID(0,0)};
    for(int c=0;c<postNativeModel(0,0).controlCount;++c)dependencies.add(postNativeControlID(0,0,c));
    for(const auto& id:dependencies) {
        const auto result=runBus(options,x,{},[&](ChimeraProcessor& p){
            auto* q=p.parameters().getParameter(id);const float old=q->getValue();
            q->setValueNotifyingHost(old<.5f?1.f:0.f);q->setValueNotifyingHost(old);
        });
        const auto* step=stepOf(result.status,P::bus);
        check(step && !step->changed && step->result.contains("edited"),"Returned bus dependency edit was not protected: "+id.toStdString());
        check(result.values==result.before && result.undone==result.before,"Returned edit left partial bus settings: "+id.toStdString());++cases;
    }
    std::cout<<"PASS bus order and dependency safety: "<<cases<<" cases\n";
}

void controllerBusApplySafety() {
    using namespace spectralforge;using P=Controller::Phase;
    std::mt19937 rng{161};const auto x=dynamicPlaying(48000,10,rng);int cases=0;
    // Catch partial writes at every actual write's begin/value/end callback.
    for(bool on:{false,true})for(int event:{1,2,3})for(int triggerControl:{busThresholdControl,busMakeupControl})
    for(int editedControl:{busThresholdControl,busMakeupControl,busSidechainControl}) {
        auto p=std::make_unique<ChimeraProcessor>();p->setRateAndBufferSizeDetails(48000,256);p->prepareToPlay(48000,256);
        const auto id=[](int n){return postNativeControlID(0,0,n);};
        const auto put=[&](const juce::String& name,float value){auto* q=p->parameters().getParameter(name);q->setValueNotifyingHost(q->convertTo0to1(value));};
        put(postNativeBypassID(0,0),on?0.f:1.f);put(id(busThresholdControl),-40);put(id(busMakeupControl),5);
        const auto before=values(*p);auto expected=before;
        Controller c(*p);c.setOptions({1,false,Style::metal,false,false,false,1.f,
            Controller::lockInput|Controller::lockGate|Controller::lockCompressor,
            false,false,false,false,false,false,false,false,false,false,true,false,false});
        struct HostEdit final:juce::AudioProcessorListener {
            int trigger{},event{};bool fired{};std::function<void()> action;
            void fire(int i,int e){if(!fired && i==trigger && event==e){fired=true;action();}}
            void audioProcessorChanged(juce::AudioProcessor*,const ChangeDetails&) override {}
            void audioProcessorParameterChanged(juce::AudioProcessor*,int i,float) override {fire(i,2);}
            void audioProcessorParameterChangeGestureBegin(juce::AudioProcessor*,int i) override {fire(i,1);}
            void audioProcessorParameterChangeGestureEnd(juce::AudioProcessor*,int i) override {fire(i,3);}
        } host;
        host.trigger=p->parameters().getParameter(id(triggerControl))->getParameterIndex();host.event=event;
        host.action=[&]{auto* q=p->parameters().getParameter(id(editedControl));
            // A real lasting user edit: retain it while rolling back only Auto Set.
            put(id(editedControl),editedControl==busSidechainControl?1.f:editedControl==busThresholdControl?-12.f:7.f);
            expected[size_t(q->getParameterIndex())]=q->getValue();};
        c.start();p->addListener(&host);double ms=juce::Time::getMillisecondCounterHiRes();
        for(size_t n=0;c.busy() && n+256<=x.size();n+=256) {
            const float* in[]{x.data()+n};for(auto tap:{Tap::deesser,Tap::output,Tap::post})p->autoSetCapture().push(tap,in,1,256);
            ms+=1000.*256/48000;c.tick(ms);
        }
        p->removeListener(&host);check(host.fired && !c.busy(),"Host callback fixture did not execute");
        const auto* step=stepOf(c.current(),P::bus);check(step && !step->changed && step->result.contains("edited while applying"),"Bus host edit left partial correction");
        const auto actual=values(*p);for(size_t n=0;n<actual.size();++n)check(near(actual[n],expected[n],1e-6f),"Bus rollback lost host edit or retained partial write");
        c.undo();const auto undone=values(*p);for(size_t n=0;n<actual.size();++n)check(near(actual[n],undone[n],1e-6f),"Bus UNDO changed retained edit");
        p->releaseResources();++cases;
    }
    std::cout<<"PASS bus apply transactions: "<<cases<<" begin/value/end host callback cases\n";
}
