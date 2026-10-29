#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-or-later
# postinstall of the optional OBS Studio filter component in the Chimera .pkg.
# The .pkg runs as root, but OBS on macOS loads plugins only from each user's
# ~/Library/Application Support/obs-studio/plugins. Install there for the
# logged-in user with the same native helper as the standalone OBS package:
# it refuses root and a running OBS, verifies hashes and the bundle signature,
# and replaces an earlier copy atomically. The payload stays in $folder so the
# filter can be installed for another user or removed with install.sh.
set -eu
folder="/Library/Application Support/SpectralForge/Chimera/OBS"
if [ "${3:-/}" != "/" ]; then
    echo "Chimera OBS filter: not installing on the startup disk; run $folder/install.sh as the OBS user later."
    exit 0
fi
user=$(/usr/bin/stat -f %Su /dev/console)
case "$user" in
    "" | root | loginwindow | _mbsetupuser)
        echo "Chimera OBS filter: nobody is logged in; run $folder/install.sh as the OBS user later."
        exit 0 ;;
esac
uid=$(/usr/bin/id -u "$user")
case "$uid" in
    "" | *[!0-9]* | 0) echo "Chimera OBS filter: invalid user identity" >&2; exit 1 ;;
esac
record=$(/usr/bin/dscl . -read "/Users/$user" NFSHomeDirectory)
home=$(printf '%s\n' "$record" | /usr/bin/sed -n 's/^NFSHomeDirectory: //p')
case "$home" in
    /*) ;;
    *) echo "Chimera OBS filter: invalid home folder for $user" >&2; exit 1 ;;
esac
[ -d "$home" ] || { echo "Chimera OBS filter: no home folder for $user" >&2; exit 1; }
# Verify the shipped executable before launching it. This checks its code seal,
# not Developer ID identity or notarization.
/usr/bin/codesign --verify --strict "$folder/chimera-obs-installer"
exec /bin/launchctl asuser "$uid" /usr/bin/sudo -u "$user" /usr/bin/env -i HOME="$home" \
    PATH=/usr/bin:/bin:/usr/sbin:/sbin "$folder/chimera-obs-installer"
