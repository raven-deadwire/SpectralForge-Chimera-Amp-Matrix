#!/usr/bin/env python3
"""Coordinator contracts; optional real production renderer smoke, never listening PASS."""
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch
import subprocess
import numpy as np
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'Tools'))
import audition_niflheimr as a


class AuditionTests(unittest.TestCase):
    def fixture(self, root):
        config = json.loads((a.Path(__file__).resolve().parents[1] / 'Validation/audition/niflheimr-di-template.json').read_text())
        config['segments'] = [{'id': 'repeat', 'start_seconds': .01, 'end_seconds': .1}]
        config['di'], config['ir'] = 'di.wav', 'ir.wav'
        t = np.arange(9600) / 48000
        di = (.12 * np.sin(2 * np.pi * 55 * t))[:, None]
        a.write_float(root / 'di.wav', 48000, di)
        a.write_float(root / 'ir.wav', 48000, np.pad(np.array([[1.], [.2], [-.1]]), ((0, 29), (0, 0))))
        a.save_json(root / 'config.json', config)
        return config

    def fake(self, command, **_):
        source, dest = Path(command[1]), Path(command[2]); dest.mkdir()
        rate, di = a.load_audio(source); rows = []
        for channel in range(5):
            signal = np.pad(np.repeat(di, 2, axis=1), ((13, 96000), (0, 0))) * (channel + 1)
            file = f'CH{channel+1}.wav'; a.write_float(dest / file, rate, signal)
            rows.append({'channel_index': channel, 'file': file, 'sha256': a.digest(dest / file), 'latency_samples': 13})
        manifest = {'input_sha256': a.digest(source), 'outputs': rows}
        if '--controls' not in command:
            manifest['ir_sha256'] = a.digest(command[3])
        a.save_json(dest / 'manifest.json', manifest)
        return subprocess.CompletedProcess(command, 0, '', '')

    def test_matching_preserves_transients_and_common_ceiling(self):
        x = np.array([[.1], [.2], [-.4], [1.]])
        matched, gains, target = a.match_group([x, x * 7], -3, -6)
        np.testing.assert_allclose(matched[0], matched[1])
        np.testing.assert_allclose(matched[0], x * gains[0])
        self.assertAlmostEqual(max(np.max(np.abs(y)) for y in matched), 10 ** (-6 / 20))
        self.assertLess(target, -3)
        with self.assertRaises(ValueError): a.match_group([np.zeros((8, 1))])

    def test_missing_di_plan_does_not_render_or_approve(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp); config = self.fixture(root); config['di'] = None
            a.save_json(root / 'config.json', config)
            a.execute(root / 'config.json', root / 'plan', None, None, plan=True)
            result = json.loads((root / 'plan/plan.json').read_text())
            self.assertEqual(result['status'], 'BLOCKED_MISSING_DI'); self.assertFalse(result['release_approved'])
            with self.assertRaises(ValueError): a.execute(root / 'config.json', root / 'run', None, None)

    def test_pipeline_baseline_alignment_and_separate_scopes(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp); self.fixture(root)
            for name in ('head', 'rig', 'oldhead', 'oldrig'): (root / name).write_text(name)
            with patch.object(a.subprocess, 'run', side_effect=self.fake):
                a.execute(root / 'config.json', root / 'run', root / 'head', root / 'rig', root / 'oldhead', root / 'oldrig')
            report = json.loads((root / 'run/report.json').read_text())
            self.assertEqual(len(report['results']), 30)
            self.assertEqual({r['mode'] for r in report['results']}, {'head-only','head-cab','preset-cab'})
            self.assertEqual(report['musical_acceptance'], 'PENDING'); self.assertFalse(report['release_approved'])
            for row in report['results']:
                rate, audio = a.load_audio(root / 'run' / row['file'])
                self.assertEqual(len(audio), 4320)
                self.assertAlmostEqual(20 * np.log10(np.sqrt(np.mean(audio.astype(float)**2))), row['rms_dbfs'], places=5)
            with self.assertRaises(ValueError): a.execute(root / 'config.json', root / 'run', root / 'head', root / 'rig')

    def test_bad_hash_preserves_failure_without_completed_report(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp); self.fixture(root)
            for name in ('head', 'rig'): (root / name).write_text(name)
            def broken(command, **kwargs):
                result = self.fake(command, **kwargs)
                manifest_path = Path(command[2]) / 'manifest.json'
                m = json.loads(manifest_path.read_text());m['outputs'][0]['sha256'] = '0' * 64
                a.save_json(manifest_path, m);return result
            with patch.object(a.subprocess, 'run', side_effect=broken):
                with self.assertRaises(RuntimeError): a.execute(root / 'config.json', root / 'run', root / 'head', root / 'rig')
            self.assertFalse((root / 'run').exists());self.assertEqual(len(list(root.glob('run.failed-*'))), 1)


if __name__ == '__main__':
    if len(sys.argv) in (3, 4):
        # Optional real-binary route test: short synthetic WAV and identity-like
        # IR exercise installation, complete presets and all three output scopes.
        from contextlib import nullcontext
        evidence = Path(sys.argv[3]) if len(sys.argv) == 4 else None
        if evidence is not None:
            evidence.mkdir(parents=True, exist_ok=True)
        context = nullcontext(tempfile.mkdtemp(prefix='synthetic-', dir=evidence)) if evidence else tempfile.TemporaryDirectory()
        with context as tmp:
            root = Path(tmp);AuditionTests().fixture(root)
            a.execute(root / 'config.json', root / 'run', sys.argv[1], sys.argv[2])
            r = json.loads((root / 'run/report.json').read_text())
            assert len(r['results']) == 15 and r['musical_acceptance'] == 'PENDING'
            print('COMPLETED_SYNTHETIC_ROUTE_CONTRACT_NOT_LISTENING_ACCEPTANCE')
    else:
        unittest.main()
