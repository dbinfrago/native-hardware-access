#!/bin/bash

# SPDX-FileCopyrightText: Copyright DB InfraGO AG
# SPDX-License-Identifier: Apache-2.0

set -euo pipefail

BUILD_DIR="${BUILD_DIR:-build}"
PICOHASH_URL="${PICOHASH_URL:-https://raw.githubusercontent.com/sethcall/picohash/refs/heads/master/picohash.h}"
PICOHASH_SHA256="e8ab833db1470350596d8d766311d6dfddf805af6abc95e51ce3109567167de9"

echo "=== Fetching third-party dependencies ==="
mkdir -p third_party/picohash
curl -sL -o third_party/picohash/picohash.h "${PICOHASH_URL}"
echo "${PICOHASH_SHA256}  third_party/picohash/picohash.h" | sha256sum -c -

echo "=== Configuring CMake ==="
cmake -B "${BUILD_DIR}" -S .

echo "=== Building ==="
cmake --build "${BUILD_DIR}" --parallel "$(nproc)"
