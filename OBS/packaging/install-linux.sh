#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-or-later
# Installs (or with --uninstall removes) the SpectralForge Chimera OBS filter
# for the current user, where OBS Studio looks for user plugins on Linux.
# Only files from this package are written or removed.
set -eu
here=$(cd "$(dirname "$0")" && pwd)
target="${XDG_CONFIG_HOME:-$HOME/.config}/obs-studio/plugins/chimera-obs"
files="bin/64bit/chimera-obs.so data/locale/en-US.ini data/locale/ko-KR.ini README.txt LICENSE.txt SOURCE.txt"

if [ "$(id -u)" = 0 ]; then
    echo "Run this as the user who runs OBS, not as root." >&2
    exit 1
fi
# OBS keeps the module loaded; replacing or removing it under OBS is unsafe.
if command -v pgrep >/dev/null 2>&1 && pgrep -x obs >/dev/null 2>&1; then
    echo "OBS Studio is running. Close OBS, then run this again. Nothing was changed." >&2
    exit 1
fi

if [ "${1:-}" = "--uninstall" ]; then
    for f in $files; do rm -f "$target/$f"; done
    for d in bin/64bit bin data/locale data ""; do rmdir "$target/$d" 2>/dev/null || true; done
    echo "Removed the Chimera OBS filter from $target"
    exit 0
fi
if [ -n "${1:-}" ]; then
    echo "usage: $0 [--uninstall]" >&2
    exit 2
fi

for f in $files; do
    mkdir -p "$(dirname "$target/$f")"
    cp -f "$here/chimera-obs/$f" "$target/$f"
done
chmod 0755 "$target/bin/64bit/chimera-obs.so"
echo "Installed the Chimera OBS filter to $target"
echo "Start OBS and add Filters > SpectralForge Chimera to an audio source."
