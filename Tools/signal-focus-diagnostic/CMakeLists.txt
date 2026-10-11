cmake_minimum_required(VERSION 3.22)
project(ChimeraSignalFocusDiagnostic VERSION 1.0.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)
include(FetchContent)
FetchContent_Declare(JUCE GIT_REPOSITORY https://github.com/juce-framework/JUCE.git GIT_TAG d6181bde38d858c283c3b7bf699ce6340c050b5d) # 8.0.8
FetchContent_MakeAvailable(JUCE)
option(CHIMERA_FOCUS_PRESERVE_CURRENT "Diagnostic candidate: preserve explicitly focused child on Linux FocusIn" OFF)
if(CHIMERA_FOCUS_PRESERVE_CURRENT)
  include("${CMAKE_CURRENT_LIST_DIR}/PreserveCurrentFocus.cmake")
  set(variant "preserve-current")
else()
  set(variant "baseline")
endif()
juce_add_console_app(ChimeraSignalFocusDiagnostic PRODUCT_NAME "Chimera Signal Focus Diagnostic")
target_sources(ChimeraSignalFocusDiagnostic PRIVATE probe.cpp)
target_compile_definitions(ChimeraSignalFocusDiagnostic PRIVATE
  JUCE_MODAL_LOOPS_PERMITTED=1 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0
  FOCUS_DIAGNOSTIC_VARIANT="${variant}")
target_link_libraries(ChimeraSignalFocusDiagnostic PRIVATE juce::juce_gui_basics)
