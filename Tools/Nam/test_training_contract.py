import copy
import hashlib
import json
from pathlib import Path
import tempfile
import unittest

from training_contract import training_identity, verify_audio_manifest


class ContractTests(unittest.TestCase):
    def test_resume_identity_covers_optimizer_sampler_and_export_response(self):
        config = dict(recipe='legacy', batch=2, frames=2048, tail_fraction=0.,
                      center_training_output=False, learning_rate={'initial': .004},
                      validation_selection='full', dataset_sha256='a', identity='ignored')
        expected = training_identity(config)
        for key, value in [('frames', 4096), ('batch', 4), ('tail_fraction', .1),
                           ('center_training_output', True), ('learning_rate', {'initial': .001}),
                           ('dataset_sha256', 'b'), ('validation_selection', 'window')]:
            changed = copy.deepcopy(config)
            changed[key] = value
            with self.subTest(key=key):
                self.assertNotEqual(training_identity(changed), expected)
        config['warm_start'] = {'run': '/moved/path'}
        self.assertEqual(training_identity(config), expected)

    def test_split_reuse_and_changed_audio_rejected(self):
        with tempfile.TemporaryDirectory() as tmp:
            p = Path(tmp)
            rows = []
            for split in ('train', 'validation', 'test', 'audition'):
                (p / split).mkdir()
                for name in ('Fenrir', 'Surtr', 'Nidhoggr', 'Fimbulvetr', 'Ragnarok'):
                    row = {'split': split, 'name': name}
                    for kind in ('input', 'output'):
                        path = p / split / (kind + (name if kind == 'output' else '') + '.npy')
                        path.write_bytes((split + kind + (name if kind == 'output' else '')).encode())
                        row[kind] = str(path.relative_to(p))
                        row[kind + '_sha256'] = hashlib.sha256(path.read_bytes()).hexdigest()
                    rows.append(row)
            manifest = {'captures': rows}
            (p / 'manifest.json').write_text(json.dumps(manifest))
            verify_audio_manifest(p)
            for row in rows:
                if row['split'] == 'test':
                    row['input'] = rows[0]['input']
                    row['input_sha256'] = rows[0]['input_sha256']
            (p / 'manifest.json').write_text(json.dumps(manifest))
            with self.assertRaisesRegex(RuntimeError, 'reused'):
                verify_audio_manifest(p)
            (p / rows[0]['output']).write_bytes(b'changed')
            with self.assertRaisesRegex(RuntimeError, 'hash mismatch'):
                verify_audio_manifest(p)


if __name__ == '__main__':
    unittest.main()
