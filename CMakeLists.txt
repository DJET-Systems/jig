# SPDX-License-Identifier: AGPL-3.0-only
cmake_minimum_required(VERSION 3.20)
project(jig VERSION 0.1.0 LANGUAGES CXX)

# jig is written in C++23 (docs/blueprint/001 D-001, D-014).
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(JIG_BUILD_TESTS "Build the unit tests" ON)
option(JIG_WARNINGS_AS_ERRORS "Treat warnings as errors in jig's own code" OFF)
option(JIG_STATIC "Link the C++ runtime statically (Linux)" OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

# The reported version gets a -dev suffix in development trees. Packagers configuring
# from the source tarball stamp JIG_RELEASE_VERSION to match the release exactly
# (docs/reference/versioning.md).
if(DEFINED ENV{JIG_RELEASE_VERSION} AND NOT "$ENV{JIG_RELEASE_VERSION}" STREQUAL "")
  set(JIG_VERSION_STRING "$ENV{JIG_RELEASE_VERSION}")
else()
  set(JIG_VERSION_STRING "${PROJECT_VERSION}-dev")
endif()
add_compile_definitions(JIG_VERSION="${JIG_VERSION_STRING}")

# Warnings apply to jig's own targets only, not to the vendored single headers.
add_library(jig-warnings INTERFACE)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(jig-warnings INTERFACE -Wall -Wextra -Wpedantic -Wshadow)
  if(JIG_WARNINGS_AS_ERRORS)
    target_compile_options(jig-warnings INTERFACE -Werror)
  endif()
endif()

# Vendored dependencies retain their individual licenses; see third_party/*/LICENSE.
add_library(jig-thirdparty-cli11 INTERFACE)
target_include_directories(jig-thirdparty-cli11 SYSTEM INTERFACE ${CMAKE_SOURCE_DIR}/third_party/CLI11)

add_library(jig-thirdparty-toml INTERFACE)
target_include_directories(jig-thirdparty-toml SYSTEM INTERFACE ${CMAKE_SOURCE_DIR}/third_party/tomlplusplus)
# Exceptions off: toml++ reports errors through parse_result and never throws across our boundary.
target_compile_definitions(jig-thirdparty-toml INTERFACE TOML_HEADER_ONLY=0 TOML_EXCEPTIONS=0)

add_library(jig-thirdparty-expected INTERFACE)
target_include_directories(jig-thirdparty-expected SYSTEM INTERFACE ${CMAKE_SOURCE_DIR}/third_party/tl-expected)

# Libraries, named as in docs/blueprint/001 (jig::core, jig::ui, ...).
function(jig_library name)
  cmake_parse_arguments(A "" "" "SOURCES;DEPS" ${ARGN})
  add_library(${name} STATIC ${A_SOURCES})
  add_library(jig::${name} ALIAS ${name})
  target_include_directories(${name} PUBLIC ${CMAKE_SOURCE_DIR}/src)
  target_link_libraries(${name} PUBLIC ${A_DEPS} PRIVATE jig-warnings)
endfunction()

jig_library(jig-core
  SOURCES src/jig-core/version.cpp src/jig-core/sha256.cpp src/jig-core/glob.cpp
          src/jig-core/platform.cpp src/jig-core/process.cpp src/jig-core/tomledit.cpp
          src/jig-core/manifest.cpp src/jig-core/lock.cpp src/jig-core/toml_impl.cpp
  DEPS jig-thirdparty-toml jig-thirdparty-expected)

jig_library(jig-ui SOURCES src/jig-ui/ui.cpp DEPS jig-core)

jig_library(jig-env
  SOURCES src/jig-env/toolchain.cpp src/jig-env/env.cpp src/jig-env/project.cpp
  DEPS jig-core jig-ui)

jig_library(jig-build
  SOURCES src/jig-build/cmakegen.cpp src/jig-build/build.cpp
  DEPS jig-core jig-ui jig-env)

jig_library(jig-init
  SOURCES src/jig-init/init.cpp src/jig-init/templates.cpp
  DEPS jig-core jig-ui jig-env)

add_executable(jig src/jig/main.cpp src/jig/commands.cpp src/jig/doctor.cpp)
target_link_libraries(jig PRIVATE jig-core jig-ui jig-env jig-build jig-init jig-thirdparty-cli11 jig-warnings)
target_compile_definitions(jig PRIVATE
  JIG_BUILD_COMPILER="${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
if(JIG_STATIC AND CMAKE_SYSTEM_NAME STREQUAL "Linux")
  target_link_options(jig PRIVATE -static-libstdc++ -static-libgcc)
endif()

install(TARGETS jig RUNTIME DESTINATION bin)

if(JIG_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
