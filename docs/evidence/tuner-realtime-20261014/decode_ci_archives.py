"""Restore exact GitHub Actions artifact members and validate their SHA256."""
from pathlib import Path
import base64, hashlib, json, lzma

root=Path(__file__).resolve().parent
for platform in ('linux','windows','macos'):
    directory=root/(platform+'-candidate')
    if not directory.exists():continue
    for item in json.loads((directory/'archive-manifest.json').read_text()):
        data=lzma.decompress(base64.b64decode((directory/item['encoded']).read_bytes()))
        assert len(data)==item['bytes'] and hashlib.sha256(data).hexdigest()==item['sha256'],item['decoded']
        target=directory/item['decoded'];target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(data)
    print(platform+': all artifact member hashes verified')
for path in root.glob('*-candidate-full.log.xz.b64'):
    path.with_name(path.name[:-7]).write_bytes(lzma.decompress(base64.b64decode(path.read_bytes())))
