#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-or-later
set -eu
here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
helper="$here/chimera-obs-installer"
# The user explicitly runs this script from the downloaded package. Verify the
# included helper before clearing its quarantine; do not change other downloads.
/usr/bin/codesign --verify --strict "$helper"
attributes=$(/usr/bin/xattr "$helper")
case "$attributes" in
    *com.apple.quarantine*) /usr/bin/xattr -d com.apple.quarantine "$helper" ;;
esac
exec "$helper" "$@"
