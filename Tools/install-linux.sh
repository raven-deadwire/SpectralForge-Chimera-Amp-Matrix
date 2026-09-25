#!/bin/sh
# Per-user installation from the supplied Linux tarball. Never writes outside HOME.
set -eu
CHIMERA_SOURCE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
CHIMERA_BIN="$HOME/.local/bin"
CHIMERA_VST="$HOME/.vst3"
CHIMERA_SHARE="$HOME/.local/share"
test -x "$CHIMERA_SOURCE/usr/bin/chimera-amp-matrix"
test -d "$CHIMERA_SOURCE/usr/lib/vst3/SpectralForge Chimera.vst3"
if ldd "$CHIMERA_SOURCE/usr/bin/chimera-amp-matrix" | grep -q 'not found'; then
    echo "Missing runtime libraries. Install the packages listed in RUNTIME-DEPENDENCIES.txt first." >&2
    exit 1
fi
mkdir -p "$CHIMERA_BIN" "$CHIMERA_VST/SpectralForge Chimera.vst3" "$CHIMERA_SHARE/applications" "$CHIMERA_SHARE/pixmaps" "$CHIMERA_SHARE/doc/chimera-amp-matrix"
install -m 755 "$CHIMERA_SOURCE/usr/bin/chimera-amp-matrix" "$CHIMERA_BIN/chimera-amp-matrix"
cp -R "$CHIMERA_SOURCE/usr/lib/vst3/SpectralForge Chimera.vst3/." "$CHIMERA_VST/SpectralForge Chimera.vst3/"
cp -R "$CHIMERA_SOURCE/usr/share/doc/chimera-amp-matrix/." "$CHIMERA_SHARE/doc/chimera-amp-matrix/"
install -m 644 "$CHIMERA_SOURCE/usr/share/pixmaps/chimera-amp-matrix.png" "$CHIMERA_SHARE/pixmaps/chimera-amp-matrix.png"
# Quote the absolute launch path so desktop activation also works with spaces in HOME.
CHIMERA_DESKTOP_EXEC=$(printf '%s' "$CHIMERA_BIN/chimera-amp-matrix" | sed 's/\\/\\\\/g; s/"/\\"/g; s/`/\\`/g; s/\$/\\$/g; s/%/%%/g')
while IFS= read -r CHIMERA_DESKTOP_LINE; do
    case "$CHIMERA_DESKTOP_LINE" in
        Exec=*) printf 'Exec="%s"\n' "$CHIMERA_DESKTOP_EXEC" ;;
        *) printf '%s\n' "$CHIMERA_DESKTOP_LINE" ;;
    esac
done < "$CHIMERA_SOURCE/usr/share/applications/chimera-amp-matrix.desktop" > "$CHIMERA_SHARE/applications/chimera-amp-matrix.desktop"
printf '%s\n' "Installed standalone: $CHIMERA_BIN/chimera-amp-matrix" "Installed VST3: $CHIMERA_VST/SpectralForge Chimera.vst3" "Manual: $CHIMERA_SHARE/doc/chimera-amp-matrix/MANUAL.html"
