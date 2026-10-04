cmake_minimum_required(VERSION 3.22)
include(cmake/ChimeraVersion.cmake)
project(ChimeraAmpMatrix VERSION ${CHIMERA_PRODUCT_VERSION} LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)
if(MSVC)
  add_compile_options(/utf-8)
  # Ship self-contained Windows binaries; no separate VC++ redist installer.
  # Set this before JUCE creates its targets so every linked library agrees.
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()
include(FetchContent)
FetchContent_Declare(JUCE GIT_REPOSITORY https://github.com/juce-framework/JUCE.git GIT_TAG 8.0.8)
FetchContent_MakeAvailable(JUCE)
include(cmake/JuceVBlankShutdown.cmake)
set(CHIMERA_FORMATS VST3 Standalone)
if(APPLE)
  list(APPEND CHIMERA_FORMATS AU)
endif()
juce_add_plugin(ChimeraAmpMatrix COMPANY_NAME "RavenForge Luthier Intelligence" COMPANY_COPYRIGHT "Copyright © 2026 RavenForge Luthier Intelligence. All rights reserved." BUNDLE_ID audio.spectralforge.chimera IS_SYNTH FALSE NEEDS_MIDI_INPUT TRUE NEEDS_MIDI_OUTPUT FALSE IS_MIDI_EFFECT FALSE COPY_PLUGIN_AFTER_BUILD FALSE MICROPHONE_PERMISSION_ENABLED TRUE MICROPHONE_PERMISSION_TEXT "SpectralForge Chimera uses audio input to process your guitar or bass." PLUGIN_MANUFACTURER_CODE SpFg PLUGIN_CODE Chm1 FORMATS ${CHIMERA_FORMATS} PRODUCT_NAME "SpectralForge Chimera" ICON_BIG "Assets/Artwork/spectralforge-emblem.png" ICON_SMALL "Assets/Artwork/spectralforge-emblem.png")
juce_add_binary_data(ChimeraIRs HEADER_NAME ChimeraIRData.h NAMESPACE ChimeraIRData SOURCES Assets/IRs/guitar_v30_sm57.wav Assets/IRs/guitar_jensen_sm57.wav)
# Explicit production assets: abandoned trial encodings are never embedded.
set(CHIMERA_ARTWORK Assets/Artwork/raven-workbench.jpg Assets/Artwork/raven-emblem.png Assets/Artwork/spectralforge-emblem.png Assets/Artwork/chimera-wordmark.png)
set(CHIMERA_MODEL_ARTWORK p808 pcentaur prat pm87 pdyna pdiamond pqtron pmutron pmuff pface pbender prc prange pmicro aglass abrit a515 arect amark asvt agk ahybrid rsslvca r1176 rla2a rn73 rv5 risa rssleq rn73eq rpultec rce2 rdimension rstone rmistress reddy rpulsar r2290 rre201 rmemory remt rlex rspring cmesa campeg cbassman cdelta pfet pmu pbddi pb3k pm82 paw3 pwool pfactory pep plpb achim30 aorange abassman asubway amatchless adumble aeich)
include(Assets/Artwork/ExpandedArtwork.cmake OPTIONAL)
foreach(model IN LISTS CHIMERA_MODEL_ARTWORK)
  list(APPEND CHIMERA_ARTWORK "Assets/Artwork/${model}.jpg" "Assets/Artwork/${model}-alpha.png")
endforeach()
juce_add_binary_data(ChimeraArtwork HEADER_NAME ChimeraArtworkData.h NAMESPACE ChimeraArtworkData SOURCES ${CHIMERA_ARTWORK})
juce_add_binary_data(ChimeraManual HEADER_NAME ChimeraManualData.h NAMESPACE ChimeraManualData SOURCES docs/MANUAL.html)
target_include_directories(ChimeraAmpMatrix PUBLIC Source)
execute_process(COMMAND git rev-parse HEAD WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
  OUTPUT_VARIABLE CHIMERA_SOURCE_SHA OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
if(CHIMERA_SOURCE_SHA)
  string(SUBSTRING "${CHIMERA_SOURCE_SHA}" 0 10 CHIMERA_BUILD_REVISION)
else()
  set(CHIMERA_BUILD_REVISION "source-archive")
endif()
chimera_set_package_identity()
configure_file(cmake/ChimeraBuildVersion.h.in generated/ChimeraBuildVersion.h @ONLY)
configure_file(cmake/chimera-build-version.json.in chimera-build-version.json @ONLY)
include_directories("${CMAKE_BINARY_DIR}/generated")
target_compile_definitions(ChimeraAmpMatrix PRIVATE CHIMERA_BUILD_REVISION="${CHIMERA_BUILD_REVISION}")
target_sources(ChimeraAmpMatrix PRIVATE Source/CabinetSelector.h Source/ReleaseSupport.cpp Source/ReleaseSupport.h Source/ReleaseInfo.h)
target_sources(ChimeraAmpMatrix PRIVATE Source/IRLibrary.cpp Source/IRLibrary.h Source/Amplifier.h Source/Cabinet.h Source/GlobalDSP.h Source/PolyPitch.h Source/FXChain.h Source/LowCompressor.h Source/StudioModules.h Source/PerformanceUtilities.h Source/FXParameters.h Source/ModelCatalog.h Source/SpatialModules.h Source/IRMetadata.h Source/IRReferenceCatalog.h Source/HardwareArtwork.h Source/RasterArtwork.h Source/IRCollection.h Source/IRBrowserPanel.h Source/PluginProcessor.cpp Source/PluginProcessor.h Source/PluginEditor.cpp Source/PluginEditor.h Source/ChimeraDSP.h Source/MatrixTone.h)
set(CHIMERA_USE_CURL 0)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  set(CHIMERA_USE_CURL 1)
  find_package(CURL REQUIRED)
  target_link_libraries(ChimeraAmpMatrix PRIVATE CURL::libcurl)
endif()
target_compile_definitions(ChimeraAmpMatrix PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=${CHIMERA_USE_CURL} JUCE_LOAD_CURL_SYMBOLS_LAZILY=0 JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(ChimeraAmpMatrix PRIVATE ChimeraIRs ChimeraArtwork ChimeraManual juce::juce_cryptography juce::juce_audio_utils juce::juce_dsp PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
enable_testing()

# Compile the actual upstream/patched VBlankThread class with a deterministic
# failing DXGI output. This runs on every platform without a physical display.
find_package(Python3 3.9 REQUIRED COMPONENTS Interpreter)
add_test(NAME ChimeraPackagingVersionTests
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/Tests/test_packaging_version.py")
find_package(Threads REQUIRED)
set(CHIMERA_VBLANK_TEST_DIR "${CMAKE_BINARY_DIR}/vblank-test-generated")
execute_process(
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/Tools/extract_vblank_test_class.py"
    --baseline-source "${CHIMERA_VBLANK_ORIGINAL_SOURCE}"
    --patched-source "${CHIMERA_VBLANK_PATCHED_SOURCE}"
    --output-dir "${CHIMERA_VBLANK_TEST_DIR}"
  COMMAND_ERROR_IS_FATAL ANY)
foreach(variant Baseline Patched)
  add_executable(ChimeraVBlank${variant}Probe Tests/VBlankThreadProbe.cpp)
  target_include_directories(ChimeraVBlank${variant}Probe PRIVATE "${CHIMERA_VBLANK_TEST_DIR}")
  target_compile_definitions(ChimeraVBlank${variant}Probe PRIVATE
    CHIMERA_VBLANK_CLASS_HEADER="VBlankThread${variant}.h")
  target_link_libraries(ChimeraVBlank${variant}Probe PRIVATE Threads::Threads)
endforeach()
add_custom_target(ChimeraVBlankFailureTests
  DEPENDS ChimeraVBlankBaselineProbe ChimeraVBlankPatchedProbe)
add_test(NAME ChimeraVBlankFailureTests
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/Tools/verify_vblank_failure.py"
    --baseline "$<TARGET_FILE:ChimeraVBlankBaselineProbe>"
    --patched "$<TARGET_FILE:ChimeraVBlankPatchedProbe>"
    --extraction-report "${CHIMERA_VBLANK_TEST_DIR}/extraction.json"
    --report "${CMAKE_BINARY_DIR}/vblank-failure-verification.json")
set_tests_properties(ChimeraVBlankFailureTests PROPERTIES TIMEOUT 20)

juce_add_console_app(ChimeraTests PRODUCT_NAME "Chimera Tests")
target_sources(ChimeraTests PRIVATE Tests/ChimeraTests.cpp Source/IRLibrary.cpp)
target_include_directories(ChimeraTests PRIVATE Source)
target_compile_definitions(ChimeraTests PRIVATE JUCE_USE_CURL=0 JUCE_WEB_BROWSER=0)
target_link_libraries(ChimeraTests PRIVATE ChimeraIRs juce::juce_data_structures juce::juce_dsp juce::juce_audio_basics juce::juce_audio_formats PUBLIC juce::juce_recommended_config_flags)
add_test(NAME ChimeraTests COMMAND ChimeraTests)

# Optional real-peer UI probe on Linux (run with xvfb-run) and Windows.
# The same harness can be copied onto an unmodified source revision for A/B.
option(CHIMERA_UI_PROBES "Build UI CPU probe and portable editor regressions" OFF)
if(CHIMERA_UI_PROBES)
  target_compile_definitions(ChimeraAmpMatrix PUBLIC JUCE_MODAL_LOOPS_PERMITTED=1)
  juce_add_console_app(ChimeraUIRefreshProbe PRODUCT_NAME "Chimera UI Refresh Probe")
  target_sources(ChimeraUIRefreshProbe PRIVATE Tests/UIRefreshProbe.cpp)
  target_include_directories(ChimeraUIRefreshProbe PRIVATE Source)
  target_link_libraries(ChimeraUIRefreshProbe PRIVATE ChimeraAmpMatrix ChimeraArtwork ChimeraManual juce::juce_cryptography juce::juce_audio_utils juce::juce_dsp)
endif()

# Run the actual editor/state code and render snapshots on the user's target OS.
if(WIN32 OR CHIMERA_UI_PROBES)
  juce_add_console_app(ChimeraUITests PRODUCT_NAME "Chimera UI Tests")
  target_sources(ChimeraUITests PRIVATE Tests/ChimeraUITests.cpp)
  target_include_directories(ChimeraUITests PRIVATE Source)
  target_compile_definitions(ChimeraUITests PRIVATE JUCE_MODAL_LOOPS_PERMITTED=1)
  target_link_libraries(ChimeraUITests PRIVATE ChimeraAmpMatrix ChimeraArtwork ChimeraManual juce::juce_cryptography juce::juce_audio_utils juce::juce_dsp)
  add_test(NAME ChimeraUITests COMMAND ChimeraUITests "${CMAKE_BINARY_DIR}/ui-snapshots")
endif()

if(WIN32)
  # Exercise the shipped DLL through the real VST3 ABI. No JUCE/Chimera link:
  # last-instance release and FreeLibrary must tear down all plugin globals.
  set(CHIMERA_VST3_SDK "${JUCE_SOURCE_DIR}/modules/juce_audio_processors/format_types/VST3_SDK")
  add_executable(ChimeraVST3LifecycleTests
    Tests/VST3LifecycleTests.cpp
    "${CHIMERA_VST3_SDK}/pluginterfaces/base/funknown.cpp"
    "${CHIMERA_VST3_SDK}/pluginterfaces/base/coreiids.cpp"
    "${CHIMERA_VST3_SDK}/public.sdk/source/vst/vstinitiids.cpp")
  target_include_directories(ChimeraVST3LifecycleTests PRIVATE "${CHIMERA_VST3_SDK}")
  target_link_libraries(ChimeraVST3LifecycleTests PRIVATE ole32 user32)
  # Keep the host independently buildable for baseline protocol checks. The
  # normal all-target build supplies the plugin used by the CTest below.
  add_test(NAME ChimeraVST3LifecycleTests
    COMMAND ChimeraVST3LifecycleTests "$<TARGET_FILE:ChimeraAmpMatrix_VST3>")
  set_tests_properties(ChimeraVST3LifecycleTests PROPERTIES TIMEOUT 370)
endif()

# Deterministic offline amp renderer for user-owned NAM reference comparisons.
juce_add_console_app(ChimeraRender PRODUCT_NAME "Chimera Render")
target_sources(ChimeraRender PRIVATE Tests/RenderAmp.cpp)
target_include_directories(ChimeraRender PRIVATE Source)
target_compile_definitions(ChimeraRender PRIVATE JUCE_USE_CURL=0 JUCE_WEB_BROWSER=0)
target_link_libraries(ChimeraRender PRIVATE juce::juce_dsp juce::juce_audio_formats PUBLIC juce::juce_recommended_config_flags)

# Update metadata parsing and SHA256 checks are offline and run on every platform.
juce_add_console_app(ChimeraReleaseTests PRODUCT_NAME "Chimera Release Tests")
target_sources(ChimeraReleaseTests PRIVATE Tests/ReleaseSupportTests.cpp Source/ReleaseSupport.cpp)
target_include_directories(ChimeraReleaseTests PRIVATE Source)
target_compile_definitions(ChimeraReleaseTests PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=${CHIMERA_USE_CURL} JUCE_LOAD_CURL_SYMBOLS_LAZILY=0)
target_link_libraries(ChimeraReleaseTests PRIVATE juce::juce_core juce::juce_cryptography PUBLIC juce::juce_recommended_config_flags)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  target_link_libraries(ChimeraReleaseTests PRIVATE CURL::libcurl)
endif()
add_test(NAME ChimeraReleaseTests COMMAND ChimeraReleaseTests)

juce_add_console_app(ChimeraStabilityTests PRODUCT_NAME "Chimera Stability Tests")
target_sources(ChimeraStabilityTests PRIVATE Tests/StabilityTests.cpp)
target_include_directories(ChimeraStabilityTests PRIVATE Source)
target_compile_definitions(ChimeraStabilityTests PRIVATE JUCE_USE_CURL=0 JUCE_WEB_BROWSER=0)
target_link_libraries(ChimeraStabilityTests PRIVATE juce::juce_dsp juce::juce_audio_basics PUBLIC juce::juce_recommended_config_flags)
add_test(NAME ChimeraStabilityTests COMMAND ChimeraStabilityTests)

juce_add_console_app(ChimeraPedalBoardTests PRODUCT_NAME "Chimera Pedal Board Tests")
target_sources(ChimeraPedalBoardTests PRIVATE Tests/PedalBoardTests.cpp)
target_include_directories(ChimeraPedalBoardTests PRIVATE Source)
target_compile_definitions(ChimeraPedalBoardTests PRIVATE JUCE_USE_CURL=0 JUCE_WEB_BROWSER=0)
target_link_libraries(ChimeraPedalBoardTests PRIVATE juce::juce_dsp juce::juce_audio_basics PUBLIC juce::juce_recommended_config_flags)
add_test(NAME ChimeraPedalBoardTests COMMAND ChimeraPedalBoardTests)

juce_add_console_app(ChimeraIRCollectionTests PRODUCT_NAME "Chimera IR Collection Tests")
target_sources(ChimeraIRCollectionTests PRIVATE Tests/IRCollectionTests.cpp)
target_include_directories(ChimeraIRCollectionTests PRIVATE Source)
target_compile_definitions(ChimeraIRCollectionTests PRIVATE JUCE_USE_CURL=0 JUCE_WEB_BROWSER=0)
target_link_libraries(ChimeraIRCollectionTests PRIVATE juce::juce_audio_formats juce::juce_cryptography PUBLIC juce::juce_recommended_config_flags)
add_test(NAME ChimeraIRCollectionTests COMMAND ChimeraIRCollectionTests)

# On-demand validation for user-owned IR files. Requires explicit CLI inputs;
# this is not a zero-input CTest or a source of redistributable capture assets.
juce_add_console_app(ChimeraValidateExternalIR PRODUCT_NAME "Chimera Validate External IR")
target_sources(ChimeraValidateExternalIR PRIVATE Tests/ValidateExternalIR.cpp Source/IRLibrary.cpp)
target_include_directories(ChimeraValidateExternalIR PRIVATE Source)
target_compile_definitions(ChimeraValidateExternalIR PRIVATE JUCE_USE_CURL=0 JUCE_WEB_BROWSER=0)
target_link_libraries(ChimeraValidateExternalIR PRIVATE ChimeraIRs juce::juce_data_structures juce::juce_dsp juce::juce_audio_formats juce::juce_cryptography PUBLIC juce::juce_recommended_config_flags)

# Appended amp topologies: separate real-DSP numerical and legacy-preservation gate.
juce_add_console_app(ChimeraNewAmpTests PRODUCT_NAME "Chimera New Amp Tests")
target_sources(ChimeraNewAmpTests PRIVATE Tests/NewAmpTests.cpp)
target_include_directories(ChimeraNewAmpTests PRIVATE Source)
target_compile_definitions(ChimeraNewAmpTests PRIVATE JUCE_USE_CURL=0 JUCE_WEB_BROWSER=0)
target_link_libraries(ChimeraNewAmpTests PRIVATE juce::juce_dsp juce::juce_audio_basics PUBLIC juce::juce_recommended_config_flags)
add_test(NAME ChimeraNewAmpTests COMMAND ChimeraNewAmpTests)

juce_add_console_app(ChimeraPedalOctaverTests PRODUCT_NAME "Chimera Pedal Octaver Tests")
target_sources(ChimeraPedalOctaverTests PRIVATE Tests/PedalOctaverTests.cpp)
target_include_directories(ChimeraPedalOctaverTests PRIVATE Source)
target_compile_definitions(ChimeraPedalOctaverTests PRIVATE JUCE_USE_CURL=0 JUCE_WEB_BROWSER=0)
target_link_libraries(ChimeraPedalOctaverTests PRIVATE juce::juce_dsp juce::juce_audio_basics PUBLIC juce::juce_recommended_config_flags)
add_test(NAME ChimeraPedalOctaverTests COMMAND ChimeraPedalOctaverTests)

foreach(native AmpNative PostNative)
  juce_add_console_app(Chimera${native}Tests PRODUCT_NAME "Chimera ${native} Tests")
  target_sources(Chimera${native}Tests PRIVATE Tests/${native}Tests.cpp)
  target_include_directories(Chimera${native}Tests PRIVATE Source)
  target_compile_definitions(Chimera${native}Tests PRIVATE JUCE_USE_CURL=0 JUCE_WEB_BROWSER=0)
  target_link_libraries(Chimera${native}Tests PRIVATE juce::juce_dsp juce::juce_audio_basics PUBLIC juce::juce_recommended_config_flags)
  add_test(NAME Chimera${native}Tests COMMAND Chimera${native}Tests)
endforeach()

# Actual processor/APVTS gate routing, state and automation regression (no editor required).
juce_add_console_app(ChimeraGateProcessorTests PRODUCT_NAME "Chimera Gate Processor Tests")
target_sources(ChimeraGateProcessorTests PRIVATE Tests/GateProcessorTests.cpp)
target_include_directories(ChimeraGateProcessorTests PRIVATE Source)
target_link_libraries(ChimeraGateProcessorTests PRIVATE ChimeraAmpMatrix ChimeraArtwork ChimeraManual juce::juce_cryptography juce::juce_audio_utils juce::juce_dsp)
add_test(NAME ChimeraGateProcessorTests COMMAND ChimeraGateProcessorTests)

juce_add_console_app(ChimeraGateTests PRODUCT_NAME "Chimera Gate Tests")
target_sources(ChimeraGateTests PRIVATE Tests/GateTests.cpp)
target_include_directories(ChimeraGateTests PRIVATE Source)
target_compile_definitions(ChimeraGateTests PRIVATE JUCE_USE_CURL=0 JUCE_WEB_BROWSER=0)
target_link_libraries(ChimeraGateTests PRIVATE juce::juce_dsp juce::juce_audio_basics PUBLIC juce::juce_recommended_config_flags)
add_test(NAME ChimeraGateTests COMMAND ChimeraGateTests)

# Combined candidate full-state presets and actual processor unity/GR checks.
juce_add_console_app(ChimeraIntegratedProcessorTests PRODUCT_NAME "Chimera Integrated Processor Tests")
target_sources(ChimeraIntegratedProcessorTests PRIVATE Tests/IntegratedProcessorTests.cpp)
target_include_directories(ChimeraIntegratedProcessorTests PRIVATE Source)
target_link_libraries(ChimeraIntegratedProcessorTests PRIVATE ChimeraAmpMatrix ChimeraArtwork ChimeraManual juce::juce_cryptography juce::juce_audio_utils juce::juce_dsp)
add_test(NAME ChimeraIntegratedProcessorTests COMMAND ChimeraIntegratedProcessorTests "${CMAKE_BINARY_DIR}/signature-snapshots")

juce_add_console_app(ChimeraPitchRegressionTests PRODUCT_NAME "Chimera Pitch Regression Tests")
target_sources(ChimeraPitchRegressionTests PRIVATE Tests/PitchRegressionTests.cpp)
target_include_directories(ChimeraPitchRegressionTests PRIVATE Source)
target_compile_definitions(ChimeraPitchRegressionTests PRIVATE JUCE_USE_CURL=0 JUCE_WEB_BROWSER=0)
target_link_libraries(ChimeraPitchRegressionTests PRIVATE juce::juce_dsp juce::juce_audio_basics PUBLIC juce::juce_recommended_config_flags)
add_test(NAME ChimeraPitchRegressionTests COMMAND ChimeraPitchRegressionTests)
