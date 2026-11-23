#!/bin/sh
# Builds p0probe for the Live III (aarch64, glibc 2.39 on the device; bookworm's 2.36 is older, so it runs).
set -e
cd "$(dirname "$0")"
docker run --rm -v "$PWD":/src -w /src arm64v8/gcc:12-bookworm sh -c \
  'apt-get -qq update >/dev/null && apt-get -qq install -y libasound2-dev libdrm-dev >/dev/null && gcc -O2 -Wall -o p0probe p0probe.c -I/usr/include/libdrm -ldrm -lasound -lm'
file p0probe
