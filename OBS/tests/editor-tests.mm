// SPDX-License-Identifier: GPL-2.0-or-later
// The Chimera editor window on macOS against the real Chimera VST3 (plan stage
// F): opens in a native window, syncs edits on its timer, hides on the close
// button, raises the same window again, and is torn down by closeEditor().
#include "chimera-host.hpp"
#include "editor-window.hpp"

#import <Cocoa/Cocoa.h>
#include <iostream>
#include <stdexcept>
#include <string>

using namespace spectralforge::obs;

namespace {
void check(bool ok, const char* what) { if (!ok) throw std::runtime_error(what); }

int notifications = 0;
void changed(void*) { ++notifications; }

// Runs the main run loop for the whole interval; a single runUntilDate can
// return early once a source fires.
void pump(double seconds) {
    NSDate* deadline = [NSDate dateWithTimeIntervalSinceNow:seconds];
    while ([deadline timeIntervalSinceNow] > 0) {
        NSDate* slice = [NSDate dateWithTimeIntervalSinceNow:0.05];
        [[NSRunLoop mainRunLoop] runMode:NSDefaultRunLoopMode beforeDate:[slice earlierDate:deadline]];
    }
}

NSWindow* windowTitled(NSString* title) {
    for (NSWindow* w in NSApp.windows)
        if ([w.title isEqualToString:title]) return w;
    return nil;
}

void run(const std::string& bundle) {
    [NSApplication sharedApplication];
    [NSApp setActivationPolicy:NSApplicationActivationPolicyAccessory];
    check(editorSupported(), "editor supported on macOS");

    ChimeraHost host;
    std::string error;
    check(host.load(bundle, error), error.c_str());
    check(host.prepare(48000.0, 2, 512), "prepare");

    const std::string title = "Chimera editor test";
    NSString* nsTitle = [NSString stringWithUTF8String:title.c_str()];
    EditorWindow* editor = nullptr;
    check(openEditor(editor, host, title, changed, nullptr, error), error.c_str());
    check(editor != nullptr, "editor handle");
    NSWindow* window = windowTitled(nsTitle);
    check(window != nil && window.visible, "editor window is shown");
    check(window.contentView.subviews.count > 0, "Chimera attached its view");
    check(window.contentView.frame.size.width >= 200 && window.contentView.frame.size.height >= 150,
          "window sized to the editor");

    notifications = 0;
    const CFAbsoluteTime started = CFAbsoluteTimeGetCurrent();
    pump(1.3);
    const std::string synced = "edits are synced every 500 ms while open (" + std::to_string(notifications) +
                               " in " + std::to_string(CFAbsoluteTimeGetCurrent() - started) + " s)";
    check(notifications >= 2, synced.c_str());

    // Opening again raises the same window rather than creating another.
    EditorWindow* same = editor;
    check(openEditor(editor, host, title, changed, nullptr, error) && editor == same, "reopen raises");
    int count = 0;
    for (NSWindow* w in NSApp.windows) count += [w.title isEqualToString:nsTitle] ? 1 : 0;
    check(count == 1, "one editor window");

    // The close button only hides; the filter decides when to destroy.
    notifications = 0;
    [window performClose:nil];
    pump(0.1);
    check(!window.visible && windowTitled(nsTitle) == window, "close button hides the window");
    check(notifications >= 1, "closing syncs the last edits");
    check(openEditor(editor, host, title, changed, nullptr, error) && window.visible, "hidden editor shown again");

    notifications = 0;
    closeEditor(editor);
    pump(0.1);
    check(editor == nullptr && notifications == 1, "closeEditor syncs once and releases");
    NSWindow* gone = windowTitled(nsTitle);
    check(gone == nil || !gone.visible, "window removed");

    // Repeated open/close (scene switches) must not leak or crash.
    for (int i = 0; i < 3; ++i) {
        check(openEditor(editor, host, title, changed, nullptr, error), error.c_str());
        pump(0.2);
        closeEditor(editor);
    }
    closeEditor(editor); // nullptr is safe
    host.unload();
}
}

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "usage: chimera-obs-editor-tests <SpectralForge Chimera.vst3>\n";
        return 2;
    }
    @autoreleasepool {
        try {
            run(argv[1]);
        } catch (const std::exception& e) {
            std::cerr << "FAIL " << e.what() << '\n';
            return 1;
        }
    }
    std::cout << "PASS macOS editor window: attach, 500 ms sync, close hides, reopen raises, closeEditor tears down\n";
    return 0;
}
