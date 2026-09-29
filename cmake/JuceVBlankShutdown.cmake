# Local shutdown fix for JUCE 8.0.8 (d6181bde38), deliberately retaining the
# pinned dependency. Upstream juce_VBlank_windows.cpp only observes flagExit
# after WaitForVBlank succeeds. A lost/unavailable output that keeps returning
# failure therefore makes ~VBlankThread's safe, unbounded join wait forever.
# Check the same atomic exit flag before every wait, keeping the inner check
# and stopThread(-1). This cannot interrupt a driver blocked inside the wait.
#
# Only the exact audited file, or this exact one-line patch, is accepted. Both
# LF and CRLF checkouts are supported; unknown source changes fail closed.
# The upstream source and its licence banner remain in the fetched dependency.

function(chimera_patch_juce_vblank)
  set(source "${JUCE_SOURCE_DIR}/modules/juce_gui_basics/native/juce_VBlank_windows.cpp")
  if(NOT EXISTS "${source}")
    message(FATAL_ERROR "JUCE 8.0.8 VBlank source is missing: ${source}")
  endif()

  set(original_hash "43c216f0ff10fa3b0912133e4479954794769d8bc87bf94f5b78c8b0a19ebc89")
  set(patched_hash "c4264615d2c8926df4e5fc35b977959c938977db66780f23696d1af40219901e")
  set(original_loop [=[        for (;;)
        {
            if (output->WaitForVBlank() == S_OK)]=])
  set(patched_loop [=[        while ((state.load() & flagExit) == 0)
        {
            if (output->WaitForVBlank() == S_OK)]=])

  file(READ "${source}" source_bytes)
  string(REPLACE "\r\n" "\n" source_lf "${source_bytes}")
  string(SHA256 source_hash "${source_lf}")

  if(source_hash STREQUAL original_hash)
    set(original_source "${source_lf}")
    string(REPLACE "${original_loop}" "${patched_loop}" patched_source "${source_lf}")
  elseif(source_hash STREQUAL patched_hash)
    set(patched_source "${source_lf}")
    string(REPLACE "${patched_loop}" "${original_loop}" original_source "${source_lf}")
  else()
    message(FATAL_ERROR
      "Unexpected JUCE VBlank source SHA256 ${source_hash}. "
      "Re-audit the shutdown patch before changing the pinned JUCE source.")
  endif()

  # Validate both directions before touching any file. These whole-file hashes
  # also guarantee that the exact replacement matched once and nothing else.
  string(SHA256 verified_original_hash "${original_source}")
  string(SHA256 verified_patched_hash "${patched_source}")
  if(NOT verified_original_hash STREQUAL original_hash OR
     NOT verified_patched_hash STREQUAL patched_hash)
    message(FATAL_ERROR "JUCE VBlank shutdown patch did not match the audited source")
  endif()

  set(original_path "${CMAKE_BINARY_DIR}/chimera-juce-patches/juce_VBlank_windows.original.cpp")
  file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/chimera-juce-patches")
  file(WRITE "${original_path}" "${original_source}")

  if(source_hash STREQUAL original_hash)
    # Preserve the checkout's newline convention and avoid rewriting an
    # already-patched dependency on later CMake runs.
    # file(READ) may normalise newlines, so inspect bytes for this choice.
    file(READ "${source}" source_hex HEX)
    string(FIND "${source_hex}" "0d0a" crlf_position)
    if(NOT crlf_position EQUAL -1)
      string(REPLACE "\n" "\r\n" patched_source "${patched_source}")
    endif()
    file(WRITE "${source}" "${patched_source}")
    message(STATUS "Applied the JUCE 8.0.8 VBlank failure-path shutdown fix")
  endif()

  # Regression probes extract the real class from these two verified files.
  set(CHIMERA_VBLANK_ORIGINAL_SOURCE "${original_path}" PARENT_SCOPE)
  set(CHIMERA_VBLANK_PATCHED_SOURCE "${source}" PARENT_SCOPE)
endfunction()

chimera_patch_juce_vblank()
