# SPDX-License-Identifier: GPL-2.0-or-later
# SpectralForge Chimera OBS filter (Issue #131, structure 2). A standalone
# project: it never links the proprietary product targets of the parent
# repository and hosts the installed Chimera VST3 at runtime instead.
cmake_minimum_required(VERSION 3.22)
project(chimera-obs VERSION 0.1.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
# Export only the OBS entry points; the module's VST3 SDK copy must not
# interpose on the one inside the hosted Chimera plugin.
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)
# Static MSVC runtime, like the product binaries: the installed module needs
# no Visual C++ redistributable. No CRT objects cross the libobs boundary.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

option(CHIMERA_OBS_BUILD_MODULE "Build the OBS module (needs libobs and the VST3 SDK)" ON)
set(CHIMERA_VST3_SDK_DIR "" CACHE PATH "Existing VST3 SDK checkout (empty: fetch the pinned MIT release)")
set(CHIMERA_OBS_TEST_BUNDLE "" CACHE PATH "Chimera .vst3 bundle used by the integration tests")

find_package(Threads REQUIRED)
enable_testing()

if(MSVC)
  set(chimera_obs_warnings /W4 /WX /permissive- /utf-8)
  add_compile_definitions(_CRT_SECURE_NO_WARNINGS NOMINMAX)
else()
  set(chimera_obs_warnings -Wall -Wextra -Werror)
endif()

# Realtime boundary: no libobs/VST3 dependency, builds and tests everywhere.
add_executable(chimera-obs-bridge-tests tests/audio-bridge-tests.cpp)
target_include_directories(chimera-obs-bridge-tests PRIVATE src)
target_compile_options(chimera-obs-bridge-tests PRIVATE ${chimera_obs_warnings})
target_link_libraries(chimera-obs-bridge-tests PRIVATE Threads::Threads)
add_test(NAME chimera-obs-bridge-tests COMMAND chimera-obs-bridge-tests)

add_executable(chimera-obs-base64-tests tests/base64-tests.cpp)
target_include_directories(chimera-obs-base64-tests PRIVATE src)
target_compile_options(chimera-obs-base64-tests PRIVATE ${chimera_obs_warnings})
add_test(NAME chimera-obs-base64-tests COMMAND chimera-obs-base64-tests)

if(NOT CHIMERA_OBS_BUILD_MODULE)
  return()
endif()

if(APPLE)
  message(FATAL_ERROR "macOS module packaging is stage F of the add-on plan and is not implemented yet")
endif()

# Steinberg VST3 SDK 3.8 (MIT). Only the hosting subset is compiled.
if(CHIMERA_VST3_SDK_DIR STREQUAL "")
  include(FetchContent)
  FetchContent_Declare(vst3sdk
    GIT_REPOSITORY https://github.com/steinbergmedia/vst3sdk.git
    GIT_TAG v3.8.0_build_66
    GIT_SHALLOW TRUE
    GIT_SUBMODULES base pluginterfaces public.sdk
    SOURCE_SUBDIR do-not-configure) # sources only; the SDK's own CMake is not used
  FetchContent_MakeAvailable(vst3sdk)
  set(CHIMERA_VST3_SDK_DIR "${vst3sdk_SOURCE_DIR}")
endif()
set(sdk "${CHIMERA_VST3_SDK_DIR}")
if(WIN32)
  set(sdk_platform_sources
    "${sdk}/public.sdk/source/vst/hosting/module_win32.cpp"
    "${sdk}/public.sdk/source/common/threadchecker_win32.cpp")
else()
  set(sdk_platform_sources
    "${sdk}/public.sdk/source/vst/hosting/module_linux.cpp"
    "${sdk}/public.sdk/source/common/threadchecker_linux.cpp")
endif()
add_library(chimera-vst3-hosting STATIC
  ${sdk}/base/source/baseiids.cpp
  ${sdk}/base/source/fbuffer.cpp
  ${sdk}/base/source/fdebug.cpp
  ${sdk}/base/source/fdynlib.cpp
  ${sdk}/base/source/fobject.cpp
  ${sdk}/base/source/fstreamer.cpp
  ${sdk}/base/source/fstring.cpp
  ${sdk}/base/source/timer.cpp
  ${sdk}/base/source/updatehandler.cpp
  ${sdk}/base/thread/source/fcondition.cpp
  ${sdk}/base/thread/source/flock.cpp
  ${sdk}/pluginterfaces/base/conststringtable.cpp
  ${sdk}/pluginterfaces/base/coreiids.cpp
  ${sdk}/pluginterfaces/base/funknown.cpp
  ${sdk}/pluginterfaces/base/ustring.cpp
  ${sdk}/public.sdk/source/common/commoniids.cpp
  ${sdk}/public.sdk/source/common/commonstringconvert.cpp
  ${sdk}/public.sdk/source/common/memorystream.cpp
  ${sdk}/public.sdk/source/vst/hosting/connectionproxy.cpp
  ${sdk}/public.sdk/source/vst/hosting/hostclasses.cpp
  ${sdk}/public.sdk/source/vst/hosting/module.cpp
  ${sdk}/public.sdk/source/vst/hosting/pluginterfacesupport.cpp
  ${sdk}/public.sdk/source/vst/utility/stringconvert.cpp
  ${sdk}/public.sdk/source/vst/vstinitiids.cpp
  ${sdk_platform_sources})
target_include_directories(chimera-vst3-hosting SYSTEM PUBLIC "${sdk}")
target_compile_definitions(chimera-vst3-hosting PUBLIC $<IF:$<CONFIG:Debug>,DEVELOPMENT=1,RELEASE=1>)
if(MSVC)
  target_compile_options(chimera-vst3-hosting PRIVATE /utf-8)
endif()
target_link_libraries(chimera-vst3-hosting PUBLIC Threads::Threads ${CMAKE_DL_LIBS})
if(WIN32)
  target_link_libraries(chimera-vst3-hosting PUBLIC ole32 shell32)
endif()

# Host adapter: VST3 SDK only, so it is integration-tested without OBS.
add_library(chimera-host STATIC src/chimera-host.cpp)
target_include_directories(chimera-host PUBLIC src)
target_compile_options(chimera-host PRIVATE ${chimera_obs_warnings})
target_link_libraries(chimera-host PUBLIC chimera-vst3-hosting)

# Windows has no libobs package: CI passes the release tag's headers and an
# import library generated from that release's obs.dll (tools/dll_exports_def.py).
set(CHIMERA_OBS_LIBOBS_INCLUDE_DIR "" CACHE PATH "libobs headers (obs-studio/libobs at a release tag)")
set(CHIMERA_OBS_LIBOBS_LIBRARY "" CACHE FILEPATH "Import library for that release's obs.dll")
if(NOT CHIMERA_OBS_LIBOBS_INCLUDE_DIR STREQUAL "")
  # obsconfig.h is generated by a full OBS build; plugins only need its
  # release flags. Values match a Windows release layout.
  set(generated "${CMAKE_BINARY_DIR}/libobs-generated")
  if(NOT EXISTS "${CHIMERA_OBS_LIBOBS_INCLUDE_DIR}/obsconfig.h")
    file(WRITE "${generated}/obsconfig.h" "#pragma once\n"
      "#define OBS_DATA_PATH \"../../data\"\n"
      "#define OBS_PLUGIN_PATH \"../../obs-plugins/64bit\"\n"
      "#define OBS_PLUGIN_DESTINATION \"obs-plugins/64bit\"\n"
      "#define OBS_INSTALL_PREFIX \"\"\n"
      "#define OBS_RELEASE_CANDIDATE 0\n"
      "#define OBS_BETA 0\n")
  endif()
  add_library(OBS::libobs UNKNOWN IMPORTED)
  set_target_properties(OBS::libobs PROPERTIES
    IMPORTED_LOCATION "${CHIMERA_OBS_LIBOBS_LIBRARY}"
    INTERFACE_INCLUDE_DIRECTORIES "${CHIMERA_OBS_LIBOBS_INCLUDE_DIR};${generated}")
else()
  find_package(libobs REQUIRED)
endif()
if(WIN32)
  set(editor_source src/editor-window-win32.cpp)
else()
  set(editor_source src/editor-window-stub.cpp)
endif()
add_library(chimera-obs MODULE src/plugin-main.cpp ${editor_source})
target_compile_options(chimera-obs PRIVATE ${chimera_obs_warnings})
target_link_libraries(chimera-obs PRIVATE chimera-host OBS::libobs)
set_target_properties(chimera-obs PROPERTIES PREFIX "")
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  # A MODULE may link with unresolved symbols; OBS would then fail to load it.
  target_link_options(chimera-obs PRIVATE -Wl,--no-undefined)
endif()
if(WIN32)
  target_link_libraries(chimera-obs PRIVATE user32)
endif()
# The module and its locale data, laid out as OBS loads them from a plugin dir.
add_custom_command(TARGET chimera-obs POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/data" "$<TARGET_FILE_DIR:chimera-obs>/data")

if(NOT CHIMERA_OBS_TEST_BUNDLE STREQUAL "")
  add_executable(chimera-obs-host-tests tests/chimera-host-tests.cpp)
  target_compile_options(chimera-obs-host-tests PRIVATE ${chimera_obs_warnings})
  target_link_libraries(chimera-obs-host-tests PRIVATE chimera-host)
  add_test(NAME chimera-obs-host-tests COMMAND chimera-obs-host-tests "${CHIMERA_OBS_TEST_BUNDLE}")

  # Headless libobs: loads the built module the way OBS does and runs audio
  # through a real filter chain on the libobs audio thread.
  add_executable(chimera-obs-filter-tests tests/obs-filter-tests.cpp)
  target_compile_options(chimera-obs-filter-tests PRIVATE ${chimera_obs_warnings})
  target_link_libraries(chimera-obs-filter-tests PRIVATE OBS::libobs chimera-host)
  add_dependencies(chimera-obs-filter-tests chimera-obs)
  add_test(NAME chimera-obs-filter-tests
    COMMAND chimera-obs-filter-tests "$<TARGET_FILE:chimera-obs>" "$<TARGET_FILE_DIR:chimera-obs>/data" "${CHIMERA_OBS_TEST_BUNDLE}")
endif()
