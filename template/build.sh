#!/bin/sh
# Builds bin/hello for the MPC's 64-bit ARM (Live III) in Docker: any arm64 image with gcc and libdrm-dev.
set -e
cd "$(dirname "$0")"
mkdir -p bin
docker run --rm --platform linux/arm64 -v "$PWD:/w" -w /w debian:bookworm sh -c \
  'apt-get update -qq >/dev/null && apt-get install -y -qq gcc libdrm-dev >/dev/null && gcc -O2 -Wall -Wextra -I/usr/include/libdrm -o bin/hello src/hello.c -ldrm'
echo "built bin/hello"
