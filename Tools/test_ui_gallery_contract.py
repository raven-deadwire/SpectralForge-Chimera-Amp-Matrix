#!/usr/bin/env python3
"""Regression checks for complete transport of the existing UI gallery."""
import fnmatch
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest
import zlib

from check_ui_gallery import EXPECTED, GLOBS, check

ROOT = Path(__file__).resolve().parents[1]


def png():
    def chunk(kind, data):
        return (struct.pack('>I', len(data)) + kind + data
                + struct.pack('>I', zlib.crc32(kind + data)))
    return (b'\x89PNG\r\n\x1a\n'
            + chunk(b'IHDR', struct.pack('>IIBBBBB', 1, 1, 8, 2, 0, 0, 0))
            + chunk(b'IDAT', zlib.compress(b'\x00\x00\x00\x00'))
            + chunk(b'IEND', b''))


class GalleryContract(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.directory = Path(self.temp.name)

    def populate(self):
        for name in EXPECTED:
            (self.directory / name).write_bytes(png())

    def test_complete_gallery(self):
        self.populate()
        self.assertEqual(check(self.directory), 29)

    def test_each_missing_image_fails(self):
        self.populate()
        for name in EXPECTED:
            with self.subTest(name=name):
                path = self.directory / name
                path.unlink()
                with self.assertRaisesRegex(ValueError, 'missing:'):
                    check(self.directory)
                path.write_bytes(png())

    def test_empty_and_stale_gallery_fail_cli(self):
        for name in ('PRE-models-1.png', 'POST-models-1.png', 'PRE-envelope-first.png'):
            (self.directory / name).write_bytes(png())
        result = subprocess.run([sys.executable, str(ROOT / 'Tools/check_ui_gallery.py'),
                                 str(self.directory)], capture_output=True, text=True)
        self.assertEqual(result.returncode, 1)
        self.assertIn('missing: PRE-types-1.png', result.stderr)

    def test_missing_whole_group_fails(self):
        for pattern in GLOBS:
            with self.subTest(group=pattern):
                self.populate()
                for path in self.directory.glob(pattern):
                    path.unlink()
                with self.assertRaises(ValueError):
                    check(self.directory)

    def test_empty_invalid_and_zero_dimension_files_fail(self):
        self.populate()
        for data in (b'', b'not a PNG', png()[:16] + b'\x00' * 8):
            (self.directory / EXPECTED[0]).write_bytes(data)
            with self.assertRaisesRegex(ValueError, 'invalid PNG header:'):
                check(self.directory)

    def test_workflow_partition_and_failure_contract(self):
        workflow = (ROOT / '.github/workflows/build.yml').read_text(encoding='utf-8')
        general = workflow.split('      - name: Upload Windows UI verification\n')[1].split('      - name:')[0]
        gallery = workflow.split('      - name: Upload Windows pedal and rack gallery\n')[1].split('      - name:')[0]
        verification = workflow.split('      - name: Verify Windows pedal and rack gallery completeness\n')[1].split('      - name:')[0]
        self.assertIn("if: runner.os == 'Windows' && always()", verification)
        self.assertIn('run: python Tools/check_ui_gallery.py build/ui-snapshots', verification)
        self.assertIn('if-no-files-found: error', gallery)
        for pattern in GLOBS:
            self.assertIn(f'!build/ui-snapshots/{pattern}', general)
            self.assertIn(f'            build/ui-snapshots/{pattern}\n', gallery)
        for name in EXPECTED:
            self.assertEqual(sum(fnmatch.fnmatchcase(name, pattern) for pattern in GLOBS), 1)
        for stale in ('PRE-models-', 'POST-models-', 'PRE-*-first'):
            self.assertNotIn(stale, workflow)
        self.assertEqual(workflow.count('python Tools/test_ui_gallery_contract.py'), 2)


if __name__ == '__main__':
    unittest.main()
