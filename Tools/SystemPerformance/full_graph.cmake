# Optional independent host; reuses the existing production static target.
# Nothing is inserted into shipped plugin callbacks or publication workflows.
option(CHIMERA_BUILD_PERFORMANCE_HOST "Build synthetic full-product graph host" OFF)
if(CHIMERA_BUILD_PERFORMANCE_HOST)
  juce_add_console_app(ChimeraFullGraphHost PRODUCT_NAME "Chimera Full Graph Measurement Host")
  target_sources(ChimeraFullGraphHost PRIVATE Tools/SystemPerformance/FullGraphHost.cpp)
  target_include_directories(ChimeraFullGraphHost PRIVATE Source)
  target_link_libraries(ChimeraFullGraphHost PRIVATE ChimeraAmpMatrix ChimeraArtwork ChimeraManual
    juce::juce_cryptography juce::juce_audio_utils juce::juce_dsp)
  # Hash actual local source inputs as well as HEAD, so a dirty build is never
  # identified by an unchanged Git SHA alone. Reconfigure on any input edit.
  file(GLOB_RECURSE PERF_INPUTS CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/Source/*" "${CMAKE_SOURCE_DIR}/Assets/*"
    "${CMAKE_SOURCE_DIR}/cmake/*" "${CMAKE_SOURCE_DIR}/Tools/SystemPerformance/*.h"
    "${CMAKE_SOURCE_DIR}/Tools/SystemPerformance/*.cpp" "${CMAKE_SOURCE_DIR}/Tools/SystemPerformance/*.py"
    "${CMAKE_SOURCE_DIR}/Tools/SystemPerformance/*.cmake" "${CMAKE_SOURCE_DIR}/Tools/SystemPerformance/*.in")
  list(APPEND PERF_INPUTS "${CMAKE_SOURCE_DIR}/CMakeLists.txt"
    "${CMAKE_SOURCE_DIR}/Validation/beta-lifecycle-policy.json"
    "${CMAKE_SOURCE_DIR}/Tools/beta_lifecycle.py" "${CMAKE_SOURCE_DIR}/Tools/chimera_version.py"
    "${CMAKE_SOURCE_DIR}/docs/MANUAL.html"
    "${CMAKE_SOURCE_DIR}/VERSION" "${CMAKE_SOURCE_DIR}/RELEASE_CHANNEL")
  list(SORT PERF_INPUTS)
  set(PERF_CONTENT "")
  foreach(input IN LISTS PERF_INPUTS)
    if(NOT IS_DIRECTORY "${input}")
      file(SHA256 "${input}" hash)
      file(RELATIVE_PATH relative "${CMAKE_SOURCE_DIR}" "${input}")
      string(APPEND PERF_CONTENT "${relative}:${hash}\n")
      set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${input}")
    endif()
  endforeach()
  file(WRITE "${CMAKE_BINARY_DIR}/full-graph-source-inputs.txt" "${PERF_CONTENT}")
  file(SHA256 "${CMAKE_BINARY_DIR}/full-graph-source-inputs.txt" PERF_HASH)
  configure_file(Tools/SystemPerformance/FullGraphBuild.h.in generated/FullGraphBuild.h @ONLY)
  set_property(SOURCE Tools/SystemPerformance/FullGraphHost.cpp APPEND PROPERTY
    OBJECT_DEPENDS "${CMAKE_BINARY_DIR}/generated/FullGraphBuild.h")
  target_compile_definitions(ChimeraFullGraphHost PRIVATE CHIMERA_PERF_CONFIG="$<CONFIG>")
  # Existing targets also watch relative and cmake/../ paths. Canonicalize
  # these to avoid duplicate Ninja regeneration outputs on Windows.
  get_property(PERF_DEPENDS DIRECTORY PROPERTY CMAKE_CONFIGURE_DEPENDS)
  set(PERF_CANONICAL "")
  foreach(input IN LISTS PERF_DEPENDS)
    get_filename_component(input "${input}" ABSOLUTE BASE_DIR "${CMAKE_SOURCE_DIR}")
    list(APPEND PERF_CANONICAL "${input}")
  endforeach()
  list(REMOVE_DUPLICATES PERF_CANONICAL)
  set_property(DIRECTORY PROPERTY CMAKE_CONFIGURE_DEPENDS "${PERF_CANONICAL}")
endif()
