import importlib.util
import json
from pathlib import Path
import tempfile
import unittest
import numpy as np
from scipy.io import wavfile

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('auto_set_di', ROOT/'Tools/AutoSet/di_regression.py')
m = importlib.util.module_from_spec(spec)
spec.loader.exec_module(m)

class DIRegressionInputs(unittest.TestCase):
    def test_unrecorded_template_is_not_pass(self):
        result, ready, profiles = m.inspect(ROOT/'Validation/transpose-di-take-template.json', ROOT/'Validation/auto-set-phase6/spec.json')
        self.assertEqual(result['status'], 'MISSING_INPUT')
        self.assertEqual(sum('no recording' in x for x in result['missing']), 9)
        self.assertFalse(result['listening_completed'])
        self.assertEqual(ready, [])

    def test_duplicate_take_rejected(self):
        with tempfile.TemporaryDirectory() as d:
            data = json.loads((ROOT/'Validation/transpose-di-take-template.json').read_text())
            data['takes'][-1] = data['takes'][0]
            path = Path(d)/'takes.json';path.write_text(json.dumps(data))
            result, _, _ = m.inspect(path, ROOT/'Validation/auto-set-phase6/spec.json')
            self.assertEqual(result['status'], 'INVALID_INPUT')

    def test_audio_contract(self):
        with tempfile.TemporaryDirectory() as d:
            path = Path(d)/'signal.wav'
            for rate, data in [(48000, np.zeros((100, 2), np.float32)), (32000, np.zeros(100, np.float32)),
                               (48000, np.ones(100, np.float32)), (48000, np.full(100, np.nan, np.float32))]:
                wavfile.write(path, rate, data)
                with self.assertRaises(ValueError):m.audio(path)
            for dtype in [np.int16, np.int32, np.float32]:
                wavfile.write(path, 44100, np.zeros(100, dtype))
                self.assertEqual(m.audio(path)[0], 44100)

    def test_fundamental_delta(self):
        rate=48000;t=np.arange(rate*2)/rate;x=.2*np.sin(2*np.pi*27.5*t)
        self.assertAlmostEqual(m.band_delta(x, x, rate, 27.5), 0, places=6)
        self.assertAlmostEqual(m.band_delta(x, x*.5, rate, 27.5), -6.0206, places=3)

if __name__ == '__main__':unittest.main()
