# Appended only to the diagnostic checkout of exact source 7c782fda.
if(NOT APPLE)
  message(FATAL_ERROR "The fairness diagnostic requires macOS")
endif()
enable_language(OBJCXX)
set(fairness_dir "${CMAKE_CURRENT_SOURCE_DIR}/diagnostics/macos-pump-fairness")
foreach(variant Original Candidate)
  set(target "ChimeraPumpFairness${variant}")
  juce_add_console_app(${target} PRODUCT_NAME ${target})
  target_sources(${target} PRIVATE "${fairness_dir}/FairnessProbe.mm")
  if(variant STREQUAL "Original")
    target_sources(${target} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/Tests/NativeMessageLoop.mm")
  else()
    target_sources(${target} PRIVATE "${fairness_dir}/candidate/NativeMessageLoop.mm")
  endif()
  target_include_directories(${target} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/Tests")
  target_compile_definitions(${target} PRIVATE JUCE_USE_CURL=0 JUCE_WEB_BROWSER=0
    JUCE_MODAL_LOOPS_PERMITTED=1 FAIRNESS_VARIANT="${variant}")
  target_link_libraries(${target} PRIVATE juce::juce_events juce::juce_core
    juce::juce_recommended_config_flags)
  set_target_properties(${target} PROPERTIES CXX_STANDARD 20 OBJCXX_STANDARD 20)
endforeach()
file(GENERATE OUTPUT "${CMAKE_BINARY_DIR}/fairness-executables-$<CONFIG>.txt"
  CONTENT "$<TARGET_FILE:ChimeraPumpFairnessOriginal>\n$<TARGET_FILE:ChimeraPumpFairnessCandidate>\n")
