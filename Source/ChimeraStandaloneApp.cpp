// Standalone application for Chimera (Issue #113). Identical to JUCE's
// default StandaloneFilterApp (same settings file, device handling and
// shutdown) except for the window: it offers a maximise button and F11,
// both of which fit the editor to the current display and toggle back.
// Compiled only into the Standalone target with
// JUCE_USE_CUSTOM_PLUGIN_STANDALONE_APP=1; plug-in formats are unaffected.
// Do not include juce_audio_plugin_client/detail headers here: on macOS they
// define Point/Component as macros, which breaks qualified juce:: names.
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h>
#include "WindowFit.h"
#include "StandaloneDiagnostics.h"
#include "StandaloneAudioDevices.h"
#include "PluginEditor.h"

namespace spectralforge {
class ChimeraStandaloneWindow final : public juce::StandaloneFilterWindow {
public:
    ChimeraStandaloneWindow(const juce::String& title,juce::Colour background,std::unique_ptr<juce::StandalonePluginHolder> holder)
        : StandaloneFilterWindow(title,background,std::move(holder)) {
        setTitleBarButtonsRequired(DocumentWindow::allButtons,false);
        if(auto* button=getMaximiseButton())button->setTooltip("Fit to screen (F11). Press again or Esc to restore.");
    }
    void maximiseButtonPressed() override {toggleFit();}
    bool keyPressed(const juce::KeyPress& key) override {
        if(key==juce::KeyPress(juce::KeyPress::F11Key) || (fitted && key==juce::KeyPress(juce::KeyPress::escapeKey))) {toggleFit();return true;}
        return StandaloneFilterWindow::keyPressed(key);
    }
private:
    ChimeraEditor* editor() {
        auto* processor=getAudioProcessor();return processor?dynamic_cast<ChimeraEditor*>(processor->getActiveEditor()):nullptr;
    }
    // Resize through the editor so its constrainer, scale menu and canvas
    // transform stay authoritative; the window follows its content.
    void toggleFit() {
        auto* view=editor();if(view==nullptr)return;
        const auto* display=juce::Desktop::getInstance().getDisplays().getDisplayForRect(getScreenBounds());
        if(display==nullptr)return;
        if(fitted) {
            fitted=false;
            view->setViewSize(ui::boundedEditorSize(restoredEditorSize,display->userArea,ui::editorWindowChrome(*view)),restoredScale);
            view->setViewPosition(restoredScroll);
            setTopLeftPosition(restoredPosition);
            // Preserve exact normal bounds on the same monitor. If that monitor
            // disappeared, keep the restored window reachable on this one.
            if(!display->userArea.contains(getScreenBounds()))
                setTopLeftPosition(ui::centredWindowPosition(display->userArea,{getWidth(),getHeight()}));
            return;
        }
        restoredEditorSize={view->getWidth(),view->getHeight()};restoredPosition=getPosition();
        restoredScale=view->preferredViewScale();restoredScroll=view->viewPosition();
        const auto size=ui::fittedEditorSize(display->userArea,ui::editorWindowChrome(*view));
        view->setViewSize(size);
        setTopLeftPosition(ui::centredWindowPosition(display->userArea,{getWidth(),getHeight()}));
        fitted=true;
    }
    bool fitted=false;
    juce::Point<int> restoredEditorSize,restoredPosition;
    juce::Point<int> restoredScroll;
    float restoredScale{};
};

// Device facts for the diagnostic snapshot. Driver-reported latency is what the
// driver claims, not a measured round trip; xruns are -1 when unsupported.
juce::var audioDeviceDiagnostics() {
    auto* holder=juce::StandalonePluginHolder::getInstance();
    if(holder==nullptr)return {};
    return standalone::deviceSnapshot(holder->deviceManager,(bool)holder->getMuteInputValue().getValue());
}

class ChimeraStandaloneApp final : public juce::JUCEApplication {
public:
    ChimeraStandaloneApp() {
        juce::PropertiesFile::Options options;
        options.applicationName=juce::CharPointer_UTF8(JucePlugin_Name);
        options.filenameSuffix=".settings";
        options.osxLibrarySubFolder="Application Support";
       #if JUCE_LINUX || JUCE_BSD
        options.folderName="~/.config";
       #else
        options.folderName="";
       #endif
        appProperties.setStorageParameters(options);
    }
    const juce::String getApplicationName() override {return juce::CharPointer_UTF8(JucePlugin_Name);}
    const juce::String getApplicationVersion() override {return JucePlugin_VersionString;}
    bool moreThanOneInstanceAllowed() override {return true;}
    void anotherInstanceStarted(const juce::String&) override {}
    void initialise(const juce::String&) override {
        standalone::audioDeviceDiagnostics=&spectralforge::audioDeviceDiagnostics;
        if(!juce::Desktop::getInstance().getDisplays().displays.isEmpty())
            mainWindow=std::make_unique<ChimeraStandaloneWindow>(getApplicationName(),
                juce::LookAndFeel::getDefaultLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId),createPluginHolder());
        if(mainWindow)mainWindow->setVisible(true);else pluginHolder=createPluginHolder();
    }
    void shutdown() override {standalone::audioDeviceDiagnostics=nullptr;pluginHolder=nullptr;mainWindow=nullptr;appProperties.saveIfNeeded();}
    void systemRequestedQuit() override {
        if(pluginHolder)pluginHolder->savePluginState();
        if(mainWindow)mainWindow->pluginHolder->savePluginState();
        if(juce::ModalComponentManager::getInstance()->cancelAllModalComponents())
            juce::Timer::callAfterDelay(100,[]{if(auto* app=juce::JUCEApplicationBase::getInstance())app->systemRequestedQuit();});
        else quit();
    }
private:
    std::unique_ptr<juce::StandalonePluginHolder> createPluginHolder() {
       #ifdef JucePlugin_PreferredChannelConfigurations
        constexpr juce::StandalonePluginHolder::PluginInOuts channels[]{JucePlugin_PreferredChannelConfigurations};
        const juce::Array<juce::StandalonePluginHolder::PluginInOuts> channelConfig(channels,juce::numElementsInArray(channels));
       #else
        const juce::Array<juce::StandalonePluginHolder::PluginInOuts> channelConfig;
       #endif
        auto holder=std::make_unique<juce::StandalonePluginHolder>(appProperties.getUserSettings(),false,juce::String{},nullptr,channelConfig,false);
       #if JUCE_WINDOWS
        standalone::preferAsioOnFirstRun(holder->deviceManager,holder->settings.get());
       #endif
        return holder;
    }
    juce::ApplicationProperties appProperties;
    std::unique_ptr<ChimeraStandaloneWindow> mainWindow;
    std::unique_ptr<juce::StandalonePluginHolder> pluginHolder;
};
}

START_JUCE_APPLICATION(spectralforge::ChimeraStandaloneApp)
