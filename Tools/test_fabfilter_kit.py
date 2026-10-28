#!/usr/bin/env python3
"""Self-test for Tools/fabfilter_kit.py without any FabFilter product.

Synthetic "reference renders" with known parameters (an RBJ bell, a delayed
copy, a peak-detector compressor) must be recovered by the analysis. The
optional Chimera probe check runs when CHIMERA_KIT_PROBE names a built
ChimeraKitProbe.
"""
import math, os, sys, tempfile, unittest
from pathlib import Path
import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parent))
import fabfilter_kit as kit

RATE = 48000

def biquad(x, b, a):
    y = np.zeros_like(x); x1 = x2 = y1 = y2 = 0.0
    for n, v in enumerate(x):
        out = b[0] * v + b[1] * x1 + b[2] * x2 - a[1] * y1 - a[2] * y2
        x2, x1, y2, y1 = x1, v, y1, out; y[n] = out
    return y

def rbj_bell(frequency, gain, q):
    w = 2 * math.pi * frequency / RATE; A = 10 ** (gain / 40); alpha = math.sin(w) / (2 * q); c = math.cos(w)
    b = [1 + alpha * A, -2 * c, 1 - alpha * A]; a = [1 + alpha / A, -2 * c, 1 - alpha / A]
    return [v / a[0] for v in b], [v / a[0] for v in a]

def peak_compressor(x, threshold, ratio, attack_ms, release_ms):
    """Feed-forward, sample-peak detector, one-pole smoothing in the dB gain domain."""
    att, rel = math.exp(-1 / (RATE * attack_ms * 1e-3)), math.exp(-1 / (RATE * release_ms * 1e-3))
    env = 0.0; gr = 0.0; y = np.zeros_like(x); hold = math.exp(-1 / (RATE * 0.002))
    for n, v in enumerate(x):
        env = max(abs(v), env * hold)
        desired = max(0.0, 20 * math.log10(max(env, 1e-12)) - threshold) * (1 - 1 / ratio)
        k = att if desired > gr else rel; gr = desired + (gr - desired) * k
        y[n] = v * 10 ** (-gr / 20)
    return y

def hysteresis_gate(x, threshold, close, range_db, attack_ms, hold_ms, release_ms):
    """Peak detector, open at threshold, close below `close`, hold, one-pole gain."""
    env = 0.0; gain = 1.0; hold = 0; is_open = True; y = np.zeros_like(x)
    decay = math.exp(-1 / (RATE * 0.020)); att = math.exp(-1 / (RATE * attack_ms * 1e-3)); rel = math.exp(-1 / (RATE * release_ms * 1e-3))
    floor = 10 ** (-range_db / 20); hold_samples = int(RATE * hold_ms * 1e-3)
    for n, v in enumerate(x):
        env = max(abs(v), env * decay); level = 20 * math.log10(max(env, 1e-12))
        if level >= threshold: is_open = True; hold = hold_samples
        elif level < close:
            if hold > 0: hold -= 1
            else: is_open = False
        target = 1.0 if is_open else floor
        gain = target + (gain - target) * (att if target > gain else rel); y[n] = v * gain
    return y

def sample_peak_limiter(x, ceiling_db, release_ms):
    ceiling = 10 ** (ceiling_db / 20); rel = math.exp(-1 / (RATE * release_ms * 1e-3)); gain = 1.0; y = np.zeros_like(x)
    for n, v in enumerate(x):
        target = min(1.0, ceiling / abs(v)) if v else 1.0
        gain = min(target, 1 + (gain - 1) * rel); y[n] = v * gain
    return y

class FabFilterKitTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory(); root = Path(cls.tmp.name)
        cls.kit = root / 'kit'; cls.renders = root / 'reference'; cls.renders.mkdir()
        cls.manifest = kit.generate(cls.kit, RATE)
        signal = lambda cid: kit.read_wav(cls.kit / 'signals' / f'{cid}.wav')[0][:, 0]
        b, a = rbj_bell(1000, 6, 1)
        kit.write_wav(cls.renders / 'eq_bell_1k_p6_q1.wav', biquad(signal('eq_bell_1k_p6_q1'), b, a), RATE)
        delayed = np.concatenate([np.zeros(64), biquad(signal('eq_bell_100_m6_q4'), *rbj_bell(100, -6, 4))])
        kit.write_wav(cls.renders / 'eq_bell_100_m6_q4.wav', delayed, RATE)
        for cid in ('comp_static_k0', 'comp_step_10_100'):
            kit.write_wav(cls.renders / f'{cid}.wav', peak_compressor(signal(cid), -20, 4, 10, 100), RATE)
        # An out-of-band tone a multiband compressor leaves untouched.
        kit.write_wav(cls.renders / 'mb_band2_static_2khz.wav', signal('mb_band2_static_2khz'), RATE)
        kit.write_wav(cls.renders / 'mb_band2_step_300hz.wav', peak_compressor(signal('mb_band2_step_300hz'), -20, 4, 10, 100), RATE)
        kit.write_wav(cls.renders / 'gate_static_hysteresis.wav', hysteresis_gate(signal('gate_static_hysteresis'), -40, -46, 40, 0.5, 0, 20), RATE)
        kit.write_wav(cls.renders / 'gate_step_hold_release.wav', hysteresis_gate(signal('gate_step_hold_release'), -40, -46, 40, 0.5, 50, 100), RATE)
        for cid in ('lim_static_ceiling', 'lim_step_release', 'lim_intersample_truepeak'):
            kit.write_wav(cls.renders / f'{cid}.wav', sample_peak_limiter(signal(cid), -1, 100), RATE)
        kit.write_wav(cls.renders / 'ds_static_1khz.wav', signal('ds_static_1khz'), RATE)
        cls.result = kit.analyse(cls.kit, cls.renders)

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def test_wav_round_trip_and_pcm24(self):
        x = np.linspace(-0.9, 0.9, 1000)
        path = Path(self.tmp.name) / 'rt.wav'; kit.write_wav(path, x, RATE)
        y, rate = kit.read_wav(path); self.assertEqual(rate, RATE); np.testing.assert_allclose(y[:, 0], x, atol=1e-7)
        pcm = (np.round(x * (1 << 23))).astype(np.int32); raw = bytearray()
        for v in pcm: raw += int(v & 0xFFFFFF).to_bytes(3, 'little')
        import struct
        header = b'RIFF' + struct.pack('<I', 36 + len(raw)) + b'WAVE' + b'fmt ' + struct.pack('<IHHIIHH', 16, 1, 1, RATE, RATE * 3, 3, 24)
        (Path(self.tmp.name) / 'p24.wav').write_bytes(header + b'data' + struct.pack('<I', len(raw)) + bytes(raw))
        y, _ = kit.read_wav(Path(self.tmp.name) / 'p24.wav'); np.testing.assert_allclose(y[:, 0], x, atol=2e-7)

    def test_manifest_lists_every_signal_with_hash(self):
        self.assertEqual(len(self.manifest['cases']), len(kit.EQ_CASES) + len(kit.COMP_CASES) + len(kit.MB_CASES) + len(kit.GATE_CASES) + len(kit.LIM_CASES) + len(kit.DS_CASES))
        for case in self.manifest['cases']:
            self.assertTrue((self.kit / 'signals' / f"{case['id']}.wav").exists()); self.assertEqual(len(case['signal_sha256']), 64)
        self.assertIn('eq_bell_1k_p6_q1', (self.kit / 'RENDER_GUIDE.md').read_text(encoding='utf-8'))

    def test_eq_bell_recovered(self):
        bell = self.result['cases']['eq_bell_1k_p6_q1']
        self.assertAlmostEqual(bell['max_db'], 6.0, delta=0.05); self.assertAlmostEqual(bell['max_hz'], 1000, delta=30)
        self.assertAlmostEqual(bell['half_gain_q'], 1.0, delta=0.05); self.assertEqual(bell['latency_samples'], 0)

    def test_latency_detected_and_phase_compensated(self):
        cut = self.result['cases']['eq_bell_100_m6_q4']
        self.assertEqual(cut['latency_samples'], 64); self.assertAlmostEqual(cut['min_db'], -6.0, delta=0.1)
        self.assertAlmostEqual(cut['half_gain_q'], 4.0, delta=0.2)

    def test_static_curve_fit(self):
        fit = self.result['cases']['comp_static_k0']['fit']
        self.assertAlmostEqual(fit['threshold_db'], -20, delta=0.5); self.assertAlmostEqual(fit['ratio'], 4, delta=0.2)
        self.assertLessEqual(fit['knee_db'], 1.0)

    def test_attack_release_times(self):
        step = self.result['cases']['comp_step_10_100']
        self.assertAlmostEqual(step['attack_63_ms'], 10, delta=2); self.assertAlmostEqual(step['release_63_ms'], 100, delta=10)
        self.assertAlmostEqual(step['release_tau_ms'], 100, delta=10)
        self.assertGreater(step['steady_gain_reduction_db'], 10)

    def test_multiband_cases_and_tones(self):
        cases = {c['id']: c for c in self.manifest['cases']}
        self.assertEqual(cases['mb_band2_static_300hz']['tone_hz'], 300.0)
        self.assertEqual(cases['mb_band2_static_300hz']['chimera']['band'], 1)
        self.assertEqual(cases['mb_neutral_dynamic_phase']['chimera']['band'], -1)
        x = kit.read_wav(self.kit / 'signals' / 'mb_band2_static_300hz.wav')[0][:, 0]
        spectrum = np.abs(np.fft.rfft(x[:RATE])); self.assertAlmostEqual(float(np.argmax(spectrum)), 300, delta=1)

    def test_untouched_out_of_band_tone_reports_no_reduction(self):
        fit = self.result['cases']['mb_band2_static_2khz']['fit']
        self.assertTrue(fit.get('no_gain_reduction')); self.assertIsNone(fit['threshold_db'])

    def test_low_tone_step_timing(self):
        step = self.result['cases']['mb_band2_step_300hz']
        self.assertAlmostEqual(step['attack_63_ms'], 10, delta=3); self.assertAlmostEqual(step['release_63_ms'], 100, delta=12)

    def test_gate_thresholds_hysteresis_and_range(self):
        gate = self.result['cases']['gate_static_hysteresis']
        self.assertAlmostEqual(gate['closed_attenuation_db'], 40, delta=0.5)
        self.assertEqual(gate['open_threshold_dbfs'], -39.5); self.assertEqual(gate['close_threshold_dbfs'], -45.5)
        self.assertEqual(gate['hysteresis_db'], 6.0)  # 1 dB steps offset by 0.5 dB

    def test_gate_step_timing(self):
        step = self.result['cases']['gate_step_hold_release']
        self.assertLess(step['open_90_ms'], 3)
        # 20 ms detector decay to -46 dB, 50 ms hold, then a 100 ms release toward -40 dB.
        self.assertGreater(step['close_10_ms'], 50); self.assertLess(step['close_50_ms'], 400)

    def test_limiter_ceiling_and_release(self):
        static = self.result['cases']['lim_static_ceiling']
        self.assertLessEqual(static['max_output_sample_peak_dbfs'], -0.99)
        top = static['curve'][-1]; self.assertAlmostEqual(top['gain_reduction_db'], 13, delta=0.2)   # +12 input, -1 ceiling
        step = self.result['cases']['lim_step_release']
        self.assertAlmostEqual(step['steady_gain_reduction_db'], 7, delta=0.2); self.assertAlmostEqual(step['release_tau_ms'], 100, delta=15)

    def test_sample_peak_limiter_misses_true_peak(self):
        tp = self.result['cases']['lim_intersample_truepeak']
        self.assertAlmostEqual(tp['input_true_peak_dbfs'], 2.5, delta=0.05)
        self.assertAlmostEqual(tp['input_sample_peak_dbfs'], 2.5 - 3.01, delta=0.05)
        self.assertLessEqual(tp['output_sample_peak_dbfs'], -0.99)
        self.assertGreater(tp['output_true_peak_dbfs'], 1.5)     # sample-peak limiting leaves ~+2 dBTP

    def test_missing_renders_are_reported_not_invented(self):
        self.assertIn('eq_notch_1k_q4', self.result['missing']); self.assertNotIn('eq_notch_1k_q4', self.result['cases'])

    def test_compare_identical_is_zero(self):
        row = kit.compare(self.result, self.result)['cases']['eq_bell_1k_p6_q1']
        self.assertEqual(row['magnitude_max_abs_delta_db'], 0.0); self.assertTrue(all(v == 0 for v in row['delta'].values()))

    def test_rate_mismatch_rejected(self):
        other = Path(self.tmp.name) / 'wrong'; other.mkdir()
        kit.write_wav(other / 'eq_bell_1k_p6_q1.wav', np.zeros(100), 44100)
        with self.assertRaises(ValueError): kit.analyse(self.kit, other)

    @unittest.skipUnless(os.environ.get('CHIMERA_KIT_PROBE'), 'set CHIMERA_KIT_PROBE to a built ChimeraKitProbe')
    def test_chimera_probe_matches_its_own_design(self):
        out = Path(self.tmp.name) / 'chimera'; kit.chimera(self.kit, os.environ['CHIMERA_KIT_PROBE'], out)
        cases = kit.analyse(self.kit, out)['cases']
        self.assertAlmostEqual(cases['eq_bell_1k_p6_q1']['max_db'], 6.0, delta=0.05)
        self.assertAlmostEqual(cases['eq_bell_1k_p6_q1']['half_gain_q'], 1.0, delta=0.05)
        # RMS detector: a sine's RMS is 3 dB under its peak, so a -20 dB threshold
        # starts at -17 dBFS peak. This documents Chimera's unit, it is not a target.
        self.assertAlmostEqual(cases['comp_static_k0']['fit']['threshold_db'], -17, delta=0.5)
        neutral = cases['mb_neutral_dynamic_phase']
        self.assertLess(max(abs(v) for v in neutral['magnitude_db']), 0.01)   # flat magnitude (allpass sum)
        inband, outband = cases['mb_band2_static_300hz']['fit'], cases['mb_band2_static_2khz']['static_curve']
        # Band 2 ratio 4: part of a 300 Hz tone sits in the neighbouring bands'
        # crossover skirts and is not compressed, so the full-band output ratio is
        # lower (measured 3.0). A Chimera property recorded for comparison.
        self.assertAlmostEqual(inband['threshold_db'], -17, delta=1.0); self.assertTrue(2.5 < inband['ratio'] < 4.0)
        mbstep = cases['mb_band2_step_300hz']
        self.assertAlmostEqual(mbstep['release_tau_ms'], 100, delta=25)
        self.assertLess(max(r['gain_reduction_db'] for r in outband), 1.0)
        gate, expander = cases['gate_static_hysteresis'], cases['expander_static_r2']
        self.assertEqual(gate['open_threshold_dbfs'], -39.5); self.assertAlmostEqual(gate['closed_attenuation_db'], 40, delta=0.5)
        self.assertGreater(gate['hysteresis_db'], 0)
        self.assertAlmostEqual(expander['expansion_ratio'], 2, delta=0.3)
        self.assertLessEqual(cases['lim_static_ceiling']['max_output_sample_peak_dbfs'], -0.99)
        self.assertGreater(cases['lim_intersample_truepeak']['output_true_peak_dbfs'], -1.0)   # Chimera limits sample peaks only
        self.assertLess(max(abs(v) for v in cases['ds_neutral_impulse']['magnitude_db']), 0.01)
        self.assertTrue(cases['ds_static_1khz']['fit'].get('no_gain_reduction'))
        # Range 12 dB, but an 8 kHz tone keeps part of its energy in the 6 kHz LR4
        # low branch, which a split-band de-esser never attenuates (measured 7.8 dB).
        ds_top = max(r['gain_reduction_db'] for r in cases['ds_static_8khz']['static_curve'])
        self.assertTrue(6.0 < ds_top <= 12.0)
        self.assertAlmostEqual(cases['ds_step_8khz']['attack_63_ms'], 2.5, delta=2.0)

if __name__ == '__main__':
    unittest.main()
