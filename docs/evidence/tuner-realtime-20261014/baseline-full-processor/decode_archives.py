"""Restore all original raw evidence after lossless checksum validation."""
from pathlib import Path
import base64,hashlib,json,lzma
root=Path(__file__).resolve().parent
for item in json.loads((root/'archive-manifest.json').read_text()):
    packed=base64.b64decode((root/item['encoded']).read_bytes())
    assert hashlib.sha256(packed).hexdigest()==item['sha256']
    data=lzma.decompress(packed)
    assert hashlib.sha256(data).hexdigest()==item['uncompressed_sha256']
    target=root/item['decoded'][:-3]
    if target.exists():
        assert target.read_bytes()==data
    else:
        with target.open('xb') as stream:stream.write(data)
