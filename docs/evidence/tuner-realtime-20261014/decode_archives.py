"""Restore the original lossless gzip archives; never overwrite prior files."""
from pathlib import Path
import base64,hashlib,json
root=Path(__file__).resolve().parent
for item in json.loads((root/'archive-manifest.json').read_text()):
    data=base64.b64decode((root/item['encoded']).read_bytes())
    assert hashlib.sha256(data).hexdigest()==item['sha256']
    target=root/item['decoded']
    if target.exists():
        assert target.read_bytes()==data
    else:
        with target.open('xb') as stream:stream.write(data)
