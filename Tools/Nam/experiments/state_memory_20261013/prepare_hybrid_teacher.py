#!/usr/bin/env python3
"""Expose a lossless native pre/post boundary for future hybrid target capture.

This is a teacher decomposition, NOT a trained NAM+native candidate. The prefix
still runs the original sag, blocking, bias and feedback. They cannot be deleted
or supplied from the reference at inference time by a future neural model.
"""
import argparse
from pathlib import Path
from prepare_state_probe import DSP_SHA, replace_once
import hashlib


def prepare(repo,out):
    source=repo/'Source/OriginalAmpDSP.h'
    if hashlib.sha256(source.read_bytes()).hexdigest()!=DSP_SHA:raise ValueError('Teacher changed')
    out.mkdir(parents=True,exist_ok=False)
    definition=(repo/'Source/OriginalAmpDefinition.h').read_text().replace('namespace spectralforge::original','namespace hybrid_research::original')
    (out/'HybridDefinition.h').write_text(definition)
    text=source.read_text().replace('namespace spectralforge::original','namespace hybrid_research::original')
    text=replace_once(text,'#include "OriginalAmpDefinition.h"','#include "HybridDefinition.h"')
    text=replace_once(text,'class OriginalAmpDSP {','class HybridTeacher {')
    original='    float tick(float input,int channel) noexcept {'
    front='''    float tick(float input,int channel) noexcept {
        if(channel<0||channel>=2)return 0;
        return tickPost(tickFront(input,channel),channel);
    }
    double tickFront(float input,int channel) noexcept {'''
    text=replace_once(text,original,front)
    boundary='        // Stable damped LF resonator:'
    replacement='''        return x;
    }
    // The output boundary keeps source-rate recursive state and the nonlinear
    // rail. Replacing only this block with an LTI filter is not exact at peaks.
    float tickPost(double x,int channel) noexcept {
        if(channel<0||channel>=2)return 0;
        auto& c=channels[std::size_t(channel)];
        const auto& p=c.config;
''' + boundary
    text=replace_once(text,boundary,replacement)
    (out/'HybridTeacher.h').write_text(text)


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    for n in ['repo','out']:p.add_argument('--'+n,type=Path,required=True)
    a=p.parse_args();prepare(a.repo,a.out)
