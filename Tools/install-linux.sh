#!/bin/sh
# Per-user installation from the supplied Linux tarball. Never writes outside HOME.
set -eu
# --with-obs-filter / --without-obs-filter answer the OBS Studio filter question
# up front; otherwise it is asked only when run from a terminal.
CHIMERA_OBS_CHOICE=ask
for CHIMERA_ARG in "$@"; do
    case "$CHIMERA_ARG" in
        --with-obs-filter) CHIMERA_OBS_CHOICE=yes ;;
        --without-obs-filter) CHIMERA_OBS_CHOICE=no ;;
        *) echo "usage: install.sh [--with-obs-filter | --without-obs-filter]" >&2; exit 2 ;;
    esac
done
CHIMERA_SOURCE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
CHIMERA_BIN="$HOME/.local/bin"
CHIMERA_VST="$HOME/.vst3"
CHIMERA_SHARE="$HOME/.local/share"
CHIMERA_DOCS="$CHIMERA_SHARE/doc/chimera-amp-matrix"
CHIMERA_MAINTENANCE="$CHIMERA_SOURCE/usr/libexec/chimera-document-maintenance"
test -x "$CHIMERA_SOURCE/usr/bin/chimera-amp-matrix"
test -x "$CHIMERA_MAINTENANCE"
test -d "$CHIMERA_SOURCE/usr/lib/vst3/SpectralForge Chimera.vst3"
if ldd "$CHIMERA_SOURCE/usr/bin/chimera-amp-matrix" | grep -q 'not found'; then
    echo "Missing runtime libraries. Install the packages listed in RUNTIME-DEPENDENCIES.txt first." >&2
    exit 1
fi
# Pin document source/target directories throughout verification, preservation,
# replacement and retirement. Shell copies must not reopen document paths after
# inspection: a renamed directory or symlink could redirect that later copy.
"$CHIMERA_MAINTENANCE" install-documents "$CHIMERA_SOURCE/usr/share/doc/chimera-amp-matrix" "$CHIMERA_DOCS"
mkdir -p "$CHIMERA_BIN" "$CHIMERA_VST/SpectralForge Chimera.vst3" "$CHIMERA_SHARE/applications" "$CHIMERA_SHARE/pixmaps"
install -m 755 "$CHIMERA_SOURCE/usr/bin/chimera-amp-matrix" "$CHIMERA_BIN/chimera-amp-matrix"
cp -R "$CHIMERA_SOURCE/usr/lib/vst3/SpectralForge Chimera.vst3/." "$CHIMERA_VST/SpectralForge Chimera.vst3/"
install -m 644 "$CHIMERA_SOURCE/usr/share/pixmaps/chimera-amp-matrix.png" "$CHIMERA_SHARE/pixmaps/chimera-amp-matrix.png"
# Quote the absolute launch path so desktop activation also works with spaces in HOME.
CHIMERA_DESKTOP_EXEC=$(printf '%s' "$CHIMERA_BIN/chimera-amp-matrix" | sed 's/\\/\\\\/g; s/"/\\"/g; s/`/\\`/g; s/\$/\\$/g; s/%/%%/g')
while IFS= read -r CHIMERA_DESKTOP_LINE; do
    case "$CHIMERA_DESKTOP_LINE" in
        Exec=*) printf 'Exec="%s"\n' "$CHIMERA_DESKTOP_EXEC" ;;
        *) printf '%s\n' "$CHIMERA_DESKTOP_LINE" ;;
    esac
done < "$CHIMERA_SOURCE/usr/share/applications/chimera-amp-matrix.desktop" > "$CHIMERA_SHARE/applications/chimera-amp-matrix.desktop"
mkdir -p "$HOME/.local/libexec"
install -m 755 "$CHIMERA_MAINTENANCE" "$HOME/.local/libexec/chimera-document-maintenance"
printf '%s\n' "Installed standalone: $CHIMERA_BIN/chimera-amp-matrix" "Installed VST3: $CHIMERA_VST/SpectralForge Chimera.vst3" "Manual: $CHIMERA_SHARE/doc/chimera-amp-matrix/MANUAL.html"
# Optional OBS Studio filter (open-source GPL module for OBS 30 or later). It has
# its own transactional installer that refuses a running OBS; Chimera above is
# already installed whatever happens here.
CHIMERA_OBS_FILTER="$CHIMERA_SOURCE/usr/share/chimera-amp-matrix/obs-filter"
[ -x "$CHIMERA_OBS_FILTER/install.sh" ] || exit 0
if [ "$CHIMERA_OBS_CHOICE" = ask ]; then
    CHIMERA_OBS_PLUGINS="${XDG_CONFIG_HOME:-$HOME/.config}/obs-studio"
    if command -v obs >/dev/null 2>&1 || [ -d "$CHIMERA_OBS_PLUGINS" ]; then CHIMERA_OBS_DEFAULT=Y; else CHIMERA_OBS_DEFAULT=N; fi
    if [ -t 0 ]; then
        printf 'Also install the SpectralForge Chimera filter for OBS Studio 30+? / OBS Studio용 필터도 설치할까요? [%s] ' \
            "$([ "$CHIMERA_OBS_DEFAULT" = Y ] && echo Y/n || echo y/N)"
        read -r CHIMERA_OBS_ANSWER || CHIMERA_OBS_ANSWER=
        case "${CHIMERA_OBS_ANSWER:-$CHIMERA_OBS_DEFAULT}" in [Yy]*) CHIMERA_OBS_CHOICE=yes ;; *) CHIMERA_OBS_CHOICE=no ;; esac
    else
        CHIMERA_OBS_CHOICE=no
    fi
fi
if [ "$CHIMERA_OBS_CHOICE" = yes ]; then
    if ! sh "$CHIMERA_OBS_FILTER/install.sh"; then
        echo "Chimera is installed, but the OBS Studio filter was not. Close OBS and run: sh \"$CHIMERA_OBS_FILTER/install.sh\"" >&2
        exit 1
    fi
else
    echo "OBS Studio filter not installed. To add it later: sh \"$CHIMERA_OBS_FILTER/install.sh\""
fi
