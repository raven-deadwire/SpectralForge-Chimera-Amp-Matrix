cmake_minimum_required(VERSION 3.22)
project(ChimeraCabSchedulingController VERSION 0.1.0 LANGUAGES C CXX)

# JUCE adds Objective-C++ sources to the parent probe target on Apple hosts.
# Enable the language at the common parent scope, before the frozen subproject.
if(APPLE)
  enable_language(OBJCXX)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CHIMERA_FROZEN_SOURCE "" CACHE PATH "Clean, immutable Chimera source checkout")
set(CHIMERA_FROZEN_SHA "" CACHE STRING "Expected full Git commit of the immutable source")
if(NOT IS_DIRECTORY "${CHIMERA_FROZEN_SOURCE}")
  message(FATAL_ERROR "CHIMERA_FROZEN_SOURCE must name a clean source checkout")
endif()
string(LENGTH "${CHIMERA_FROZEN_SHA}" frozen_sha_length)
if(NOT frozen_sha_length EQUAL 40 OR NOT CHIMERA_FROZEN_SHA MATCHES "^[0-9a-f]+$")
  message(FATAL_ERROR "CHIMERA_FROZEN_SHA must be a full lowercase 40-character commit")
endif()
find_package(Git REQUIRED)
execute_process(COMMAND "${GIT_EXECUTABLE}" rev-parse HEAD
  WORKING_DIRECTORY "${CHIMERA_FROZEN_SOURCE}" OUTPUT_VARIABLE frozen_head
  OUTPUT_STRIP_TRAILING_WHITESPACE RESULT_VARIABLE frozen_head_result)
execute_process(COMMAND "${GIT_EXECUTABLE}" status --porcelain --untracked-files=all
  WORKING_DIRECTORY "${CHIMERA_FROZEN_SOURCE}" OUTPUT_VARIABLE frozen_status
  OUTPUT_STRIP_TRAILING_WHITESPACE RESULT_VARIABLE frozen_status_result)
if(NOT frozen_head_result EQUAL 0 OR NOT frozen_status_result EQUAL 0
   OR NOT frozen_head STREQUAL CHIMERA_FROZEN_SHA OR NOT frozen_status STREQUAL "")
  message(FATAL_ERROR "Frozen source SHA differs or its checkout is dirty")
endif()

if(MSVC)
  add_compile_options(/utf-8)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()
enable_testing()
# Reuse the exact frozen product CMake, JUCE 8.0.8 dependency, generated IR data,
# compiler options and test definitions. No source overlay or adapter changes.
# Original CTest entries retain their frozen-product binary working directory.
add_subdirectory("${CHIMERA_FROZEN_SOURCE}" "${CMAKE_BINARY_DIR}/frozen-product" EXCLUDE_FROM_ALL)

# Only this independently built executable can request another caller QoS.
# No Source file, original assertion, or deployed CTest is modified.
juce_add_console_app(ChimeraCabSchedulingProbe
  PRODUCT_NAME "Chimera CAB Scheduling Diagnostic")
target_sources(ChimeraCabSchedulingProbe PRIVATE CabSchedulingProbe.cpp
  "${CHIMERA_FROZEN_SOURCE}/Source/IRLibrary.cpp")
target_include_directories(ChimeraCabSchedulingProbe PRIVATE
  "${CHIMERA_FROZEN_SOURCE}/Source" "${CHIMERA_FROZEN_SOURCE}/Tests")
target_compile_definitions(ChimeraCabSchedulingProbe PRIVATE JUCE_USE_CURL=0 JUCE_WEB_BROWSER=0
  CHIMERA_FROZEN_SHA="${CHIMERA_FROZEN_SHA}"
  FROZEN_CAB_TEST_FILE="${CHIMERA_FROZEN_SOURCE}/Tests/OriginalCabIntegrationTests.cpp")
target_link_libraries(ChimeraCabSchedulingProbe PRIVATE ChimeraIRs
  juce::juce_data_structures juce::juce_dsp juce::juce_audio_formats
  PUBLIC juce::juce_recommended_config_flags)

file(SHA256 "${CHIMERA_FROZEN_SOURCE}/Tests/OriginalCabIntegrationTests.cpp" frozen_test_hash)
file(SHA256 "${CMAKE_CURRENT_SOURCE_DIR}/CabSchedulingProbe.cpp" wrapper_hash)
file(WRITE "${CMAKE_BINARY_DIR}/controller-source.txt"
  "frozen_sha=${CHIMERA_FROZEN_SHA}\nfrozen_test_sha256=${frozen_test_hash}\nwrapper_sha256=${wrapper_hash}\njuce_tag=8.0.8\ndiagnostic_only=1\n")
