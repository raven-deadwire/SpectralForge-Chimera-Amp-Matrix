#pragma once
// Included by GraphicalEQUITests.cpp: uses the same real editor, mouse-event
// helpers and processor as the existing UI regressions. No DSP/UI doubles.
namespace eqBassZoomTests {
using Values=std::vector<float>;
struct Listen {
    juce::AudioProcessorParameter& parameter;HostEvents events;
    explicit Listen(juce::AudioProcessorParameter& p):parameter(p){parameter.addListener(&events);}
    ~Listen(){parameter.removeListener(&events);}
};
Values values(ChimeraProcessor& p) {
    Values result;for(auto* parameter:p.getParameters())result.push_back(parameter->getValue());return result;
}
juce::MemoryBlock saved(ChimeraProcessor& p) {
    juce::MemoryBlock result;check(p.tryGetStateInformation(result),"Bass zoom: state capture failed");return result;
}
struct Snapshot {
    Values parameters;juce::MemoryBlock binary;bool undo,redo;juce::String undoText,redoText;
    uint64_t revision;int slot;
    explicit Snapshot(ChimeraProcessor& p):parameters(values(p)),binary(saved(p)),
        undo(p.eqEditing().canUndo()),redo(p.eqEditing().canRedo()),
        undoText(p.eqEditing().undoDescription()),redoText(p.eqEditing().redoDescription()),
        revision(p.eqEditing().revision()),slot(p.comparisonSlot()){}
    void unchanged(ChimeraProcessor& p) const {
        check(values(p)==parameters,"Bass view changed a host parameter");
        check(saved(p)==binary,"Bass view changed serialized state/A-B slots");
        auto& h=p.eqEditing();
        check(h.canUndo()==undo && h.canRedo()==redo && h.undoDescription()==undoText
              && h.redoDescription()==redoText && h.revision()==revision,"Bass view changed edit history/revision");
        check(p.comparisonSlot()==slot,"Bass view changed active A/B slot");
    }
};
float expectedX(const GraphicalEQPanel& panel,float hz,int view) {
    constexpr double upper[]{20000.,2000.,500.};const auto plot=panel.graphBounds();
    return float(plot.getX()+plot.getWidth()*std::log(double(hz)/20.)/std::log(upper[view]/20.));
}
void cycle(ChimeraProcessor& p,GraphicalEQPanel& panel) {
    const Snapshot before(p);
    std::vector<std::unique_ptr<Listen>> listeners;
    for(auto* parameter:p.getParameters())listeners.push_back(std::make_unique<Listen>(*parameter));
    for(int view:{2,0,1,2,1,0,2}) {panel.setFrequencyView(view);settle(20);before.unchanged(p);}
    for(const auto& listener:listeners)check(listener->events.changes==0 && listener->events.starts==0
        && listener->events.ends==0,"Frequency view notified host automation");
}
void onlyChanged(ChimeraProcessor& p,const Values& before,int bank,bool frequency,bool gain) {
    size_t i=0;for(auto* parameter:p.getParameters()) {
        const bool allowed=(frequency && parameter==p.parameters().getParameter(eqID(bank,0,"frequency")))
            || (gain && parameter==p.parameters().getParameter(eqID(bank,0,"gain")));
        if(!allowed)check(parameter->getValue()==before[i],"Bass drag changed an unrelated parameter");++i;
    }
}
void fixture(ChimeraProcessor& p,GraphicalEQPanel& panel,int bank,float hz,float db=0.f) {
    auto& h=p.eqEditing();
    for(int b=0;b<graphicalEQBands;++b)if(h.isOccupied(bank,b))check(h.removeBand(bank,b),"Bass fixture remove band");
    check(h.addBand(bank,hz,db)==0,"Bass fixture create band");
    // Actual external writes establish a fresh history, as in modifierTransitions.
    set(p,eqID(bank,0,"frequency"),123.f);set(p,eqID(bank,0,"frequency"),hz);
    settle(60);panel.selectBandForEditing(0);panel.setDisplayRange(24);
    check(!h.canUndo() && !h.canRedo(),"Bass fixture retained stale history");
    closeTo(get(p,eqID(bank,0,"frequency")),hz,.002f,"Bass fixture quantized the low note");
}
void drag(ChimeraProcessor& p,GraphicalEQPanel& panel,int bank,juce::Point<float> delta,int mods,
          float expectedHz,float expectedDB,bool frequency,bool gain) {
    auto& h=p.eqEditing();const auto before=values(p);const auto press=panel.visibleNodePosition(0)+juce::Point<float>(-3,2);
    Listen frequencyEvents(*p.parameters().getParameter(eqID(bank,0,"frequency")));
    Listen gainEvents(*p.parameters().getParameter(eqID(bank,0,"gain")));
    Listen qEvents(*p.parameters().getParameter(eqID(bank,0,"q")));
    panel.mouseDown(mouseWith(panel,press,press,false,mods));
    check(values(p)==before,"Bass off-centre press moved parameters");
    panel.mouseDrag(mouseWith(panel,press+delta*.5f,press,true,mods));
    panel.mouseDrag(mouseWith(panel,press+delta,press,true,mods));
    panel.mouseUp(mouseWith(panel,press+delta,press,true,mods));
    check(frequencyEvents.events.starts==1 && frequencyEvents.events.ends==1
          && gainEvents.events.starts==1 && gainEvents.events.ends==1,"Bass drag host gesture imbalance");
    check(qEvents.events.changes==0 && qEvents.events.starts==0 && qEvents.events.ends==0,"Bass drag notified Q");
    closeTo(get(p,eqID(bank,0,"frequency")),expectedHz,.025f,"Bass drag frequency mapping");
    closeTo(get(p,eqID(bank,0,"gain")),expectedDB,.025f,"Bass drag gain mapping");
    onlyChanged(p,before,bank,frequency,gain);const auto after=values(p);
    check(after!=before && h.canUndo(),"Bass drag did not create an edit");
    // Exercise a populated undo stack and then a populated redo stack.
    cycle(p,panel);
    check(h.undo() && values(p)==before && !h.canUndo() && h.canRedo(),"Bass drag is not one exact undo step");
    cycle(p,panel);
    check(h.redo() && values(p)==after && !h.canRedo(),"Bass drag redo did not restore exact values");
    check(panel.frequencyViewIndex()==2,"Undo/Redo changed Bass view");
}
void notes(ChimeraProcessor& p,GraphicalEQPanel& panel,int bank) {
    for(float hz:{27.5f,30.87f}) {
        fixture(p,panel,bank,hz);const Snapshot initial(p);
        for(int view:{0,1,2}) {
            panel.setFrequencyView(view);const auto bounds=panel.graphBounds();
            const auto expected=juce::Point<float>(expectedX(panel,hz,view),bounds.getCentreY());
            closeTo(panel.nodePosition(0).x,expected.x,.05f,"Low note horizontal position");
            closeTo(panel.visibleNodePosition(0).y,expected.y,.05f,"Low note vertical position");
            const auto readout=panel.readoutAt(expected);
            check(readout.startsWith(hz==27.5f?"27.5 Hz":"30.9 Hz"),"Low note cursor rounding/mapping");
            panel.mouseMove(mouse(panel,expected,expected,false));check(panel.hoverPosition()==expected,"Low note hover");
            const auto press=expected+juce::Point<float>(-5,3);
            panel.mouseDown(mouse(panel,press,press,false));panel.mouseUp(mouse(panel,press,press,false));
            initial.unchanged(p);
            auto* selector=find<juce::ComboBox>(panel,eqID(bank,-1,"band"));
            check(selector && selector->getSelectedId()==1,"Low note click selected wrong band");
        }
        cycle(p,panel);
        const auto width=panel.graphBounds().getWidth(),height=panel.graphBounds().getHeight();
        const auto qID=eqID(bank,0,"q");const float originalQ=get(p,qID);
        // Free drag, fine Shift drag, Alt gain-only and Alt frequency-only.
        drag(p,panel,bank,{40,-12},0,hz*std::pow(25.f,40/width),12*48/height,true,true);
        fixture(p,panel,bank,hz);panel.setFrequencyView(2);
        drag(p,panel,bank,{40,-12},juce::ModifierKeys::shiftModifier,hz*std::pow(25.f,4/width),1.2f*48/height,true,true);
        fixture(p,panel,bank,hz);panel.setFrequencyView(2);
        drag(p,panel,bank,{0,-20},juce::ModifierKeys::altModifier,hz,20*48/height,false,true);
        fixture(p,panel,bank,hz);panel.setFrequencyView(2);
        drag(p,panel,bank,{35,0},juce::ModifierKeys::altModifier,hz*std::pow(25.f,35/width),0,true,false);
        closeTo(get(p,qID),originalQ,0,"Bass dragging changed Q");
        // Cross the left edge from each note: global 20 Hz floor, never a
        // viewport-dependent minimum. A no-motion press must keep the note.
        fixture(p,panel,bank,hz);panel.setFrequencyView(2);
        drag(p,panel,bank,{-width,0},juce::ModifierKeys::altModifier,20,0,true,false);
        // Low note below the displayed gain range remains selectable at bottom.
        fixture(p,panel,bank,hz,-18);panel.setFrequencyView(2);panel.setDisplayRange(12);
        const auto bottom=panel.visibleNodePosition(0);closeTo(bottom.y,panel.graphBounds().getBottom(),.05f,"Low note bottom pin");
        const auto before=values(p);panel.mouseDown(mouse(panel,bottom,bottom,false));
        check(values(p)==before,"Bottom-edge low note press changed true values");
        panel.mouseDrag(mouseWith(panel,bottom+juce::Point<float>(25,0),bottom,true,juce::ModifierKeys::altModifier));
        panel.mouseUp(mouse(panel,bottom+juce::Point<float>(25,0),bottom,true));
        closeTo(get(p,eqID(bank,0,"frequency")),hz*std::pow(25.f,25/width),.025f,"Bottom-edge low note could not move");
        onlyChanged(p,before,bank,true,false);check(p.eqEditing().undo() && values(p)==before,"Bottom-edge low note undo");
        std::cout<<"PASS bass note bank="<<bank<<" hz="<<hz<<" views=3 click/free/Shift/Alt/left/bottom/Undo/Redo\n";
    }
    // Adjacent A0/B0 nodes at equal gain must each be selectable, even when
    // their hit circles overlap in Full view. Test both selection directions.
    fixture(p,panel,bank,27.5f);check(p.eqEditing().addBand(bank,30.87f,0)==1,"Adjacent B0 fixture");settle(60);
    for(int view:{0,1,2}) {panel.setFrequencyView(view);const auto before=values(p);
        for(int band:{1,0,1}) {const auto at=panel.visibleNodePosition(band);panel.mouseDown(mouse(panel,at,at,false));panel.mouseUp(mouse(panel,at,at,false));
            check(find<juce::ComboBox>(panel,eqID(bank,-1,"band"))->getSelectedId()==band+1,"Adjacent low note selection");
            check(values(p)==before,"Adjacent low note click changed values");}
    }
}
void boundariesAndMenu(ChimeraProcessor& p,GraphicalEQPanel& panel,int bank) {
    for(float hz:{20.f,500.f,5000.f,20000.f}) {
        fixture(p,panel,bank,hz);panel.setFrequencyView(2);const auto plot=panel.graphBounds();
        const auto at=panel.visibleNodePosition(0);closeTo(at.x,hz==20?plot.getX():plot.getRight(),.05f,"Bass edge placement");
        const auto before=values(p);panel.mouseDown(mouse(panel,at,at,false));panel.mouseUp(mouse(panel,at,at,false));
        check(values(p)==before,"Bass edge click clamped the original frequency");
        // The unclipped anchor preserves a 5 kHz/20 kHz band's true value.
        const float dx=hz==20?30.f:-30.f;
        drag(p,panel,bank,{dx,0},juce::ModifierKeys::altModifier,
             hz*std::pow(25.f,dx/plot.getWidth()),0,true,false);
    }
    fixture(p,panel,bank,5000);panel.setFrequencyView(2);
    const auto width=panel.graphBounds().getWidth();
    drag(p,panel,bank,{width,0},juce::ModifierKeys::altModifier,20000,0,true,false);
    const auto plot=panel.graphBounds();
    check(panel.readoutAt({plot.getX(),plot.getCentreY()}).startsWith("20.0 Hz"),"Bass left readout");
    const auto rightText=panel.readoutAt({plot.getRight()-.01f,plot.getCentreY()});
    check(rightText.contains(" Hz"),"Bass right interior readout missing");
    closeTo(rightText.upToFirstOccurrenceOf(" Hz",false,false).getFloatValue(),
            500.f*std::pow(25.f,-.01f/plot.getWidth()),.05f,"Bass right interior frequency");
    std::cout<<"MEASURE bass right interior bank="<<bank<<" readout="<<rightText<<'\n';
    for(auto point:{juce::Point<float>(plot.getX()-1,plot.getCentreY()),{plot.getRight(),plot.getCentreY()},
                    {plot.getCentreX(),plot.getY()-1},{plot.getCentreX(),plot.getBottom()}})
        check(panel.readoutAt(point).isEmpty(),"Readout outside half-open plot bounds");
    const Snapshot before(p);
    panel.setDisplayRange(12);panel.setFrequencyView(2);
    for(int id:{std::numeric_limits<int>::min(),-1,0,4,50,99,103,999,std::numeric_limits<int>::max()}) {
        panel.applyViewMenuResult(id);
        check(panel.frequencyViewIndex()==2 && panel.displayRange()==12,"Unknown view menu ID was applied");before.unchanged(p);
    }
    for(int id:{1,2,3}) {panel.applyViewMenuResult(id);check(panel.displayRange()==GraphicalEQPanel::displayRanges[size_t(id-1)],"Valid dB menu ID rejected");}
    for(int id:{100,101,102}) {panel.applyViewMenuResult(id);check(panel.frequencyViewIndex()==id-100,"Valid frequency menu ID rejected");}
    before.unchanged(p);panel.setDisplayRange(24);
    std::cout<<"PASS bass boundaries/menu bank="<<bank<<" original 5k/20k anchors, global 20..20k clamps, invalid IDs ignored\n";
}
void recall(ChimeraProcessor& p,GraphicalEQPanel& panel,int bank,const juce::File& directory) {
    fixture(p,panel,bank,27.5f,3);panel.setFrequencyView(2);
    p.copyComparison();const auto a=values(p);p.selectComparison(1);
    settle(60);panel.selectBandForEditing(0);
    const auto plot=panel.graphBounds();
    drag(p,panel,bank,{float(plot.getWidth()*std::log(30.87/27.5)/std::log(25.)),plot.getHeight()/8},0,
         30.87f,-3.f,true,true);
    const auto b=values(p);
    cycle(p,panel);p.selectComparison(0);check(values(p)==a,"Bass A slot corrupted");
    cycle(p,panel);p.selectComparison(1);check(values(p)==b,"Bass B slot corrupted");
    check(!p.eqEditing().canUndo() && !p.eqEditing().canRedo(),"A/B recall retained old history");
    const auto binary=saved(p);const auto file=directory.getChildFile("bass-bank-"+juce::String(bank)+".chimera");
    check(file.replaceWithData(binary.getData(),binary.getSize()),"Bass reference save failed");
    juce::MemoryBlock disk;check(file.loadFileAsData(disk) && disk==binary,"Bass reference disk roundtrip");
    // Binary roundtrip can normalize unrelated defaults by one float ULP
    // (deEsserFrequency is 0.5 -> 0.5000000596 in Full too). Require every EQ
    // parameter to remain exact, then compare ALL parameters with Full recall.
    const auto eqUnchanged=[&](const Values& expected) {
        size_t i=0;for(auto* parameter:p.getParameters()) {
            const auto id=dynamic_cast<juce::AudioProcessorParameterWithID*>(parameter)->paramID;
            if(id.startsWith("toneEQ_") || id.startsWith("finalEQ_"))
                check(parameter->getValue()==expected[i],"Binary restore changed an EQ parameter");++i;
        }
    };
    panel.setFrequencyView(0);p.loadFactoryPreset(0);p.setStateInformation(disk.getData(),int(disk.getSize()));settle(60);
    eqUnchanged(b);const auto recalledB=values(p);
    p.selectComparison(0);eqUnchanged(a);const auto recalledA=values(p);
    // Same production serializer as SAVE AS/LOAD; native file chooser is not driven.
    for(int view:{0,1,2}) {
        panel.setFrequencyView(view);p.loadFactoryPreset(0);p.setStateInformation(disk.getData(),int(disk.getSize()));settle(60);
        if(values(p)!=recalledB) {
            size_t i=0;for(auto* parameter:p.getParameters()) {
                if(parameter->getValue()!=recalledB[i])std::cerr<<"RESTORE view="<<view<<" id="
                    <<dynamic_cast<juce::AudioProcessorParameterWithID*>(parameter)->paramID
                    <<" delta="<<parameter->getValue()-recalledB[i]<<'\n';++i;
            }
        }
        check(values(p)==recalledB && p.comparisonSlot()==1,"Bass binary restore differs from Full-view B");
        check(panel.frequencyViewIndex()==view,"Project/reference restore changed frequency view");
        p.selectComparison(0);check(values(p)==recalledA,"Restored inactive A slot differs from Full");
        p.selectComparison(1);check(values(p)==recalledB,"Restored B slot differs from Full");
    }
    // Compare the complete factory result with the same restore point and
    // preset in every view, rather than merely checking one low band.
    Values factory;juce::MemoryBlock factoryBinary;
    for(int view:{0,1,2}) {
        p.setStateInformation(binary.getData(),int(binary.getSize()));panel.setFrequencyView(view);p.loadFactoryPreset(0);settle(60);
        check(panel.frequencyViewIndex()==view,"Factory recall changed frequency view");
        if(view==0){factory=values(p);factoryBinary=saved(p);}else {
            check(values(p)==factory && saved(p)==factoryBinary,"Factory recall differs with frequency view");
        }
        check(!p.eqEditing().canUndo() && !p.eqEditing().canRedo(),"Factory recall retained history");
    }
    p.setStateInformation(binary.getData(),int(binary.getSize()));panel.setFrequencyView(2);settle(60);
    panel.selectBandForEditing(0);closeTo(panel.nodePosition(0).x,expectedX(panel,30.87f,2),.05f,"Restored B0 node position");
    auto* frequency=find<juce::Slider>(panel,eqID(bank,0,"frequency"));
    check(frequency && frequency->getMinimum()==20 && frequency->getMaximum()==20000,"Bass view narrowed numeric parameter range");
    closeTo(float(frequency->getValue()),30.87f,.002f,"Restored B0 numeric control");
    snapshot(panel,directory.getChildFile("eq-bass-bank-"+juce::String(bank)+".png"));
    std::cout<<"PASS bass recall bank="<<bank<<" distinct A0/B0 slots, .chimera save/load, factory equivalence in all views\n";
}
void run(ChimeraProcessor& p,std::unique_ptr<ChimeraEditor>& editor,const juce::File& directory) {
    check(directory.createDirectory().wasOk(),"Bass evidence directory");
    for(int bank=0;bank<2;++bank) {
        auto* path=find<juce::TextButton>(*editor,bank==0?"path.tone.eq":"path.final.eq");check(path,"Bass EQ navigation");path->triggerClick();
        auto* panel=find<GraphicalEQPanel>(*editor,bank==0?"toneEQ_panel":"finalEQ_panel");
        check(panel && waitFor([&]{return panel->isShowing();}),"Bass panel not visible");
        recall(p,*panel,bank,directory);notes(p,*panel,bank);boundariesAndMenu(p,*panel,bank);
    }
    const Snapshot before(p);editor.reset();editor=std::make_unique<ChimeraEditor>(p);
    editor->addToDesktop(juce::ComponentPeer::windowIsTemporary);editor->setVisible(true);settle(100);
    for(const char* id:{"toneEQ_panel","finalEQ_panel"}) {
        auto* panel=find<GraphicalEQPanel>(*editor,id);check(panel && panel->frequencyViewIndex()==0,"Reopened editor did not default to Full");
    }
    before.unchanged(p);std::cout<<"PASS bass editor recreation: Full view restored, processor/state unchanged (no DAW host)\n";
}
}
