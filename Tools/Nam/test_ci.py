"""Negative tests for evidence identity and fail-closed acceptance, no DSP mocks."""
import copy
import json
from pathlib import Path
import subprocess
import tempfile
import unittest

from ci_pipeline import NAMES, THRESHOLDS, numeric_pass, seal, verify_dataset, verify_training
from ci_sources import LOCK, check_checkout, sha256


class EvidenceTests(unittest.TestCase):
    def test_current_profile_rejects_bad_quiet_decay_and_active_audio(self):
        import numpy as np
        from quality_profile import fidelity
        ref = np.zeros(48000)
        ref[10000:30000] = .1 * np.sin(np.arange(20000) * .03)
        self.assertTrue(fidelity(ref, ref, 'Fenrir')['fidelity_pass'])
        bad = ref.copy()
        bad[35000:] = .03
        result = fidelity(bad, ref, 'Fenrir')
        self.assertFalse(result['quiet_pass'])
        self.assertFalse(result['fidelity_pass'])
        self.assertFalse(fidelity(ref * .5, ref, 'Fenrir')['fidelity_pass'])

    def test_metric_failures_and_missing_nonfinite(self):
        good = {k: v / 2 for k, v in THRESHOLDS.items()}
        self.assertTrue(numeric_pass(good))
        for key in THRESHOLDS:
            for value in (float('nan'), float('inf'), THRESHOLDS[key] * 1.01):
                self.assertFalse(numeric_pass(dict(good, **{key: value})))
            missing = dict(good)
            del missing[key]
            self.assertFalse(numeric_pass(missing))
        self.assertFalse(numeric_pass(dict(good, rms_error_db=-.51)))

    def test_source_mismatch_and_dirty_checkout(self):
        with tempfile.TemporaryDirectory() as td:
            root = Path(td)
            subprocess.run(['git', 'init', '-q', td], check=True)
            (root / 'source').write_text('original')
            subprocess.run(['git', '-C', td, 'add', 'source'], check=True)
            subprocess.run(['git', '-C', td, '-c', 'user.name=Test', '-c', 'user.email=test@example.invalid', 'commit', '-qm', 'fixture'], check=True)
            sha = subprocess.check_output(['git', '-C', td, 'rev-parse', 'HEAD'], text=True).strip()
            check_checkout(root, sha)
            with self.assertRaisesRegex(RuntimeError, 'mismatch'):
                check_checkout(root, '0' * 40)
            (root / 'source').write_text('tampered')
            with self.assertRaisesRegex(RuntimeError, 'Modified'):
                check_checkout(root, sha)

    def test_audio_identity_and_channel_completeness(self):
        with tempfile.TemporaryDirectory() as td:
            root = Path(td)
            (root / 'audio').write_bytes(b'capture')
            digest = sha256(root / 'audio')
            manifest = {'source': {'source_commit': LOCK['capture']}, 'captures': [
                {'split': s, 'name': n, 'input': 'audio', 'output': 'audio', 'input_sha256': digest, 'output_sha256': digest}
                for s in ('train', 'validation', 'test', 'audition') for n in NAMES]}
            path = root / 'manifest.json'
            path.write_text(json.dumps(manifest))
            verify_dataset(root)
            bad = copy.deepcopy(manifest)
            bad['captures'].pop()
            path.write_text(json.dumps(bad))
            with self.assertRaisesRegex(RuntimeError, 'Missing'):
                verify_dataset(root)
            path.write_text(json.dumps(manifest))
            (root / 'audio').write_bytes(b'tampered')
            with self.assertRaisesRegex(RuntimeError, 'hash mismatch'):
                verify_dataset(root)

    def test_missing_pipeline_result_never_approves_and_hashes_partial_evidence(self):
        with tempfile.TemporaryDirectory() as td:
            root = Path(td)
            (root / 'checkpoint.pt').write_bytes(b'partial-checkpoint')
            (root / 'trigger.json').write_text(json.dumps({'profile': 'full', 'reason': 'maintainer-label'}))
            seal(root, 'a' * 40, 'full')
            result = json.loads((root / 'acceptance.json').read_text())
            self.assertEqual(result['status'], 'FAIL')
            self.assertFalse(result['release_approved'])
            self.assertEqual(result['instrument_DI_listening'], 'BLOCKED')
            self.assertEqual(result['GUI_DAW_acceptance'], 'BLOCKED')
            manifest = json.loads((root / 'artifact-manifest.json').read_text())
            self.assertEqual(manifest['files']['checkpoint.pt'], sha256(root / 'checkpoint.pt'))
            self.assertEqual(manifest['files']['trigger.json'], sha256(root / 'trigger.json'))
            (root / 'acceptance.json').write_text(json.dumps({'status': 'PASS', 'release_approved': True}))
            seal(root, 'a' * 40, 'smoke')
            self.assertFalse(json.loads((root / 'acceptance.json').read_text())['release_approved'])

    def test_training_requires_matching_dataset_models_and_checkpoint(self):
        with tempfile.TemporaryDirectory() as td:
            root = Path(td)
            (root / 'data').mkdir()
            run = root / 'run'
            run.mkdir()
            manifest = {'source': {'source_commit': LOCK['capture']}}
            (root / 'data/manifest.json').write_text(json.dumps(manifest))
            config = {'dataset_sha256': sha256(root / 'data/manifest.json'), 'source': manifest['source'],
                      'trainer_commit': LOCK['trainer']['commit'], 'target_player_commit': LOCK['player']['commit']}
            (run / 'training-config.json').write_text(json.dumps(config))
            checkpoint = run / 'checkpoint.pt'
            checkpoint.write_bytes(b'checkpoint fixture; never deserialized')
            rows = {}
            for name in NAMES:
                path = run / f'Nastrond-{name}.nam'
                path.write_text(name)
                rows[name] = {'sha256': sha256(path), 'official_roundtrip_max_abs': 0.}
            (run / 'validation.json').write_text(json.dumps({'channels': rows}))
            verify_training(root, manifest)
            checkpoint.unlink()
            with self.assertRaisesRegex(RuntimeError, 'checkpoint'):
                verify_training(root, manifest)
            checkpoint.write_bytes(b'checkpoint fixture')
            (run / 'Nastrond-Fenrir.nam').write_text('wrong export')
            with self.assertRaisesRegex(RuntimeError, 'hash mismatch'):
                verify_training(root, manifest)
            config['source'] = {'source_commit': 'wrong'}
            (run / 'training-config.json').write_text(json.dumps(config))
            with self.assertRaisesRegex(RuntimeError, 'source mismatch'):
                verify_training(root, manifest)


if __name__ == '__main__':
    unittest.main()
