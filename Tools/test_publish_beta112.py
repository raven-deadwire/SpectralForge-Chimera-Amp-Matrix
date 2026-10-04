#!/usr/bin/env python3
"""Reject stale or modified release inputs before any publishing request."""
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

    def test_accepts_complete_current_inputs(self):
        notes,assets=publisher.prepare_assets(self.root,HEAD,RUN)
        self.assertIn(self.version,notes)
        self.assertIn('NATIVE_NAM_CALIBRATION.md',assets)
        self.assertEqual(sum(name.startswith(self.prefix) for name in assets),5)

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
