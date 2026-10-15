#!/usr/bin/env python3
"""Materialize the exact PR75 S8T patch in an isolated PR73 checkout only."""
import argparse
import hashlib
import os
from pathlib import Path
import subprocess

PARENT = 'c2263e4c90bffb9a56cc41285ebf8207e641b3aa'
NEON = '29865abb4da3ab78592c970a263ee62af63fc673'
PATCH_REF = '2df0b1e7c1a1e7022ed77a577bf0effb99a672fa'
PATCH_PATH = 'docs/evidence/pr73-neon-structure-20261015/snapshot-candidate/S8T.patch'
PATCH_SHA256 = '7aaef8ab7367c44083009121c6b2ac66d5b6378bc25b731b2b3720f5afbfcde0'
HEADER_SHA256 = 'f95f02d204497aa8c1f18bcb7ad66954b2174780db2570d4a47bd7dba3dbeba4'
HEADER = 'Source/ModeledCabConvolution.h'

def capture(cmd, cwd):
    return subprocess.check_output(cmd, cwd=cwd, text=True).strip()

def prepare(source):
    source = Path(source).resolve()
    if capture(['git', 'rev-parse', 'HEAD'], source) != NEON:
        raise RuntimeError('Requires a fresh checkout of pinned PR73, not a product branch')
    if capture(['git', 'status', '--porcelain', '--untracked-files=all'], source):
        raise RuntimeError('Refusing a dirty checkout')
    patch = Path(__file__).resolve().parents[2] / PATCH_PATH
    if hashlib.sha256(patch.read_bytes()).hexdigest() != PATCH_SHA256:
        raise RuntimeError('PR75 patch bytes changed')
    subprocess.run(['git', 'apply', '--check', str(patch)], cwd=source, check=True)
    subprocess.run(['git', 'apply', str(patch)], cwd=source, check=True)
    if hashlib.sha256((source / HEADER).read_bytes()).hexdigest() != HEADER_SHA256:
        raise RuntimeError('Unexpected S8T header bytes')
    if capture(['git', 'diff', '--name-only'], source).splitlines() != [HEADER]:
        raise RuntimeError('Unexpected candidate changes')
    subprocess.run(['git', 'add', '--', HEADER], cwd=source, check=True)
    tree = capture(['git', 'write-tree'], source)
    env = os.environ.copy()
    for role in ('AUTHOR', 'COMMITTER'):
        env['GIT_' + role + '_NAME'] = 'Chimera S8T Experiment'
        env['GIT_' + role + '_EMAIL'] = 's8t-experiment@invalid.local'
        env['GIT_' + role + '_DATE'] = '2026-10-15T00:00:00Z'
    sha = subprocess.check_output(
        ['git', '-c', 'commit.gpgsign=false', 'commit-tree', tree, '-p', NEON],
        input='Experiment only: exact PR75 S8T patch; not approved for integration\n',
        cwd=source, env=env, text=True).strip()
    # The index/worktree already equal this tree. Move detached HEAD without discarding files.
    subprocess.run(['git', 'reset', '--soft', sha], cwd=source, check=True)
    if capture(['git', 'status', '--porcelain', '--untracked-files=all'], source):
        raise RuntimeError('Candidate checkout is not clean')
    print(sha)
    return sha

if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--candidate', type=Path, required=True)
    prepare(parser.parse_args().candidate)
