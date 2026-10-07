#!/usr/bin/env python3
"""Restore download-artifact v4's unmerged singleton directory using API names.

Do not infer an artifact's name from its payload: unexpected artifacts must
remain unexpected to existing consumers. No file bytes or verdicts are edited.
"""
import argparse
import fnmatch
import json
import os
from pathlib import Path
import urllib.request


def artifact_names(pattern):
    names = []
    page = 1
    while True:
        url = (os.environ['GITHUB_API_URL'] + '/repos/' + os.environ['GITHUB_REPOSITORY'] +
               '/actions/runs/' + os.environ['GITHUB_RUN_ID'] + f'/artifacts?per_page=100&page={page}')
        req = urllib.request.Request(url, headers={
            'Authorization': 'Bearer ' + os.environ['GH_TOKEN'],
            'Accept': 'application/vnd.github+json', 'X-GitHub-Api-Version': '2022-11-28'})
        with urllib.request.urlopen(req, timeout=60) as response:
            batch = json.load(response)['artifacts']
        names.extend(a['name'] for a in batch if fnmatch.fnmatchcase(a['name'], pattern))
        if len(batch) < 100:
            return sorted(set(names))
        page += 1


def restore(path, names):
    if len(names) != 1:
        return
    name = names[0]
    if not name or name in ('.', '..') or '/' in name or '\\' in name:
        raise ValueError('Unsafe artifact name')
    if not path.is_dir():
        raise ValueError('Singleton artifact download directory is missing')
    children = list(path.iterdir())
    destination = path / name
    if destination.exists():
        raise ValueError('Refusing to overwrite an existing artifact directory')
    destination.mkdir()
    for child in children:
        child.rename(destination / child.name)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--path', type=Path, required=True)
    parser.add_argument('--pattern', required=True)
    args = parser.parse_args()
    # Current consumers use literal names plus '*', matching Minimatch semantics.
    if any(char in args.pattern for char in '?[]{}!+@()\\'):
        parser.error('Only literal names and * patterns are supported')
    restore(args.path, artifact_names(args.pattern))
