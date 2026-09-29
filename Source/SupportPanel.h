#pragma once
#include "ReleaseSupport.h"
#include "ChimeraManualData.h"
#include <juce_gui_basics/juce_gui_basics.h>

// Hosted instances pass no service: network lifetime never belongs to a DAW.
class ChimeraSupportPanel final : public juce::Component, private juce::Timer {
public:
    ChimeraSupportPanel(std::shared_ptr<spectralforge::release::ReleaseSupport> service,
                        spectralforge::release::Diagnostics diagnostics)
        : support(std::move(service)) {
        setComponentID("supportPanel");
        check.setComponentID("supportUpdates");
        automatic.setComponentID("supportAutomatic");
        download.setComponentID("supportDownload");
        reveal.setComponentID("supportReveal");
        cancel.setComponentID("supportCancel");
        status.setComponentID("supportStatus");
        title.setText(juce::String("SpectralForge Chimera  /  ") + spectralforge::release::displayVersion,juce::dontSendNotification);
        title.setFont(juce::FontOptions(21.f,juce::Font::bold));
        status.setMultiLine(true,true);status.setReadOnly(true);status.setScrollbarsShown(true);
        status.setColour(juce::TextEditor::backgroundColourId,juce::Colour(0xff101615));
        status.setColour(juce::TextEditor::textColourId,juce::Colour(0xffe1dbce));
        note.setText("Automatic checks contact GitHub at most once a day. Downloads require a click and are verified with SHA-256. Close Chimera and your DAW before running an installer.",juce::dontSendNotification);
        note.setFont(juce::FontOptions(13.f));note.setColour(juce::Label::textColourId,juce::Colour(0xffb8c0b8));
        note.setJustificationType(juce::Justification::topLeft);
        for(auto* c:std::initializer_list<juce::Component*>{&title,&manual,&report,&check,&download,&reveal,&cancel,&automatic,&status,&note}) addAndMakeVisible(*c);
        manual.onClick=[this]{int bytes=0;const auto* html=ChimeraManualData::getNamedResource("MANUAL_html",bytes);showError(spectralforge::release::openManual(html,(size_t)bytes));};
        report.onClick=[this,diagnostics]{if(!spectralforge::release::bugReportUrl(diagnostics).launchInDefaultBrowser())showError(juce::Result::fail("Could not open your browser. Please report at the project's GitHub Issues page."));};
        report.setTooltip("Opens a report draft with version, OS and audio configuration. No audio, file paths or device names are sent. You review and submit it.");
        check.onClick=[this]{
            if(support) {support->checkNow();refresh();}
            else if(!juce::URL(juce::String(spectralforge::release::repositoryUrl)+"/releases").launchInDefaultBrowser())
                showError(juce::Result::fail("Could not open your browser. Open the project's GitHub Releases page."));
        };
        download.onClick=[this]{if(support){support->downloadUpdate();refresh();}};
        cancel.onClick=[this]{if(support){support->cancel();refresh();}};
        reveal.onClick=[this]{if(support)showError(support->revealVerifiedInstaller());};
        automatic.onClick=[this]{if(support){showError(support->setAutomaticChecksEnabled(automatic.getToggleState()));refresh();}};
        if(!support) {
            check.setButtonText("OPEN RELEASES");
            for(auto* component:std::initializer_list<juce::Component*>{&automatic,&download,&reveal,&cancel})
                component->setVisible(false);
            note.setText("Download updates from the release page or use the Chimera standalone application. Close your DAW before installing an update.",juce::dontSendNotification);
        }
        setSize(660,support ? 360 : 300);refresh();if(support)startTimerHz(4);
    }
    void paint(juce::Graphics& g) override {g.fillAll(juce::Colour(0xff171b1b));}
    void resized() override {
        title.setBounds(22,18,616,34);manual.setBounds(24,66,182,32);report.setBounds(218,66,182,32);check.setBounds(412,66,224,32);
        automatic.setBounds(24,110,612,28);status.setBounds(24,147,612,84);
        download.setBounds(24,244,248,32);reveal.setBounds(284,244,230,32);cancel.setBounds(526,244,110,32);note.setBounds(24,293,612,59);
        if(!support) {status.setBounds(24,116,612,90);note.setBounds(24,224,612,59);}
    }
private:
    void timerCallback() override {refresh();}
    void refresh() {
        if(!support) {
            status.setText("Updates open in your browser. Automatic checks and the verified downloader are available in Chimera standalone.",false);
            return;
        }
        using State=spectralforge::release::ReleaseSupport::State;
        const auto s=support->snapshot();const bool busy=s.state==State::checking || s.state==State::downloading;
        check.setEnabled(!busy);download.setEnabled(s.state==State::available);reveal.setEnabled(s.state==State::ready);
        cancel.setEnabled(busy);
        automatic.setToggleState(s.automaticChecks,juce::dontSendNotification);
        auto message=s.message;
        if(s.state==State::downloading)message+="\n"+juce::String(s.progress*100,0)+"%";
        if(status.getText()!=message)status.setText(message,false);
    }
    void showError(const juce::Result& result) {if(result.failed())juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,"Chimera support",result.getErrorMessage(),"OK",this);}
    std::shared_ptr<spectralforge::release::ReleaseSupport> support;
    juce::Label title,note;
    juce::TextEditor status;
    juce::TextButton manual{"OPEN MANUAL"},report{"REPORT A BUG"},check{"CHECK FOR UPDATES"},download{"DOWNLOAD VERIFIED UPDATE"},reveal{"SHOW INSTALLER"},cancel{"CANCEL"};
    juce::ToggleButton automatic{"Automatically check for Open Beta updates"};
};
