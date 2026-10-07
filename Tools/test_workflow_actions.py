#!/usr/bin/env python3
"""Node 24 migration contract. Run with Python 3.12 and PyYAML 6.0.3."""
import hashlib
import json
from pathlib import Path
import unittest

import yaml

ROOT = Path(__file__).resolve().parents[1]
PINS = {
    'actions/checkout': ('v4', 'v5'),
    'actions/setup-python': ('v5', 'v6'),
    'actions/setup-node': ('v4', 'v5'),
    'actions/upload-artifact': ('v4', 'v6'),
    'actions/download-artifact': ('v4', 'v7'),
}
OLD_CHECKOUT = '11bd71901bbe5b1630ceea73d27597364c9af683'
NEW_CHECKOUT = 'fbc6f3992d24b796d5a048ff273f7fcc4a7b6c09'
KINDS = ('windows', 'linux', 'macos', 'assembly')
OLD_DOWNLOAD = """      - name: Download this run's evidence without merging platform files
        uses: actions/download-artifact@v4
        with:
          pattern: Chimera-release-evidence-*
          path: release-inputs
          merge-multiple: false
"""


LAYOUT_STEP = """      - name: Preserve v4 unmerged singleton artifact layout
        if: always()
        env:
          GH_TOKEN: ${{ github.token }}
        run: python Tools/artifact_download_layout.py --path release-inputs --pattern 'Chimera-release-evidence-*'
"""
LAYOUT_PERMISSIONS = """    permissions:
      contents: read
      actions: read
"""


def reverse_migration(text):
    text = text.replace(LAYOUT_STEP, '').replace(LAYOUT_PERMISSIONS, '')
    text = text.replace('          package-manager-cache: false\n', '')
    for action, (old, new) in PINS.items():
        text = text.replace(f'{action}@{new}', f'{action}@{old}')
    return text.replace(NEW_CHECKOUT, OLD_CHECKOUT).replace('# v5.1.0', '# v4.2.2')


class WorkflowContracts(unittest.TestCase):
    def test_existing_workflows_preserve_every_other_byte(self):
        baseline = json.loads((ROOT / 'Tools/fixtures/node24-workflows.json').read_text())
        for name, digest in baseline['workflows'].items():
            with self.subTest(workflow=name):
                current = (ROOT / '.github/workflows' / name).read_text()
                self.assertEqual(hashlib.sha256(reverse_migration(current).encode()).hexdigest(), digest,
                    'Only action pins, explicit Node cache opt-out and the singleton layout adapter may change')
        actual = {p.name for p in (ROOT / '.github/workflows').glob('*.yml')}
        self.assertEqual(actual, set(baseline['workflows']) | {'workflow-actions.yml'})

    def test_supported_actions_and_unchanged_cache_credentials(self):
        for file in sorted((ROOT / '.github/workflows').glob('*.yml')):
            doc = yaml.safe_load(file.read_text())
            for job in doc['jobs'].values():
                for step in job.get('steps', []):
                    action, _, version = step.get('uses', '').partition('@')
                    if not action or action.startswith('./'):
                        continue
                    with self.subTest(workflow=file.name, action=action):
                        self.assertIn(action, PINS)
                        self.assertIn(version, [PINS[action][1]] + ([NEW_CHECKOUT] if action == 'actions/checkout' else []))
                        inputs = step.get('with', {})
                        if action == 'actions/setup-python':
                            self.assertNotIn('cache', inputs)  # all audited baselines have caching disabled
                            self.assertNotIn('cache-dependency-path', inputs)
                            self.assertEqual(str(inputs['python-version']), '3.12')
                        if action == 'actions/setup-node':
                            self.assertIs(inputs.get('package-manager-cache'), False)
                            self.assertEqual(str(inputs['node-version']), '22')
            self.assertNotRegex(file.read_text(), r'(FORCE_JAVASCRIPT_ACTIONS_TO_NODE24|ACTIONS_ALLOW_USE_UNSECURE_NODE_VERSION)')

    def test_evidence_downloads_preserve_single_and_multiple_paths(self):
        doc = yaml.safe_load((ROOT / '.github/workflows/build.yml').read_text())
        job = doc['jobs'].get('consolidate-release-gate')
        if job is None:
            self.skipTest('Consolidator exists only on the release-gate branch')
        downloads = [s for s in job['steps'] if s.get('uses', '').startswith('actions/download-artifact@')]
        self.assertEqual(len(downloads), 1)
        self.assertEqual(downloads[0]['with'], {'pattern': 'Chimera-release-evidence-*',
            'path': 'release-inputs', 'merge-multiple': False})
        self.assertTrue(all('continue-on-error' not in s for s in job['steps']))
        self.assertIn(LAYOUT_STEP, (ROOT / '.github/workflows/build.yml').read_text())

    def test_layout_restores_api_name_and_preserves_unexpected_bundles(self):
        import tempfile
        from artifact_download_layout import restore
        for names in ([], ['first', 'second'], ['Chimera-release-evidence-windows'],
                      ['Chimera-release-evidence-unexpected']):
            with self.subTest(names=names), tempfile.TemporaryDirectory() as directory:
                path = Path(directory)
                (path / 'evidence.json').write_bytes(b'unchanged identity and hashes')
                restore(path, names)
                relative = (names[0] + '/' if len(names) == 1 else '') + 'evidence.json'
                self.assertEqual((path / relative).read_bytes(), b'unchanged identity and hashes')
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory)
            for name in ('..', '../escape', 'bad/name', 'bad\\name'):
                with self.subTest(name=name), self.assertRaises(ValueError):
                    restore(path, [name])
            (path / 'existing').mkdir()
            with self.assertRaises(ValueError):
                restore(path, ['existing'])

    def test_archive_digest_and_identity_reject_mutations(self):
        from workflow_actions_smoke import check_digest, check_identity
        data = b'fixture archive bytes'
        digest = hashlib.sha256(data).hexdigest()
        check_digest(data, digest)
        with self.assertRaises(AssertionError):
            check_digest(data + b'corrupt', digest)
        expected = {'source': 'a' * 40, 'run': '123', 'attempt': '2'}
        check_identity(expected, expected)
        for key in expected:
            with self.subTest(key=key), self.assertRaises(AssertionError):
                check_identity({**expected, key: 'stale'}, expected)


if __name__ == '__main__':
    unittest.main()
