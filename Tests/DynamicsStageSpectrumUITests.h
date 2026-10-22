#pragma once
// Included by CommonDynamicsUITests after its production-audio helpers.
namespace commonDynamicsUITests {
inline void measuredStageContracts(const juce::File& directory) {
    auto processor=std::make_unique<ChimeraProcessor>();auto& p=*processor;
    const auto setChecked=[&](const juce::String& id,float value) {
        set(p,id,value);
        require(std::abs(p.parameters().getRawParameterValue(id)->load()-value)<.005f,"Stage fixture requested an out-of-range or quantized parameter value");
    };
    const auto isolate=[&] {
        for(const auto* id:{"gateon","boardEnabled","transposeon","tuneron","cleanCompOn","liveLimiterOn","mbOn","deEsserOn","doubleron","metronome"})setChecked(id,0);
        for(const auto& spec:spectralforge::fxSpecs)if(spec.toggle)setChecked(spec.id,0);
        for(int i=1;i<=3;++i){setChecked("ampon"+juce::String(i),0);setChecked("cab"+juce::String(i),0);setChecked("level"+juce::String(i),0);}
        for(auto* parameter:p.getParameters())if(auto* ranged=dynamic_cast<juce::RangedAudioParameter*>(parameter)) {
            const auto& id=ranged->paramID;
            if((id.startsWith("nativeAmp_") && id.endsWith("_enabled")) || (id.startsWith("pn_") && id.endsWith("_native")))setChecked(id,0);
        }
        for(int bank=0;bank<2;++bank)setChecked(spectralforge::eqID(bank,-1,"bypass"),1);
        for(int slot=0;slot<4;++slot)setChecked(spectralforge::dynamicSlotID(slot,"target"),0);
        setChecked("mode",0);setChecked("input",0);setChecked("output",0);setChecked("oversampling",0);
    };
    isolate();p.prepareToPlay(48000,256);
    auto editor=std::make_unique<ChimeraEditor>(p);editor->addToDesktop(juce::ComponentPeer::windowIsTemporary);editor->setVisible(true);settle(100);openWorkspace(*editor);
    auto* panel=find<spectralforge::CommonDynamicsPanel>(*editor,"commonDynamics.panel");require(panel,"Stage measurement workspace missing");
    auto* measured=find<spectralforge::DynamicsSpectrumPanel>(*panel,"commonDynamics.spectrum");require(measured,"Stage measurement analyzer missing");
    const auto observe=[&](Page page,double hz,const char* label) {
        panel->selectPage(page);settle(70);acknowledge(p);audio(p,hz,128);
        const int bin=juce::roundToInt(hz*4096./48000.);
        require(measured->analyzerHasData(),"Selected stage has no measured frame");
        const float in=measured->spectrumDB(false,bin),out=measured->spectrumDB(true,bin);
        std::cout<<"spectrum "<<label<<" hz="<<hz<<" in_db="<<in<<" out_db="<<out<<'\n';
        require(in>-110 && out>-119,"Selected stage lost its actual test tone");
        signalPathTests::capture(*editor,directory,juce::String("measured-")+label);
        return std::array<float,2>{in,out};
    };
    const auto cut=[&](Page page,double hz,float minimum,const char* label) {
        const auto levels=observe(page,hz,label);require(levels[0]-levels[1]>minimum,"Stage OUT does not contain its own measured attenuation");return levels;
    };
    setChecked("cleanCompOn",1);setChecked("cleanCompThreshold",-32);setChecked("cleanCompRatio",4);setChecked("cleanCompAttack",1);setChecked("cleanCompKnee",0);setChecked("cleanCompMakeup",0);setChecked("cleanCompMix",1);setChecked("cleanCompDetector",0);
    cut(Page::compressor,937.5,8,"clean-compressor");
    isolate();setChecked("gateon",1);setChecked("gatethreshold",-15);setChecked("input",-12);setChecked("gateRangeDb",12);setChecked("gatehold",0);setChecked("gaterelease",5);setChecked("gateMode",0);
    for(int position=0;position<2;++position) {
        setChecked("gateAfterRig",float(position));setChecked("level1",6);
        const auto levels=cut(Page::gate,937.5,11,position==0?"gate-before-rig":"gate-after-rig");
        // The +6 dB lane level belongs before only the after-rig gate tap.
        require(std::abs(levels[0]-(-24.0412f+6.f*float(position)))<.2f,"Gate input comes from the wrong rig boundary");
    }
    isolate();setChecked("liveLimiterOn",1);setChecked("liveLimiterCeiling",-18);setChecked("output",12);
    const auto limited=cut(Page::limiter,937.5,16,"limiter-after-output-trim");
    require(std::abs(limited[0]+.0412f)<.2f,"Limiter IN omitted output trim");
    isolate();setChecked("mbOn",1);setChecked("mbMix",1);setChecked("mbDetector",0);
    for(int band=0;band<4;++band) {
        setChecked(spectralforge::multibandID(band,"threshold"),-48);setChecked(spectralforge::multibandID(band,"ratio"),20);
        setChecked(spectralforge::multibandID(band,"range"),6);setChecked(spectralforge::multibandID(band,"attack"),.1f);
    }
    cut(Page::multiband,937.5,5,"multiband");
    isolate();setChecked("deEsserOn",1);setChecked("deEsserFrequency",2000);setChecked("deEsserThreshold",-48);setChecked("deEsserRange",9);setChecked("deEsserRatio",20);setChecked("deEsserAttack",.1f);
    cut(Page::deesser,6000,7,"deesser-high-band");
    const auto low=observe(Page::deesser,93.75,"deesser-low-band");require(std::abs(low[0]-low[1])<.2f,"De-esser spectrum lost the unaffected low band");
    isolate();
    // Both dynamic EQ banks display their own measured cut. A deliberate POST
    // makeup gain makes the two bank inputs distinguishable at the same tone.
    setChecked("buscompon",1);setChecked("busratio",1);setChecked("busmakeup",6);
    for(int bank=0;bank<2;++bank) {
        setChecked(spectralforge::eqID(bank,-1,"bypass"),0);setChecked(spectralforge::eqID(bank,0,"enabled"),1);setChecked(spectralforge::eqID(bank,0,"type"),0);
        setChecked(spectralforge::eqID(bank,0,"frequency"),937.5);setChecked(spectralforge::eqID(bank,0,"gain"),0);setChecked(spectralforge::eqID(bank,0,"q"),4);
        setChecked(spectralforge::dynamicSlotID(0,"target"),float(bank*12+1));setChecked(spectralforge::dynamicSlotID(0,"threshold"),-48);
        setChecked(spectralforge::dynamicSlotID(0,"range"),6);setChecked(spectralforge::dynamicSlotID(0,"ratio"),20);setChecked(spectralforge::dynamicSlotID(0,"attack"),1);
        panel->selectPage(Page::dynamicEQ);panel->selectDynamicSlot(0);settle(70);
        auto* eq=find<spectralforge::GraphicalEQPanel>(*panel,bank==0?"toneEQ_panel":"finalEQ_panel");require(eq,"Dynamic EQ bank editor missing");
        // acknowledge() syncs the dynamics capture, not this EQ bank's analyzer;
        // a stalled macOS loop can leave the borrowed editor capturing only the
        // silent acknowledgement. Measure again (bounded); assertions unchanged.
        for(int attempt=0;attempt<3;++attempt) {
            acknowledge(p);audio(p,937.5,128);
            if(eq->isShowing() && eq->analyzerHasData() && eq->spectrumDB(false,80)>-110.f)break;
            std::cerr<<"Dynamic EQ bank="<<bank<<" attempt="<<attempt<<" showing="<<eq->isShowing()<<" data="<<eq->analyzerHasData()
                     <<" in_db="<<eq->spectrumDB(false,80)<<'\n';
        }
        require(eq->isShowing() && eq->analyzerHasData(),"Dynamic EQ bank has no real spectrum");
        const float in=eq->spectrumDB(false,80),out=eq->spectrumDB(true,80);
        std::cout<<"spectrum dynamic-EQ bank="<<bank<<" in_db="<<in<<" out_db="<<out<<'\n';
        require(std::abs(in-(-12.0412f+6.f*float(bank)))<.25f && in-out>5.5f,"Dynamic EQ spectrum uses the wrong bank or omits its dynamic cut");
        signalPathTests::capture(*editor,directory,bank==0?"measured-dynamic-tone":"measured-dynamic-final");
        setChecked(spectralforge::eqID(bank,-1,"bypass"),1);
    }
    isolate();setChecked("precompon",1);setChecked("precomp",0);setChecked("precomplevel",6);setChecked("buscompon",1);setChecked("busratio",1);setChecked("busmakeup",9);
    const auto pre=observe(Page::preCompressor,937.5,"legacy-pre");
    const auto post=observe(Page::postCompressor,937.5,"legacy-post");
    require(std::abs(pre[0]+12.0412f)<.2f && std::abs(pre[1]-pre[0]-6)<.2f && std::abs(post[0]-pre[1])<.2f && std::abs(post[1]-post[0]-9)<.2f,"PRE and POST compressor sources were mixed up");
    isolate();setChecked("mode",2);setChecked("x1",350);setChecked("lowcomp",1);
    cut(Page::lowCompressor,93.75,12,"matrix-low");
    const auto rejected=observe(Page::lowCompressor,3000,"matrix-low-high-rejection");
    // LOW must show the filtered DI branch, not the full-range merged output.
    require(rejected[0]<-45,"Matrix LOW spectrum displayed a full-range source");
    setChecked("mode",0);settle(70);acknowledge(p);audio(p);
    require(!measured->analyzerHasData(),"Inactive Matrix LOW retained another route's spectrum");
    editor.reset();p.releaseResources();
    std::cout<<"PASS measured IN/OUT for six workspaces, both gate positions, Tone/Final dynamic EQ, distinct PRE/POST and Matrix LOW sources\n";
}
}
