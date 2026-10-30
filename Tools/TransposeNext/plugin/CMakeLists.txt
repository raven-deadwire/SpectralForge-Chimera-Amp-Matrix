# Research-only plug-in and headless host check for T6 DAW acceptance.
# Not part of the product build; nothing here is installed or released.
cmake_minimum_required(VERSION 3.22)
project(ChimeraTransposeResearch VERSION 0.0.1 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# The host check loads the plug-in into its own process: both carry JUCE, built
# with different options, so nothing may be exported across that boundary.
set(CMAKE_C_VISIBILITY_PRESET hidden)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)
set(JUCE_DIR "" CACHE PATH "JUCE 8.0.8 source (d6181bde38d858c283c3b7bf699ce6340c050b5d)")
if(NOT EXISTS "${JUCE_DIR}/CMakeLists.txt")
  message(FATAL_ERROR "Set -DJUCE_DIR=<JUCE 8.0.8 source>")
endif()
execute_process(COMMAND git -C "${JUCE_DIR}" rev-parse HEAD
  OUTPUT_VARIABLE JUCE_SOURCE_SHA OUTPUT_STRIP_TRAILING_WHITESPACE
  RESULT_VARIABLE JUCE_GIT_RESULT)
if(NOT JUCE_GIT_RESULT EQUAL 0 OR NOT JUCE_SOURCE_SHA STREQUAL "d6181bde38d858c283c3b7bf699ce6340c050b5d")
  message(FATAL_ERROR "Research validation requires pinned JUCE 8.0.8 git checkout")
endif()
add_subdirectory("${JUCE_DIR}" juce)

set(ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../..")
set(RESEARCH_FORMATS VST3)
if(APPLE)
  list(APPEND RESEARCH_FORMATS AU)
endif()

juce_add_plugin(ChimeraTransposeResearch
  COMPANY_NAME "RavenForge Research"
  PLUGIN_MANUFACTURER_CODE RfRs
  PLUGIN_CODE CtR2
  BUNDLE_ID audio.spectralforge.research.transpose
  IS_SYNTH FALSE NEEDS_MIDI_INPUT FALSE NEEDS_MIDI_OUTPUT FALSE IS_MIDI_EFFECT FALSE
  COPY_PLUGIN_AFTER_BUILD FALSE
  FORMATS ${RESEARCH_FORMATS}
  PRODUCT_NAME "Chimera Transpose Research")
target_sources(ChimeraTransposeResearch PRIVATE ResearchProcessor.cpp)
target_include_directories(ChimeraTransposeResearch PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/.." "${ROOT}/Source")
target_compile_definitions(ChimeraTransposeResearch PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(ChimeraTransposeResearch PRIVATE juce::juce_audio_utils juce::juce_dsp
  PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)

juce_add_console_app(TransposeResearchHostCheck PRODUCT_NAME "TransposeResearchHostCheck")
target_sources(TransposeResearchHostCheck PRIVATE HostCheck.cpp)
target_include_directories(TransposeResearchHostCheck PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/.." "${ROOT}/Source")
target_compile_definitions(TransposeResearchHostCheck PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_PLUGINHOST_VST3=1 JUCE_MODAL_LOOPS_PERMITTED=1)
target_link_libraries(TransposeResearchHostCheck PRIVATE juce::juce_audio_utils juce::juce_dsp
  PUBLIC juce::juce_recommended_config_flags)

# Portable probe build for Windows/macOS as well as Linux. build.py remains
# the existing GCC/O2 research path. Record the actual executable hash used.
juce_add_console_app(TransposeProbe PRODUCT_NAME "TransposeProbe")
target_sources(TransposeProbe PRIVATE ../TransposeProbe.cpp)
target_include_directories(TransposeProbe PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/.." "${ROOT}/Source")
target_compile_definitions(TransposeProbe PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
target_link_libraries(TransposeProbe PRIVATE juce::juce_dsp juce::juce_audio_formats
  PUBLIC juce::juce_recommended_config_flags)
