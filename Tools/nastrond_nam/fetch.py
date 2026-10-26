"""Acquire the exact public reference inputs; verify SHA256, never redistribute."""
import hashlib
import json
from urllib.parse import urlparse
from urllib.request import urlopen

from . import references

HOST = 'api.tone3000.com'
LIMIT = 32 * 1024 * 1024


def run(manifest_path, out, log=print):
    manifest = references.load(manifest_path)
    root = out.resolve()
    root.mkdir(parents=True, exist_ok=True)
    for item in manifest['references']:
        dest = (root / item['file']).resolve()
        if not dest.is_relative_to(root):
            raise ValueError('Reference path escapes output directory')
        if dest.exists() and hashlib.sha256(dest.read_bytes()).hexdigest() == item['sha256']:
            log('VERIFIED', item['model_id'], item['name'])
            continue
        url = urlparse(item['model_url'])
        if url.scheme != 'https' or url.hostname != HOST:
            raise ValueError('Unexpected reference download host')
        with urlopen(item['model_url'], timeout=60) as response:
            data = response.read(LIMIT + 1)
        if len(data) > LIMIT or hashlib.sha256(data).hexdigest() != item['sha256']:
            raise ValueError(f"Source changed or download invalid: {item['model_id']}")
        json.loads(data)  # Do not save an HTML error as a NAM file.
        dest.parent.mkdir(parents=True, exist_ok=True)
        temporary = dest.with_suffix('.download')
        temporary.write_bytes(data)
        temporary.replace(dest)
        log('ACQUIRED', item['model_id'], item['name'])
