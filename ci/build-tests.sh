#!/bin/bash

# SPDX-FileCopyrightText: Copyright DB InfraGO AG
# SPDX-License-Identifier: Apache-2.0

set -euo pipefail

# Fixed paths — must match CMake compile_definitions for test binary
SRC_DIR="/tmp/nha-test/src"
BUILD_DIR="/tmp/nha-test/build"
ARTIFACT_DIR="${ARTIFACT_DIR:-.}/tmp/nha-test"

PICOHASH_URL="${PICOHASH_URL:-https://raw.githubusercontent.com/sethcall/picohash/refs/heads/master/picohash.h}"
PICOHASH_SHA256="e8ab833db1470350596d8d766311d6dfddf805af6abc95e51ce3109567167de9"

echo "=== Preparing fixed-path source tree ==="
rm -rf /tmp/nha-test
mkdir -p "${SRC_DIR}" "${BUILD_DIR}"

cp -a . "${SRC_DIR}/"

echo "=== Fetching third-party dependencies ==="
mkdir -p "${SRC_DIR}/third_party/picohash"
curl -sL -o "${SRC_DIR}/third_party/picohash/picohash.h" "${PICOHASH_URL}"
echo "${PICOHASH_SHA256}  ${SRC_DIR}/third_party/picohash/picohash.h" | sha256sum -c -

echo "=== Configuring CMake ==="
cmake -B "${BUILD_DIR}" -S "${SRC_DIR}"

echo "=== Building ==="
cmake --build "${BUILD_DIR}" --parallel "$(nproc)"

echo "=== Packaging test artifact ==="
mkdir -p "${ARTIFACT_DIR}/build/test"
mkdir -p "${ARTIFACT_DIR}/src/resources"

cp "${BUILD_DIR}/nha"            "${ARTIFACT_DIR}/build/nha"
cp "${BUILD_DIR}/nhac"           "${ARTIFACT_DIR}/build/nhac"
cp "${BUILD_DIR}/test/nha_tests" "${ARTIFACT_DIR}/build/test/nha_tests"
cp "${SRC_DIR}/resources/nha.conf.default" "${ARTIFACT_DIR}/src/resources/nha.conf.default"

echo "=== Test artifact ready ==="
