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
    juce::AudioProcessorEditor* editor() {
        auto* processor=getAudioProcessor();return processor?processor->getActiveEditor():nullptr;
    }
    // Resize through the editor so its constrainer, scale menu and canvas
    // transform stay authoritative; the window follows its content.
    void toggleFit() {
        auto* view=editor();if(view==nullptr)return;
        if(fitted) {
            fitted=false;view->setSize(restoredEditorSize.x,restoredEditorSize.y);
            setTopLeftPosition(restoredPosition);return;
        }
        restoredEditorSize={view->getWidth(),view->getHeight()};restoredPosition=getPosition();
        const auto* display=juce::Desktop::getInstance().getDisplays().getDisplayForRect(getScreenBounds());
        if(display==nullptr)return;
        const juce::Point<int> chrome{getWidth()-view->getWidth(),getHeight()-view->getHeight()};
        const auto size=ui::editorSizeForScale(ui::fittedEditorScale(display->userArea,chrome));
        view->setSize(size.x,size.y);
        setTopLeftPosition(ui::centredWindowPosition(display->userArea,{getWidth(),getHeight()}));
        fitted=true;
    }
    bool fitted=false;
    juce::Point<int> restoredEditorSize,restoredPosition;
};

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
        if(!juce::Desktop::getInstance().getDisplays().displays.isEmpty())
            mainWindow=std::make_unique<ChimeraStandaloneWindow>(getApplicationName(),
                juce::LookAndFeel::getDefaultLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId),createPluginHolder());
        if(mainWindow)mainWindow->setVisible(true);else pluginHolder=createPluginHolder();
    }
    void shutdown() override {pluginHolder=nullptr;mainWindow=nullptr;appProperties.saveIfNeeded();}
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
        return std::make_unique<juce::StandalonePluginHolder>(appProperties.getUserSettings(),false,juce::String{},nullptr,channelConfig,false);
    }
    juce::ApplicationProperties appProperties;
    std::unique_ptr<ChimeraStandaloneWindow> mainWindow;
    std::unique_ptr<juce::StandalonePluginHolder> pluginHolder;
};
}

START_JUCE_APPLICATION(spectralforge::ChimeraStandaloneApp)
