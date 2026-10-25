#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-or-later
set -eu
here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
command -v python3 >/dev/null 2>&1 || { echo "Python 3.9+ is required. Nothing was changed." >&2; exit 1; }
exec python3 "$here/install_module.py" --platform macos "$@"
