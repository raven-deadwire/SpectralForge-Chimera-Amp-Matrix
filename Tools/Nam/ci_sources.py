#!/usr/bin/env python3
"""Verify checkout identity before compilation and again before accepting evidence."""
import argparse
import hashlib
import json
import re
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
LOCK = json.loads((Path(__file__).with_name('ci-sources.json')).read_text())


def sha256(path):
    h = hashlib.sha256()
    with Path(path).open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            h.update(block)
    return h.hexdigest()


def git(root, *args):
    return subprocess.check_output(['git', '-C', str(root), *args], text=True).strip()


def check_checkout(root, expected):
    actual = git(root, 'rev-parse', 'HEAD')
    if not re.fullmatch(r'[0-9a-f]{40}', expected) or actual != expected:
        raise RuntimeError(f'Source mismatch: {root}: {actual} != {expected}')
    if git(root, 'status', '--porcelain', '--untracked-files=no'):
        raise RuntimeError(f'Modified tracked source: {root}')
    return {'commit': actual, 'tree': git(root, 'rev-parse', 'HEAD^{tree}')}


def capture_hashes(root=ROOT):
    # Walk the renderer's transitive local DSP includes, preserving capture.py's contract.
    hashes, pending = {}, ['Source/Amplifier.h']
    while pending:
        path = pending.pop()
        if path in hashes:
            continue
        raw = (root / path).read_bytes()
        frozen = subprocess.check_output(['git', '-C', str(root), 'show', f'{LOCK["capture"]}:{path}'])
        if raw != frozen:
            raise RuntimeError('DSP differs from frozen source: ' + path)
        hashes[path] = sha256(root / path)
        for inc in re.findall(r'^#include "([^"]+)"', raw.decode(), re.M):
            child = str(Path(path).parent / inc)
            if (root / child).is_file():
                pending.append(child)
    return hashes


def verify(deps, expected):
    report = {'pipeline': check_checkout(ROOT, expected), 'capture_commit': LOCK['capture'],
              'capture_headers': capture_hashes(), 'dependencies': {}}
    for name in ('juce', 'trainer', 'player'):
        folder = deps / name
        entry = check_checkout(folder, LOCK[name]['commit'])
        raw = subprocess.check_output(['git', '-C', str(folder), 'submodule', 'status', '--recursive'], text=True)
        entries = []
        for line in raw.splitlines():
            if not line.startswith(' '):
                raise RuntimeError('Uninitialized or mismatched submodule: ' + line)
            revision, path = line.strip().split()[:2]
            entries.append(dict(path=path, **check_checkout(folder / path, revision)))
        entry['submodules'] = entries
        report['dependencies'][name] = entry
    core = deps / 'player/plugin/NeuralAmpModelerCore'
    report['dependencies']['core'] = check_checkout(core, LOCK['core'])
    paths = git(ROOT, 'ls-files', 'Tools/Nam', '.github/workflows/nam-validation.yml').splitlines()
    report['pipeline_files'] = {p: sha256(ROOT / p) for p in paths}
    return report


def main():
    p = argparse.ArgumentParser()
    p.add_argument('--deps', type=Path, required=True)
    p.add_argument('--expected-source', required=True)
    p.add_argument('--out', type=Path, required=True)
    a = p.parse_args()
    a.out.parent.mkdir(parents=True, exist_ok=True)
    try:
        report = verify(a.deps, a.expected_source)
        report['status'] = 'PASS'
    except Exception as error:
        a.out.write_text(json.dumps({'status': 'FAIL', 'error': str(error), 'release_approved': False}, indent=2) + '\n')
        raise
    a.out.write_text(json.dumps(report, indent=2) + '\n')


if __name__ == '__main__':
    main()
