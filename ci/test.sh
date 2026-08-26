#!/bin/bash

# SPDX-FileCopyrightText: Copyright DB InfraGO AG
# SPDX-License-Identifier: Apache-2.0

set -euo pipefail

BUILD_DIR="${BUILD_DIR:-build}"

echo "=== Running tests ==="
"./${BUILD_DIR}/test/nha_tests" --gtest_output="xml:${BUILD_DIR}/test_results.xml"
