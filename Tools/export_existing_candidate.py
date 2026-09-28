#!/usr/bin/env python3
"""Export existing successful CI bytes without building or repackaging the product."""
import hashlib
import json
from pathlib import Path
import shutil
import zipfile

SOURCE_SHA = '50271a69203919224fdcc68a8d0a285c724e8a7e'
RUN_ID = 36414431518
ARCHIVE_NAME = 'Chimera-update-50271a6920-win64.zip'
ORIGINAL_SHA256 = '9cfdb65bed262b456fe8bcc588aad4d9154b0e565c6744c872585cd6621a28ae'
CHUNK_BYTES = 20 * 1024 * 1024
DOWNLOAD_LIMIT = 32 * 1024 * 1024


def digest(path):
    h = hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            h.update(block)
    return h.hexdigest()


def main():
    candidate = Path('original-candidate')
    evidence = Path('original-evidence')
    out = Path('exported')
    out.mkdir(exist_ok=False)
    matches = list(candidate.rglob(ARCHIVE_NAME))
    if len(matches) != 1:
        raise RuntimeError('Expected exactly one original candidate ZIP')
    archive = matches[0]
    sidecar = archive.with_name(archive.name + '.sha256')
    fields = sidecar.read_text(encoding='utf-8').split()
    original_digest = digest(archive)
    if fields != [original_digest, ARCHIVE_NAME] or original_digest != ORIGINAL_SHA256:
        raise RuntimeError('Original ZIP checksum sidecar mismatch')
    with zipfile.ZipFile(archive) as z:
        build_manifest = json.loads(z.read('build-manifest.json'))
    if build_manifest['source_sha'] != SOURCE_SHA or str(build_manifest['run_id']) != str(RUN_ID):
        raise RuntimeError('Original candidate source or CI run mismatch')

    parts = []
    reconstructed_digest = hashlib.sha256()
    offset = 0
    with archive.open('rb') as stream:
        while block := stream.read(CHUNK_BYTES):
            part = out / (ARCHIVE_NAME + '.part' + str(len(parts) + 1))
            part.write_bytes(block)
            reconstructed_digest.update(block)
            parts.append({'file': part.name, 'offset': offset, 'bytes': len(block),
                          'sha256': digest(part)})
            offset += len(block)
    if len(parts) != 2 or offset != archive.stat().st_size or reconstructed_digest.hexdigest() != original_digest:
        raise RuntimeError('Split byte reconstruction check failed')
    if any(p['bytes'] + 1024 * 1024 >= DOWNLOAD_LIMIT for p in parts):
        raise RuntimeError('Part lacks sufficient artifact ZIP overhead allowance')
    shutil.copy2(sidecar, out / sidecar.name)

    evidence_files = sorted(p for p in evidence.rglob('*') if p.is_file() and (
        p.match('Universal-*.png') or p.suffix == '.json' or p.name == 'LastTest.log'))
    if not any(p.match('Universal-*.png') for p in evidence_files) or not any(p.name == 'LastTest.log' for p in evidence_files):
        raise RuntimeError('Original UI screenshots or LastTest.log missing')
    evidence_zip = out / 'Chimera-existing-50271a6920-small-evidence.zip'
    with zipfile.ZipFile(evidence_zip, 'w', zipfile.ZIP_DEFLATED) as z:
        for p in evidence_files:
            z.write(p, p.relative_to(evidence).as_posix())
    if evidence_zip.stat().st_size + 1024 * 1024 >= DOWNLOAD_LIMIT:
        raise RuntimeError('Selected evidence exceeds artifact download budget')
    manifest = {
        'kind': 'byte-preserving-export-of-existing-ci-artifacts',
        'source_sha': SOURCE_SHA, 'source_run_id': RUN_ID,
        'source_candidate_artifact_id': 10966919648,
        'source_evidence_artifact_id': 10966779814,
        'original_archive': {'file': ARCHIVE_NAME, 'bytes': archive.stat().st_size,
                             'sha256': original_digest},
        'original_sidecar': {'file': sidecar.name, 'sha256': digest(sidecar)},
        'parts': parts,
        'reconstruction': 'Concatenate the listed parts in manifest order as binary bytes; verify original_archive.sha256 before opening the restored ZIP.',
        'product_rebuilt': False, 'product_archive_repackaged': False,
        'evidence_zip': {'file': evidence_zip.name, 'bytes': evidence_zip.stat().st_size,
                         'sha256': digest(evidence_zip)},
        'evidence_files': [{'path': p.relative_to(evidence).as_posix(),
                            'bytes': p.stat().st_size, 'sha256': digest(p)} for p in evidence_files],
    }
    (out / 'parts-manifest.json').write_text(json.dumps(manifest, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(manifest, indent=2))


if __name__ == '__main__':
    main()
