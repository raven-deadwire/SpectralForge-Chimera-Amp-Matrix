#!/usr/bin/env python3
"""Acquire the exact public reference inputs; verify SHA256, never redistribute.

Save outside the repository. T3K attribution and source qualifications are in
docs/reference/nastrond-nam-manifest.json. Downloads need no account credentials.
"""
import argparse
import hashlib
import json
from pathlib import Path
from urllib.parse import urlparse
from urllib.request import urlopen


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--manifest', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    root = args.out.resolve()
    root.mkdir(parents=True, exist_ok=True)
    for item in json.loads(args.manifest.read_text())['references']:
        dest = (root/item['file']).resolve()
        if not dest.is_relative_to(root):
            raise ValueError('Reference path escapes output directory')
        if dest.exists() and hashlib.sha256(dest.read_bytes()).hexdigest() == item['sha256']:
            print('VERIFIED', item['model_id'], item['name'])
            continue
        url = urlparse(item['model_url'])
        if url.scheme != 'https' or url.hostname != 'api.tone3000.com':
            raise ValueError('Unexpected reference download host')
        with urlopen(item['model_url'], timeout=60) as response:
            data = response.read(32*1024*1024+1)
        if len(data)>32*1024*1024 or hashlib.sha256(data).hexdigest()!=item['sha256']:
            raise ValueError(f"Source changed or download invalid: {item['model_id']}")
        json.loads(data)  # Do not save an HTML error as a NAM file.
        dest.parent.mkdir(parents=True, exist_ok=True)
        temporary = dest.with_suffix('.download')
        temporary.write_bytes(data)
        temporary.replace(dest)
        print('ACQUIRED', item['model_id'], item['name'])


if __name__ == '__main__':
    main()
