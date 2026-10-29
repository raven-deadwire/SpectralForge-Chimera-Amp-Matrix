// SPDX-License-Identifier: GPL-2.0-or-later
// The Chimera editor window on Linux against the real Chimera VST3 (plan stage
// F): opens in an X11 window with Chimera's view embedded, services Chimera
// through the GLib main loop as OBS's Qt does, syncs edits on its timer, hides
// on the window manager's close request, raises the same window again, and is
// torn down by closeEditor(). Run under an X server (CI: xvfb-run).
#include "chimera-host.hpp"
#include "editor-window.hpp"

#include <glib.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>

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

// Iterates the default GLib main context for the whole interval, like Qt's
// event dispatcher on OBS's UI thread.
void pump(double seconds) {
    const auto start = Clock::now();
    while (since(start) < seconds) {
        if (!g_main_context_iteration(nullptr, FALSE)) g_usleep(5000);
    }
}

// The test's own X connection, to find and inspect the editor's top-level window.
struct Probe {
    Display* display = XOpenDisplay(nullptr);
    ~Probe() {
        if (display != nullptr) XCloseDisplay(display);
    }

    std::string name(Window w) const {
        char* n = nullptr;
        std::string result;
        if (XFetchName(display, w, &n) != 0 && n != nullptr) result = n;
        if (n != nullptr) XFree(n);
        return result;
    }

    // All top-level windows with this title (no window manager runs in CI).
    int find(const std::string& title, Window& found) const {
        Window root = 0, parent = 0, *children = nullptr;
        unsigned count = 0;
        int matches = 0;
        XSync(display, False);
        if (XQueryTree(display, DefaultRootWindow(display), &root, &parent, &children, &count) == 0) return 0;
        for (unsigned i = 0; i < count; ++i) {
            if (name(children[i]) == title) {
                found = children[i];
                ++matches;
            }
        }
        if (children != nullptr) XFree(children);
        return matches;
    }

    bool viewable(Window w) const {
        XWindowAttributes a{};
        XSync(display, False);
        return XGetWindowAttributes(display, w, &a) != 0 && a.map_state == IsViewable;
    }

    unsigned childCount(Window w) const {
        Window root = 0, parent = 0, *children = nullptr;
        unsigned count = 0;
        XSync(display, False);
        if (XQueryTree(display, w, &root, &parent, &children, &count) == 0) return 0;
        if (children != nullptr) XFree(children);
        return count;
    }

    void size(Window w, int& width, int& height) const {
        XWindowAttributes a{};
        XGetWindowAttributes(display, w, &a);
        width = a.width;
        height = a.height;
    }

    // What a window manager sends when the user clicks the close button.
    void requestClose(Window w) const {
        XEvent e{};
        e.xclient.type = ClientMessage;
        e.xclient.window = w;
        e.xclient.message_type = XInternAtom(display, "WM_PROTOCOLS", False);
        e.xclient.format = 32;
        e.xclient.data.l[0] = static_cast<long>(XInternAtom(display, "WM_DELETE_WINDOW", False));
        e.xclient.data.l[1] = CurrentTime;
        XSendEvent(display, w, False, NoEventMask, &e);
        XFlush(display);
    }
};

void run(const std::string& bundle) {
    check(editorSupported(), "editor supported on Linux");
    Probe probe;
    check(probe.display != nullptr, "X display for the test (run under xvfb-run)");
    // A desktop's window manager creates these; JUCE only looks them up and
    // fails on a bare X server without them.
    for (const char* atom : {"WM_PROTOCOLS", "WM_TAKE_FOCUS", "WM_DELETE_WINDOW", "_NET_WM_PING", "WM_CHANGE_STATE",
                             "WM_STATE", "_NET_WM_WINDOW_TYPE", "_NET_WM_WINDOW_TYPE_NORMAL", "_NET_WM_STATE",
                             "_NET_WM_STATE_HIDDEN", "_NET_WM_ALLOWED_ACTIONS", "_NET_FRAME_EXTENTS", "_MOTIF_WM_HINTS"})
        XInternAtom(probe.display, atom, False);
    XSync(probe.display, False);

    ChimeraHost host;
    std::string error;
    const bool loaded = host.load(bundle, error);
    require(loaded, "load Chimera", error);
    check(host.prepare(48000.0, 2, 512), "prepare");

    const std::string title = "Chimera editor test";
    EditorWindow* editor = nullptr;
    // Off the GLib owner thread the editor must refuse rather than hang.
    check(!openEditor(editor, host, title, changed, nullptr, error) && editor == nullptr,
          "refuses without the GLib main context");
    // Qt's dispatcher owns the default context on OBS's UI thread.
    check(g_main_context_acquire(nullptr), "acquire the default GLib main context");
    const bool opened = openEditor(editor, host, title, changed, nullptr, error);
    require(opened, "open the editor", error);
    check(editor != nullptr, "editor handle");
    pump(0.5);
    Window window = 0;
    check(probe.find(title, window) == 1 && probe.viewable(window), "editor window is shown");
    check(probe.childCount(window) > 0, "Chimera embedded its view");
    int width = 0, height = 0;
    probe.size(window, width, height);
    check(width >= 200 && height >= 150, "window sized to the editor");

    // The first draw can hold the thread on a CI VM, so wait (bounded) for two syncs.
    notifications = 0;
    const auto started = Clock::now();
    while (notifications < 2 && since(started) < 10.0) pump(0.1);
    const std::string synced = "edits are synced every 500 ms while open (" + std::to_string(notifications) +
                               " in " + std::to_string(since(started)) + " s)";
    check(notifications >= 2, synced.c_str());

    // Opening again raises the same window rather than creating another.
    EditorWindow* same = editor;
    check(openEditor(editor, host, title, changed, nullptr, error) && editor == same, "reopen raises");
    check(probe.find(title, window) == 1, "one editor window");

    // The close button only hides; the filter decides when to destroy.
    notifications = 0;
    probe.requestClose(window);
    const auto closing = Clock::now();
    while (probe.viewable(window) && since(closing) < 5.0) pump(0.05);
    check(!probe.viewable(window) && probe.find(title, window) == 1, "close button hides the window");
    check(notifications >= 1, "closing syncs the last edits");
    check(openEditor(editor, host, title, changed, nullptr, error), "hidden editor shown again");
    pump(0.2);
    check(probe.viewable(window), "hidden editor visible again");

    notifications = 0;
    closeEditor(editor);
    pump(0.1);
    check(editor == nullptr && notifications == 1, "closeEditor syncs once and releases");
    check(probe.find(title, window) == 0, "window removed");

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
    check(probe.find(title, window) == 0, "no editor windows left");
    pump(0.2);
    g_main_context_release(nullptr);
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
    std::cout << "PASS Linux editor window: X11 embed, GLib run loop, 500 ms sync, close hides, reopen raises, "
                 "closeEditor tears down\n";
    return 0;
}
