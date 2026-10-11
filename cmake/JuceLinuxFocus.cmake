# JUCE 8.0.8 (d6181bde38) can restore a stale child on Linux FocusIn after
# grabKeyboardFocus has already assigned an explicit target in that peer.
# Preserve that target as the restore choice, then run the original native
# focus notifications. No deferred request or recurring focus grab is added.
# Only the audited upstream file or this exact patch is accepted (LF/CRLF).

function(chimera_patch_juce_linux_focus)
  if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
    return()
  endif()
  set(source "${JUCE_SOURCE_DIR}/modules/juce_gui_basics/windows/juce_ComponentPeer.cpp")
  if(NOT EXISTS "${source}")
    message(FATAL_ERROR "JUCE 8.0.8 ComponentPeer source is missing: ${source}")
  endif()
  set(original_hash "23dcbdbc3c36aa41f2abf5f2ba125db21c748ec073350715eac47b1a2d9e2c28")
  set(patched_hash "17ae1b67aafe8b80c0e8fe67804baf15911ed0c5cca0b6063694b97c746df64a")
  set(original [=[void ComponentPeer::handleFocusGain()
{
    if (component.isParentOf (lastFocusedComponent)]=])
  set(patched [=[void ComponentPeer::handleFocusGain()
{
   #if JUCE_LINUX
    // Native FocusIn can follow an explicit cross-peer focus assignment.
    // Preserve its current target while retaining normal gain notifications.
    if (auto* current = Component::getCurrentlyFocusedComponent();
        current != nullptr
        && component.isParentOf (current)
        && current->getPeer() == this
        && current->isShowing()
        && current->getWantsKeyboardFocus())
    {
        lastFocusedComponent = current;
    }
   #endif

    if (component.isParentOf (lastFocusedComponent)]=])
  file(READ "${source}" source_bytes)
  string(REPLACE "\r\n" "\n" source_lf "${source_bytes}")
  string(SHA256 source_hash "${source_lf}")
  if(source_hash STREQUAL original_hash)
    set(original_source "${source_lf}")
    string(REPLACE "${original}" "${patched}" patched_source "${source_lf}")
  elseif(source_hash STREQUAL patched_hash)
    set(patched_source "${source_lf}")
    string(REPLACE "${patched}" "${original}" original_source "${source_lf}")
  else()
    message(FATAL_ERROR
      "Unexpected JUCE ComponentPeer source SHA256 ${source_hash}. "
      "Re-audit the Linux native focus patch before changing the pinned source.")
  endif()
  string(SHA256 verified_original_hash "${original_source}")
  string(SHA256 verified_patched_hash "${patched_source}")
  if(NOT verified_original_hash STREQUAL original_hash OR
     NOT verified_patched_hash STREQUAL patched_hash)
    message(FATAL_ERROR "JUCE Linux focus patch did not match the audited source")
  endif()
  file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/chimera-juce-patches")
  file(WRITE "${CMAKE_BINARY_DIR}/chimera-juce-patches/juce_ComponentPeer.original.cpp" "${original_source}")
  if(source_hash STREQUAL original_hash)
    file(READ "${source}" source_hex HEX)
    string(FIND "${source_hex}" "0d0a" crlf_position)
    if(NOT crlf_position EQUAL -1)
      string(REPLACE "\n" "\r\n" patched_source "${patched_source}")
    endif()
    file(WRITE "${source}" "${patched_source}")
    message(STATUS "Applied the JUCE 8.0.8 Linux native focus restoration fix")
  endif()
endfunction()

chimera_patch_juce_linux_focus()
