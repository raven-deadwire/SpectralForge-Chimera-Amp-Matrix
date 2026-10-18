cmake_minimum_required(VERSION 3.22)
project(ChimeraCabCallbackAttribution VERSION 0.1.0 LANGUAGES C CXX)

# JUCE attaches Objective-C++ sources to targets in this parent directory.
# Enabling the language only inside the product subdirectory is insufficient.
if(APPLE)
  enable_language(OBJCXX)
endif()
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(MSVC)
  add_compile_options(/utf-8)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

get_filename_component(default_source "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
set(CHIMERA_ATTRIBUTION_SOURCE "${default_source}" CACHE PATH "Source under diagnostic investigation")
if(NOT EXISTS "${CHIMERA_ATTRIBUTION_SOURCE}/Source/CabCallbackTrace.h")
  message(FATAL_ERROR "Attribution source must contain the opt-in CAB trace API")
endif()
find_package(Git REQUIRED)
execute_process(COMMAND "${GIT_EXECUTABLE}" rev-parse HEAD
  WORKING_DIRECTORY "${CHIMERA_ATTRIBUTION_SOURCE}" OUTPUT_VARIABLE source_sha
  OUTPUT_STRIP_TRAILING_WHITESPACE RESULT_VARIABLE source_sha_result)
if(NOT source_sha_result EQUAL 0)
  message(FATAL_ERROR "Cannot identify attribution source commit")
endif()

# Reuse production JUCE, binary IR data and unchanged test targets. Only the two
# named probes are built by the attribution controller; no plug-in is needed.
# FETCHCONTENT_SOURCE_DIR_JUCE remains available for an existing local JUCE tree.
enable_testing()
add_subdirectory("${CHIMERA_ATTRIBUTION_SOURCE}" "${CMAKE_BINARY_DIR}/product" EXCLUDE_FROM_ALL)

foreach(target IN ITEMS ChimeraCabCallbackProbe ChimeraCabCallbackProbeControl)
  juce_add_console_app(${target} PRODUCT_NAME "${target}")
  target_sources(${target} PRIVATE CabCallbackProbe.cpp "${CHIMERA_ATTRIBUTION_SOURCE}/Source/IRLibrary.cpp")
  target_include_directories(${target} PRIVATE "${CHIMERA_ATTRIBUTION_SOURCE}/Source"
    "${CHIMERA_ATTRIBUTION_SOURCE}/Tests")
  target_compile_definitions(${target} PRIVATE JUCE_USE_CURL=0 JUCE_WEB_BROWSER=0
    CHIMERA_ATTRIBUTION_SOURCE_SHA="${source_sha}"
    CHIMERA_ATTRIBUTION_COMPILER="${CMAKE_CXX_COMPILER_ID}-${CMAKE_CXX_COMPILER_VERSION}"
    CHIMERA_ATTRIBUTION_SYSTEM="${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}")
  target_link_libraries(${target} PRIVATE ChimeraIRs juce::juce_data_structures
    juce::juce_dsp juce::juce_audio_formats PUBLIC juce::juce_recommended_config_flags)
endforeach()
# This definition is PRIVATE. The product and frozen acceptance targets retain
# their existing build and their original performance/response assertions.
target_compile_definitions(ChimeraCabCallbackProbe PRIVATE CHIMERA_CAB_ATTRIBUTION=1)

juce_add_console_app(ChimeraCabAttributionRealtimeTests PRODUCT_NAME "Chimera CAB Attribution Semantic Tests")
target_sources(ChimeraCabAttributionRealtimeTests PRIVATE
  "${CHIMERA_ATTRIBUTION_SOURCE}/Tests/CabRealtimeRegressionTests.cpp")
target_include_directories(ChimeraCabAttributionRealtimeTests PRIVATE "${CHIMERA_ATTRIBUTION_SOURCE}/Source")
target_compile_definitions(ChimeraCabAttributionRealtimeTests PRIVATE JUCE_USE_CURL=0 JUCE_WEB_BROWSER=0
  CHIMERA_CAB_ATTRIBUTION=1)
target_link_libraries(ChimeraCabAttributionRealtimeTests PRIVATE juce::juce_dsp
  PUBLIC juce::juce_recommended_config_flags)
add_test(NAME ChimeraCabAttributionRealtimeTests COMMAND ChimeraCabAttributionRealtimeTests)

add_executable(ChimeraCabCallbackTraceTests CabCallbackTraceTests.cpp)
target_include_directories(ChimeraCabCallbackTraceTests PRIVATE "${CHIMERA_ATTRIBUTION_SOURCE}/Source")
add_test(NAME ChimeraCabCallbackTraceTests COMMAND ChimeraCabCallbackTraceTests)

set(manifest "schema_version=1\nsource_commit=${source_sha}\ndiagnostic_only=1\njuce_tag=8.0.8\n")
foreach(relative IN ITEMS Source/Cabinet.h Source/ModeledCabConvolution.h
    Source/IRLibrary.cpp Source/IRLibrary.h Source/CabCallbackTrace.h
    Tests/OriginalCabIntegrationTests.cpp Tools/CabCallbackAttribution/CabCallbackProbe.cpp
    Tools/CabCallbackAttribution/CabCallbackTraceTests.cpp Tools/CabCallbackAttribution/CMakeLists.txt)
  file(SHA256 "${CHIMERA_ATTRIBUTION_SOURCE}/${relative}" digest)
  string(APPEND manifest "${relative}=${digest}\n")
endforeach()
file(WRITE "${CMAKE_BINARY_DIR}/attribution-source.txt" "${manifest}")
