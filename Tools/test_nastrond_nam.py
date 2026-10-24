#!/usr/bin/env python3
"""Regression tests for the Náströnd NAM toolchain. Needs numpy/scipy, no NAM files.

Frozen evidence must regenerate exactly; the comparison runs end to end
against stand-in renderers with a stand-in manifest.
"""
import hashlib
import json
import os
import stat
import sys
import tempfile
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'Tools'))

import nastrond_nam_tool  # noqa: E402
from nastrond_nam import analysis, compare, protocol, references, states, summary  # noqa: E402
from validate_nam import SR  # noqa: E402

MANIFEST = ROOT / 'docs/reference/nastrond-nam-manifest.json'
EVIDENCE = ROOT / 'docs/evidence'
FROZEN_GROUPS = {'crush': ('vh4', [569473, 569474, 569477]), 'impact': ('twinjet', [652436, 652437]),
                 'rot': ('granophyre', [403977, 421075, 421074]), 'bloom': ('matamp', [614727, 561845, 543854])}


def raises(function, text):
    try:
        function()
    except ValueError as error:
        assert text in str(error), error
        return
    raise AssertionError(f'Expected ValueError containing {text!r}')


def test_protocol():
    assert protocol.TOTAL_SAMPLES == 37 * SR and protocol.TONE_SECONDS == 21
    assert len(protocol.make_probe()) == protocol.TOTAL_SAMPLES
    # v1 offsets: one second per step, last 250 ms measured; notes from 21 s.
    assert protocol.tone_window(1, 2) == slice(9 * SR + 3 * SR // 4, 10 * SR)
    assert protocol.segment('chord') == slice(29 * SR, 37 * SR)
    assert list(protocol.harmonic_bins(400)) == [int(400 * k / 4) for k in range(1, 13)]
    raises(lambda: protocol.harmonic_bins(401), 'not coherent')


def test_references():
    manifest = references.load(MANIFEST)
    assert references.target_groups(manifest) == FROZEN_GROUPS
    for change, text in [({'role': 'FUZZ'}, 'Unknown role'), ({'file': '../x.nam'}, 'escapes'),
                         ({'capture_scope': 'amp_cab'}, 'amp-head'), ({'sample_rate': 44100}, 'sample rate')]:
        broken = json.loads(MANIFEST.read_text())
        broken['references'][8].update(change)
        with tempfile.NamedTemporaryFile('w', suffix='.json', delete=False) as handle:
            json.dump(broken, handle)
        try:
            raises(lambda: references.load(handle.name), text)
        finally:
            os.unlink(handle.name)


def test_states():
    assert len(states.frozen_v1()) == 17
    with tempfile.TemporaryDirectory() as directory:
        path = Path(directory) / 'states.json'
        for extra, text in [({'comparison': {}}, 'reserved'), ({'Bad-Name': {}}, 'Unsafe'),
                            ({'gain_max': {}}, 'must not replace'),
                            ({'both': {'preset': 'original.nastrond.fenrir.v1', 'channel': 0}}, 'mutually exclusive')]:
            path.write_text(json.dumps(extra))
            raises(lambda: states.suite(path), text)
        path.write_text(json.dumps({'channel_surtr': {'channel': 1}}))
        assert 'channel_surtr' in states.suite(path) and len(states.suite(path)) == 18


def test_band_metric_and_polarity():
    rng = np.random.default_rng(7)
    x = rng.standard_normal(8 * SR)
    from scipy import signal
    low = signal.lfilter(*signal.butter(2, 250 / (SR / 2)), x)
    bass_cut = x - .9 * low  # ~-20 dB below 250 Hz, unchanged treble
    result = analysis.compare_segments(x, bass_cut)
    # The linear-bin v1 metric barely sees a 20 dB bass loss; octave bands do.
    assert result['band_spectrum_rms_db'] > 2 * result['log_spectrum_rms_db'], result
    inverted = analysis.compare_segments(x, -x)
    assert inverted['polarity'] == -1 and inverted['correlation'] < -.99
    assert inverted['band_spectrum_rms_db'] < 1e-6


def test_frozen_evidence_regenerates():
    report = json.loads((EVIDENCE / 'nastrond-nam-20261005/comparison.json').read_text())
    assert summary.gain_csv(report) == (EVIDENCE / 'nastrond-nam-20261005/gain-summary.csv').read_text()
    folder = EVIDENCE / 'nastrond-macro-direction-20261005'
    frozen = json.loads((folder / 'summary.json').read_text())
    with tempfile.TemporaryDirectory() as directory:
        out = Path(directory) / 'summary.json'
        nastrond_nam_tool.main(['audit', '--baseline', str(EVIDENCE / 'nastrond-nam-20261005/comparison.json'),
                                '--incoming', str(EVIDENCE / 'nastrond-voicing-20261005/comparison.json'),
                                '--candidate', str(folder / 'comparison.json'), '--out', str(out)])
        result = json.loads(out.read_text())
    assert {key: result[key] for key in frozen} == frozen
    # Frozen reports predate the octave-band metric; the audit must say so, not guess.
    reports = {label: json.loads((EVIDENCE / name / 'comparison.json').read_text()) for label, name in
               [('baseline', 'nastrond-nam-20261005'), ('incoming', 'nastrond-voicing-20261005'),
                ('candidate', 'nastrond-macro-direction-20261005')]}
    raises(lambda: summary.audit(reports, FROZEN_GROUPS, metric='band_spectrum_rms_db'), 'analysis_version 2')


def executable(path, body):
    path.write_text(f'#!{sys.executable}\n' + body)
    path.chmod(path.stat().st_mode | stat.S_IXUSR)
    return path


def test_compare_end_to_end():
    with tempfile.TemporaryDirectory() as directory:
        root = Path(directory)
        calls = root / 'calls.log'
        common = f"""import json, sys
import numpy as np
from scipy.io import wavfile
open({str(calls)!r}, 'a').write(sys.argv[0] + '\\n')
"""
        original = executable(root / 'original', common + """
_, source, dest, config = sys.argv
state = json.load(open(config))
drive = 4 + 30 * state.get('controls', {}).get('gain', .72) + (5 if 'preset' in state else 0)
rate, x = wavfile.read(source)
np.tanh(drive * x.astype(np.float64)).astype(np.float32).tofile(dest)
print('rate=48000 oversampling=4 latency=0 gain=0.72 samples=%d' % len(x))
""")
        nam = executable(root / 'nam', common + """
_, _, _, model, source, dest = sys.argv
rate, x = wavfile.read(source)
wavfile.write(dest, rate, (-.5 * np.tanh(json.load(open(model))['k'] * x)).astype(np.float32))
""")
        manifest = json.loads(MANIFEST.read_text())
        picked = [next(r for r in manifest['references'] if r['family'] == family) for family in ('meshuggah', 'vh4')]
        (root / 'models').mkdir()
        for i, item in enumerate(picked):
            model = root / 'models' / f'{i}.nam'
            model.write_text(json.dumps({'k': 10 + 40 * i}))
            item.update(file=model.name, sha256=hashlib.sha256(model.read_bytes()).hexdigest())
        manifest['references'] = picked
        manifest_path = root / 'manifest.json'
        manifest_path.write_text(json.dumps(manifest))
        out = root / 'out'
        args = (root / 'models', manifest_path, nam, original, out)
        report = compare.run(*args, workers=3, log=lambda *a, **k: None)
        first_calls = len(calls.read_text().splitlines())
        assert first_calls == 17 + 1 + 2, first_calls  # VH4 has a metadata-calibrated variant
        assert compare.run(*args, workers=3, log=lambda *a, **k: None) == report
        assert len(calls.read_text().splitlines()) == first_calls, 'Cached renders were repeated'

        assert report['analysis_version'] == 2 and report['protocol'] == json.loads(
            (EVIDENCE / 'nastrond-nam-20261005/comparison.json').read_text())['protocol']
        assert report['original']['default']['renderer_info']['latency'] == 0
        cab, head = report['references']
        assert cab['variants']['digital_equal']['spectral_comparisons'] == {}
        assert set(head['variants']) == {'digital_equal', 'metadata_11_5_dbu'}
        scores = head['variants']['digital_equal']['spectral_comparisons']['default']['chord']
        assert scores['polarity'] == -1 and np.isfinite(scores['band_spectrum_rms_db'])
        response = report['original']['gain_max']['measurements']['response'][1]
        assert response['readings'][-1]['thd_2_to_12'] > .2, 'A driven tanh stage must measure harmonics'
        assert response['growth_24db_input_db'] < 20, 'A saturating stage must compress level growth'
        assert summary.gain_csv(json.loads((out / 'comparison.json').read_text())).count('\n') == 1 + 3 * (17 + 3)


def main():
    for name, test in list(globals().items()):
        if name.startswith('test_'):
            test()
            print('PASS', name)


if __name__ == '__main__':
    main()
