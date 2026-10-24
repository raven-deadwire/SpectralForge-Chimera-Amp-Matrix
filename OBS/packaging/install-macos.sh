#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-or-later
# Installs (or with --uninstall removes) the SpectralForge Chimera OBS filter
# for the current user, where OBS Studio looks for user plugins on macOS.
set -eu
here=$(cd "$(dirname "$0")" && pwd)
plugins="$HOME/Library/Application Support/obs-studio/plugins"
target="$plugins/chimera-obs.plugin"

# OBS keeps the module loaded; replacing or removing it under OBS is unsafe.
if pgrep -x OBS >/dev/null 2>&1 || pgrep -x obs >/dev/null 2>&1; then
    echo "OBS Studio is running. Quit OBS, then run this again. Nothing was changed." >&2
    exit 1
fi

if [ "${1:-}" = "--uninstall" ]; then
    rm -rf "$target"
    echo "Removed the Chimera OBS filter from $plugins"
    exit 0
fi
if [ -n "${1:-}" ]; then
    echo "usage: $0 [--uninstall]" >&2
    exit 2
fi

mkdir -p "$plugins"
rm -rf "$target"
ditto "$here/chimera-obs.plugin" "$target"
# A downloaded package is quarantined; clear the flag on the installed copy only.
xattr -dr com.apple.quarantine "$target" 2>/dev/null || true
echo "Installed the Chimera OBS filter to $target"
echo "Start OBS and add Filters > SpectralForge Chimera to an audio source."
