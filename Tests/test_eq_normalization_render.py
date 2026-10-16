import importlib.util
from pathlib import Path
import struct
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]/'Tools'))
from eq_normalization_render import audio_equal


class RenderControls(unittest.TestCase):
    def setUp(self):
        self.good = struct.pack('<32768f', *([.125, -.0625]*16384))

    def test_exact(self):
        audio_equal(self.good, self.good)

    def test_one_bit_rejected(self):
        changed = bytearray(self.good); changed[0] ^= 1
        with self.assertRaisesRegex(ValueError, 'nonidentical'):
            audio_equal(self.good, changed)

    def test_silence_rejected(self):
        with self.assertRaisesRegex(ValueError, 'silent'):
            audio_equal(bytes(len(self.good)), bytes(len(self.good)))

    def test_truncation_rejected(self):
        with self.assertRaisesRegex(ValueError, 'truncated'):
            audio_equal(self.good, self.good[:-4])

    def test_nan_rejected(self):
        bad = struct.pack('<f', float('nan')) + self.good[4:]
        with self.assertRaisesRegex(ValueError, 'nonfinite'):
            audio_equal(bad, bad)


if __name__ == '__main__':
    unittest.main()
