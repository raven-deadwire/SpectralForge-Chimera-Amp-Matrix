# Keep the issuance secret out of the CMake cache and diagnostic output.
option(CHIMERA_REQUIRE_TESTER_SECRET "Reject release candidates without a tester secret" OFF)
function(chimera_configure_tester_secret secret destination required)
  set(CHIMERA_TESTER_SECRET_HEX "${secret}")
  string(STRIP "${CHIMERA_TESTER_SECRET_HEX}" CHIMERA_TESTER_SECRET_HEX)
  string(TOLOWER "${CHIMERA_TESTER_SECRET_HEX}" CHIMERA_TESTER_SECRET_HEX)
  if(CHIMERA_TESTER_SECRET_HEX STREQUAL "")
    if(required)
      message(FATAL_ERROR "A tester secret is required for this release candidate")
    endif()
    message(STATUS "Beta tester codes: disabled (no secret configured)")
  else()
    string(LENGTH "${CHIMERA_TESTER_SECRET_HEX}" secret_length)
    math(EXPR odd_length "${secret_length} % 2")
    if(NOT CHIMERA_TESTER_SECRET_HEX MATCHES "^[0-9a-f]+$" OR
       secret_length LESS 32 OR odd_length)
      message(FATAL_ERROR "Tester secret must contain at least 16 complete hexadecimal bytes")
    endif()
    message(STATUS "Beta tester codes: enabled")
  endif()
  configure_file("${CMAKE_CURRENT_FUNCTION_LIST_DIR}/ChimeraTesterSecret.h.in"
    "${destination}/ChimeraTesterSecret.h" @ONLY)
endfunction()

chimera_configure_tester_secret("$ENV{CHIMERA_TESTER_SECRET}"
  "${CMAKE_BINARY_DIR}/generated" "${CHIMERA_REQUIRE_TESTER_SECRET}")
