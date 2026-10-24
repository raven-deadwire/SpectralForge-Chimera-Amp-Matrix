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
// Called on the UI thread while the editor is open (every 500 ms) and when it
// is hidden, so the owner can copy edited state into the scene settings.
using EditorChanged = void (*)(void* context);

// Opens a new window, or raises the existing one. Returns false with a reason.
bool openEditor(EditorWindow*& window, ChimeraHost& host, const std::string& title, EditorChanged changed,
                void* context, std::string& error);
// Detaches the view from the plugin and destroys the window. Safe on nullptr.
void closeEditor(EditorWindow*& window) noexcept;
}
