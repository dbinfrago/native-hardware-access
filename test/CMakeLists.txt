# SPDX-FileCopyrightText: Copyright DB InfraGO AG
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.25)

include(FetchContent)

# Allow CI to override the GoogleTest source URL (e.g. Artifactory remote mirror).
set(GOOGLETEST_URL_DEFAULT "https://github.com/google/googletest/releases/download/v1.16.0/googletest-1.16.0.tar.gz")
if(DEFINED ENV{GOOGLETEST_URL} AND NOT "$ENV{GOOGLETEST_URL}" STREQUAL "")
  set(GOOGLETEST_URL "$ENV{GOOGLETEST_URL}")
else()
  set(GOOGLETEST_URL "${GOOGLETEST_URL_DEFAULT}")
endif()

FetchContent_Declare(
  googletest
  URL ${GOOGLETEST_URL}
)
FetchContent_MakeAvailable(googletest)

enable_testing()

set(TEST_SOURCES
  nha_test_case.cpp
  nha_ok_tests.cpp
  nha_malformed_tests.cpp
  nha_unsupported_tests.cpp
  nha_performance_tests.cpp
)

add_executable(
  nha_tests
  ${TEST_SOURCES}
)

set(CONF_FILE_PATH "${CMAKE_SOURCE_DIR}/resources/nha.conf.default")
set(NHA_RAW_PATH "$<TARGET_FILE:nha>")
set(NHAC_RAW_PATH "$<TARGET_FILE:nhac>")

string(REPLACE
    "__PATH__"
    "${NHA_RAW_PATH}"
    NHA_DEFINITION
    "NHA_BIN_PATH=\"__PATH__\""
)

string(REPLACE
    "__PATH__"
    "${NHAC_RAW_PATH}"
    NHAC_DEFINITION
    "NHAC_BIN_PATH=\"__PATH__\""
)

string(REPLACE
    "__PATH__"
    "${CONF_FILE_PATH}"
    CONF_DEFINITION
    "NHA_CONF_PATH=\"__PATH__\""
)

# pass location to code
target_compile_definitions(nha_tests PRIVATE
  ${NHA_DEFINITION}
  ${NHAC_DEFINITION}
  ${CONF_DEFINITION}
)

target_link_libraries(
  nha_tests
  GTest::gtest_main
)
add_dependencies(nha_tests nha nhac)

include(GoogleTest)
gtest_discover_tests(nha_tests)
add_test(NAME NHA_TESTS
         COMMAND nha_tests)
