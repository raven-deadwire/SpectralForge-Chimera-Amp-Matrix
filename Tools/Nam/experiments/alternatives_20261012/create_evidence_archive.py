#!/usr/bin/env python3
"""Package research outputs with checksums; this never publishes/approves models."""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import re
import zipfile


def main():
    p = argparse.ArgumentParser(description=__doc__)
    for key in ('fit', 'evaluation', 'supplement', 'probe-inputs', 'out'):
        p.add_argument('--' + key, type=Path, required=True)
    p.add_argument('--source-commit', required=True)
    a = p.parse_args()
    if not re.fullmatch('[0-9a-f]{40}', a.source_commit):
        raise ValueError('Provide the published immutable experiment commit')
    if a.out.exists():
        raise ValueError('Refuse to replace an existing archive')
    root = Path(__file__).resolve().parent
    if not (root / 'nonlinear-memory/delta-waveforms.npz').is_file():
        raise RuntimeError('Original nonlinear delta curves are required for a complete evidence archive')
    comparison = json.loads((a.evaluation / 'comparison.json').read_text())
    selected = json.loads((a.supplement / 'selected-summary.json').read_text())
    assert comparison['complete'] and selected['complete']
    assert not comparison['release_approved'] and not selected['release_approved']
    assert selected['source_comparison_sha256'] == hashlib.sha256(
        (a.evaluation / 'comparison.json').read_bytes()).hexdigest()
    records = []
    for prefix, directory in (
            ('fit', a.fit), ('evaluation', a.evaluation),
            ('selected-supplement', a.supplement), ('nonlinear-inputs', a.probe_inputs)):
        for f in sorted(directory.rglob('*')):
            if f.is_file() and '__pycache__' not in f.parts:
                records.append((prefix + '/' + f.relative_to(directory).as_posix(), f))
    # Preserve the raw diagnostic curves outside Git. Compact probe reports are
    # included with their measurements, without duplicating the source checkout.
    for f in sorted((root / 'nonlinear-memory').iterdir()):
        if f.suffix in ('.npz', '.json'):
            records.append(('nonlinear-diagnostics/' + f.name, f))
    for f in (root/'memory-factorization').glob('*.json'):
        records.append(('memory-factorization/' + f.name, f))
    names = [n for n, _ in records]
    assert len(names) == len(set(names))
    code_url = ('https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/tree/'
                + a.source_commit + '/Tools/Nam/experiments/alternatives_20261012')
    manifest = {
        'name': 'Nastrond NAM isolated alternatives research evidence, 2026-10-12',
        'source_commit': a.source_commit, 'source_url': code_url,
        'parent_source': '859fea9bc49c4b9981c926a314655e485c51027c',
        'original_input_archive': 'Nastrond-NAM-Refinement-Evidence-2026-10-11.zip',
        'original_input_archive_sha256': '4b3705e3bb515462250f6f59d8d05a96917467efd5b2bd6fdacb78a4615fb6ce',
        'release_approved': False, 'fidelity_pass_count': selected['numerical_pass_count'],
        'scoped_engine_pass_count': selected['engine_pass_count'],
        'instrument_DI_listening': 'NOT_EXECUTED', 'GUI_DAW_acceptance': 'NOT_EXECUTED',
        'scope': 'All new fit/evaluation outputs; original immutable inputs and source checkout are referenced, not duplicated. Nonlinear diagnostics retain all delta curves, input bytes, output hashes and same-window energy summaries; not all full native probe-output traces were retained.',
        'files': []
    }
    readme = f'''NÁSTRÖND NAM ALTERNATIVES — RESEARCH EVIDENCE — 2026-10-12

All five channels FAIL full numerical fidelity. RELEASE APPROVAL IS FALSE.
These .nam and .t3kpreset files are research evidence, not approved products.
Real instrument DI listening and full installed GUI/DAW acceptance were not run.

Complete scientific report, source code, policy references and reproduction:
{code_url}
Immutable source commit: {a.source_commit}
Original PR #22 source: 859fea9bc49c4b9981c926a314655e485c51027c

fit/ contains the archived fallback, all 30 newly fitted heads, descriptive
mode-best candidates, preselected model/job pairs, train/validation projections,
the preregistered plan and sealed selection. Nonlinear backbone updates: zero.

evaluation/ contains the single new 24-second synthetic input (seed2610122201),
five unchanged native references, actual engine outputs for 5 baselines plus
10 mode-best pairs, their jobs/native presets, and old revealed-test outputs.

selected-supplement/ contains the omitted but already preselected Nidhoggr
lambda=.01 measurement and the complete five-channel selected summary. The
other four rows reuse exact model/job hashes measured in evaluation/. This is
shared-test completion, not another test or post-test selection.

memory-factorization/ and nonlinear-diagnostics/ contain frozen supported EQ
settings and structural diagnostic evidence. nonlinear-inputs/ contains the
stimuli and teacher jobs. All 50 difference-in-differences curves are retained.
Full native probe-output traces were not retained for all 150 unique conditions;
their input/source/output hashes and measured summaries allow reproduction.
The 20 trace energy reconstructions were byte-identical to their original hashes.

The original TRAIN, validation and old exposed-test input/native arrays remain
in Nastrond-NAM-Refinement-Evidence-2026-10-11.zip, SHA256:
4b3705e3bb515462250f6f59d8d05a96917467efd5b2bd6fdacb78a4615fb6ce
They are not needlessly duplicated here. The new input/references are included.

Use manifest.json to verify every retained file's SHA-256 and byte count.
Use only the original fixed source-output anchors and unchanged quality profile.
Preset EQ and model hashes form a pair; a bare .nam omits the supported EQ state.
No output pad, test fitting, retiming, tail cutoff or relaxed threshold is authorized
by these research results. For exact run procedures, read REPRODUCE.md at the
immutable source link above. Original absolute scratch paths in seals document
the run and can be mapped to extracted paths while retaining byte hashes.
'''
    a.out.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(a.out, 'w', compression=zipfile.ZIP_DEFLATED, compresslevel=6,
                         allowZip64=True) as z:
        for name, f in sorted(records):
            data = f.read_bytes()
            digest = hashlib.sha256(data).hexdigest()
            manifest['files'].append({'path': name, 'bytes': len(data), 'sha256': digest})
            info = zipfile.ZipInfo(name, date_time=(2026, 10, 12, 0, 0, 0))
            info.compress_type = zipfile.ZIP_STORED if f.suffix == '.npz' else zipfile.ZIP_DEFLATED
            z.writestr(info, data, compresslevel=6)
        z.writestr('README.txt', readme)
        z.writestr('manifest.json', json.dumps(manifest, ensure_ascii=False, indent=2)+'\n')
    archive_hash = hashlib.sha256()
    with a.out.open('rb') as stream:
        for chunk in iter(lambda: stream.read(8*1024*1024), b''):
            archive_hash.update(chunk)
    print(json.dumps({'path': str(a.out), 'bytes': a.out.stat().st_size,
                      'sha256': archive_hash.hexdigest(), 'indexed_files': len(records),
                      'source_commit': a.source_commit, 'release_approved': False}))


if __name__ == '__main__':
    main()
