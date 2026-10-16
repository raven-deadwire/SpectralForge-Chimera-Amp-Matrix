from pathlib import Path
import copy
import csv
import struct
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]/'Tools'))
from eq_normalization_render import audio_equal, compare
from eq_tuner_parameter_contract import APPENDED_IDS, RELEASED_COUNT, rows


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


class ComparisonControls(unittest.TestCase):
    """Exercise the real TSV/state/audio readers, including the result-list path."""

    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.a, self.b = (Path(self.temp.name)/name for name in ('a', 'b'))
        self.old = rows(Path(__file__).parent/'fixtures/eq-beta2/parameters.tsv')
        self.new = copy.deepcopy(self.old[:RELEASED_COUNT])
        self.new += [dict(index=str(RELEASED_COUNT+i), id=id, title='new',
                          steps='0', default='0', flags='1')
                     for i, id in enumerate(APPENDED_IDS)]
        self.new += [dict(row, index=str(int(row['index'])+59))
                     for row in self.old[RELEASED_COUNT:]]
        for folder in (self.a, self.b):
            folder.mkdir()
            (folder/'audio.f32').write_bytes(struct.pack('<32768f', *([.125, -.0625]*16384)))
            (folder/'render.txt').write_text('sample_rate=48000\nblock_size=256\nlatency=0\n')
            self.write_state(folder, '0.5')
            self.write_parameters(folder, self.old)

    def write_state(self, folder, value):
        data = f'<PARAMS><PARAM id="toneEQ_b1_gain" value="{value}"/></PARAMS>'.encode()
        (folder/'component.bin').write_bytes(struct.pack('<II', 0x21324356, len(data)) + data + b'\0')

    def write_parameters(self, folder, inventory):
        with (folder/'parameters.tsv').open('w', newline='') as stream:
            writer = csv.DictWriter(stream, fieldnames=list(self.old[0]), delimiter='\t')
            writer.writeheader()
            writer.writerows(inventory)
        (folder/'normalized.tsv').write_text('id\tvalue\n' + ''.join(
            f"{row['id']}\t{row['default']}\n" for row in inventory))

    def assert_rejected(self, pattern):
        results = []
        with self.assertRaisesRegex(ValueError, pattern):
            compare('negative control', self.a, self.b, results)
        self.assertEqual(results, [], 'a rejected comparison must not record PASS')

    def test_compare_exact_and_records_evidence(self):
        results = []
        compare('candidate A/A', self.a, self.b, results)
        self.assertEqual(len(results), 1)
        self.assertEqual(results[0]['comparison'], 'candidate A/A')
        self.assertEqual(results[0]['status'], 'PASS')
        self.assertEqual(results[0]['samples'], 32768)
        for key in ('exact_audio_bits', 'normalized_values_exact', 'latency_exact', 'serialized_state_exact'):
            self.assertIs(results[0][key], True)

    def test_compare_frozen_append_in_both_directions(self):
        self.write_parameters(self.b, self.new)
        results = []
        compare('beta2 -> candidate', self.a, self.b, results)
        compare('candidate -> beta2', self.b, self.a, results)
        self.assertEqual([row['status'] for row in results], ['PASS', 'PASS'])

    def test_compare_rejects_audio_drift(self):
        changed = bytearray((self.b/'audio.f32').read_bytes()); changed[0] ^= 1
        (self.b/'audio.f32').write_bytes(changed)
        self.assert_rejected('nonidentical render')

    def test_compare_rejects_normalized_drift(self):
        path = self.b/'normalized.tsv'
        lines = path.read_text().splitlines()
        lines[1] = lines[1].split('\t')[0] + '\t0.50000001'
        path.write_text('\n'.join(lines) + '\n')
        self.assert_rejected('normalized values changed')

    def test_compare_rejects_appended_nondefault(self):
        self.write_parameters(self.b, self.new)
        path = self.b/'normalized.tsv'
        path.write_text(path.read_text().replace(APPENDED_IDS[0]+'\t0\n', APPENDED_IDS[0]+'\t0.00000001\n'))
        self.assert_rejected('appended normalized value is not its default')

    def test_compare_rejects_invalid_append_metadata(self):
        for key, value in (('id', self.new[1]['id']), ('steps', '999'), ('default', '.2'), ('flags', '0')):
            with self.subTest(key=key):
                bad = copy.deepcopy(self.new); bad[0][key] = value
                self.write_parameters(self.b, bad)
                self.assert_rejected('duplicate VST3 ParamID|released metadata changed')

    def test_compare_rejects_latency_drift(self):
        (self.b/'render.txt').write_text('sample_rate=48000\nblock_size=256\nlatency=1\n')
        self.assert_rejected('render.txt mismatch')

    def test_compare_rejects_serialized_state_drift(self):
        self.write_state(self.b, '0.50000001')
        self.assert_rejected('serialized state mismatch')


if __name__ == '__main__':
    unittest.main()
