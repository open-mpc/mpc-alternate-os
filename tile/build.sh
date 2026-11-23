#!/bin/sh
# Build launcher-page for the Live III (aarch64) in Docker. Needs modeslot.so from plugin-manager (modeslot/build.sh).
set -eu
cd "$(dirname "$0")"
docker run --rm --platform linux/arm64 -v "$PWD:/w" -w /w arm64v8/gcc:12-bookworm \
    gcc -O2 -Wall -Wno-format-truncation -o launcher-page launcher-page.c
