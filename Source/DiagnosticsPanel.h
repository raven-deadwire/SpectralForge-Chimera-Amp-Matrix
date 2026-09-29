#pragma once
#include "PluginProcessor.h"
class DiagnosticsPanel final : public juce::Component {
    ChimeraProcessor& processor;
    juce::TextEditor preview;
    juce::TextButton refresh{"REFRESH SNAPSHOT"},copy{"COPY JSON"},save{"SAVE JSON"};
    juce::Label note;
    std::unique_ptr<juce::FileChooser> chooser;
public:
    explicit DiagnosticsPanel(ChimeraProcessor& p):processor(p) {
        for(juce::Component* c:std::initializer_list<juce::Component*>{&preview,&refresh,&copy,&save,&note})addAndMakeVisible(c);
        preview.setMultiLine(true);preview.setReadOnly(true);preview.setScrollbarsShown(true);preview.setText(processor.diagnosticReport(),false);
        note.setText("Review before sharing. Audio, device names, user paths and preset names are excluded. RMS is not LUFS. A snapshot does not prove a DAW fault.",juce::dontSendNotification);
        note.setFont(juce::FontOptions(12.f));note.setJustificationType(juce::Justification::topLeft);
        refresh.onClick=[this]{preview.setText(processor.diagnosticReport(),false);};
        copy.onClick=[this]{juce::SystemClipboard::copyTextToClipboard(preview.getText());};
        save.onClick=[this]{chooser=std::make_unique<juce::FileChooser>("Save reviewed diagnostic snapshot",juce::File{},"*.json");
            const juce::Component::SafePointer<DiagnosticsPanel> safe(this);const auto text=preview.getText();
            chooser->launchAsync(juce::FileBrowserComponent::saveMode|juce::FileBrowserComponent::canSelectFiles|juce::FileBrowserComponent::warnAboutOverwriting,[safe,text](const juce::FileChooser& c){
                if(!safe||c.getResult()==juce::File{})return;
                if(!c.getResult().replaceWithText(text))juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,"Diagnostics","Could not save the report.","OK",safe);
            });};setSize(760,560);
    }
    void resized() override {refresh.setBounds(18,16,228,29);copy.setBounds(263,16,220,29);save.setBounds(500,16,242,29);preview.setBounds(18,59,724,428);note.setBounds(18,501,724,48);}
    void paint(juce::Graphics& g) override {g.fillAll(juce::Colour(0xff171b1b));}
};
