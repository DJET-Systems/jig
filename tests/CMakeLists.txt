# SPDX-License-Identifier: AGPL-3.0-only
add_executable(jig-tests unit_tests.cpp)
target_link_libraries(jig-tests PRIVATE jig-core jig-ui jig-env jig-build jig-init jig-warnings)
add_test(NAME unit COMMAND jig-tests)

# End-to-end: build a real project with the built `jig` binary (needs a C++ compiler and cmake on PATH).
find_program(JIG_BASH bash)
if(JIG_BASH)
  add_test(NAME e2e COMMAND ${JIG_BASH} ${CMAKE_CURRENT_SOURCE_DIR}/e2e.sh $<TARGET_FILE:jig>)
endif()
