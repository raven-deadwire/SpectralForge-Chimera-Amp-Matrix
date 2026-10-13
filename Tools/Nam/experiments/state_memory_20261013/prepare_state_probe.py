#!/usr/bin/env python3
"""Create an isolated source overlay with one-shot native-state ablations.

The production headers and renderer are never edited. This is a causal
diagnostic of the frozen teacher, not a model and not a shipping DSP option.
"""
import argparse
import hashlib
import json
from pathlib import Path
import shutil

DSP_SHA = '7595c2bf32af62d7b3863d0b9551139d319b7ec3b108a0053b34f6072559de3c'
HOOK = r'''
namespace research {
inline int mode = 0;
inline long long at = -1, position = 0;
template<class Channel> void beforeSample(Channel& c) noexcept {
    if (position++ != at) return;
    // 1: output resonance/filter history; 2: slow nonlinear states;
    // 3: all audio memory. Individual slow-state arms: 4 sag, 5 blocking, 6 bias.
    if (mode == 1 || mode == 3) {
        c.sagLow={};c.bloom1=0;c.bloom2=0;c.voicing={};c.output={};c.dc={};
    }
    if (mode == 2 || mode == 3 || mode == 4) c.sag=0;
    if (mode == 2 || mode == 3 || mode == 5)
        for (auto& cell:c.cells) cell.blockingMemory=0;
    if (mode == 2 || mode == 3 || mode == 6)
        for (auto& cell:c.cells) cell.biasMemory=0;
    if (mode == 3) {
        c.cells={};c.input={};c.tightener={};c.bass={};c.treble={};
        c.feedbackLow={};c.impactLow={};c.impactSub={};c.presence={};
        c.middle={};c.characterPre={};c.fast=0;c.slow=0;c.feedback=0;
    }
}
}
'''


def replace_once(text, old, new):
    if text.count(old) != 1:
        raise ValueError('Source anchor changed')
    return text.replace(old, new)


def prepare(repo, out):
    source = repo / 'Source/OriginalAmpDSP.h'
    if hashlib.sha256(source.read_bytes()).hexdigest() != DSP_SHA:
        raise ValueError('Frozen teacher source mismatch')
    if out.exists():
        raise ValueError('Use a new overlay directory')
    shutil.copytree(repo / 'Source', out / 'Source')
    tools = out / 'Tools/Nam'
    tools.mkdir(parents=True)
    for name in ['CMakeLists.txt', 'RenderNativeAmp.cpp']:
        shutil.copy2(repo / 'Tools/Nam' / name, tools / name)
    header = source.read_text()
    header = replace_once(header, '// Allocation-free sample core.', HOOK + '\n// Allocation-free sample core.')
    header = replace_once(header, 'const auto& p=c.config;\n        double x=',
                          'const auto& p=c.config;\n        research::beforeSample(c);\n        double x=')
    (out / 'Source/OriginalAmpDSP.h').write_text(header)
    renderer = (tools / 'RenderNativeAmp.cpp').read_text()
    old = '    const auto* events=job["events"].getArray();'
    new = '''    original::research::mode=int(job["research_mode"]);
    original::research::at=job.hasProperty("research_onset") ? 4LL*int(job["research_onset"]) : -1;
    original::research::position=0;
''' + old
    (tools / 'RenderNativeAmp.cpp').write_text(replace_once(renderer, old, new))
    return {'teacher_header_sha256': DSP_SHA, 'overlay_only': True,
            'original_headers_modified': False, 'release_approved': False,
            'hook_rate': 192000, 'audio_rate': 48000,
            'scope': 'one-shot controlled source-state diagnostic; altered arms are not reference/candidate audio'}


if __name__ == '__main__':
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--repo', type=Path, required=True)
    p.add_argument('--out', type=Path, required=True)
    a = p.parse_args()
    print(json.dumps(prepare(a.repo, a.out), indent=2))
