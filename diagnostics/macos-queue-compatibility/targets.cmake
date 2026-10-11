# Included only in the isolated diagnostic checkout of exact source S.
if(NOT APPLE)
  message(FATAL_ERROR "The queue compatibility diagnostic requires macOS")
endif()
enable_language(OBJCXX)
set(compat_dir "${CMAKE_CURRENT_SOURCE_DIR}/diagnostics/macos-queue-compatibility")
foreach(variant Original Candidate)
  set(target "ChimeraQueueCompatibility${variant}")
  juce_add_console_app(${target} PRODUCT_NAME ${target})
  target_sources(${target} PRIVATE "${compat_dir}/QueueCompatibilityProbe.mm")
  if(variant STREQUAL "Original")
    target_sources(${target} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/Tests/NativeMessageLoop.mm")
    set(helper_source "3171605606e224c1ca63fc0f3827e992a19b481f")
  else()
    target_sources(${target} PRIVATE "${compat_dir}/candidate/NativeMessageLoop.mm")
    set(helper_source "c291d7132ba33561db922e7d5a992ab40212345d")
  endif()
  target_include_directories(${target} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/Tests")
  target_compile_definitions(${target} PRIVATE JUCE_USE_CURL=0 JUCE_WEB_BROWSER=0
    JUCE_MODAL_LOOPS_PERMITTED=1 COMPAT_VARIANT="${variant}"
    COMPAT_HELPER_SOURCE_SHA="${helper_source}")
  target_link_libraries(${target} PRIVATE juce::juce_events juce::juce_core
    juce::juce_recommended_config_flags)
  set_target_properties(${target} PROPERTIES CXX_STANDARD 20 OBJCXX_STANDARD 20)
endforeach()
file(GENERATE OUTPUT "${CMAKE_BINARY_DIR}/compatibility-executables-$<CONFIG>.txt"
  CONTENT "$<TARGET_FILE:ChimeraQueueCompatibilityOriginal>\n$<TARGET_FILE:ChimeraQueueCompatibilityCandidate>\n")
