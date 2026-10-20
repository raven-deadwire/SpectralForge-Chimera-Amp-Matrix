#pragma once
// Included by CommonDynamicsUITests after its production-audio helpers.
namespace commonDynamicsUITests {
inline void measuredStageContracts(const juce::File& directory) {
    auto processor=std::make_unique<ChimeraProcessor>();auto& p=*processor;
    const auto isolate=[&] {
        for(const auto* id:{"gateon","boardEnabled","transposeon","tuneron","cleanCompOn","liveLimiterOn","mbOn","deEsserOn","doubleron","metronome"})set(p,id,0);
        for(const auto& spec:spectralforge::fxSpecs)if(spec.toggle)set(p,spec.id,0);
        for(int i=1;i<=3;++i){set(p,"ampon"+juce::String(i),0);set(p,"cab"+juce::String(i),0);set(p,"level"+juce::String(i),0);}
        for(auto* parameter:p.getParameters())if(auto* ranged=dynamic_cast<juce::RangedAudioParameter*>(parameter)) {
            const auto& id=ranged->paramID;
            if((id.startsWith("nativeAmp_") && id.endsWith("_enabled")) || (id.startsWith("pn_") && id.endsWith("_native")))set(p,id,0);
        }
        for(int bank=0;bank<2;++bank)set(p,spectralforge::eqID(bank,-1,"bypass"),1);
        for(int slot=0;slot<4;++slot)set(p,spectralforge::dynamicSlotID(slot,"target"),0);
        set(p,"mode",0);set(p,"input",0);set(p,"output",0);set(p,"oversampling",0);
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
    set(p,"cleanCompOn",1);set(p,"cleanCompThreshold",-32);set(p,"cleanCompRatio",4);set(p,"cleanCompAttack",1);set(p,"cleanCompKnee",0);set(p,"cleanCompMakeup",0);set(p,"cleanCompMix",1);set(p,"cleanCompDetector",0);
    cut(Page::compressor,937.5,8,"clean-compressor");
    isolate();set(p,"gateon",1);set(p,"gatethreshold",-3);set(p,"gateRangeDb",12);set(p,"gatehold",0);set(p,"gaterelease",5);set(p,"gateMode",0);
    for(int position=0;position<2;++position) {
        set(p,"gateAfterRig",float(position));set(p,"level1",6);
        const auto levels=cut(Page::gate,937.5,11,position==0?"gate-before-rig":"gate-after-rig");
        // The +6 dB lane level belongs before only the after-rig gate tap.
        require(std::abs(levels[0]-(-12.0412f+6.f*float(position)))<.2f,"Gate input comes from the wrong rig boundary");
    }
    isolate();set(p,"liveLimiterOn",1);set(p,"liveLimiterCeiling",-24);set(p,"output",6);
    const auto limited=cut(Page::limiter,937.5,16,"limiter-after-output-trim");
    require(std::abs(limited[0]+6.0412f)<.2f,"Limiter IN omitted output trim");
    isolate();set(p,"mbOn",1);set(p,"mbMix",1);set(p,"mbDetector",0);
    for(int band=0;band<4;++band) {
        set(p,spectralforge::multibandID(band,"threshold"),-48);set(p,spectralforge::multibandID(band,"ratio"),20);
        set(p,spectralforge::multibandID(band,"range"),6);set(p,spectralforge::multibandID(band,"attack"),.1f);
    }
    cut(Page::multiband,937.5,5,"multiband");
    isolate();set(p,"deEsserOn",1);set(p,"deEsserFrequency",2000);set(p,"deEsserThreshold",-48);set(p,"deEsserRange",9);set(p,"deEsserRatio",20);set(p,"deEsserAttack",.1f);
    cut(Page::deesser,6000,7,"deesser-high-band");
    const auto low=observe(Page::deesser,93.75,"deesser-low-band");require(std::abs(low[0]-low[1])<.2f,"De-esser spectrum lost the unaffected low band");
    isolate();
    // Both dynamic EQ banks display their own measured cut. A deliberate POST
    // makeup gain makes the two bank inputs distinguishable at the same tone.
    set(p,"buscompon",1);set(p,"busratio",1);set(p,"busmakeup",6);
    for(int bank=0;bank<2;++bank) {
        set(p,spectralforge::eqID(bank,-1,"bypass"),0);set(p,spectralforge::eqID(bank,0,"enabled"),1);set(p,spectralforge::eqID(bank,0,"type"),0);
        set(p,spectralforge::eqID(bank,0,"frequency"),937.5);set(p,spectralforge::eqID(bank,0,"gain"),0);set(p,spectralforge::eqID(bank,0,"q"),4);
        set(p,spectralforge::dynamicSlotID(0,"target"),float(bank*12+1));set(p,spectralforge::dynamicSlotID(0,"threshold"),-48);
        set(p,spectralforge::dynamicSlotID(0,"range"),6);set(p,spectralforge::dynamicSlotID(0,"ratio"),20);set(p,spectralforge::dynamicSlotID(0,"attack"),1);
        panel->selectPage(Page::dynamicEQ);panel->selectDynamicSlot(0);settle(70);acknowledge(p);audio(p,937.5,128);
        auto* eq=find<spectralforge::GraphicalEQPanel>(*panel,bank==0?"toneEQ_panel":"finalEQ_panel");require(eq && eq->isShowing() && eq->analyzerHasData(),"Dynamic EQ bank has no real spectrum");
        const float in=eq->spectrumDB(false,80),out=eq->spectrumDB(true,80);
        std::cout<<"spectrum dynamic-EQ bank="<<bank<<" in_db="<<in<<" out_db="<<out<<'\n';
        require(std::abs(in-(-12.0412f+6.f*float(bank)))<.25f && in-out>5.5f,"Dynamic EQ spectrum uses the wrong bank or omits its dynamic cut");
        signalPathTests::capture(*editor,directory,bank==0?"measured-dynamic-tone":"measured-dynamic-final");
        set(p,spectralforge::eqID(bank,-1,"bypass"),1);
    }
    isolate();set(p,"precompon",1);set(p,"precomp",0);set(p,"precomplevel",6);set(p,"buscompon",1);set(p,"busratio",1);set(p,"busmakeup",9);
    const auto pre=observe(Page::preCompressor,937.5,"legacy-pre");
    const auto post=observe(Page::postCompressor,937.5,"legacy-post");
    require(std::abs(pre[0]+12.0412f)<.2f && std::abs(pre[1]-pre[0]-6)<.2f && std::abs(post[0]-pre[1])<.2f && std::abs(post[1]-post[0]-9)<.2f,"PRE and POST compressor sources were mixed up");
    isolate();set(p,"mode",2);set(p,"x1",350);set(p,"lowcomp",1);
    cut(Page::lowCompressor,93.75,12,"matrix-low");
    const auto rejected=observe(Page::lowCompressor,3000,"matrix-low-high-rejection");
    // LOW must show the filtered DI branch, not the full-range merged output.
    require(rejected[0]<-45,"Matrix LOW spectrum displayed a full-range source");
    set(p,"mode",0);settle(70);acknowledge(p);audio(p);
    require(!measured->analyzerHasData(),"Inactive Matrix LOW retained another route's spectrum");
    editor.reset();p.releaseResources();
    std::cout<<"PASS measured IN/OUT for six workspaces, both gate positions, Tone/Final dynamic EQ, distinct PRE/POST and Matrix LOW sources\n";
}
}
