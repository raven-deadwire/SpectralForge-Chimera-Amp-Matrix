// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 RavenForge Luthier Intelligence
#include "editor-window.hpp"
#include "chimera-host.hpp"

#include "pluginterfaces/base/funknownimpl.h"
#include "pluginterfaces/gui/iplugview.h"

#include <glib-unix.h>
#include <glib.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include <cstdint>
#include <cstring>
#include <list>

using namespace Steinberg;

namespace spectralforge::obs {
namespace {
constexpr guint syncIntervalMs = 500;
}

// IPlugFrame lets Chimera's editor resize its host window. On Linux there is no
// global event loop, so the frame is also the editor's Linux::IRunLoop. It runs
// on the default GLib main context, which OBS's Qt event dispatcher iterates on
// the UI thread.
class EditorWindow : public U::Implements<U::Directly<IPlugFrame, Linux::IRunLoop>> {
public:
    struct FdWatch {
        EditorWindow* owner;
        IPtr<Linux::IEventHandler> handler;
        Linux::FileDescriptor fd;
        guint source;
    };
    struct TimerWatch {
        EditorWindow* owner;
        IPtr<Linux::ITimerHandler> handler;
        guint source;
    };

    Display* display{};
    Window window{};
    Atom deleteWindow{};
    guint xSource{};
    guint syncSource{};
    std::list<FdWatch> fds;
    std::list<TimerWatch> timers;
    IPtr<IPlugView> view;
    EditorChanged changed{};
    void* context{};
    int width{}, height{};
    bool resizing{};

    void notify() noexcept {
        if (changed != nullptr) changed(context);
    }

    void setContentSize(const ViewRect& r) {
        width = r.getWidth();
        height = r.getHeight();
        resizing = true;
        XResizeWindow(display, window, static_cast<unsigned>(width), static_cast<unsigned>(height));
        XFlush(display);
        resizing = false;
    }

    tresult PLUGIN_API resizeView(IPlugView* requester, ViewRect* size) override {
        if (requester != view.get() || size == nullptr || window == 0) return kInvalidArgument;
        setContentSize(*size);
        view->onSize(size);
        return kResultTrue;
    }

    void contentResized(int w, int h) {
        if (!view || resizing || (w == width && h == height)) return;
        width = w;
        height = h;
        const ViewRect wanted{0, 0, w, h};
        ViewRect r = wanted;
        // A view without size constraints still gets the new size; a constrained
        // one may adjust it, and the window follows.
        if (view->checkSizeConstraint(&r) != kResultTrue) r = wanted;
        else if (r.getWidth() != wanted.getWidth() || r.getHeight() != wanted.getHeight()) setContentSize(r);
        view->onSize(&r);
    }

    // Window manager events for our top-level window: the close button only
    // hides it; the filter owns destruction (closeEditor()).
    void processXEvents() {
        while (XPending(display) > 0) {
            XEvent e;
            XNextEvent(display, &e);
            if (e.xany.window != window) continue;
            if (e.type == ClientMessage && static_cast<Atom>(e.xclient.data.l[0]) == deleteWindow) {
                XUnmapWindow(display, window);
                XFlush(display);
                notify();
            } else if (e.type == ConfigureNotify) {
                contentResized(e.xconfigure.width, e.xconfigure.height);
            }
        }
    }

    // Linux::IRunLoop: the plug-in's file descriptors and timers are serviced
    // by the GLib main loop on the UI thread.
    tresult PLUGIN_API registerEventHandler(Linux::IEventHandler* handler, Linux::FileDescriptor fd) override {
        if (handler == nullptr || fd < 0) return kInvalidArgument;
        FdWatch& w = fds.emplace_back(FdWatch{this, shared(handler), fd, 0});
        w.source = g_unix_fd_add(fd, static_cast<GIOCondition>(G_IO_IN | G_IO_HUP | G_IO_ERR),
            [](gint readyFd, GIOCondition, gpointer data) -> gboolean {
                auto* watch = static_cast<FdWatch*>(data);
                IPtr<Linux::IEventHandler> keep = watch->handler; // the handler may unregister itself
                keep->onFDIsSet(readyFd);
                return G_SOURCE_CONTINUE;
            },
            &w);
        return kResultTrue;
    }

    tresult PLUGIN_API unregisterEventHandler(Linux::IEventHandler* handler) override {
        if (handler == nullptr) return kInvalidArgument;
        for (auto it = fds.begin(); it != fds.end();) {
            if (it->handler.get() == handler) {
                g_source_remove(it->source);
                it = fds.erase(it);
            } else {
                ++it;
            }
        }
        return kResultTrue;
    }

    tresult PLUGIN_API registerTimer(Linux::ITimerHandler* handler, Linux::TimerInterval milliseconds) override {
        if (handler == nullptr || milliseconds == 0) return kInvalidArgument;
        TimerWatch& w = timers.emplace_back(TimerWatch{this, shared(handler), 0});
        w.source = g_timeout_add(static_cast<guint>(milliseconds),
            [](gpointer data) -> gboolean {
                IPtr<Linux::ITimerHandler> keep = static_cast<TimerWatch*>(data)->handler;
                keep->onTimer();
                return G_SOURCE_CONTINUE;
            },
            &w);
        return kResultTrue;
    }

    tresult PLUGIN_API unregisterTimer(Linux::ITimerHandler* handler) override {
        if (handler == nullptr) return kInvalidArgument;
        for (auto it = timers.begin(); it != timers.end();) {
            if (it->handler.get() == handler) {
                g_source_remove(it->source);
                it = timers.erase(it);
            } else {
                ++it;
            }
        }
        return kResultTrue;
    }

    void detach() noexcept {
        if (view) {
            view->setFrame(nullptr);
            view->removed();
            view = nullptr;
        }
    }

    // Removes every GLib source that refers to this window, then the window.
    void destroy() noexcept {
        if (syncSource != 0) g_source_remove(syncSource);
        syncSource = 0;
        detach();
        for (auto& w : fds) g_source_remove(w.source);
        fds.clear();
        for (auto& w : timers) g_source_remove(w.source);
        timers.clear();
        if (xSource != 0) g_source_remove(xSource);
        xSource = 0;
        if (display != nullptr) {
            if (window != 0) XDestroyWindow(display, window);
            XCloseDisplay(display);
        }
        window = 0;
        display = nullptr;
    }
};

bool editorSupported() noexcept { return true; }

bool openEditor(EditorWindow*& window, ChimeraHost& host, const std::string& title, EditorChanged changed,
                void* context, std::string& error) {
    // OBS's Qt runs its event loop through the default GLib main context on the
    // UI thread; the editor's run loop needs that context to be serviced here.
    if (!g_main_context_is_owner(g_main_context_default())) {
        error = "the editor must be opened on the OBS UI thread (GLib main loop)";
        return false;
    }
    if (window != nullptr && window->window != 0) {
        XMapRaised(window->display, window->window);
        XFlush(window->display);
        return true;
    }
    IPtr<IPlugView> view = owned(host.createEditorView());
    if (!view || view->isPlatformTypeSupported(kPlatformTypeX11EmbedWindowID) != kResultTrue) {
        error = "Chimera did not provide an X11 editor view";
        return false;
    }
    Display* display = XOpenDisplay(nullptr);
    if (display == nullptr) {
        error = "no X11 display (DISPLAY is unset; OBS on Wayland needs XWayland for the editor)";
        return false;
    }
    ViewRect size{0, 0, 800, 600};
    view->getSize(&size);

    auto* created = new EditorWindow();
    created->display = display;
    created->width = size.getWidth();
    created->height = size.getHeight();
    created->window = XCreateSimpleWindow(display, DefaultRootWindow(display), 0, 0,
        static_cast<unsigned>(created->width), static_cast<unsigned>(created->height), 0, 0,
        BlackPixel(display, DefaultScreen(display)));
    XSelectInput(display, created->window, StructureNotifyMask);
    created->deleteWindow = XInternAtom(display, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(display, created->window, &created->deleteWindow, 1);
    XStoreName(display, created->window, title.c_str());
    const Atom netName = XInternAtom(display, "_NET_WM_NAME", False);
    const Atom utf8 = XInternAtom(display, "UTF8_STRING", False);
    XChangeProperty(display, created->window, netName, utf8, 8, PropModeReplace,
        reinterpret_cast<const unsigned char*>(title.data()), static_cast<int>(title.size()));
    if (view->canResize() != kResultTrue) {
        XSizeHints hints{};
        hints.flags = PMinSize | PMaxSize;
        hints.min_width = hints.max_width = created->width;
        hints.min_height = hints.max_height = created->height;
        XSetWMNormalHints(display, created->window, &hints);
    }
    created->xSource = g_unix_fd_add(ConnectionNumber(display), G_IO_IN,
        [](gint, GIOCondition, gpointer data) -> gboolean {
            static_cast<EditorWindow*>(data)->processXEvents();
            return G_SOURCE_CONTINUE;
        },
        created);
    created->view = view;
    created->changed = changed;
    created->context = context;
    view->setFrame(created);
    if (view->attached(reinterpret_cast<void*>(static_cast<std::uintptr_t>(created->window)),
                       kPlatformTypeX11EmbedWindowID) != kResultOk) {
        error = "Chimera editor failed to attach";
        view->setFrame(nullptr);
        created->view = nullptr;
        created->destroy();
        created->release();
        return false;
    }
    // The view may have chosen its size while attaching.
    if (view->getSize(&size) == kResultTrue) created->setContentSize(size);
    created->syncSource = g_timeout_add(syncIntervalMs,
        [](gpointer data) -> gboolean {
            static_cast<EditorWindow*>(data)->notify();
            return G_SOURCE_CONTINUE;
        },
        created);
    XMapRaised(display, created->window);
    XFlush(display);
    window = created;
    return true;
}

void closeEditor(EditorWindow*& window) noexcept {
    if (window == nullptr) return;
    window->notify(); // last edits reach the settings before the view goes away
    window->destroy();
    window->release();
    window = nullptr;
}
}
