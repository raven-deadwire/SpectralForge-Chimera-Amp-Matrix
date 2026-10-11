# Diagnostic candidate only. Never included by the product CMakeLists.txt.
set(peer_file "${JUCE_SOURCE_DIR}/modules/juce_gui_basics/windows/juce_ComponentPeer.cpp")
file(READ "${peer_file}" peer_source)
string(REPLACE "\r\n" "\n" peer_source "${peer_source}")
set(original [=[void ComponentPeer::handleFocusGain()
{
    if (component.isParentOf (lastFocusedComponent)]=])
set(candidate [=[void ComponentPeer::handleFocusGain()
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
string(FIND "${peer_source}" "${original}" match)
if(match EQUAL -1)
  string(FIND "${peer_source}" "${candidate}" already_patched)
  if(already_patched EQUAL -1)
    message(FATAL_ERROR "Unrecognized JUCE focus handler; diagnostic patch not applied")
  endif()
else()
  string(REPLACE "${original}" "${candidate}" peer_source "${peer_source}")
  file(WRITE "${peer_file}" "${peer_source}")
endif()
