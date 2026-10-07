#!/usr/bin/env python3
"""Reject stale or modified release inputs before any publishing request."""
import json
from pathlib import Path
import tempfile
import unittest
import zipfile
from unittest.mock import patch
import publish_beta12 as publisher
import release_evidence as evidence
import evaluate_release_gate as gate

HEAD = 'a' * 40
RUN = 123

class PublicationInputs(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        # This publisher and its release notes are intentionally pinned to 1.2.0.
        # Regression fixtures must not adopt a later development branch VERSION.
        identity = patch.object(publisher, 'VERSION', '1.2.0-beta.1')
        identity.start(); self.addCleanup(identity.stop)
        self.version = publisher.VERSION
        self.prefix = f'SpectralForge-Chimera-{self.version}-'
        self.release_url = f'https://github.com/{publisher.transport.REPO}/releases/tag/v{self.version}'
        self.download_url = f'https://github.com/{publisher.transport.REPO}/releases/download/v{self.version}/'
        self.context = patch.multiple(publisher.transport, RELEASE_URL=self.release_url, DOWNLOAD_URL=self.download_url)
        self.context.start(); self.addCleanup(self.context.stop)
        for name in publisher.DOCUMENTS:
            (self.root / name).write_text('fixture')
        self.source = dict(version=self.version, tag='v'+self.version, revision=HEAD, runId=str(RUN), published=False, publisherSigned=False, macOSNotarized=False)
        self.write_json('candidate-source.json', self.source)
        for suffix in publisher.SUFFIXES:
            (self.root / (self.prefix+suffix)).write_bytes(b'fixture-package')
        with zipfile.ZipFile(self.root / (self.prefix+'win64.zip'), 'w') as archive:
            archive.writestr('payload-manifest.json',json.dumps({'version':self.version}))
            archive.writestr('Standalone/fixture.exe', b'fixture')
        self.manifest = dict(schema=1, version=self.version, channel='beta', releaseUrl=self.release_url, assets=[])
        for platform, arch, suffix in [('windows','x86_64','win64-Setup.exe'),('macos','universal','macos-universal.pkg'),('linux','x86_64','linux-x86_64.deb')]:
            name=self.prefix+suffix; file=self.root/name
            self.manifest['assets'].append(dict(platform=platform,arch=arch,name=name,url=self.download_url+name,size=file.stat().st_size,sha256=publisher.sha256(file)))
        self.write_json('update-beta.json',self.manifest)
        self.refresh_sums()

    def write_json(self,name,value):
        (self.root/name).write_text(json.dumps(value))

    def refresh_sums(self):
        lines=[]
        for suffix in publisher.SUFFIXES:
            name=self.prefix+suffix; line=publisher.sha256(self.root/name)+'  '+name+'\n'
            lines.append(line);(self.root/(name+'.sha256.txt')).write_text(line)
        (self.root/'SHA256SUMS.txt').write_text(''.join(lines))

    def gate_archive(self, **changes):
        policy, _ = evidence.configuration()
        checks = {cid: evidence.make_report(policy, cid, HEAD, passed=True)
                  for stage in policy['profiles']['beta_1_2']['required_stages']
                  for cid in policy['stages'][stage]['required_checks']}
        # Fabricated complete evidence is confined to this positive test fixture.
        verdict = gate.evaluate_release(policy, {'waivers': []}, checks, 'beta_1_2', HEAD)
        verdict['revision'] = evidence.identity(HEAD, RUN, 1)
        verdict['producer'] = {'name': 'consolidate_release_gate', 'errors': []}
        verdict.update(changes)
        path = self.root / 'release-gate.zip'
        with zipfile.ZipFile(path, 'w') as archive:
            archive.writestr('validation/release-gate.json', json.dumps(verdict))
            for cid, report in checks.items():
                archive.writestr('validation/checks/' + cid + '.json', json.dumps(report))
        return path

    def test_accepts_exact_source_pass_release_gate(self):
        verdict = publisher.verify_release_gate_archive(
            self.gate_archive(), HEAD, 'beta_1_2', RUN, 1)
        self.assertEqual(verdict['verdict'], 'PASS')

    def test_rejects_blocked_stale_or_wrong_profile_release_gate(self):
        cases = (
            {'verdict': 'BLOCKED', 'ready': False,
             'counts': {'checks': {'blocked': 1}},
             'hard_gates': {'blocked': 1},
             'blockers': [{'id': 'B12.RESULT'}]},
            {'revision': {'commit_sha': 'b' * 40}},
            {'profile': 'pull_request'},
        )
        for changes in cases:
            with self.subTest(changes=changes), self.assertRaises(RuntimeError):
                publisher.verify_release_gate_archive(
                    self.gate_archive(**changes), HEAD, 'beta_1_2', RUN, 1)

    def test_rejects_missing_or_duplicate_release_gate(self):
        path = self.root / 'release-gate.zip'
        with zipfile.ZipFile(path, 'w') as archive:
            archive.writestr('unrelated.json', '{}')
        with self.assertRaises(RuntimeError):
            publisher.verify_release_gate_archive(path, HEAD, 'beta_1_2', RUN, 1)
        with zipfile.ZipFile(path, 'w') as archive:
            archive.writestr('one/release-gate.json', '{}')
            archive.writestr('two/release-gate.json', '{}')
        with self.assertRaises(RuntimeError):
            publisher.verify_release_gate_archive(path, HEAD, 'beta_1_2', RUN, 1)

    def test_accepts_complete_current_inputs(self):
        notes,assets=publisher.prepare_assets(self.root,HEAD,RUN)
        self.assertIn(self.version,notes)
        self.assertIn('NATIVE_NAM_CALIBRATION.md',assets)
        self.assertEqual(sum(name.startswith(self.prefix) for name in assets),5)

    def test_rejects_preview_before_any_publication_request(self):
        with patch.dict(publisher.os.environ, {'GITHUB_SHA': HEAD}), \
             patch.object(publisher, 'IDENTITY', {'product_version': '1.2.0'}), \
             patch.object(publisher, 'VERSION', '1.2.0-preview.'+HEAD[:10]), \
             patch.object(publisher.transport, 'GitHub') as api:
            with self.assertRaisesRegex(RuntimeError, 'pinned to Open Beta 1.2'):
                publisher.main()
            api.assert_not_called()

    def test_selects_exact_head_and_rejects_failed_or_pending_latest(self):
        good = dict(id=123, head_sha=HEAD, event="pull_request", status="completed", conclusion="success")
        self.assertEqual(publisher.verified_run([good], HEAD, ("pull_request",)), 123)
        for changes in ({"head_sha": "b"*40}, {"event": "push"}, {"status": "in_progress"}, {"conclusion": "failure"}):
            with self.subTest(changes=changes), self.assertRaises(RuntimeError):
                publisher.verified_run([{**good, **changes}], HEAD, ("pull_request",))
        with self.assertRaisesRegex(RuntimeError, "Latest"):
            publisher.verified_run([good, {**good, "id": 124, "conclusion": "failure"}], HEAD, ("pull_request",))

    def test_rejects_stale_source_or_run(self):
        for key,value in [('revision','b'*40),('runId','124'),('version','1.1.1-beta.1'),('published',True)]:
            with self.subTest(key=key):
                self.write_json('candidate-source.json',{**self.source,key:value})
                with self.assertRaisesRegex(RuntimeError,'identity mismatch'):
                    publisher.prepare_assets(self.root,HEAD,RUN)

    def test_rejects_modified_binary(self):
        (self.root/(self.prefix+'win64-Setup.exe')).write_bytes(b'modified')
        with self.assertRaisesRegex(RuntimeError,'checksum mismatch'):
            publisher.prepare_assets(self.root,HEAD,RUN)

    def test_rejects_unofficial_update_target(self):
        self.manifest['assets'][0]['url']='https://example.invalid/download.exe'
        self.write_json('update-beta.json',self.manifest)
        with self.assertRaisesRegex(RuntimeError,'asset mismatch'):
            publisher.prepare_assets(self.root,HEAD,RUN)

    def test_rejects_capture_even_with_refreshed_checksums(self):
        with zipfile.ZipFile(self.root/(self.prefix+'win64.zip'),'a') as archive:
            archive.writestr('capture.nam','private capture')
        self.refresh_sums()
        with self.assertRaisesRegex(RuntimeError,'Private capture'):
            publisher.prepare_assets(self.root,HEAD,RUN)

if __name__=='__main__':unittest.main()
