cmake_minimum_required(VERSION 3.22)
project(ChimeraAmpMatrix VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(JUCE
  GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
  GIT_TAG 8.0.8)
FetchContent_MakeAvailable(JUCE)

set(CHIMERA_FORMATS VST3 Standalone)
if(APPLE)
  list(APPEND CHIMERA_FORMATS AU)
endif()

juce_add_plugin(ChimeraAmpMatrix
  COMPANY_NAME "SpectralForge"
  BUNDLE_ID "audio.spectralforge.chimera"
  PLUGIN_MANUFACTURER_CODE SpFg
  PLUGIN_CODE Chmx
  FORMATS ${CHIMERA_FORMATS}
  PRODUCT_NAME "Chimera Amp Matrix"
  IS_SYNTH FALSE
  NEEDS_MIDI_INPUT FALSE
  NEEDS_MIDI_OUTPUT FALSE)

target_sources(ChimeraAmpMatrix PRIVATE
  Source/PluginProcessor.cpp Source/PluginProcessor.h
  Source/PluginEditor.cpp Source/PluginEditor.h
  Source/dsp/ChimeraEngine.cpp Source/dsp/ChimeraEngine.h
  Source/dsp/ThreeBandCrossover.cpp Source/dsp/ThreeBandCrossover.h
  Source/dsp/LaneProcessor.cpp Source/dsp/LaneProcessor.h
  Source/dsp/PreFxRack.cpp Source/dsp/PreFxRack.h
  Source/dsp/CabModule.cpp Source/dsp/CabModule.h
  Source/dsp/PostFxRack.cpp Source/dsp/PostFxRack.h)

target_compile_definitions(ChimeraAmpMatrix PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
target_link_libraries(ChimeraAmpMatrix PRIVATE juce::juce_audio_utils juce::juce_dsp
  PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)
