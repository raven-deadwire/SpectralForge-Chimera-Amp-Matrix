#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-or-later
# Builds the Apple silicon OBS filter bundle (chimera-obs.plugin) against a
# pinned OBS Studio release (plan stage F). macOS has no libobs development
# package: the module links the libobs.framework inside that release's OBS.app,
# with the libobs headers of the same tag. With a Chimera VST3 given, the
# module's tests run against it and that libobs.
# Usage: build-macos-module.sh <work dir> [Chimera.vst3]
# Prints the build directory (containing chimera-obs.plugin) on the last line.
set -eu
obs_version=31.1.2
dmg_sha256=8f18142c8599d4e80baee5913b03f8357c3e5fd0b76835f54e76dedf9f857188
work=$1
bundle=${2:-}
source=$(cd "$(dirname "$0")/.." && pwd)
mkdir -p "$work"
work=$(cd "$work" && pwd)

dmg="$work/OBS-Studio-$obs_version-macOS-Apple.dmg"
if [ ! -f "$dmg" ]; then
    curl -fsSL -o "$dmg" "https://github.com/obsproject/obs-studio/releases/download/$obs_version/OBS-Studio-$obs_version-macOS-Apple.dmg"
fi
actual=$(shasum -a 256 "$dmg" | cut -d' ' -f1)
[ "$actual" = "$dmg_sha256" ] || { echo "OBS $obs_version release hash mismatch: $actual" >&2; exit 1; }
if [ ! -d "$work/OBS.app" ]; then
    mount="$work/obs-image"
    mkdir -p "$mount"
    hdiutil attach -nobrowse -readonly -mountpoint "$mount" "$dmg" >/dev/null
    ditto "$mount/OBS.app" "$work/OBS.app"
    hdiutil detach "$mount" >/dev/null
fi
libobs="$work/OBS.app/Contents/Frameworks/libobs.framework/Versions/A/libobs"
[ -f "$libobs" ] || { echo "libobs.framework not found in OBS.app:" >&2; ls "$work/OBS.app/Contents/Frameworks" >&2; exit 1; }

headers="$work/obs-src"
if [ ! -f "$headers/libobs/obs-module.h" ]; then
    git clone --depth 1 --branch "$obs_version" --filter=blob:none --sparse https://github.com/obsproject/obs-studio "$headers"
    git -C "$headers" sparse-checkout set libobs
fi

build="$work/build"
set -- -S "$source" -B "$build" -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=arm64 \
    -DCHIMERA_OBS_LIBOBS_INCLUDE_DIR="$headers/libobs" -DCHIMERA_OBS_LIBOBS_LIBRARY="$libobs" \
    -DCHIMERA_OBS_SOURCE_SHA="$(git -C "$source" rev-parse HEAD)"
[ -n "$bundle" ] && set -- "$@" -DCHIMERA_OBS_TEST_BUNDLE="$(cd "$(dirname "$bundle")" && pwd)/$(basename "$bundle")"
cmake "$@"
cmake --build "$build" --parallel 3
if [ -n "$bundle" ]; then
    ctest --test-dir "$build" --output-on-failure --no-tests=error
fi
echo "$build"
