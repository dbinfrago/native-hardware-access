# SPDX-FileCopyrightText: Copyright DB InfraGO AG
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.25)

project(NativeHardwareAccess)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Determine version from git describe
execute_process(
    COMMAND git describe --always --dirty
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_VERSION
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)
if(NOT GIT_VERSION)
    set(GIT_VERSION "unknown")
endif()

set(BINARY nha)

set(SOURCES
    src/main.cpp
    src/responder.cpp
    src/messagebuilder.cpp
    src/tpmwatcher.cpp
    src/monotonictimer.cpp
    src/configurationparser.cpp
    src/timer.cpp
    src/configurationparser.h
    src/messagebuilder.h
    src/monotonictimer.h
    src/responder.h
    src/timer.h
    src/tpmwatcher.h
)

find_package(OpenSSL REQUIRED)
find_package(PkgConfig REQUIRED)
# Note: PkgConfig variable names are capitalized (SYSTEMD)
pkg_check_modules(SYSTEMD REQUIRED libsystemd)
pkg_check_modules(TSS2_ESYS REQUIRED tss2-esys)
pkg_check_modules(TSS2_RC REQUIRED tss2-rc)
pkg_check_modules(TSS2_TCTILDR REQUIRED tss2-tctildr)

add_executable(${BINARY} ${SOURCES})
target_compile_definitions(${BINARY} PRIVATE TPM_POSIX NHA_VERSION="${GIT_VERSION}")
target_link_libraries(${BINARY} ${TSS2_ESYS_LIBRARIES} ${TSS2_RC_LIBRARIES} ${TSS2_TCTILDR_LIBRARIES} ${SYSTEMD_LIBRARIES} OpenSSL::SSL OpenSSL::Crypto)

# Client tool for testing
# picohash.h must be placed in third_party/picohash/ before building.
# CI downloads it automatically; for local builds see README.md.
set(PICOHASH_HEADER "${CMAKE_SOURCE_DIR}/third_party/picohash/picohash.h")
if(NOT EXISTS "${PICOHASH_HEADER}")
    message(FATAL_ERROR
        "Missing ${PICOHASH_HEADER}\n"
        "Download it with:\n"
        "  mkdir -p third_party/picohash\n"
        "  curl -sL -o third_party/picohash/picohash.h \\\n"
        "    https://raw.githubusercontent.com/sethcall/picohash/refs/heads/master/picohash.h")
endif()
add_library(picohash INTERFACE)
target_include_directories(picohash INTERFACE ${CMAKE_SOURCE_DIR}/third_party/picohash)

set(TEST_TOOL_BINARY nhac)
set(TEST_TOOL_SOURCES
     tools/src/nhac.cpp
)
add_executable (${TEST_TOOL_BINARY} ${TEST_TOOL_SOURCES})
target_link_libraries(${TEST_TOOL_BINARY} PRIVATE picohash)

# guard to skip tests when building for Debian package
if(NOT BUILD_DEB_PACKAGE)
    add_subdirectory(test)
endif()
