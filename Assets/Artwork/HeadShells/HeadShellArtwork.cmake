# Complete RGBA enclosures. Their roof, corners, hardware and feet are composed
# once in the production image; live controls remain JUCE components.
file(GLOB CHIMERA_HEAD_SHELL_ARTWORK CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_LIST_DIR}/head-shell-[0-9][0-9].png")
list(SORT CHIMERA_HEAD_SHELL_ARTWORK)
list(APPEND CHIMERA_ARTWORK ${CHIMERA_HEAD_SHELL_ARTWORK})
