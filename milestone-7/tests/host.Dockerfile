# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
# Required Notice: Copyright (c) 2026 nanaisu - NBT-AAD (https://github.com/Sq00ky/Supra-iDrive6-Android-Auto)
#
# The host test suite (`make -C tests check`) needs Linux g++, OpenSSL headers and python3. This image has them, for
# running the suite from Windows or macOS with Docker:
#
#   docker build -t nbt-host-test -f tests/host.Dockerfile tests
#   docker run --rm --security-opt seccomp=unconfined -v "$PWD:/w" -w /w nbt-host-test make -C tests check
#
# --security-opt seccomp=unconfined: the TSAN target runs `setarch -R` (the personality syscall), which Docker's default
# seccomp profile denies ("failed to set personality"). Without it every test still runs; only the TSAN run fails.
# protoc is not installed, so test_schema.py is skipped (add protobuf-compiler to the apt line to run it).
FROM debian:bookworm-slim
RUN apt-get update \
 && apt-get install -y --no-install-recommends build-essential libssl-dev python3 util-linux \
 && rm -rf /var/lib/apt/lists/*
WORKDIR /w
