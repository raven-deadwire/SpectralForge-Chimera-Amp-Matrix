// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 RavenForge Luthier Intelligence
#pragma once
#include <string>

namespace spectralforge::obs {
class ChimeraHost;
class EditorWindow;

// A top-level native window hosting Chimera's own VST3 editor. OBS UI thread
// only. Windows (HWND) first; other platforms report unsupported until their
// run-loop integration is implemented (Issue #131 stage F).
bool editorSupported() noexcept;
// Opens a new window, or raises the existing one. Returns false with a reason.
bool openEditor(EditorWindow*& window, ChimeraHost& host, const std::string& title, std::string& error);
// Detaches the view from the plugin and destroys the window. Safe on nullptr.
void closeEditor(EditorWindow*& window) noexcept;
}
