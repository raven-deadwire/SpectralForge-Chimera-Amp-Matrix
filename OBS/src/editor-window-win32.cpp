// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 RavenForge Luthier Intelligence
#include "editor-window.hpp"
#include "chimera-host.hpp"

#include "pluginterfaces/base/funknownimpl.h"
#include "pluginterfaces/gui/iplugview.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

using namespace Steinberg;

namespace spectralforge::obs {
namespace {
constexpr wchar_t windowClass[] = L"SpectralForgeChimeraOBSEditor";

std::wstring widen(const std::string& text) {
    const int n = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, nullptr, 0);
    std::wstring out(n > 0 ? static_cast<size_t>(n - 1) : 0u, L'\0');
    if (n > 1) MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, out.data(), n);
    return out;
}

void clientToWindow(HWND hwnd, const ViewRect& r, int& width, int& height) {
    RECT rect{0, 0, r.getWidth(), r.getHeight()};
    AdjustWindowRectEx(&rect, static_cast<DWORD>(GetWindowLongPtrW(hwnd, GWL_STYLE)), FALSE,
                       static_cast<DWORD>(GetWindowLongPtrW(hwnd, GWL_EXSTYLE)));
    width = rect.right - rect.left;
    height = rect.bottom - rect.top;
}
}

// IPlugFrame lets Chimera's editor resize its host window.
class EditorWindow : public U::Implements<U::Directly<IPlugFrame>> {
public:
    HWND hwnd{};
    IPtr<IPlugView> view;
    bool resizing{};

    tresult PLUGIN_API resizeView(IPlugView* requester, ViewRect* size) override {
        if (requester != view.get() || size == nullptr || hwnd == nullptr) return kInvalidArgument;
        resizing = true;
        int width = 0, height = 0;
        clientToWindow(hwnd, *size, width, height);
        SetWindowPos(hwnd, nullptr, 0, 0, width, height, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
        resizing = false;
        view->onSize(size);
        return kResultTrue;
    }

    void detach() noexcept {
        if (view) {
            view->setFrame(nullptr);
            view->removed();
            view = nullptr;
        }
    }

    static LRESULT CALLBACK procedure(HWND hwnd, UINT message, WPARAM w, LPARAM l) {
        auto* self = reinterpret_cast<EditorWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        switch (message) {
        case WM_SIZE:
            if (self != nullptr && self->view && !self->resizing) {
                ViewRect r{0, 0, LOWORD(l), HIWORD(l)};
                if (self->view->checkSizeConstraint(&r) == kResultTrue) self->view->onSize(&r);
            }
            return 0;
        case WM_CLOSE:
            // Closing only hides; the filter owns destruction (closeEditor()).
            ShowWindow(hwnd, SW_HIDE);
            return 0;
        default:
            return DefWindowProcW(hwnd, message, w, l);
        }
    }
};

bool editorSupported() noexcept { return true; }

bool openEditor(EditorWindow*& window, ChimeraHost& host, const std::string& title, std::string& error) {
    if (window != nullptr && window->hwnd != nullptr) {
        ShowWindow(window->hwnd, SW_SHOWNORMAL);
        SetForegroundWindow(window->hwnd);
        return true;
    }
    IPtr<IPlugView> view = owned(host.createEditorView());
    if (!view || view->isPlatformTypeSupported(kPlatformTypeHWND) != kResultTrue) {
        error = "Chimera did not provide a Windows editor view";
        return false;
    }
    const HINSTANCE instance = GetModuleHandleW(nullptr);
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = &EditorWindow::procedure;
    wc.hInstance = instance;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.lpszClassName = windowClass;
    RegisterClassExW(&wc); // A second registration fails harmlessly.

    auto* created = new EditorWindow();
    created->hwnd = CreateWindowExW(0, windowClass, widen(title).c_str(), WS_OVERLAPPEDWINDOW,
                                    CW_USEDEFAULT, CW_USEDEFAULT, 800, 600, nullptr, nullptr, instance, nullptr);
    if (created->hwnd == nullptr) {
        error = "the editor window could not be created";
        created->release();
        return false;
    }
    SetWindowLongPtrW(created->hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(created));
    created->view = view;
    view->setFrame(created);
    if (view->attached(created->hwnd, kPlatformTypeHWND) != kResultOk) {
        error = "Chimera editor failed to attach";
        view->setFrame(nullptr);
        created->view = nullptr;
        DestroyWindow(created->hwnd);
        created->release();
        return false;
    }
    if (view->canResize() != kResultTrue) {
        const auto style = GetWindowLongPtrW(created->hwnd, GWL_STYLE) & ~(WS_THICKFRAME | WS_MAXIMIZEBOX);
        SetWindowLongPtrW(created->hwnd, GWL_STYLE, style);
    }
    ViewRect size{};
    if (view->getSize(&size) == kResultTrue) {
        int width = 0, height = 0;
        clientToWindow(created->hwnd, size, width, height);
        SetWindowPos(created->hwnd, nullptr, 0, 0, width, height, SWP_NOMOVE | SWP_NOZORDER | SWP_FRAMECHANGED);
    }
    ShowWindow(created->hwnd, SW_SHOWNORMAL);
    window = created;
    return true;
}

void closeEditor(EditorWindow*& window) noexcept {
    if (window == nullptr) return;
    window->detach();
    if (window->hwnd != nullptr) {
        SetWindowLongPtrW(window->hwnd, GWLP_USERDATA, 0);
        DestroyWindow(window->hwnd);
        window->hwnd = nullptr;
    }
    window->release();
    window = nullptr;
}
}
