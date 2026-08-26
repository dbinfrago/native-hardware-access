# SPDX-FileCopyrightText: Copyright DB InfraGO AG
# SPDX-License-Identifier: Apache-2.0

FROM debian:trixie-slim

RUN apt-get update -qq && \
    apt-get install -y --no-install-recommends \
        cmake \
        pkg-config \
        build-essential \
        ca-certificates \
        curl \
        libssl-dev \
        libsystemd-dev \
        libtss2-dev \
        debhelper \
    && rm -rf /var/lib/apt/lists/*
