# AAX Native is a development-only, opt-in format. No SDK download is needed:
# JUCE 8.0.8 initializes its bundled AAX SDK when an AAX target is requested.
option(CHIMERA_BUILD_AAX "Build unsigned AAX Native for Pro Tools Developer" OFF)
if(CHIMERA_BUILD_AAX)
  if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    if(NOT MSVC OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8
       OR CMAKE_GENERATOR_PLATFORM MATCHES "[Aa][Rr][Mm]"
       OR CMAKE_SYSTEM_PROCESSOR MATCHES "[Aa][Rr][Mm]|aarch64")
      message(FATAL_ERROR "Chimera AAX Native requires Windows x64 with MSVC (-A x64).")
    endif()
  elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set(_chimera_aax_archs ${CMAKE_OSX_ARCHITECTURES})
    list(SORT _chimera_aax_archs)
    if(NOT "${_chimera_aax_archs}" STREQUAL "arm64;x86_64")
      message(FATAL_ERROR "Chimera AAX Native requires -DCMAKE_OSX_ARCHITECTURES=arm64;x86_64 (Universal).")
    endif()
  else()
    message(FATAL_ERROR "Chimera AAX Native supports only Windows x64 and macOS Universal; leave CHIMERA_BUILD_AAX=OFF here.")
  endif()
endif()
