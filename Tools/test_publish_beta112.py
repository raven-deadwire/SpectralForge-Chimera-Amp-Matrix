#!/usr/bin/env python3
"""Reject stale or modified release inputs before any publishing request."""
import io
import json
from pathlib import Path
import tempfile
import unittest
import zipfile
from unittest.mock import patch
import publish_beta112 as publisher

HEAD = 'a' * 40
RUN = 123

class PublicationInputs(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        # This publisher and its release notes are intentionally frozen at 1.1.2.
        # Regression fixtures must not adopt a later development branch VERSION.
        identity = patch.object(publisher, 'VERSION', '1.1.2-beta.1')
        identity.start(); self.addCleanup(identity.stop)
        self.version = publisher.VERSION
        # Each historical publisher owns its release-branch documents. Do not
        # read the current branch's notes after a later version is prepared.
        self.repo = self.root / 'repo'
        (self.repo / 'docs').mkdir(parents=True)
        (self.repo / 'Tools').mkdir()
        repo = patch.object(publisher, 'ROOT', self.repo)
        repo.start(); self.addCleanup(repo.stop)
        for name in ('STUDIO_ONE_TEARDOWN.md', 'AMP_NATIVE_DSP.md',
                     'POST_NATIVE_DSP.md', 'PEDAL_BOARD_DSP.md',
                     'EXTERNAL_BASS_IRS.md', 'NATIVE_NAM_CALIBRATION.md'):
            (self.repo / 'docs' / name).write_text('fixture', encoding='utf-8')
        (self.repo / 'Tools/Trace-Chimera-Session.ps1').write_text('fixture', encoding='utf-8')
        (self.repo / 'COPYRIGHT.txt').write_text('fixture', encoding='utf-8')
        self.notes_path = self.repo / 'docs/OPEN_BETA_1_1_2_RELEASE_NOTES.md'
        self.expected_notes = f'SpectralForge Chimera {self.version} — Náströnd\nStudio One · 캐비넷 검증'
        self.notes_path.write_text(self.expected_notes, encoding='utf-8')
        self.prefix = f'SpectralForge-Chimera-{self.version}-'
        self.release_url = f'https://github.com/{publisher.transport.REPO}/releases/tag/v{self.version}'
        self.download_url = f'https://github.com/{publisher.transport.REPO}/releases/download/v{self.version}/'
        self.context = patch.multiple(publisher.transport, RELEASE_URL=self.release_url, DOWNLOAD_URL=self.download_url)
        self.context.start(); self.addCleanup(self.context.stop)
        for name in publisher.DOCUMENTS:
            (self.root / name).write_text('fixture', encoding='utf-8')
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
        (self.root/name).write_text(json.dumps(value, ensure_ascii=False), encoding='utf-8')

    def refresh_sums(self):
        lines=[]
        for suffix in publisher.SUFFIXES:
            name=self.prefix+suffix; line=publisher.sha256(self.root/name)+'  '+name+'\n'
            lines.append(line);(self.root/(name+'.sha256.txt')).write_text(line, encoding='utf-8')
        (self.root/'SHA256SUMS.txt').write_text(''.join(lines), encoding='utf-8')

    def prepare_with_windows_code_page(self):
        # Reproduce Python 3.12's Windows locale default at the text I/O boundary
        # on every CI OS, without enabling UTF-8 mode to conceal a missing encoding.
        native_open = io.open
        def windows_open(file, mode='r', buffering=-1, encoding=None, errors=None,
                         newline=None, closefd=True, opener=None):
            if 'b' not in mode and encoding in (None, 'locale'):
                encoding = 'cp1252'
            return native_open(file, mode, buffering, encoding, errors, newline,
                               closefd=closefd, opener=opener)
        with patch('io.open', side_effect=windows_open):
            return publisher.prepare_assets(self.root, HEAD, RUN)

    def test_reads_utf8_notes_under_windows_code_page(self):
        notes, _ = self.prepare_with_windows_code_page()
        self.assertEqual(notes, self.expected_notes)

    def test_reads_utf8_candidate_json_under_windows_code_page(self):
        self.write_json('candidate-source.json', {**self.source, 'description': '캐비넷 검증'})
        self.write_json('update-beta.json', {**self.manifest, 'description': '캐비넷 검증'})
        notes, _ = self.prepare_with_windows_code_page()
        self.assertEqual(notes, self.expected_notes)

    def test_accepts_utf8_bom_candidate_json(self):
        for name in ('candidate-source.json', 'update-beta.json'):
            path = self.root / name
            path.write_bytes(path.read_text(encoding='utf-8').encode('utf-8-sig'))
        notes, _ = self.prepare_with_windows_code_page()
        self.assertEqual(notes, self.expected_notes)

    def test_rejects_invalid_utf8_release_notes(self):
        self.notes_path.write_bytes(self.expected_notes.encode('utf-8') + b'\xff')
        with self.assertRaises(UnicodeDecodeError):
            publisher.prepare_assets(self.root, HEAD, RUN)

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
            with self.assertRaisesRegex(RuntimeError, 'pinned to Open Beta 1.1.2'):
                publisher.main()
            api.assert_not_called()

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
