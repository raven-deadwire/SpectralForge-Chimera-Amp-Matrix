cmake_minimum_required(VERSION 3.22)
include(cmake/ChimeraVersion.cmake)
project(ChimeraAmpMatrix VERSION ${CHIMERA_PRODUCT_VERSION} LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)
include(cmake/ChimeraAAX.cmake)
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
include(cmake/JuceLinuxFocus.cmake)
set(CHIMERA_FORMATS VST3 Standalone)
if(APPLE)
  list(APPEND CHIMERA_FORMATS AU)
endif()
if(CHIMERA_BUILD_AAX)
  list(APPEND CHIMERA_FORMATS AAX)
endif()
juce_add_plugin(ChimeraAmpMatrix COMPANY_NAME "RavenForge Luthier Intelligence" COMPANY_COPYRIGHT "Copyright © 2026 RavenForge Luthier Intelligence. All rights reserved." BUNDLE_ID audio.spectralforge.chimera IS_SYNTH FALSE NEEDS_MIDI_INPUT TRUE NEEDS_MIDI_OUTPUT FALSE IS_MIDI_EFFECT FALSE COPY_PLUGIN_AFTER_BUILD FALSE MICROPHONE_PERMISSION_ENABLED TRUE MICROPHONE_PERMISSION_TEXT "SpectralForge Chimera uses audio input to process your guitar or bass." PLUGIN_MANUFACTURER_CODE SpFg PLUGIN_CODE Chm1 AAX_IDENTIFIER audio.spectralforge.chimera AAX_CATEGORY Effect DISABLE_AAX_MULTI_MONO TRUE FORMATS ${CHIMERA_FORMATS} PRODUCT_NAME "SpectralForge Chimera" ICON_BIG "Assets/Artwork/spectralforge-emblem.png" ICON_SMALL "Assets/Artwork/spectralforge-emblem.png")
if(CHIMERA_BUILD_AAX)
  # Restrict first delivery to Native mono/stereo inserts. Host multi-mono and
  # AudioSuite require separate acceptance; neither affects internal Dual Mono.
  target_compile_definitions(ChimeraAmpMatrix_AAX PRIVATE
    JucePlugin_AAXDisableAudioSuite=1 JUCE_FORCE_USE_LEGACY_PARAM_IDS=0)
endif()
juce_add_binary_data(ChimeraIRs HEADER_NAME ChimeraIRData.h NAMESPACE ChimeraIRData SOURCES Assets/IRs/guitar_v30_sm57.wav Assets/IRs/guitar_jensen_sm57.wav)
# Explicit production assets: abandoned trial encodings are never embedded.
set(CHIMERA_ARTWORK Assets/Artwork/raven-workbench.jpg Assets/Artwork/raven-emblem.png Assets/Artwork/spectralforge-emblem.png Assets/Artwork/chimera-wordmark.png Assets/Artwork/anastrond.png Assets/Artwork/aniflheimr.png)
set(CHIMERA_MODEL_ARTWORK p808 pcentaur prat pm87 pdyna pdiamond pqtron pmutron pmuff pface pbender prc prange pmicro aglass abrit a515 arect amark asvt agk ahybrid rsslvca r1176 rla2a rn73 rv5 risa rssleq rn73eq rpultec rce2 rdimension rstone rmistress reddy rpulsar r2290 rre201 rmemory remt rlex rspring cmesa campeg cbassman cdelta pfet pmu pbddi pb3k pm82 paw3 pwool pfactory pep plpb achim30 aorange abassman asubway amatchless adumble aeich)
include(Assets/Artwork/ExpandedArtwork.cmake OPTIONAL)
foreach(model IN LISTS CHIMERA_MODEL_ARTWORK)
  list(APPEND CHIMERA_ARTWORK "Assets/Artwork/${model}.jpg" "Assets/Artwork/${model}-alpha.png")
endforeach()
include(Assets/Artwork/Cab/CabArtwork.cmake)
include(Assets/Artwork/HeadShells/HeadShellArtwork.cmake)
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
option(CHIMERA_EQ_DEVELOPMENT_PREVIEW "Identify the isolated EQ development preview (never a public release)" OFF)
if(CHIMERA_EQ_DEVELOPMENT_PREVIEW)
  set(CHIMERA_PACKAGE_VERSION "${PROJECT_VERSION}-preview.${CHIMERA_BUILD_REVISION}")
  set(CHIMERA_PACKAGE_DISPLAY "${PROJECT_VERSION} EQ Preview ${CHIMERA_BUILD_REVISION}")
endif()
find_package(Python3 3.9 REQUIRED COMPONENTS Interpreter)
option(CHIMERA_BETA_QUALIFICATION_ONLY "Use a marked synthetic expiry for CI; never publish" OFF)
set(CHIMERA_BETA_CONFIGURE_ARGS)
if(CHIMERA_BETA_QUALIFICATION_ONLY)
  list(APPEND CHIMERA_BETA_CONFIGURE_ARGS --qualification-only)
endif()
execute_process(COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/Tools/beta_lifecycle.py"
  --root "${CMAKE_CURRENT_SOURCE_DIR}" --source-sha "${CHIMERA_SOURCE_SHA}" --configure
  ${CHIMERA_BETA_CONFIGURE_ARGS}
  --json "${CMAKE_BINARY_DIR}/chimera-beta-lifecycle.json"
  --header "${CMAKE_BINARY_DIR}/generated/ChimeraBetaLifecycleBuild.h"
  COMMAND_ERROR_IS_FATAL ANY)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/Validation/beta-lifecycle-policy.json"
  "${CMAKE_CURRENT_SOURCE_DIR}/Tools/beta_lifecycle.py")
configure_file(cmake/ChimeraBuildVersion.h.in generated/ChimeraBuildVersion.h @ONLY)
file(SHA256 "${CMAKE_BINARY_DIR}/chimera-beta-lifecycle.json" CHIMERA_BETA_METADATA_SHA256)
configure_file(cmake/chimera-build-version.json.in chimera-build-version.json @ONLY)
include_directories("${CMAKE_BINARY_DIR}/generated")
# Keep source identity in the generated header shared by product and consumers.
# A commit must not invalidate every unchanged JUCE translation unit.
target_sources(ChimeraAmpMatrix PRIVATE Source/CabinetSelector.h Source/CabPanel.h Source/MicrophoneCatalog.h Source/ReleaseSupport.cpp Source/ReleaseSupport.h Source/ReleaseInfo.h)
target_sources(ChimeraAmpMatrix PRIVATE Source/IRLibrary.cpp Source/IRLibrary.h Source/Amplifier.h Source/Cabinet.h Source/GlobalDSP.h Source/PolyPitch.h Source/FXChain.h Source/LowCompressor.h Source/StudioModules.h Source/PerformanceUtilities.h Source/FXParameters.h Source/ModelCatalog.h Source/SpatialModules.h Source/IRMetadata.h Source/IRReferenceCatalog.h Source/HardwareArtwork.h Source/RasterArtwork.h Source/IRCollection.h Source/IRBrowserPanel.h Source/PluginProcessor.cpp Source/PluginProcessor.h Source/PluginEditor.cpp Source/PluginEditor.h Source/ChimeraDSP.h Source/MatrixTone.h)
set(CHIMERA_USE_CURL 0)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  set(CHIMERA_USE_CURL 1)
  find_package(CURL REQUIRED)
  target_link_libraries(ChimeraAmpMatrix PRIVATE CURL::libcurl)
endif()
target_compile_definitions(ChimeraAmpMatrix PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=${CHIMERA_USE_CURL} JUCE_LOAD_CURL_SYMBOLS_LAZILY=0 JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(ChimeraAmpMatrix PRIVATE ChimeraIRs ChimeraArtwork ChimeraManual juce::juce_cryptography juce::juce_audio_utils juce::juce_dsp PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
# Standalone window (Issue #113): maximise/F11 fit the editor to the display.
# Only the Standalone target uses the custom application; plug-ins and tests
# keep JUCE's wrappers.
if(TARGET ChimeraAmpMatrix_Standalone)
  target_sources(ChimeraAmpMatrix_Standalone PRIVATE Source/ChimeraStandaloneApp.cpp)
  target_compile_definitions(ChimeraAmpMatrix_Standalone PRIVATE JUCE_USE_CUSTOM_PLUGIN_STANDALONE_APP=1)
endif()
# Windows Standalone ASIO (Issue #113). JUCE does not bundle Steinberg's ASIO
# SDK; using it requires accepting Steinberg's licence, so the SDK is never
# vendored here. Point CHIMERA_ASIO_SDK_DIR at an extracted SDK (the folder
# containing common/iasiodrv.h). Hosts (DAWs) provide their own ASIO either way.
set(CHIMERA_ASIO_SDK_DIR "$ENV{CHIMERA_ASIO_SDK_DIR}" CACHE PATH "Steinberg ASIO SDK root for the Windows Standalone")
option(CHIMERA_REQUIRE_ASIO "Fail a Windows configure when the ASIO SDK is missing" OFF)
set(CHIMERA_ASIO_ENABLED 0)
if(WIN32)
  if(CHIMERA_ASIO_SDK_DIR AND EXISTS "${CHIMERA_ASIO_SDK_DIR}/common/iasiodrv.h")
    set(CHIMERA_ASIO_ENABLED 1)
    # PUBLIC: every target linking the product compiles its own JUCE modules.
    target_include_directories(ChimeraAmpMatrix PUBLIC "${CHIMERA_ASIO_SDK_DIR}/common")
    target_compile_definitions(ChimeraAmpMatrix PUBLIC JUCE_ASIO=1)
    message(STATUS "Chimera: Windows Standalone ASIO enabled from ${CHIMERA_ASIO_SDK_DIR}")
  elseif(CHIMERA_REQUIRE_ASIO)
    message(FATAL_ERROR "CHIMERA_REQUIRE_ASIO is ON but CHIMERA_ASIO_SDK_DIR does not contain common/iasiodrv.h")
  else()
    message(STATUS "Chimera: Windows Standalone ASIO disabled (set CHIMERA_ASIO_SDK_DIR); WASAPI/DirectSound remain available")
  endif()
endif()
enable_testing()
juce_add_console_app(ChimeraStandaloneAudioDevicesTests PRODUCT_NAME "Chimera Standalone Audio Device Tests")
target_sources(ChimeraStandaloneAudioDevicesTests PRIVATE Tests/StandaloneAudioDevicesTests.cpp)
target_include_directories(ChimeraStandaloneAudioDevicesTests PRIVATE Source)
target_compile_definitions(ChimeraStandaloneAudioDevicesTests PRIVATE JUCE_USE_CURL=0)
target_link_libraries(ChimeraStandaloneAudioDevicesTests PRIVATE juce::juce_audio_devices)
add_test(NAME ChimeraStandaloneAudioDevicesTests COMMAND ChimeraStandaloneAudioDevicesTests)
set_tests_properties(ChimeraStandaloneAudioDevicesTests PROPERTIES TIMEOUT 120)
if(CHIMERA_ASIO_ENABLED)
  juce_add_console_app(ChimeraAsioAvailabilityTests PRODUCT_NAME "Chimera ASIO Availability Tests")
  target_sources(ChimeraAsioAvailabilityTests PRIVATE Tests/AsioAvailabilityTests.cpp)
  target_link_libraries(ChimeraAsioAvailabilityTests PRIVATE ChimeraAmpMatrix ChimeraArtwork ChimeraManual
    juce::juce_cryptography juce::juce_audio_utils juce::juce_dsp)
  add_test(NAME ChimeraAsioAvailabilityTests COMMAND ChimeraAsioAvailabilityTests)
endif()
add_executable(ChimeraHostBypassTests Tests/HostBypassTests.cpp)
target_include_directories(ChimeraHostBypassTests PRIVATE Source)
add_test(NAME ChimeraHostBypassTests COMMAND ChimeraHostBypassTests)
add_test(NAME ChimeraAAXPackagingTests
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/Tests/test_aax_native.py")
if(CHIMERA_BUILD_AAX)
  juce_add_console_app(ChimeraAAXContractTests PRODUCT_NAME "Chimera AAX Contract Tests")
  target_sources(ChimeraAAXContractTests PRIVATE Tests/AAXContractTests.cpp)
  target_link_libraries(ChimeraAAXContractTests PRIVATE ChimeraAmpMatrix ChimeraArtwork ChimeraManual
    juce::juce_cryptography juce::juce_audio_utils juce::juce_dsp)
  add_test(NAME ChimeraAAXContractTests COMMAND ChimeraAAXContractTests)
  set_tests_properties(ChimeraAAXContractTests PROPERTIES TIMEOUT 180)
endif()

# Serverless beta lifecycle policy. This target is deliberately independent of
# JUCE and the audio graph so injected-clock and active-instance latch behavior
# can be validated without networking, a host, or the machine clock.
add_executable(ChimeraBetaLifecycleTests Tests/BetaLifecycleTests.cpp)
target_include_directories(ChimeraBetaLifecycleTests PRIVATE Source)
add_test(NAME ChimeraBetaLifecycleTests COMMAND ChimeraBetaLifecycleTests)

# Link the real processor, serializer, editor and update panel. Inject only a
# private test constructor clock; shipped factories always use compiled policy.
juce_add_console_app(ChimeraBetaLifecycleProcessorTests PRODUCT_NAME "Chimera Beta Lifecycle Processor Tests")
target_sources(ChimeraBetaLifecycleProcessorTests PRIVATE Tests/BetaLifecycleProcessorTests.cpp)
target_include_directories(ChimeraBetaLifecycleProcessorTests PRIVATE Source)
target_link_libraries(ChimeraBetaLifecycleProcessorTests PRIVATE ChimeraAmpMatrix ChimeraArtwork ChimeraManual juce::juce_cryptography juce::juce_audio_utils juce::juce_dsp)
add_test(NAME ChimeraBetaLifecycleProcessorTests COMMAND ChimeraBetaLifecycleProcessorTests "${CMAKE_BINARY_DIR}/beta-lifecycle-evidence")
set_tests_properties(ChimeraBetaLifecycleProcessorTests PROPERTIES TIMEOUT 180)

# 1.2 Original engine: isolated core tests alongside the production native route.
add_executable(ChimeraOriginalAmpTests Tests/OriginalAmpTests.cpp)
target_include_directories(ChimeraOriginalAmpTests PRIVATE Source)
add_test(NAME ChimeraOriginalAmpTests COMMAND ChimeraOriginalAmpTests)
juce_add_console_app(ChimeraOriginalAmpIntegrationTests PRODUCT_NAME "Chimera Original Amp Tests")
target_sources(ChimeraOriginalAmpIntegrationTests PRIVATE Tests/OriginalAmpIntegrationTests.cpp)
target_include_directories(ChimeraOriginalAmpIntegrationTests PRIVATE Source)
target_compile_definitions(ChimeraOriginalAmpIntegrationTests PRIVATE JUCE_USE_CURL=0 JUCE_WEB_BROWSER=0)
target_link_libraries(ChimeraOriginalAmpIntegrationTests PRIVATE juce::juce_audio_utils juce::juce_dsp PUBLIC juce::juce_recommended_config_flags)
add_test(NAME ChimeraOriginalAmpIntegrationTests COMMAND ChimeraOriginalAmpIntegrationTests)

# Niflheimr development core and real native-route evidence. These technical
# gates do not replace instrument-DI/Cab listening or authorize publication.
add_executable(ChimeraNiflheimrCoreTests Tests/NiflheimrCoreTests.cpp)
target_include_directories(ChimeraNiflheimrCoreTests PRIVATE Source)
add_test(NAME ChimeraNiflheimrCoreTests COMMAND ChimeraNiflheimrCoreTests)
juce_add_console_app(ChimeraNiflheimrIntegrationTests PRODUCT_NAME "Chimera Niflheimr Integration Tests")
target_sources(ChimeraNiflheimrIntegrationTests PRIVATE Tests/NiflheimrIntegrationTests.cpp)
target_include_directories(ChimeraNiflheimrIntegrationTests PRIVATE Source)
target_compile_definitions(ChimeraNiflheimrIntegrationTests PRIVATE JUCE_USE_CURL=0 JUCE_WEB_BROWSER=0)
target_link_libraries(ChimeraNiflheimrIntegrationTests PRIVATE juce::juce_audio_processors juce::juce_dsp PUBLIC juce::juce_recommended_config_flags)
add_test(NAME ChimeraNiflheimrIntegrationTests COMMAND ChimeraNiflheimrIntegrationTests)
juce_add_console_app(ChimeraNiflheimrRender PRODUCT_NAME "Chimera Niflheimr Render")
target_sources(ChimeraNiflheimrRender PRIVATE Tests/RenderNiflheimr.cpp)
target_include_directories(ChimeraNiflheimrRender PRIVATE Source)
target_compile_definitions(ChimeraNiflheimrRender PRIVATE JUCE_USE_CURL=0 JUCE_WEB_BROWSER=0)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
  Source/NiflheimrDSP.h Source/NiflheimrDefinition.h Source/Amplifier.h)
file(SHA256 "${CMAKE_CURRENT_SOURCE_DIR}/Source/NiflheimrDSP.h" CHIMERA_NIFLHEIMR_CORE_SHA256)
file(SHA256 "${CMAKE_CURRENT_SOURCE_DIR}/Source/NiflheimrDefinition.h" CHIMERA_NIFLHEIMR_DEFINITION_SHA256)
file(SHA256 "${CMAKE_CURRENT_SOURCE_DIR}/Source/Amplifier.h" CHIMERA_NIFLHEIMR_WRAPPER_SHA256)
# Measurement provenance also covers the native dispatcher and all source headers.
file(GLOB CHIMERA_NIFLHEIMR_HEADERS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Source/*.h")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${CHIMERA_NIFLHEIMR_HEADERS} Tests/RenderNiflheimr.cpp)
list(SORT CHIMERA_NIFLHEIMR_HEADERS CASE SENSITIVE)
set(CHIMERA_NIFLHEIMR_HEADER_DIGESTS "")
foreach(header IN LISTS CHIMERA_NIFLHEIMR_HEADERS)
  file(SHA256 "${header}" digest)
  get_filename_component(name "${header}" NAME)
  string(APPEND CHIMERA_NIFLHEIMR_HEADER_DIGESTS "${name}:${digest}\n")
endforeach()
string(SHA256 CHIMERA_NIFLHEIMR_HEADER_SET_SHA256 "${CHIMERA_NIFLHEIMR_HEADER_DIGESTS}")
file(SHA256 "${CMAKE_CURRENT_SOURCE_DIR}/Tests/RenderNiflheimr.cpp" CHIMERA_NIFLHEIMR_RENDERER_SHA256)
configure_file(cmake/NiflheimrMeasurementBuild.h.in generated/NiflheimrMeasurementBuild.h @ONLY)
target_compile_definitions(ChimeraNiflheimrRender PRIVATE CHIMERA_MEASUREMENT_CONFIG="$<CONFIG>")
# Bind the renderer translation unit only: changing a voicing must not rebuild
# every JUCE module solely because a source fingerprint changed.
set_property(SOURCE Tests/RenderNiflheimr.cpp APPEND PROPERTY COMPILE_DEFINITIONS
  CHIMERA_NIFLHEIMR_GIT_HEAD="${CHIMERA_SOURCE_SHA}"
  CHIMERA_NIFLHEIMR_CORE_SHA256="${CHIMERA_NIFLHEIMR_CORE_SHA256}"
  CHIMERA_NIFLHEIMR_DEFINITION_SHA256="${CHIMERA_NIFLHEIMR_DEFINITION_SHA256}"
  CHIMERA_NIFLHEIMR_WRAPPER_SHA256="${CHIMERA_NIFLHEIMR_WRAPPER_SHA256}")
target_link_libraries(ChimeraNiflheimrRender PRIVATE juce::juce_dsp juce::juce_audio_formats juce::juce_cryptography PUBLIC juce::juce_recommended_config_flags)

juce_add_console_app(ChimeraOriginalAmpRender PRODUCT_NAME "Chimera Original Amp Render")
target_sources(ChimeraOriginalAmpRender PRIVATE Tests/RenderOriginalAmp.cpp)
target_include_directories(ChimeraOriginalAmpRender PRIVATE Source)
target_compile_definitions(ChimeraOriginalAmpRender PRIVATE JUCE_USE_CURL=0 JUCE_WEB_BROWSER=0)
target_link_libraries(ChimeraOriginalAmpRender PRIVATE juce::juce_dsp juce::juce_audio_formats PUBLIC juce::juce_recommended_config_flags)

# Compile the actual upstream/patched VBlankThread class with a deterministic
# failing DXGI output. This runs on every platform without a physical display.
find_package(Python3 3.9 REQUIRED COMPONENTS Interpreter)
add_test(NAME ChimeraBetaLifecycleToolTests
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/Tools/test_beta_lifecycle.py")
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
  target_compile_definitions(ChimeraAmpMatrix PUBLIC JUCE_MODAL_LOOPS_PERMITTED=1 CHIMERA_ROUTING_DIAGNOSTICS=1)
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
  add_test(NAME ChimeraPrecisionTunerUITests COMMAND ChimeraUITests "${CMAKE_BINARY_DIR}/precision-tuner-snapshots" --precision-tuner-only)
  add_test(NAME ChimeraEasyModeUITests COMMAND ChimeraUITests "${CMAKE_BINARY_DIR}/easy-mode-snapshots" --easy-mode-only)
  add_test(NAME ChimeraCommonDynamicsUITests COMMAND ChimeraUITests "${CMAKE_BINARY_DIR}/common-dynamics-snapshots" --common-dynamics-only)
  add_test(NAME ChimeraNativeDynamicsUITests COMMAND ChimeraUITests "${CMAKE_BINARY_DIR}/native-dynamics-snapshots" --native-dynamics-only)
  add_test(NAME ChimeraNativeDynamicsQueueUITests COMMAND ChimeraUITests "${CMAKE_BINARY_DIR}/native-dynamics-queue-snapshots" --native-dynamics-queue-only)
  add_test(NAME ChimeraHeadCabLowUITests COMMAND ChimeraUITests "${CMAKE_BINARY_DIR}/head-cab-low-snapshots" --review-fixes-only)
  add_test(NAME ChimeraSignalPathUITests COMMAND ChimeraUITests "${CMAKE_BINARY_DIR}/signal-path-snapshots" --signal-path-only)
  add_test(NAME ChimeraSmallScreenUITests COMMAND ChimeraUITests "${CMAKE_BINARY_DIR}/small-screen-snapshots" --small-screen-only)
  # 27 native UI cases also exercise compressor gestures and full state recall.
  set_tests_properties(ChimeraSmallScreenUITests PROPERTIES TIMEOUT 900 RUN_SERIAL TRUE)
  add_test(NAME ChimeraNativeStateTests COMMAND ChimeraUITests --native-state-only "${CMAKE_BINARY_DIR}/native-state")
endif()

# The production Linux build runs this even when optional UI probes are off.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  juce_add_console_app(ChimeraNativeFocusTests PRODUCT_NAME "Chimera Native Focus Tests")
  target_sources(ChimeraNativeFocusTests PRIVATE Tests/NativeFocusTests.cpp)
  target_compile_definitions(ChimeraNativeFocusTests PRIVATE JUCE_MODAL_LOOPS_PERMITTED=1 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
  target_link_libraries(ChimeraNativeFocusTests PRIVATE juce::juce_gui_basics)
  add_test(NAME ChimeraNativeFocusTests COMMAND ChimeraNativeFocusTests)
  set_tests_properties(ChimeraNativeFocusTests PROPERTIES TIMEOUT 30 RUN_SERIAL TRUE)
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
add_test(NAME ChimeraNativeRendererTests
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/Tests/test_native_renderer.py" "$<TARGET_FILE:ChimeraRender>")
add_test(NAME ChimeraOriginalRendererTests
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/Tests/test_original_renderer.py" "$<TARGET_FILE:ChimeraOriginalAmpRender>")
add_test(NAME ChimeraNiflheimrRendererTests
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/Tests/test_niflheimr_renderer.py" "$<TARGET_FILE:ChimeraNiflheimrRender>")

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

# Removing an imported entry must not change the audio embedded in a session.
juce_add_console_app(ChimeraIRLibraryTests PRODUCT_NAME "Chimera IR Library Tests")
target_sources(ChimeraIRLibraryTests PRIVATE Tests/IRLibraryTests.cpp Source/IRLibrary.cpp)
target_include_directories(ChimeraIRLibraryTests PRIVATE Source)
target_compile_definitions(ChimeraIRLibraryTests PRIVATE JUCE_USE_CURL=0 JUCE_WEB_BROWSER=0)
target_link_libraries(ChimeraIRLibraryTests PRIVATE ChimeraIRs juce::juce_data_structures juce::juce_dsp juce::juce_audio_formats juce::juce_cryptography PUBLIC juce::juce_recommended_config_flags)
add_test(NAME ChimeraIRLibraryTests COMMAND ChimeraIRLibraryTests)

juce_add_console_app(ChimeraCabPanelTests PRODUCT_NAME "Chimera CAB Panel Tests")
target_sources(ChimeraCabPanelTests PRIVATE Tests/CabPanelTests.cpp Source/IRLibrary.cpp)
target_include_directories(ChimeraCabPanelTests PRIVATE Source)
target_compile_definitions(ChimeraCabPanelTests PRIVATE JUCE_USE_CURL=0 JUCE_WEB_BROWSER=0)
target_link_libraries(ChimeraCabPanelTests PRIVATE ChimeraIRs juce::juce_data_structures juce::juce_dsp juce::juce_audio_formats PUBLIC juce::juce_recommended_config_flags)
add_test(NAME ChimeraCabPanelTests COMMAND ChimeraCabPanelTests)

juce_add_console_app(ChimeraCabPanelStateTests PRODUCT_NAME "Chimera CAB Panel State Tests")
target_sources(ChimeraCabPanelStateTests PRIVATE Tests/CabPanelStateTests.cpp)
target_include_directories(ChimeraCabPanelStateTests PRIVATE Source)
target_compile_definitions(ChimeraCabPanelStateTests PRIVATE JUCE_MODAL_LOOPS_PERMITTED=1)
target_link_libraries(ChimeraCabPanelStateTests PRIVATE ChimeraAmpMatrix ChimeraArtwork ChimeraManual juce::juce_cryptography juce::juce_audio_utils juce::juce_dsp)
add_test(NAME ChimeraCabPanelStateTests COMMAND ChimeraCabPanelStateTests "${CMAKE_BINARY_DIR}/cab-panel.png")
add_test(NAME ChimeraCabPanelRoutingTests COMMAND ChimeraCabPanelStateTests --classic-room-regression)
add_test(NAME ChimeraCabPanelIRIsolationTests COMMAND ChimeraCabPanelStateTests --ir-isolation-regression)



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
# GateProcessorTests includes EQPendingInputTests, which pumps native messages.
# This target must also work with CHIMERA_UI_PROBES=OFF (Graphical EQ/Open Beta).
target_compile_definitions(ChimeraGateProcessorTests PRIVATE JUCE_MODAL_LOOPS_PERMITTED=1)
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
add_test(NAME ChimeraNiflheimrPresetTests COMMAND ChimeraIntegratedProcessorTests --niflheimr-presets "${CMAKE_BINARY_DIR}/niflheimr-preset-evidence")

juce_add_console_app(ChimeraPitchRegressionTests PRODUCT_NAME "Chimera Pitch Regression Tests")
target_sources(ChimeraPitchRegressionTests PRIVATE Tests/PitchRegressionTests.cpp)
target_include_directories(ChimeraPitchRegressionTests PRIVATE Source)
target_compile_definitions(ChimeraPitchRegressionTests PRIVATE JUCE_USE_CURL=0 JUCE_WEB_BROWSER=0)
target_link_libraries(ChimeraPitchRegressionTests PRIVATE juce::juce_dsp juce::juce_audio_basics PUBLIC juce::juce_recommended_config_flags)
add_test(NAME ChimeraPitchRegressionTests COMMAND ChimeraPitchRegressionTests)

# Optional measurement contracts: normal product builds do not need NumPy.
option(CHIMERA_ENABLE_NIFLHEIMR_MEASUREMENTS "Enable NumPy measurement harness tests" OFF)
if(CHIMERA_ENABLE_NIFLHEIMR_MEASUREMENTS)
  execute_process(COMMAND "${Python3_EXECUTABLE}" -c "import numpy"
    RESULT_VARIABLE CHIMERA_NUMPY_RESULT)
  if(NOT CHIMERA_NUMPY_RESULT EQUAL 0)
    message(FATAL_ERROR "Niflheimr measurements require NumPy in the selected Python interpreter")
  endif()
  add_test(NAME ChimeraNiflheimrMeasurementTests
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/Tests/test_niflheimr_measurements.py" "$<TARGET_FILE:ChimeraNiflheimrRender>")
  set_tests_properties(ChimeraNiflheimrMeasurementTests PROPERTIES TIMEOUT 300)
endif()

# Controlled DI audition through the production processor and common cabinet.
juce_add_console_app(ChimeraNiflheimrRigRender PRODUCT_NAME "Chimera Niflheimr Rig Render")
target_sources(ChimeraNiflheimrRigRender PRIVATE Tests/RenderNiflheimrRig.cpp)
target_include_directories(ChimeraNiflheimrRigRender PRIVATE Source)
target_compile_definitions(ChimeraNiflheimrRigRender PRIVATE CHIMERA_MEASUREMENT_CONFIG="$<CONFIG>" CHIMERA_NIFLHEIMR_GIT_HEAD="${CHIMERA_SOURCE_SHA}")
target_link_libraries(ChimeraNiflheimrRigRender PRIVATE ChimeraAmpMatrix ChimeraArtwork ChimeraManual juce::juce_cryptography juce::juce_audio_utils juce::juce_dsp)
file(SHA256 "${CMAKE_CURRENT_SOURCE_DIR}/Tests/RenderNiflheimrRig.cpp" CHIMERA_NIFLHEIMR_RIG_RENDERER_SHA256)
file(SHA256 "${CMAKE_CURRENT_SOURCE_DIR}/Source/PluginProcessor.cpp" CHIMERA_NIFLHEIMR_PROCESSOR_SHA256)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS Tests/RenderNiflheimrRig.cpp Source/PluginProcessor.cpp)
target_compile_definitions(ChimeraNiflheimrRigRender PRIVATE
  CHIMERA_NIFLHEIMR_RIG_RENDERER_SHA256="${CHIMERA_NIFLHEIMR_RIG_RENDERER_SHA256}"
  CHIMERA_NIFLHEIMR_PROCESSOR_SHA256="${CHIMERA_NIFLHEIMR_PROCESSOR_SHA256}")
if(CHIMERA_ENABLE_NIFLHEIMR_MEASUREMENTS)
  add_test(NAME ChimeraNiflheimrAuditionTests COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/Tests/test_niflheimr_audition.py")
  add_test(NAME ChimeraNiflheimrAuditionRouteTests COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/Tests/test_niflheimr_audition.py" "$<TARGET_FILE:ChimeraNiflheimrRender>" "$<TARGET_FILE:ChimeraNiflheimrRigRender>" "${CMAKE_BINARY_DIR}/audition-route-evidence")
  set_tests_properties(ChimeraNiflheimrAuditionRouteTests PROPERTIES TIMEOUT 300)
endif()

# Original linear cabinet model; no commercial IR inputs in these contracts.
add_executable(ChimeraCabExpansionModelTests Tests/CabExpansionModelTests.cpp)
target_include_directories(ChimeraCabExpansionModelTests PRIVATE Source)
add_test(NAME ChimeraCabExpansionModelTests COMMAND ChimeraCabExpansionModelTests)
add_executable(ChimeraOriginalCabModelTests Tests/OriginalCabModelTests.cpp)
target_include_directories(ChimeraOriginalCabModelTests PRIVATE Source)
add_test(NAME ChimeraOriginalCabModelTests COMMAND ChimeraOriginalCabModelTests)
add_executable(ChimeraCabTimingDiagnosticsTests Tests/CabTimingDiagnosticsTests.cpp)
add_test(NAME ChimeraCabTimingDiagnosticsTests COMMAND ChimeraCabTimingDiagnosticsTests)
juce_add_console_app(ChimeraOriginalCabIntegrationTests PRODUCT_NAME "Chimera Original CAB Tests")
target_sources(ChimeraOriginalCabIntegrationTests PRIVATE Tests/OriginalCabIntegrationTests.cpp Source/IRLibrary.cpp)
target_include_directories(ChimeraOriginalCabIntegrationTests PRIVATE Source)
target_compile_definitions(ChimeraOriginalCabIntegrationTests PRIVATE JUCE_USE_CURL=0 JUCE_WEB_BROWSER=0)
target_link_libraries(ChimeraOriginalCabIntegrationTests PRIVATE ChimeraIRs juce::juce_data_structures juce::juce_dsp juce::juce_audio_formats PUBLIC juce::juce_recommended_config_flags)
add_test(NAME ChimeraOriginalCabIntegrationTests COMMAND ChimeraOriginalCabIntegrationTests)

# Optional stage clocks use a separate executable. The shipped plugin and all
# deadline acceptance suites keep the hooks compiled out.
juce_add_console_app(ChimeraCabProfiling PRODUCT_NAME "Chimera CAB Stage Profiling")
set_target_properties(ChimeraCabProfiling PROPERTIES EXCLUDE_FROM_ALL TRUE)
target_sources(ChimeraCabProfiling PRIVATE Tests/OriginalCabIntegrationTests.cpp Source/IRLibrary.cpp)
target_include_directories(ChimeraCabProfiling PRIVATE Source)
target_compile_definitions(ChimeraCabProfiling PRIVATE JUCE_USE_CURL=0 JUCE_WEB_BROWSER=0 CHIMERA_CAB_PROFILE=1)
target_link_libraries(ChimeraCabProfiling PRIVATE ChimeraIRs juce::juce_data_structures juce::juce_dsp juce::juce_audio_formats PUBLIC juce::juce_recommended_config_flags)

juce_add_console_app(ChimeraOriginalCabRealtimeTests PRODUCT_NAME "Chimera CAB Realtime Regression Tests")
target_sources(ChimeraOriginalCabRealtimeTests PRIVATE Tests/CabRealtimeRegressionTests.cpp)
target_include_directories(ChimeraOriginalCabRealtimeTests PRIVATE Source)
target_compile_definitions(ChimeraOriginalCabRealtimeTests PRIVATE JUCE_USE_CURL=0 JUCE_WEB_BROWSER=0)
target_link_libraries(ChimeraOriginalCabRealtimeTests PRIVATE juce::juce_dsp PUBLIC juce::juce_recommended_config_flags)
add_test(NAME ChimeraOriginalCabRealtimeTests COMMAND ChimeraOriginalCabRealtimeTests)

add_test(NAME ChimeraCabExpansionIntegrationTests COMMAND ChimeraOriginalCabIntegrationTests --expanded)
add_test(NAME ChimeraCabExtendedTimingTests COMMAND ChimeraOriginalCabIntegrationTests --expanded --extended-buffers)
add_test(NAME ChimeraCabExpansionStateTests COMMAND ChimeraCabPanelStateTests --expansion-only)

add_executable(ChimeraCabLayoutModelTests Tests/CabLayoutModelTests.cpp)
target_include_directories(ChimeraCabLayoutModelTests PRIVATE Source)
add_test(NAME ChimeraCabLayoutModelTests COMMAND ChimeraCabLayoutModelTests)
add_executable(ChimeraCabCameraFramingTests Tests/CabCameraFramingTests.cpp)
target_include_directories(ChimeraCabCameraFramingTests PRIVATE Source)
target_compile_features(ChimeraCabCameraFramingTests PRIVATE cxx_std_20)
add_test(NAME ChimeraCabCameraFramingTests COMMAND ChimeraCabCameraFramingTests)
add_test(NAME ChimeraCabLayoutIntegrationTests COMMAND ChimeraOriginalCabIntegrationTests --layouts)
add_test(NAME ChimeraCabLayoutStateTests COMMAND ChimeraCabPanelStateTests --layout-only)

# CAB deadline measurements must not inherit opt-in lifecycle file logging.
# Keep the candidate's shutdown/UI traces, but measure these three callbacks
# under the same untraced conditions as the product and dedicated CAB jobs.
# RUN_SERIAL also prevents future CTest parallelism from adding competing tests;
# the IR worker and forced six-slot swaps inside each test remain enabled.
set(CHIMERA_CAB_TIMING_TESTS
  ChimeraOriginalCabIntegrationTests
  ChimeraCabExpansionIntegrationTests
  ChimeraCabExtendedTimingTests
  ChimeraCabLayoutIntegrationTests)
set_tests_properties(${CHIMERA_CAB_TIMING_TESTS} PROPERTIES RUN_SERIAL TRUE)
set_property(TEST ${CHIMERA_CAB_TIMING_TESTS} APPEND PROPERTY
  ENVIRONMENT_MODIFICATION "CHIMERA_LIFECYCLE_TRACE=unset:")

add_test(NAME ChimeraCabDriverVisualTests COMMAND ChimeraCabPanelStateTests --visual-only)
# This large render/capture batch can exceed the candidate's default 1200 s.
# Bound it separately without changing any real-time audio assertions.
set_tests_properties(ChimeraCabDriverVisualTests PROPERTIES TIMEOUT 1800)

# EQ and dynamics contracts do not publish release artifacts.
foreach(eqtest GraphicalEQ GraphicalEQUI EQAnalyzer GraphicalEQEditing EQSnapshotFreeze CommonDynamics MultibandDynamics AutoSet)
  juce_add_console_app(Chimera${eqtest}Tests PRODUCT_NAME "Chimera ${eqtest} Tests")
  target_sources(Chimera${eqtest}Tests PRIVATE Tests/${eqtest}Tests.cpp)
  target_include_directories(Chimera${eqtest}Tests PRIVATE Source)
  target_compile_definitions(Chimera${eqtest}Tests PRIVATE JUCE_MODAL_LOOPS_PERMITTED=1)
  target_link_libraries(Chimera${eqtest}Tests PRIVATE ChimeraAmpMatrix ChimeraArtwork ChimeraManual juce::juce_cryptography juce::juce_audio_utils juce::juce_dsp)
endforeach()
add_test(NAME ChimeraGraphicalEQTests COMMAND ChimeraGraphicalEQTests "${CMAKE_BINARY_DIR}/eq-cpu.csv")
add_test(NAME ChimeraGraphicalEQUITests COMMAND ChimeraGraphicalEQUITests "${CMAKE_BINARY_DIR}")
add_test(NAME ChimeraEQAnalyzerTests COMMAND ChimeraEQAnalyzerTests "${CMAKE_BINARY_DIR}")
add_test(NAME ChimeraGraphicalEQEditingTests COMMAND ChimeraGraphicalEQEditingTests)
add_test(NAME ChimeraCommonDynamicsTests COMMAND ChimeraCommonDynamicsTests)
add_test(NAME ChimeraMultibandDynamicsTests COMMAND ChimeraMultibandDynamicsTests)
add_test(NAME ChimeraAutoSetTests COMMAND ChimeraAutoSetTests)
add_executable(ChimeraMultibandDynamicsCoreTests Tests/MultibandDynamicsCoreTests.cpp)
target_include_directories(ChimeraMultibandDynamicsCoreTests PRIVATE Source)
target_compile_features(ChimeraMultibandDynamicsCoreTests PRIVATE cxx_std_20)
add_test(NAME ChimeraMultibandDynamicsCoreTests COMMAND ChimeraMultibandDynamicsCoreTests)
set_tests_properties(ChimeraMultibandDynamicsTests ChimeraMultibandDynamicsCoreTests PROPERTIES TIMEOUT 180 RUN_SERIAL TRUE)
add_test(NAME ChimeraEQSnapshotFreezeTests COMMAND ChimeraEQSnapshotFreezeTests)
set_tests_properties(ChimeraGraphicalEQEditingTests ChimeraEQSnapshotFreezeTests PROPERTIES TIMEOUT 180 RUN_SERIAL TRUE)
set_tests_properties(ChimeraGraphicalEQTests ChimeraGraphicalEQUITests ChimeraEQAnalyzerTests PROPERTIES TIMEOUT 180 RUN_SERIAL TRUE)
# Six 10-second native high-rate observations replace the former single 2.6 s
# probe. Accommodate the added measured duration without changing assertions.
set_tests_properties(ChimeraGraphicalEQUITests PROPERTIES TIMEOUT 240)
# Required temporal coverage is also run by ChimeraEQAnalyzerTests. Keep this
# focused target convenient for investigation, with the same detection gates.
add_custom_target(ChimeraEQTemporalCoverageProbe
  COMMAND $<TARGET_FILE:ChimeraEQAnalyzerTests> --temporal-coverage-regression "${CMAKE_BINARY_DIR}"
  DEPENDS ChimeraEQAnalyzerTests
  COMMENT "Verify EQ transient coverage and update-partition invariance")

# Same-executable baseline/candidate FFT cost and numerical equivalence. This
# analysis-only target needs no processor, editor, artwork or audio device.
juce_add_console_app(ChimeraEQAnalyzerPerformanceTests PRODUCT_NAME "Chimera EQ Analyzer Performance Tests")
target_sources(ChimeraEQAnalyzerPerformanceTests PRIVATE Tests/EQAnalyzerPerformanceTests.cpp)
target_include_directories(ChimeraEQAnalyzerPerformanceTests PRIVATE Source)
target_compile_definitions(ChimeraEQAnalyzerPerformanceTests PRIVATE JUCE_USE_CURL=0 JUCE_WEB_BROWSER=0)
target_link_libraries(ChimeraEQAnalyzerPerformanceTests PRIVATE juce::juce_dsp juce::juce_graphics PUBLIC juce::juce_recommended_config_flags)
add_test(NAME ChimeraEQAnalyzerPerformanceTests COMMAND ChimeraEQAnalyzerPerformanceTests "${CMAKE_BINARY_DIR}")
set_tests_properties(ChimeraEQAnalyzerPerformanceTests PROPERTIES TIMEOUT 180 RUN_SERIAL TRUE)

# Normal-thread producer/UI attribution is observational. Its functional gates
# verify accounting, not a DAW/device real-time performance qualification.
add_test(NAME ChimeraEQHighRateAttributionTests
  COMMAND ChimeraGraphicalEQUITests --attribution "${CMAKE_BINARY_DIR}/eq-attribution")
set_tests_properties(ChimeraEQHighRateAttributionTests PROPERTIES TIMEOUT 360 RUN_SERIAL TRUE)

# Console UI probes pump AppKit explicitly. Tahoe delays events when the legacy
# JUCE nested dispatcher waits for a future NSEvent deadline (JUCE issue #1574).
# Keep this adapter out of all product/plug-in targets and preserve JUCE 8.0.8.
if(APPLE)
  enable_language(OBJCXX)
  foreach(ui_test ChimeraUITests ChimeraCabPanelStateTests ChimeraGraphicalEQUITests ChimeraGateProcessorTests)
    if(TARGET ${ui_test})
      target_sources(${ui_test} PRIVATE Tests/NativeMessageLoop.mm)
      set_property(TARGET ${ui_test} PROPERTY OBJCXX_STANDARD 20)
    endif()
  endforeach()
endif()

juce_add_console_app(ChimeraAmpHeadControlTests PRODUCT_NAME "Chimera Amp Head Control Tests")
target_sources(ChimeraAmpHeadControlTests PRIVATE Tests/AmpHeadControlTests.cpp)
target_include_directories(ChimeraAmpHeadControlTests PRIVATE Source)
target_compile_definitions(ChimeraAmpHeadControlTests PRIVATE JUCE_MODAL_LOOPS_PERMITTED=1)
target_link_libraries(ChimeraAmpHeadControlTests PRIVATE ChimeraAmpMatrix ChimeraArtwork ChimeraManual juce::juce_cryptography juce::juce_audio_utils juce::juce_dsp)
if(APPLE)
  target_sources(ChimeraAmpHeadControlTests PRIVATE Tests/NativeMessageLoop.mm)
  set_property(TARGET ChimeraAmpHeadControlTests PROPERTY OBJCXX_STANDARD 20)
endif()
add_test(NAME ChimeraAmpHeadControlTests COMMAND ChimeraAmpHeadControlTests "${CMAKE_BINARY_DIR}/amp-head-snapshots")
set_tests_properties(ChimeraAmpHeadControlTests PROPERTIES TIMEOUT 300 RUN_SERIAL TRUE)

# Precision estimator/profile contracts use the same header as the product worker.
add_executable(ChimeraPrecisionTunerTests Tests/PrecisionTunerTests.cpp)
target_include_directories(ChimeraPrecisionTunerTests PRIVATE Source)
add_test(NAME ChimeraPrecisionTunerTests COMMAND ChimeraPrecisionTunerTests)

# Source-frozen baseline versus production tuner acquisition and lifecycle gates.
juce_add_console_app(ChimeraPrecisionTunerIntegrationTests PRODUCT_NAME "Chimera Tuner Integration Tests")
target_sources(ChimeraPrecisionTunerIntegrationTests PRIVATE Tests/PrecisionTunerIntegrationTests.cpp)
target_include_directories(ChimeraPrecisionTunerIntegrationTests PRIVATE Source)
target_compile_definitions(ChimeraPrecisionTunerIntegrationTests PRIVATE JUCE_USE_CURL=0 JUCE_WEB_BROWSER=0 CHIMERA_TUNER_TEST_HOOKS=1)
target_link_libraries(ChimeraPrecisionTunerIntegrationTests PRIVATE juce::juce_dsp PUBLIC juce::juce_recommended_config_flags)
add_test(NAME ChimeraPrecisionTunerIntegrationTests COMMAND ChimeraPrecisionTunerIntegrationTests)
set_tests_properties(ChimeraPrecisionTunerIntegrationTests PROPERTIES TIMEOUT 120)

# Test-only timing attribution; product targets never enable tuner test hooks.
add_executable(ChimeraTunerTimingTests Tests/TunerTimingTests.cpp)
target_include_directories(ChimeraTunerTimingTests PRIVATE Source)
add_test(NAME ChimeraTunerTimingTests COMMAND ChimeraTunerTimingTests)
add_test(NAME ChimeraTunerWorkerTimingTests COMMAND ChimeraPrecisionTunerIntegrationTests --timing-only)
set_tests_properties(ChimeraTunerWorkerTimingTests PROPERTIES TIMEOUT 10)

# BEGIN CHIMERA PRESET MAINTENANCE EXPORTER
# The existing Product build and full CTest matrix produce and verify this pack.
# build.yml already retains build/validation on Windows, Linux and macOS.
juce_add_console_app(ChimeraFactoryPresetExport PRODUCT_NAME "Chimera Factory Preset Export")
target_sources(ChimeraFactoryPresetExport PRIVATE Tools/ExportFactoryPresetPack.cpp)
target_include_directories(ChimeraFactoryPresetExport PRIVATE Source)
target_link_libraries(ChimeraFactoryPresetExport PRIVATE ChimeraAmpMatrix ChimeraArtwork ChimeraManual juce::juce_cryptography juce::juce_audio_utils juce::juce_dsp)
add_test(NAME ChimeraFactoryPresetExport COMMAND ChimeraFactoryPresetExport "${CMAKE_BINARY_DIR}/validation/preset-maintenance")
set_tests_properties(ChimeraFactoryPresetExport PROPERTIES TIMEOUT 300 RUN_SERIAL TRUE)
# END CHIMERA PRESET MAINTENANCE EXPORTER
