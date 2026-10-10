#!/usr/bin/env python3
"""Numerical controls and end-to-end contracts, never a product/CPU speed gate."""
import copy
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'Tools'))
import measure_niflheimr as m

RENDERER = Path(sys.argv.pop(1)).resolve() if len(sys.argv) > 1 and not sys.argv[1].startswith('-') else None


class NumericalControls(unittest.TestCase):
    def test_header_fingerprint_has_platform_independent_order(self):
        with tempfile.TemporaryDirectory() as temp:
            directory = Path(temp)
            for name in ('beta.h', 'Alpha.h', 'Zed.h'):
                (directory / name).write_bytes(name.encode())
            records = ''.join(f'{name}:{m.digest(directory / name)}\n' for name in ('Alpha.h', 'Zed.h', 'beta.h'))
            self.assertEqual(m.header_fingerprint(directory), hashlib.sha256(records.encode()).hexdigest())

    def test_failed_run_preserves_diagnostics_without_success_evidence(self):
        with tempfile.TemporaryDirectory() as temp:
            destination = Path(temp) / 'result'
            with self.assertRaisesRegex(RuntimeError, 'diagnostic files preserved'):
                with m.measurement_stage(destination) as stage:
                    (stage / 'probe.wav').write_bytes(b'fixture')
                    raise ValueError('deliberate hash mismatch')
            self.assertFalse(destination.exists())
            failed = next(Path(temp).glob('result.failed-*'))
            self.assertEqual((failed / 'probe.wav').read_bytes(), b'fixture')
            report = json.loads((failed / 'failure.json').read_text())
            self.assertIs(report['release_approved'], False)
            self.assertEqual(report['status'], 'FAILED_NOT_ACCEPTANCE_EVIDENCE')

    def test_first_read_truncation_is_preserved_and_rejected(self):
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp) / 'output.wav'
            m.float_wav(path, 96000, np.zeros((257792, 2)))
            complete = path.read_bytes()
            row = {'sha256': hashlib.sha256(complete).hexdigest()}
            path.write_bytes(complete[:829498])
            with self.assertRaisesRegex(ValueError, 'Output integrity mismatch'):
                m.read_output(path, row, len(complete), [])
            observations = json.loads((path.parent / 'io-observations.json').read_text())
            self.assertEqual(observations[0]['phase'], 'parent_first_read')
            self.assertEqual(observations[0]['bytes_read'], 829498)
            self.assertEqual(observations[0]['expected_bytes'], 2062394)
            self.assertEqual((path.parent / 'output.wav.first-read.bin').read_bytes(), complete[:829498])

    @unittest.skipUnless(RENDERER, 'Pass built ChimeraNiflheimrRender for integration')
    def test_96k_8x_gain075_standard_length_publication(self):
        # Exact formerly failing route: stereo, 257792 frames, 2062394 bytes.
        # Three fresh subprocesses exercise close / publish / parent first read.
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            rate, n, frames = 96000, 32768, 257792
            k = round(3001 * n / rate) | 1
            x = 10 ** (-12 / 20) * np.sin(2 * np.pi * k * np.arange(frames) / n)
            source, controls = root / 'input.wav', root / 'controls.json'
            m.float_wav(source, rate, np.column_stack((x, x)))
            m.save_json(controls, {'controls': {'gain': .75, 'blend': 1.}})
            previous = None
            for repeat in range(3):
                out = root / f'render-{repeat}'
                result = subprocess.run([str(RENDERER), str(source), str(out), '--controls', str(controls),
                                         '--oversampling', '8', '--block-size', '256', '--tail-seconds', '0',
                                         '--input-kind', 'synthetic'], capture_output=True, text=True,
                                        encoding='utf-8', timeout=120)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                manifest = json.loads((out / 'manifest.json').read_text())
                self.assertIs(manifest['release_approved'], False)
                observations = [json.loads(line[len('NIFLHEIMR_IO '):]) for line in result.stdout.splitlines()
                                if line.startswith('NIFLHEIMR_IO ')]
                self.assertEqual(len(observations), 20)
                hashes = []
                for row in manifest['outputs']:
                    raw = m.read_output(out / row['file'], row, 2062394, observations)
                    actual_rate, audio = m.read_wav(None, raw=raw)
                    self.assertEqual(actual_rate, rate)
                    self.assertEqual(audio.shape, (frames, 2))
                    self.assertEqual(row['controls']['gain'], .75)
                    phases = [o for o in observations if o['file'] == row['file']]
                    self.assertEqual([o['phase'] for o in phases],
                                     ['post_flush', 'post_file_publish', 'pre_publish', 'post_publish', 'parent_first_read'])
                    for observation in phases:
                        self.assertEqual(observation['sha256'], row['sha256'])
                        for field in ('bytes', 'bytes_read', 'post_read_bytes'):
                            self.assertEqual(observation[field], 2062394)
                    hashes.append(row['sha256'])
                if previous is not None:
                    self.assertEqual(hashes, previous)
                previous = hashes

    def test_linear_and_true_inband_harmonics_are_not_foldback(self):
        n, k = 32768, 2051
        t = 2 * np.pi * k * np.arange(n) / n
        linear = m.alias_metrics(m.spectrum(np.sin(t)), n, k)
        harmonic = m.alias_metrics(m.spectrum(np.sin(t) + .2*np.sin(3*t)), n, k)
        self.assertLess(linear['foldback_candidate_dbc'], -180)
        self.assertLess(harmonic['foldback_candidate_dbc'], -180)
        self.assertAlmostEqual(10 ** (linear['output_total_dbfs']/10), .5, places=12)

    def test_known_folded_third_harmonic_has_correct_energy(self):
        n, k = 32768, 7001  # third harmonic is beyond Nyquist
        t = 2 * np.pi * k * np.arange(n) / n
        power = m.spectrum(np.sin(t) + .1*np.sin(3*t))
        metrics = m.alias_metrics(power, n, k)
        self.assertAlmostEqual(metrics['foldback_candidate_dbc'], -20, places=8)
        self.assertAlmostEqual(metrics['off_harmonic_residual_dbc'], -20, places=8)

    def test_float_roundtrip_preserves_headroom(self):
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp) / 'float.wav'
            samples = np.array([[2., -3.], [.25, -.5]], dtype=np.float32)
            m.float_wav(path, 48000, samples)
            rate, actual = m.read_wav(path)
            self.assertEqual(rate, 48000)
            np.testing.assert_array_equal(actual, samples)

    def test_cpu_summary_is_descriptive_and_retains_spikes(self):
        values = [1., 2., 3., 100.]
        summary = m.summarize_cpu({'block_budget_us': 10., 'measured_blocks_per_repeat': 4,
                                  'repetitions': [{'repeat': 0, 'block_us': values}]})
        self.assertEqual(summary['repeats'][0]['observed_over_budget_blocks'], 1)
        self.assertEqual(summary['repeats'][0]['max_us'], 100.)
        self.assertEqual(summary['median_of_repeat_medians_us'], 2.5)
        self.assertEqual(summary['verdict'], 'MEASURED_NO_HARD_CPU_GATE')
        with self.assertRaises(ValueError):
            m.summarize_cpu({'block_budget_us': 10., 'measured_blocks_per_repeat': 1,
                             'repetitions': [{'repeat': 0, 'block_us': [float('nan')]}]})

    def test_comparison_requires_matching_provenance_and_complete_routes(self):
        summary = {'median_of_repeat_medians_us': 10., 'repeat_median_min_us': 9., 'repeat_median_max_us': 11.}
        baseline = {'schema': m.SCHEMA, 'protocol': m.PROTOCOL, 'configuration': {},
                    'environment': {'cpu_model': 'fixture-cpu', 'ci': {}}, 'build': {},
                    'source': {'head': 'before'}, 'cpu': [
                        {'id': 'route', 'summary': summary, 'controls': {'gain': .5}, 'input_sha256': 'hash'}]}
        current = copy.deepcopy(baseline)
        current['source']['head'] = 'after'
        current['cpu'][0]['summary']['median_of_repeat_medians_us'] = 12.
        compared = m.comparison(current, baseline)
        self.assertEqual(compared['status'], 'DESCRIPTIVE_COMPARISON_ONLY')
        self.assertEqual(compared['rows'][0]['ratio'], 1.2)
        self.assertIs(compared['release_approved'], False)
        for key, value in [('cpu_model', 'other-cpu'), ('affinity', [3]), ('os_release', 'other-os')]:
            different = copy.deepcopy(current)
            different['environment'][key] = value
            self.assertEqual(m.comparison(different, baseline)['status'], 'NOT_COMPARABLE')
        different = copy.deepcopy(current)
        different['cpu'][0]['input_sha256'] = 'changed'
        self.assertEqual(m.comparison(different, baseline)['status'], 'NOT_COMPARABLE')
        different['cpu'] = []
        self.assertEqual(m.comparison(different, baseline)['status'], 'NOT_COMPARABLE')

    @unittest.skipUnless(RENDERER, 'Pass built ChimeraNiflheimrRender for integration')
    def test_real_renderer_evidence_and_negative_paths(self):
        with tempfile.TemporaryDirectory() as temp:
            output = Path(temp) / 'measurement'
            argv = [sys.executable, str(m.ROOT / 'Tools/measure_niflheimr.py'), '--renderer', str(RENDERER),
                    '--output', str(output), '--profile', 'smoke', '--factors', '1', '--machine-label', 'contract-test']
            result = subprocess.run(argv, capture_output=True, text=True, encoding='utf-8', timeout=240)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            evidence = json.loads((output / 'evidence.json').read_text())
            self.assertEqual(len(evidence['alias']), 5)
            self.assertEqual(len(evidence['cpu']), 5)
            self.assertIs(evidence['release_approved'], False)
            self.assertTrue(all(value == 'PENDING_MANUAL_EVIDENCE' for value in evidence['manual_acceptance'].values()))
            self.assertEqual(evidence['comparison']['status'], 'NO_BASELINE')
            for path, expected in evidence['files'].items():
                self.assertEqual(m.digest(output / path), expected)
            for row in evidence['cpu']:
                manifest = json.loads((output / row['manifest']).read_text())
                channel = next(x for x in manifest['outputs'] if x['channel_key'] == row['channel_key'])
                self.assertEqual(len(channel['cpu']['repetitions']), 2)
                self.assertEqual(len(channel['cpu']['repetitions'][0]['block_us']), 64)
                self.assertEqual(channel['cpu']['repetitions'][0]['output_energy_checksum'],
                                 channel['cpu']['repetitions'][1]['output_energy_checksum'])
                self.assertEqual(manifest['build']['configuration'], 'Release')
            for row in evidence['alias']:
                with np.load(output / row['spectrum']) as spectra:
                    self.assertEqual(spectra['power_fs2'].shape, (2, 2, 4097))
                    self.assertTrue(np.isfinite(spectra['power_fs2']).all())
            original = m.digest(output / 'evidence.json')
            self.assertNotEqual(subprocess.run(argv, capture_output=True).returncode, 0)
            self.assertEqual(m.digest(output / 'evidence.json'), original)
            argv[argv.index('--output')+1] = str(Path(temp) / 'invalid')
            self.assertNotEqual(subprocess.run(argv + ['--gains', 'nan'], capture_output=True).returncode, 0)
            self.assertFalse((Path(temp) / 'invalid').exists())
            self.assertFalse(list(Path(temp).glob('*.partial-*')))


if __name__ == '__main__':
    unittest.main()
