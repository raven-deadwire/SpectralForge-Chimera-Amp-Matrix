#!/bin/sh
# Guard BOTH components before Apple Installer replaces any of their payload.
# No Python dependency, process termination, or user-data changes.
set -eu
for process in OBS obs; do
    status=0
    /usr/bin/pgrep -x "$process" >/dev/null 2>&1 || status=$?
    case "$status" in
        1) ;; # pgrep: no matching process
        0) echo "Chimera: close OBS Studio and keep it closed until installation finishes." >&2; exit 1 ;;
        *) echo "Chimera: cannot determine OBS process state; installation blocked." >&2; exit 1 ;;
    esac
done
here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
# Only the mandatory component carries the document maintenance executable.
if [ -x "$here/document-maintenance" ]; then
    exec "$here/document-maintenance" prepare-upgrade "${3:-/}/Library/Application Support/SpectralForge/Chimera/Documentation"
fi
