#pragma once

// Included inside GraphicalEQUITests.cpp's test namespace. Each scenario owns
// its processor/panel; a failed scenario cannot leave selection/history behind.
struct GroupHostTrace {
    struct Event : juce::AudioProcessorParameter::Listener {
        int starts{},ends{},changes{},depth{};bool bad{};
        void parameterValueChanged(int,float) override {++changes;}
        void parameterGestureChanged(int,bool start) override {
            if(start){++starts;if(depth++!=0)bad=true;}
            else{++ends;if(depth--!=1)bad=true;}
        }
    };
    std::array<Event,graphicalEQParameterCount> events{};
    std::array<juce::RangedAudioParameter*,graphicalEQParameterCount> parameters{};
    static constexpr std::array<const char*,5> fields{"enabled","frequency","gain","q","type"};
    explicit GroupHostTrace(ChimeraProcessor& processor) {
        int i=0;
        for(int bank=0;bank<2;++bank) {
            attach(processor,i++,eqID(bank,-1,"bypass"));
            for(int b=0;b<graphicalEQBands;++b)for(auto* field:fields)attach(processor,i++,eqID(bank,b,field));
        }
    }
    ~GroupHostTrace(){for(size_t i=0;i<parameters.size();++i)if(parameters[i])parameters[i]->removeListener(&events[i]);}
    void attach(ChimeraProcessor& processor,int i,const juce::String& id) {
        parameters[size_t(i)]=processor.parameters().getParameter(id);
        check(parameters[size_t(i)]!=nullptr,"Group trace parameter missing");
        parameters[size_t(i)]->addListener(&events[size_t(i)]);
    }
    Event& event(int bank,int band,int field){return events[size_t(bank*61+1+band*5+field)];}
    void balanced() const {for(const auto& e:events)check(!e.bad && e.depth==0 && e.starts==e.ends,"Unbalanced/nested per-parameter group gesture");}
    void quiet() const {for(const auto& e:events)check(e.starts==0 && e.ends==0 && e.changes==0,"Selection/menu cancellation notified a host parameter");}
};

struct GroupFixture {
    std::unique_ptr<ChimeraProcessor> processor=std::make_unique<ChimeraProcessor>();
    std::unique_ptr<GraphicalEQPanel> panel;
    int bank;
    explicit GroupFixture(int bankToTest,int count=4):bank(bankToTest) {
        auto& edits=processor->eqEditing();
        for(int b=0;b<count;++b) {
            const float hz=b==0?27.5f:b==1?55.f:float(100.*std::pow(1.5,b));
            check(edits.addBand(bank,hz,float(b%3-1)*3)==b,"Group fixture add failed");
        }
        panel=std::make_unique<GraphicalEQPanel>(processor->parameters(),processor->graphicalEQ(bank),bank,edits,processor->eqCaptureBoundary());
        panel->setSize(1200,640);panel->addToDesktop(juce::ComponentPeer::windowIsTemporary);panel->setVisible(true);
        settle(20);panel->selectBandForEditing(0);baseline();
    }
    void baseline(){processor->eqEditing().barrier();check(!processor->eqEditing().canUndo(),"Fixture history did not reset");}
    void group(int a=1,int b=2){panel->selectBandForEditing(a);panel->toggleBandInSelection(b);}
    bool key(int code,int mods=0){return panel->keyPressed(juce::KeyPress(code,juce::ModifierKeys(mods),0));}
    juce::TextButton& button(const char* name){auto* b=find<juce::TextButton>(*panel,eqID(bank,-1,name));check(b!=nullptr,"Group button missing");return *b;}
    void press(const char* name){button(name).triggerClick();settle(20);}
    struct State {
        std::array<float,graphicalEQParameterCount> values{};
        std::array<uint16_t,2> occupied{};
        bool operator==(const State& other)const{return values==other.values && occupied==other.occupied;}
    };
    State state() const {
        State result;int i=0;
        for(int e=0;e<2;++e) {
            result.occupied[size_t(e)]=processor->eqEditing().occupiedMask(e);
            result.values[size_t(i++)]=processor->parameters().getParameter(eqID(e,-1,"bypass"))->getValue();
            for(int b=0;b<graphicalEQBands;++b)for(auto* field:GroupHostTrace::fields)
                result.values[size_t(i++)]=processor->parameters().getParameter(eqID(e,b,field))->getValue();
        }
        return result;
    }
};

void groupSingleSelectionSafety(int bank) {
    const int command=juce::ModifierKeys::commandModifier;
    for(int operation=0;operation<4;++operation) {
        GroupFixture f(bank);f.group();const auto before=f.state();
        if(operation==0)f.press("addBand");
        if(operation==1) {
            const auto at=f.panel->graphBounds().getTopRight()+juce::Point<float>(-40,35);
            f.panel->mouseDoubleClick(mouse(*f.panel,at,at,false));
        }
        if(operation==2)f.panel->selectBandForEditing(3);
        if(operation==3)f.panel->selectBandForEditing(1); // Existing member is still explicit single selection.
        const int selected=f.panel->selectedBand();
        check(selected>=0 && f.panel->selectionMask()==uint16_t(1u<<selected),"Explicit single selection retained old members");
        if(operation<2) {
            check(selected==4,"New band did not become the only selected band");
            check(f.key('z',command) && f.state()==before,"Add undo did not restore all EQ values/occupancy");
            check(!f.processor->eqEditing().canUndo(),"Add created multiple undo steps");
        } else check(f.state()==before && !f.processor->eqEditing().canUndo(),"Navigation changed audio/history");
    }
    GroupFixture f(bank,12);f.group();const auto before=f.state();const auto mask=f.panel->selectionMask();
    // A full bank refuses creation; invalid targets and selection commands are presentation only.
    const auto at=f.panel->graphBounds().getTopRight()+juce::Point<float>(-40,35);
    f.panel->mouseDoubleClick(mouse(*f.panel,at,at,false));
    f.panel->selectBandForEditing(-1);f.panel->selectBandForEditing(12);
    check(f.panel->selectionMask()==mask && f.state()==before,"Rejected single choice lost the group");
    GroupHostTrace trace(*f.processor);juce::MemoryBlock saved,after;
    f.processor->getStateInformation(saved);f.panel->selectAllBands();
    check(f.button("deleteBand").getButtonText()=="DELETE (12)","DELETE count did not update immediately");
    check(f.button("deleteBand").getTooltip().contains("12 selected bands"),"DELETE tooltip still describes one band");
    f.panel->collapseSelection();check(f.button("deleteBand").getButtonText()=="DELETE","Single DELETE label stale");
    f.processor->getStateInformation(after);check(saved==after && f.state()==before,"Selection changed serialized/audio state");
    trace.quiet();
}

void groupDeleteEntryPoints(int bank) {
    const int command=juce::ModifierKeys::commandModifier;
    for(int count:{1,2,12})for(int entry=0;entry<4;++entry) {
        GroupFixture f(bank,count==12?12:4);
        if(count==1)f.panel->selectBandForEditing(1);else if(count==2)f.group();else f.panel->selectAllBands();
        // A bypassed but occupied member must be restored bypassed, not enabled.
        set(*f.processor,eqID(bank,1,"enabled"),0);f.baseline();
        const auto before=f.state();const auto mask=f.panel->selectionMask();GroupHostTrace trace(*f.processor);
        if(entry==0)f.press("deleteBand");
        if(entry==1)check(f.key(juce::KeyPress::deleteKey),"Delete key not handled");
        if(entry==2)check(f.key(juce::KeyPress::backspaceKey),"Backspace not handled");
        if(entry==3)f.panel->makeDeleteMenuCallback()(1);
        const auto deleted=f.state();
        check(deleted.occupied[size_t(bank)]==uint16_t(before.occupied[size_t(bank)]&~mask),"Delete entry removed the wrong members");
        check(deleted.occupied[size_t(1-bank)]==before.occupied[size_t(1-bank)],"Delete touched other EQ bank");
        for(int b=0;b<graphicalEQBands;++b)if((mask>>b)&1)check(get(*f.processor,eqID(bank,b,"enabled"))==0,"Deleted band remained audible");
        const auto fallback=f.panel->selectionMask();
        check(f.key('z',command) && f.state()==before,"Group delete undo did not restore exact values/occupancy");
        check(!f.processor->eqEditing().canUndo(),"Group delete was more than one undo step");
        // Selection is editor-local: restored bands do not resurrect the old group.
        if(fallback!=0)check(f.panel->selectionMask()==fallback,"Delete undo resurrected a previous group");
        check(f.key('z',command|juce::ModifierKeys::shiftModifier) && f.state()==deleted,"Group delete redo was not exact");
        trace.balanced();
        for(int e=0;e<2;++e)for(int b=0;b<graphicalEQBands;++b)for(int field=0;field<5;++field)
            if(e!=bank || !((mask>>b)&1) || field!=0)
                check(trace.event(e,b,field).starts==0 && trace.event(e,b,field).changes==0,"Deletion gestured/wrote an unrelated parameter");
        if(count==12)check(!f.button("deleteBand").isEnabled(),"Empty bank DELETE remained enabled");
    }
}

void groupSelectionAfterRemoval(int bank) {
    GroupFixture f(bank);f.group();
    check(f.processor->eqEditing().removeBand(bank,2),"Partial removal fixture failed");
    check(waitFor([&]{return f.panel->selectedBand()==1;}),"Removed active member did not hand selection to survivor");
    check(f.panel->selectionMask()==0b0010,"Outside band joined surviving selection");
    check(f.key(juce::KeyPress::deleteKey) && f.panel->isBandVisible(0) && f.panel->isBandVisible(3),"Following Delete reached outside original group");

    GroupFixture rejected(bank);rejected.group();const auto before=rejected.state();
    // Another editing owner has an active transaction; the panel must not
    // clear its selection when removeBands rejects this request.
    check(rejected.processor->eqEditing().beginEdit(1-bank,"Foreign transaction"),"Rejected deletion fixture failed");
    rejected.key(juce::KeyPress::deleteKey);
    check(rejected.state()==before && rejected.panel->selectionMask()==0b0110,"Rejected group deletion cleared selection or wrote values");
    rejected.processor->eqEditing().endEdit();
}

void groupEditGestures(int bank) {
    for(bool wheel:{false,true}) {
        GroupFixture f(bank);set(*f.processor,eqID(bank,1,"type"),3);f.baseline();f.group(0,1);
        const auto before=f.state();GroupHostTrace trace(*f.processor);
        const auto grab=f.panel->visibleNodePosition(0);
        if(wheel) {
            juce::MouseWheelDetails details{};details.deltaY=.2f;
            f.panel->mouseWheelMove(mouse(*f.panel,grab,grab,false),details);
        } else {
            const auto plot=f.panel->graphBounds();
            const juce::Point<float> movement{plot.getWidth()*std::log(2.f)/std::log(1000.f),-plot.getHeight()*4.f/48.f};
            f.panel->mouseDown(mouse(*f.panel,grab,grab,false));
            f.panel->mouseDrag(mouse(*f.panel,grab+movement,grab,true));
            f.panel->mouseUp(mouse(*f.panel,grab+movement,grab,true));
            closeTo(get(*f.processor,eqID(bank,0,"frequency")),55.f,.03f,"Drop A0 group ratio incorrect");
            closeTo(get(*f.processor,eqID(bank,1,"frequency")),110.f,.06f,"Drop A1 group ratio incorrect");
        }
        const auto changed=f.state();trace.balanced();check(!(changed==before),"Group edit did not change fixture");
        for(int e=0;e<2;++e)for(int b=0;b<graphicalEQBands;++b)for(int field=0;field<5;++field) {
            const bool touched=e==bank && b<2 && (wheel?field==3:(field==1 || (b==0 && field==2)));
            check(trace.event(e,b,field).starts==(touched?1:0),"Group drag/Q gesture target mismatch");
        }
        check(f.key('z',juce::ModifierKeys::commandModifier) && f.state()==before,"Group edit undo mismatch");
        check(f.panel->selectionMask()==0b11 && !f.processor->eqEditing().canUndo(),"Group edit undo lost selection or created extra history");
        check(f.key('z',juce::ModifierKeys::commandModifier|juce::ModifierKeys::shiftModifier) && f.state()==changed,"Group edit redo mismatch");
        check(f.panel->selectionMask()==0b11,"Group edit redo lost selection");trace.balanced();
    }
}

void groupDragInterruption(int bank) {
    for(int operation=0;operation<8;++operation) {
        GroupFixture f(bank);f.panel->selectBandForEditing(0);f.panel->toggleBandInSelection(1);f.panel->toggleBandInSelection(2);
        const auto before=f.state();GroupHostTrace trace(*f.processor);
        const auto grab=f.panel->visibleNodePosition(2);
        f.panel->mouseDown(mouse(*f.panel,grab,grab,false));
        f.panel->mouseDrag(mouse(*f.panel,grab+juce::Point<float>(30,-10),grab,true));
        check(!(f.state()==before),"Group interruption fixture did not move");
        if(operation==0)f.key(juce::KeyPress::escapeKey);
        if(operation==1)f.panel->selectAllBands();
        if(operation==2)f.panel->toggleBandInSelection(0); // Non-active member: selectBand is not called.
        if(operation==3)f.panel->selectBandForEditing(3);
        if(operation==4)f.panel->selectBandsIn(f.panel->graphBounds(),true);
        if(operation==5){f.panel->setVisible(false);f.panel->setVisible(true);}
        if(operation==6) {
            auto* chooser=find<juce::ComboBox>(*f.panel,eqID(bank,-1,"band"));check(chooser!=nullptr,"Band chooser missing");
            chooser->setSelectedId(4,juce::sendNotificationSync);
        }
        if(operation==7){set(*f.processor,eqID(1-bank,0,"gain"),7);settle(60);}
        trace.balanced();
        for(int b=0;b<4;++b)for(int field=1;field<=2;++field)
            check(trace.event(bank,b,field).starts==(b<3?1:0),"Interrupted drag did not open exactly one gesture per member/field");
        const auto stopped=f.state();const auto selection=f.panel->selectionMask();
        f.panel->mouseDrag(mouse(*f.panel,grab+juce::Point<float>(100,-40),grab,true));
        f.panel->mouseUp(mouse(*f.panel,grab+juce::Point<float>(100,-40),grab,true));
        check(f.state()==stopped,"Pointer continued writing after selection/ownership change");trace.balanced();
        if(operation==7)check(!f.processor->eqEditing().canUndo() && !f.processor->eqEditing().canRedo(),"External EQ automation retained stale history");
        else {
            check(f.key('z',juce::ModifierKeys::commandModifier) && f.state()==before,"Interrupted drag was not one exact undo step");
            check(!f.processor->eqEditing().canUndo() && f.panel->selectionMask()==selection,"Undo changed selection or left extra drag history");
            check(f.key('z',juce::ModifierKeys::commandModifier|juce::ModifierKeys::shiftModifier) && f.state()==stopped,"Interrupted drag redo failed");
        }
    }
    GroupFixture f(bank);f.group();const auto area=f.panel->graphBounds();
    const auto start=area.getTopLeft()+juce::Point<float>(8,8),end=area.getBottomRight()-juce::Point<float>(8,8);
    f.panel->mouseDown(mouse(*f.panel,start,start,false));f.panel->mouseDrag(mouse(*f.panel,end,start,true));
    check(!f.panel->marqueeArea().isEmpty(),"Marquee fixture missing");f.key(juce::KeyPress::escapeKey);
    const auto mask=f.panel->selectionMask();f.panel->mouseUp(mouse(*f.panel,end,start,true));
    check(f.panel->marqueeArea().isEmpty() && f.panel->selectionMask()==mask && f.panel->selectionCount()==1,"Mouse-up revived an escaped marquee");
}

void groupMenuLifetime(int bank) {
    for(int operation=0;operation<9;++operation) {
        GroupFixture f(bank);f.group();auto callback=f.panel->makeDeleteMenuCallback();
        if(operation==1)f.panel->selectBandForEditing(3);
        if(operation==2){f.panel->selectBandForEditing(3);f.group();} // Same selected band AND mask again.
        if(operation==3){f.panel->setVisible(false);f.panel->setVisible(true);}
        if(operation==4)f.panel->setVisible(false);
        if(operation==5) {
            auto& edits=f.processor->eqEditing();check(edits.beginEdit(bank,"Menu intervening edit"),"Menu edit fixture");
            check(edits.setValue(bank,1,"gain",9),"Menu edit write");edits.endEdit();check(edits.undo(),"Menu undo fixture");
        }
        if(operation==6)f.processor->eqEditing().barrier(); // Same-valued restore still invalidates the menu.
        if(operation==7)f.panel.reset();
        if(operation==8)f.panel->selectBandsIn(f.panel->graphBounds(),true);
        const auto before=f.state();const bool undo=f.processor->eqEditing().canUndo(),redo=f.processor->eqEditing().canRedo();
        GroupHostTrace trace(*f.processor);
        callback(operation==0?0:1);
        check(f.state()==before && f.processor->eqEditing().canUndo()==undo && f.processor->eqEditing().canRedo()==redo,"Stale/cancelled menu changed EQ state/history");
        trace.quiet();
    }
}

void groupCrossBankHistory() {
    auto processor=std::make_unique<ChimeraProcessor>();auto& edits=processor->eqEditing();
    for(int bank=0;bank<2;++bank)for(int b=0;b<2;++b)check(edits.addBand(bank,100.f*float(b+1),0)==b,"Cross-bank fixture");
    auto editor=std::make_unique<ChimeraEditor>(*processor);editor->addToDesktop(juce::ComponentPeer::windowIsTemporary);editor->setVisible(true);settle(40);
    std::array<GraphicalEQPanel*,2> panels{};
    for(int bank=0;bank<2;++bank) {
        panels[size_t(bank)]=find<GraphicalEQPanel>(*editor,bank==0?"toneEQ_panel":"finalEQ_panel");check(panels[size_t(bank)]!=nullptr,"Cross-bank panel missing");
        panels[size_t(bank)]->selectBandForEditing(0);panels[size_t(bank)]->toggleBandInSelection(1);
    }
    edits.barrier();check(!edits.canUndo(),"Cross-bank baseline");
    for(int bank=0;bank<2;++bank) {
        check(edits.beginEdit(bank,"Cross-bank group Q"),"Cross-bank edit rejected");
        for(int b=0;b<2;++b)check(edits.setValue(bank,b,"q",float(bank+2)),"Cross-bank Q write");
        edits.endEdit();
    }
    const auto navigate=[&](int bank){auto* button=find<juce::TextButton>(*editor,bank==0?"path.tone.eq":"path.final.eq");check(button!=nullptr,"Cross-bank navigation missing");button->triggerClick();settle(30);};
    navigate(0);
    for(int bank:{1,0}) {
        auto* active=panels[0]->isShowing()?panels[0]:panels[1];
        check(active->keyPressed(juce::KeyPress('z',juce::ModifierKeys(juce::ModifierKeys::commandModifier),0)),"Cross-bank Undo key");
        check(waitFor([&]{return panels[size_t(bank)]->isShowing();}),"Undo did not show its target bank");
        check(edits.nextRedoBank()==bank,"Cross-bank undo order changed");
        for(auto* panel:panels)check(panel->selectionMask()==0b11,"Cross-bank history navigation dropped a group");
    }
    check(!edits.canUndo(),"Cross-bank edits created extra history");
    for(int bank:{0,1}) {
        auto* active=panels[0]->isShowing()?panels[0]:panels[1];
        active->keyPressed(juce::KeyPress('z',juce::ModifierKeys(juce::ModifierKeys::commandModifier|juce::ModifierKeys::shiftModifier),0));
        check(waitFor([&]{return panels[size_t(bank)]->isShowing();}),"Redo did not show its target bank");
        for(int b=0;b<2;++b)closeTo(get(*processor,eqID(bank,b,"q")),float(bank+2),.001f,"Cross-bank redo Q incorrect");
        for(auto* panel:panels)check(panel->selectionMask()==0b11,"Redo navigation changed a group");
    }
}

void groupSafetyRegression(int bank) {
    groupSingleSelectionSafety(bank);groupDeleteEntryPoints(bank);groupSelectionAfterRemoval(bank);groupEditGestures(bank);
    groupDragInterruption(bank);groupMenuLifetime(bank);
    std::cout<<"PASS EQ group safety: single selection, all delete entries, survivor/failure selection, per-field gestures, interrupted drag redo, menu lifetime bank="<<bank<<'\n';
}
