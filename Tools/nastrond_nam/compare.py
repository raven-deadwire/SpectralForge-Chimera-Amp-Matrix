"""Render the probe through OriginalAmp states and NAM captures; write comparison.json.

Every v1 key keeps its meaning, so v1 audits still accept the report. Added:
analysis_version, source provenance, states_sha256, parsed renderer info,
and per-comparison polarity and band_spectrum_rms_db.
"""
import concurrent.futures
import json
import subprocess
from pathlib import Path

import numpy as np
from scipy.io import wavfile

from validate_nam import SR
from . import analysis, protocol, references, renderers, states as state_suite

ANALYSIS_VERSION = 2
CALIBRATION_DBU = 11.5


def source_provenance():
    root = Path(__file__).resolve().parents[2]
    try:
        commit = subprocess.run(['git', '-C', root, 'rev-parse', 'HEAD'], capture_output=True,
                                text=True, check=True).stdout.strip()
        dirty = bool(subprocess.run(['git', '-C', root, 'status', '--porcelain', '--', 'Source', 'Tests', 'Tools'],
                                    capture_output=True, text=True, check=True).stdout.strip())
        return dict(tool_commit=commit, tool_tree_dirty=dirty,
                    note='Commit of this checkout; the renderer binary hash identifies the DSP under test')
    except (OSError, subprocess.CalledProcessError):
        return dict(tool_commit=None, tool_tree_dirty=None, note='Not a git checkout')


def variants(item):
    result = [('digital_equal', 0.)]
    calibration = item['metadata'].get('input_level_dbu')
    if calibration is not None:
        result.append(('metadata_11_5_dbu', CALIBRATION_DBU - float(calibration)))
    return result


def run(models, manifest_path, nam_render, original_render, out, workers=2, states_path=None, log=print):
    out.mkdir(parents=True, exist_ok=True)
    manifest = references.load(manifest_path)
    suite = state_suite.suite(states_path)
    probe = protocol.make_probe()
    probe_path = out / 'probe.wav'
    wavfile.write(probe_path, SR, probe)
    probe_sha, original_sha, nam_sha = (renderers.digest(p) for p in (probe_path, original_render, nam_render))

    def original_case(name):
        info, audio, sha = renderers.original(original_render, original_sha, probe_path, probe_sha,
                                              out, name, suite[name])
        return name, dict(state=suite[name], renderer=info, renderer_info=renderers.parse_info(info),
                          measurements=analysis.measure(audio), audio_sha256=sha), audio

    def reference_case(item):
        model = models / item['file']
        model_sha = renderers.digest(model)
        if model_sha != item['sha256']:
            raise ValueError(f'Reference hash mismatch: {model}')
        result = dict(family=item['family'], model_id=item['model_id'], name=item['name'],
                      capture_scope=item['capture_scope'], gain_group=item['gain_group'],
                      sha256=item['sha256'], variants={})
        for variant, gain in variants(item):
            y, sha = renderers.nam(nam_render, nam_sha, model, model_sha,
                                   (probe * 10 ** (gain / 20)).astype(np.float32),
                                   out / f"nam-{item['model_id']}-{variant}")
            comparisons = {}
            # Cab-inclusive captures cannot establish amp-only spectral parity.
            if item['head_timbre_comparison_eligible']:
                for name, audio in original_audio.items():
                    comparisons[name] = {kind: analysis.compare_segments(
                        y[protocol.segment(kind)].astype(np.float64),
                        audio[protocol.segment(kind)].astype(np.float64)) for kind in protocol.SEGMENTS}
            result['variants'][variant] = dict(input_gain_db=gain, measurements=analysis.measure(y),
                                               spectral_comparisons=comparisons, audio_sha256=sha)
        log('NAM', item['family'], item['model_id'], flush=True)
        return result

    originals, original_audio = {}, {}
    with concurrent.futures.ThreadPoolExecutor(max_workers=max(1, workers)) as pool:
        for name, record, audio in pool.map(original_case, suite):
            originals[name], original_audio[name] = record, audio
            log('ORIGINAL', name, flush=True)
        refs = list(pool.map(reference_case, manifest['references']))
    report = dict(schema=1, analysis_version=ANALYSIS_VERSION,
                  manifest_sha256=renderers.digest(manifest_path), probe_sha256=probe_sha,
                  nam_renderer_sha256=nam_sha, original_renderer_sha256=original_sha,
                  states_sha256=renderers.digest(states_path) if states_path else None,
                  source=source_provenance(), protocol=protocol.describe(),
                  original=originals, references=refs)
    (out / 'comparison.json').write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n')
    log('COMPLETE', len(refs), 'NAM and', len(originals), 'Original states', flush=True)
    return report
