// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 RavenForge Luthier Intelligence
#include "editor-window.hpp"

namespace spectralforge::obs {
bool editorSupported() noexcept { return false; }
bool openEditor(EditorWindow*&, ChimeraHost&, const std::string&, std::string& error) {
    error = "the Chimera editor window is not implemented on this platform yet";
    return false;
}
void closeEditor(EditorWindow*& window) noexcept { window = nullptr; }
}
