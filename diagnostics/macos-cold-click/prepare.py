#!/usr/bin/env python3
"""Apply declared observation-only edits to a clean exact-source CI checkout."""
import argparse
import difflib
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess

SOURCE = '781d03dd49bba51d6cfa7b1c6a0ec8d9e75a0333'
EXPECTED = {
    'Tests/CabSceneUITests.h': '3468e4aed6b281e3c635677802868118c6be2b0fde77ff196649c3519640a2d4',
    'Tests/CabMicrophoneUITests.h': '8add9a45adf921cd140ff18493c0879a9e32f9bd3a46b22a3da667e576bb217e',
    'Tests/NativeMessageLoop.mm': '23ed1c047145f68f592840facd68eb628fb4cfe591757c00a23836eaa58f08e2',
    'Tests/NativeMessageLoop.h': '3768899735474cf425a96592adfc7fb6319a6a41e7a8a9403741f4131b045b38',
    'Tests/CabPanelStateTests.cpp': '5e444621dabf1da6245889f920574716688290ae1ae6f7a7e4edde33cb183da9',
    'CMakeLists.txt': '68e30347ccee79ef97bc0e733ee39f4cf36029733b475391b8b56bc4b8ca3aa3',
}
JUCE_QUEUE_LF_SHA256 = '34b360544f8e565e188ebbf89d9669013c7be7d746fdbe2531f7b2be9707a649'
CONTROLLER_FILES = [
    '.github/workflows/build.yml',
    'diagnostics/macos-cold-click/ColdClickTrace.h',
    'diagnostics/macos-cold-click/ColdClickTrace.mm',
    'diagnostics/macos-cold-click/README.md',
    'diagnostics/macos-cold-click/collect.py',
    'diagnostics/macos-cold-click/prepare.py',
]


def digest(data):
    return hashlib.sha256(data).hexdigest()


def git(root, *args):
    return subprocess.check_output(['git', '-C', str(root), *args], text=True).strip()


def replace(text, old, new):
    assert text.count(old) == 1, ('non-unique observation anchor', old)
    return text.replace(old, new)


def instrument(relative, text):
    if relative == 'Tests/CabPanelStateTests.cpp':
        text = replace(text, '#include "CabPanel.h"', '#include "ColdClickTrace.h"\n#include "CabPanel.h"')
        return replace(text, 'juce::ScopedJuceInitialiser_GUI gui;auto folder=',
                       'juce::ScopedJuceInitialiser_GUI gui;coldClickTrace::Session diagnosticSession;auto folder=')
    if relative == 'CMakeLists.txt':
        return text + '''
# DIAGNOSTIC ONLY: source remains pinned; no release target is executed.
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/diagnostics/macos-cold-click")
target_sources(ChimeraCabPanelStateTests PRIVATE diagnostics/macos-cold-click/ColdClickTrace.mm)
'''
    if relative == 'Tests/CabSceneUITests.h':
        text = replace(text, '#pragma once', '#pragma once\n#include "ColdClickTrace.h"')
        text = replace(text, 'explicit Showing(juce::Component& value):component(value) {',
                       'explicit Showing(juce::Component& value):component(value) {\n        coldClickTrace::Span diagnosticShowing(coldClickTrace::showingBegin,coldClickTrace::showingEnd,coldClickTrace::address(&value));')
        text = replace(text, 'void buttonClicked(juce::Button*) override {',
                       'void buttonClicked(juce::Button* clicked) override {\n            coldClickTrace::record(coldClickTrace::listener,coldClickTrace::address(clicked));')
        text = replace(text, '    const auto before=workspaceState();\n    button.triggerClick();', '''    const auto before=workspaceState();
    // Numeric identity avoids formatting or I/O in the original timed path.
    std::uint64_t diagnosticTag=14695981039346656037ull;
    for(unsigned char character:identity) { diagnosticTag^=character; diagnosticTag*=1099511628211ull; }
    coldClickTrace::Span diagnosticClick(coldClickTrace::clickBegin,coldClickTrace::clickEnd,coldClickTrace::address(&button),diagnosticTag);
    {
        coldClickTrace::Span diagnosticTrigger(coldClickTrace::triggerBegin,coldClickTrace::triggerEnd,coldClickTrace::address(&button));
        button.triggerClick();
    }''')
        text = replace(text, '    if(!dispatchUntil([&] {\n        const auto now=',
                       '    if(!dispatchUntil([&] {\n        coldClickTrace::Span diagnosticPredicate(coldClickTrace::predicateBegin,coldClickTrace::predicateEnd,coldClickTrace::address(&button));\n        const auto now=')
        text = replace(text, '    })) {\n        std::ostringstream detail;',
                       '    })) {\n        coldClickTrace::record(coldClickTrace::originalFailure,coldClickTrace::address(&button));\n        std::ostringstream detail;')
        text = replace(text, '        const auto postFailureStarted=',
                       '        coldClickTrace::Span diagnosticFailureProbe(coldClickTrace::failureProbeBegin,coldClickTrace::failureProbeEnd,coldClickTrace::address(&button));\n        const auto postFailureStarted=')
        text = replace(text, 'inline void sceneInteractions(const juce::File& folder,const juce::File& screenshots) {',
                       'inline void sceneInteractions(const juce::File& folder,const juce::File& screenshots) {\n    coldClickTrace::Span diagnosticScene(coldClickTrace::sceneBegin,coldClickTrace::sceneEnd);')
        return text
    if relative == 'Tests/NativeMessageLoop.mm':
        text = replace(text, '#include "NativeMessageLoop.h"', '#include "NativeMessageLoop.h"\n#include "ColdClickTrace.h"')
        text = replace(text, 'bool dispatchFor(int milliseconds) {',
                       'bool dispatchFor(int milliseconds) {\n    coldClickTrace::Span diagnosticDispatch(coldClickTrace::dispatchBegin,coldClickTrace::dispatchEnd,static_cast<std::uint64_t>(milliseconds));')
        text = replace(text, '        @autoreleasepool {',
                       '        coldClickTrace::Span diagnosticPool(coldClickTrace::autoreleaseBegin,coldClickTrace::autoreleaseEnd);\n        @autoreleasepool {')
        text = replace(text, '            CFRunLoopRunInMode(kCFRunLoopDefaultMode,juce::jmin(.01,remaining*.001),false);', '''            const auto diagnosticCF=coldClickTrace::record(coldClickTrace::cfBegin);
            const auto diagnosticCFResult=CFRunLoopRunInMode(kCFRunLoopDefaultMode,juce::jmin(.01,remaining*.001),false);
            coldClickTrace::record(coldClickTrace::cfEnd,diagnosticCF,static_cast<std::uint64_t>(diagnosticCFResult));''')
        text = replace(text, '''            if(NSEvent* event=[NSApp nextEventMatchingMask:NSEventMaskAny
                                               untilDate:[NSDate distantPast]
                                                  inMode:NSDefaultRunLoopMode
                                                 dequeue:YES])
                if(juce::isEventBlockedByModalComps==nullptr
                    || !juce::isEventBlockedByModalComps(event))
                    [NSApp sendEvent:event];''', '''            const auto diagnosticNext=coldClickTrace::record(coldClickTrace::nextBegin);
            NSEvent* event=[NSApp nextEventMatchingMask:NSEventMaskAny
                                               untilDate:[NSDate distantPast]
                                                  inMode:NSDefaultRunLoopMode
                                                 dequeue:YES];
            coldClickTrace::record(coldClickTrace::nextEnd,diagnosticNext,coldClickTrace::address(event));
            if(event) {
                const auto diagnosticFilter=coldClickTrace::record(coldClickTrace::modalFilterBegin);
                const bool diagnosticAllowed=juce::isEventBlockedByModalComps==nullptr
                    || !juce::isEventBlockedByModalComps(event);
                coldClickTrace::record(coldClickTrace::modalFilterEnd,diagnosticFilter,diagnosticAllowed);
                if(diagnosticAllowed) {
                    coldClickTrace::Span diagnosticSend(coldClickTrace::sendBegin,coldClickTrace::sendEnd,coldClickTrace::address(event));
                    [NSApp sendEvent:event];
                }
            }''')
        return text
    return text


def instrument_queue(text):
    text = replace(text, 'namespace juce\n{', '#include "ColdClickTrace.h"\n\nnamespace juce\n{')
    text = replace(text, '    void post (MessageManager::MessageBase* const message)\n    {', '''    void post (MessageManager::MessageBase* const message)
    {
        coldClickTrace::Span diagnosticPost(coldClickTrace::queuePostBegin,coldClickTrace::queuePostEnd,coldClickTrace::address(message),coldClickTrace::address(this));''')
    text = replace(text, '                nextMessage->messageCallback();', '''                coldClickTrace::Span diagnosticCallback(coldClickTrace::callbackBegin,coldClickTrace::callbackEnd,coldClickTrace::address(nextMessage.get()),coldClickTrace::address(this));
                nextMessage->messageCallback();''')
    text = replace(text, '    void runLoopCallback() noexcept\n    {', '''    void runLoopCallback() noexcept
    {
        coldClickTrace::Span diagnosticSource(coldClickTrace::sourceBegin,coldClickTrace::sourceEnd,coldClickTrace::address(this));''')
    return text


def save_versions(out, relative, original, changed):
    for kind, data in [('original-source', original), ('instrumented-source', changed)]:
        target = out / kind / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(data)
    return dict(original_sha256=digest(original), instrumented_sha256=digest(changed))


def prepare(root, controller, out, local_check=False):
    assert git(root, 'rev-parse', 'HEAD') == SOURCE
    assert not git(root, 'diff', '--name-only')
    assert not git(root, 'diff', '--cached', '--name-only')
    out.mkdir(parents=True, exist_ok=True)
    controller_sha = git(controller, 'rev-parse', 'HEAD')
    if not local_check:
        assert controller_sha == os.environ['GITHUB_SHA']
        assert git(controller, 'rev-parse', 'HEAD^') == SOURCE
        assert git(controller, 'diff', '--name-only', 'HEAD^', 'HEAD').splitlines() == sorted(CONTROLLER_FILES)
    records = {}
    for relative, expected in EXPECTED.items():
        original = (root / relative).read_bytes()
        assert digest(original) == expected, relative
        changed = instrument(relative, original.decode()).encode()
        records[relative] = save_versions(out, relative, original, changed)
        if changed != original:
            (root / relative).write_bytes(changed)
    copied = root / 'diagnostics/macos-cold-click'
    copied.mkdir(parents=True, exist_ok=True)
    for relative in CONTROLLER_FILES:
        file = controller / relative
        target = out / 'controller' / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(file, target)
        if relative.endswith(('.h', '.mm')):
            shutil.copy2(file, copied / file.name)
    assert git(root, 'diff', '--name-only').splitlines() == sorted(
        name for name, item in records.items() if item['original_sha256'] != item['instrumented_sha256'])
    (out / 'instrumentation.patch').write_text(git(root, 'diff', '--', *EXPECTED) + '\n')
    provenance = dict(
        purpose='DIAGNOSTIC ONLY; one unchanged CAB flow, not release acceptance',
        product_source_sha=SOURCE, controller_sha=controller_sha, files=records,
        controller_sha256={p: digest((controller/p).read_bytes()) for p in CONTROLLER_FILES},
        workflow_ref=os.environ.get('GITHUB_WORKFLOW_REF'), run_id=os.environ.get('GITHUB_RUN_ID'),
        run_attempt=os.environ.get('GITHUB_RUN_ATTEMPT'),
        runner_label='macos-15', runner_image=os.environ.get('ImageOS'),
        runner_image_version=os.environ.get('ImageVersion'),
        configuration='Release -O3 -DNDEBUG plus symbols/frame pointers; no ASan',
        limitations=['Passive instrumentation/sampling can perturb scheduling.',
                    'A passing diagnostic is NOREPRO and cannot approve the failed source.',
                    'CF observer boundaries classify activity; they do not name every intervening system observer.'])
    (out / 'provenance.json').write_text(json.dumps(provenance, indent=2)+'\n')


def prepare_juce(build, out):
    juce = build / '_deps/juce-src'
    relative = 'modules/juce_events/native/juce_MessageQueue_mac.h'
    original = (juce / relative).read_bytes()
    normalized = original.replace(b'\r\n', b'\n')
    assert digest(normalized) == JUCE_QUEUE_LF_SHA256
    assert git(juce, 'rev-parse', 'HEAD') == git(juce, 'rev-parse', '8.0.8^{commit}')
    changed = instrument_queue(normalized.decode()).encode()
    if b'\r\n' in original:
        changed = changed.replace(b'\n', b'\r\n')
    records = save_versions(out, 'JUCE/'+relative, original, changed)
    records.update(juce_sha=git(juce, 'rev-parse', 'HEAD'), tag='8.0.8',
                   normalized_original_sha256=digest(normalized))
    (juce / relative).write_bytes(changed)
    (out / 'juce-instrumentation.patch').write_text(''.join(difflib.unified_diff(
        original.decode().splitlines(True), changed.decode().splitlines(True),
        fromfile='original/JUCE/'+relative, tofile='instrumented/JUCE/'+relative)))
    (out / 'juce-provenance.json').write_text(json.dumps(records, indent=2)+'\n')


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('mode', choices=['prepare', 'juce'])
    parser.add_argument('--root', type=Path, default=Path('.'))
    parser.add_argument('--controller', type=Path, default=Path('diagnostic-controller'))
    parser.add_argument('--build', type=Path, default=Path('build-macos-cold-click'))
    args = parser.parse_args()
    out = args.build / 'diagnostics'
    if args.mode == 'prepare':
        prepare(args.root, args.controller, out)
    else:
        prepare_juce(args.build, out)
