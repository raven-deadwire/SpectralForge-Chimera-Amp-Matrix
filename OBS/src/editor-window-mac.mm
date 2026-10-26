// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 RavenForge Luthier Intelligence
#include "editor-window.hpp"
#include "chimera-host.hpp"

#include "pluginterfaces/base/funknownimpl.h"
#include "pluginterfaces/gui/iplugview.h"

#import <Cocoa/Cocoa.h>

using namespace Steinberg;

namespace spectralforge::obs {
class EditorWindow;
}

// Window delegate: closing only hides; the filter owns destruction (closeEditor()).
@interface ChimeraOBSEditorDelegate : NSObject <NSWindowDelegate>
@property(nonatomic, assign) spectralforge::obs::EditorWindow* owner;
@end

namespace spectralforge::obs {
namespace {
constexpr NSTimeInterval syncInterval = 0.5;
}

// IPlugFrame lets Chimera's editor resize its host window.
class EditorWindow : public U::Implements<U::Directly<IPlugFrame>> {
public:
    NSWindow* window{nil};
    ChimeraOBSEditorDelegate* delegate{nil};
    NSTimer* timer{nil};
    IPtr<IPlugView> view;
    EditorChanged changed{};
    void* context{};
    bool resizing{};

    void notify() noexcept {
        if (changed != nullptr) changed(context);
    }

    void setContentSize(const ViewRect& r) {
        resizing = true;
        [window setContentSize:NSMakeSize(r.getWidth(), r.getHeight())];
        resizing = false;
    }

    tresult PLUGIN_API resizeView(IPlugView* requester, ViewRect* size) override {
        if (requester != view.get() || size == nullptr || window == nil) return kInvalidArgument;
        setContentSize(*size);
        view->onSize(size);
        return kResultTrue;
    }

    void contentResized() {
        if (!view || resizing) return;
        const NSSize s = window.contentView.frame.size;
        ViewRect r{0, 0, static_cast<int32>(s.width), static_cast<int32>(s.height)};
        if (view->checkSizeConstraint(&r) == kResultTrue) view->onSize(&r);
    }

    void detach() noexcept {
        if (view) {
            view->setFrame(nullptr);
            view->removed();
            view = nullptr;
        }
    }
};

bool editorSupported() noexcept { return true; }

bool openEditor(EditorWindow*& window, ChimeraHost& host, const std::string& title, EditorChanged changed,
                void* context, std::string& error) {
    if (![NSThread isMainThread]) {
        error = "the editor must be opened on the main thread";
        return false;
    }
    if (window != nullptr && window->window != nil) {
        [window->window makeKeyAndOrderFront:nil];
        [NSApp activateIgnoringOtherApps:YES];
        return true;
    }
    IPtr<IPlugView> view = owned(host.createEditorView());
    if (!view || view->isPlatformTypeSupported(kPlatformTypeNSView) != kResultTrue) {
        error = "Chimera did not provide a macOS editor view";
        return false;
    }
    ViewRect size{0, 0, 800, 600};
    view->getSize(&size);
    NSWindowStyleMask style = NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskMiniaturizable;
    if (view->canResize() == kResultTrue) style |= NSWindowStyleMaskResizable;

    auto* created = new EditorWindow();
    created->window = [[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, size.getWidth(), size.getHeight())
                                                  styleMask:style
                                                    backing:NSBackingStoreBuffered
                                                      defer:NO];
    created->window.releasedWhenClosed = NO;
    created->window.title = [NSString stringWithUTF8String:title.c_str()] ?: @"SpectralForge Chimera";
    created->delegate = [ChimeraOBSEditorDelegate new];
    created->delegate.owner = created;
    created->window.delegate = created->delegate;
    created->view = view;
    created->changed = changed;
    created->context = context;
    view->setFrame(created);
    if (view->attached((__bridge void*)created->window.contentView, kPlatformTypeNSView) != kResultOk) {
        error = "Chimera editor failed to attach";
        view->setFrame(nullptr);
        created->view = nullptr;
        created->window.delegate = nil;
        [created->window close];
        created->window = nil;
        created->release();
        return false;
    }
    // The view may have chosen its size while attaching.
    if (view->getSize(&size) == kResultTrue) created->setContentSize(size);
    [created->window center];
    EditorWindow* raw = created;
    created->timer = [NSTimer scheduledTimerWithTimeInterval:syncInterval
                                                     repeats:YES
                                                       block:^(NSTimer*) { raw->notify(); }];
    [created->window makeKeyAndOrderFront:nil];
    [NSApp activateIgnoringOtherApps:YES];
    window = created;
    return true;
}

void closeEditor(EditorWindow*& window) noexcept {
    if (window == nullptr) return;
    window->notify(); // last edits reach the settings before the view goes away
    [window->timer invalidate];
    window->timer = nil;
    window->detach();
    if (window->window != nil) {
        window->delegate.owner = nullptr;
        window->window.delegate = nil;
        [window->window orderOut:nil];
        [window->window close];
        window->window = nil;
    }
    window->delegate = nil;
    window->release();
    window = nullptr;
}
}

@implementation ChimeraOBSEditorDelegate
- (BOOL)windowShouldClose:(NSWindow*)sender {
    [sender orderOut:nil];
    if (self.owner != nullptr) self.owner->notify();
    return NO;
}
- (void)windowDidResize:(NSNotification*)notification {
    (void)notification;
    if (self.owner != nullptr) self.owner->contentResized();
}
@end
