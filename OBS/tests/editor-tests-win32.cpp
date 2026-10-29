// SPDX-License-Identifier: GPL-2.0-or-later
// The Chimera editor window on Windows against the real Chimera VST3 (plan stage
// F): opens a top-level window with Chimera's HWND view embedded, syncs edits on
// its timer while this thread pumps messages as OBS's Qt UI thread does, hides
// on the close button, shows the same window again, and is torn down by
// closeEditor(). Needs a desktop session (CI's Windows runners provide one).
#include "chimera-host.hpp"
#include "editor-window.hpp"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <chrono>
#include <iostream>
#include <stdexcept>
#include <string>

using namespace spectralforge::obs;

namespace {
void check(bool ok, const char* what) { if (!ok) throw std::runtime_error(what); }
// For calls that report through `error`: evaluated before the message is read.
void require(bool ok, const std::string& what, const std::string& error) {
    if (!ok) throw std::runtime_error(what + ": " + error);
}

int notifications = 0;
void changed(void*) { ++notifications; }

using Clock = std::chrono::steady_clock;
double since(Clock::time_point start) { return std::chrono::duration<double>(Clock::now() - start).count(); }

// Dispatches this thread's messages for the whole interval, like the UI thread's event loop.
void pump(double seconds) {
    const auto start = Clock::now();
    while (since(start) < seconds) {
        MSG msg;
        bool any = false;
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            any = true;
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        if (!any) MsgWaitForMultipleObjects(0, nullptr, FALSE, 5, QS_ALLINPUT);
    }
}

struct Search {
    std::wstring title;
    HWND found = nullptr;
    int matches = 0;
};

// All top-level windows with this title.
int find(const std::wstring& title, HWND& found) {
    Search s{title};
    EnumWindows(
        [](HWND hwnd, LPARAM data) -> BOOL {
            auto* search = reinterpret_cast<Search*>(data);
            wchar_t text[256] = {};
            GetWindowTextW(hwnd, text, 256);
            if (search->title == text) {
                search->found = hwnd;
                ++search->matches;
            }
            return TRUE;
        },
        reinterpret_cast<LPARAM>(&s));
    if (s.found != nullptr) found = s.found;
    return s.matches;
}

void run(const std::string& bundle) {
    check(editorSupported(), "editor supported on Windows");

    ChimeraHost host;
    std::string error;
    const bool loaded = host.load(bundle, error);
    require(loaded, "load Chimera", error);
    check(host.prepare(48000.0, 2, 512), "prepare");

    const std::string title = "Chimera editor test";
    const std::wstring wideTitle = L"Chimera editor test";
    EditorWindow* editor = nullptr;
    const bool opened = openEditor(editor, host, title, changed, nullptr, error);
    require(opened, "open the editor", error);
    check(editor != nullptr, "editor handle");
    pump(0.5);
    HWND window = nullptr;
    check(find(wideTitle, window) == 1 && IsWindowVisible(window), "editor window is shown");
    check(GetWindow(window, GW_CHILD) != nullptr, "Chimera embedded its view");
    RECT client{};
    GetClientRect(window, &client);
    check(client.right - client.left >= 200 && client.bottom - client.top >= 150, "window sized to the editor");

    // The first draw can hold the thread on a CI VM, so wait (bounded) for two syncs.
    notifications = 0;
    const auto started = Clock::now();
    while (notifications < 2 && since(started) < 10.0) pump(0.1);
    const std::string synced = "edits are synced every 500 ms while open (" + std::to_string(notifications) +
                               " in " + std::to_string(since(started)) + " s)";
    check(notifications >= 2, synced.c_str());

    // Opening again shows the same window rather than creating another.
    EditorWindow* same = editor;
    check(openEditor(editor, host, title, changed, nullptr, error) && editor == same, "reopen shows the same editor");
    check(find(wideTitle, window) == 1, "one editor window");

    // The close button only hides; the filter decides when to destroy.
    notifications = 0;
    PostMessageW(window, WM_CLOSE, 0, 0);
    const auto closing = Clock::now();
    while (IsWindowVisible(window) && since(closing) < 5.0) pump(0.05);
    check(!IsWindowVisible(window) && IsWindow(window) && find(wideTitle, window) == 1,
          "close button hides the window");
    check(notifications >= 1, "closing syncs the last edits");
    check(openEditor(editor, host, title, changed, nullptr, error), "hidden editor shown again");
    pump(0.2);
    check(IsWindowVisible(window), "hidden editor visible again");

    notifications = 0;
    closeEditor(editor);
    pump(0.1);
    check(editor == nullptr && notifications == 1, "closeEditor syncs once and releases");
    check(!IsWindow(window) && find(wideTitle, window) == 0, "window removed");

    // Repeated open/close (scene switches) must not leak or crash, and the
    // plug-in keeps working through it.
    for (int i = 0; i < 3; ++i) {
        const bool again = openEditor(editor, host, title, changed, nullptr, error);
        require(again, "open the editor again", error);
        pump(0.3);
        closeEditor(editor);
        pump(0.05);
    }
    closeEditor(editor); // nullptr is safe
    check(find(wideTitle, window) == 0, "no editor windows left");
    pump(0.2);
    host.unload();
}
}

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "usage: chimera-obs-editor-tests <SpectralForge Chimera.vst3>\n";
        return 2;
    }
    try {
        run(argv[1]);
    } catch (const std::exception& e) {
        std::cerr << "FAIL " << e.what() << '\n';
        return 1;
    }
    std::cout << "PASS Windows editor window: HWND embed, 500 ms sync, close hides, reopen shows, "
                 "closeEditor tears down\n";
    return 0;
}
