"""Synthetic recording-QC regression tests. These are not real DI evidence."""
import argparse
import contextlib
import copy
import io
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
import wave

import numpy as np
from scipy.io import wavfile

import di_validation as di
from di_input import inspect_wav, inspect_segment, segment_structure
from di_comparison import summarize


class RecordingQC(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.rate = 48000

    def signal(self, rate=48000):
        z = np.zeros(4*rate, dtype=np.float32)
        t = np.arange(3*rate)/rate
        z[rate//2:7*rate//2] = .2*np.sin(2*np.pi*55*t)
        return z

    def write(self, z=None, rate=48000):
        p = self.root/'take.wav'
        wavfile.write(p, rate, self.signal(rate) if z is None else z)
        return p

    def segment(self):
        return dict(id='vibrato', kind='guard', start_s=0., end_s=4.,
                    silence_before_end_s=.5, silence_after_start_s=3.5)

    def manifest(self):
        self.write()
        (self.root/'labels.csv').write_text('sample,keep\n24000,yes\n')
        m = dict(version=1, status='SELFTEST_SYNTHETIC', takes=[dict(id='synthetic',
                 instrument='guitar_dropA_A1', rate=48000, channel=0,
                 wav='take.wav', labels='labels.csv', labels_reviewed=True,
                 take_note={'note': 'synthetic regression test only'}, segments=[self.segment()])])
        p = self.root/'takes.json'; p.write_text(json.dumps(m), encoding='utf-8')
        return p, di.load_manifest(p)

    def test_native_rates_and_unmodified_hash(self):
        for rate in (44100, 48000, 96000):
            with self.subTest(rate=rate):
                p = self.write(rate=rate); before = di.sha(p)
                z, actual, errors, qc = inspect_wav(p)
                self.assertEqual(errors, []); self.assertEqual(actual, rate)
                self.assertEqual(qc['frames'], 4*rate)
                self.assertEqual(qc['bits_per_sample'], 32)
                self.assertAlmostEqual(qc['peak_dbfs'], -13.9794, places=3)
                self.assertEqual(di.sha(p), before)

    def test_negative_full_scale_pcm16(self):
        z = np.zeros(48000, dtype=np.int16); z[24000] = -32768
        _, _, errors, qc = inspect_wav(self.write(z))
        self.assertIn('clipped (full-scale samples)', errors)
        self.assertEqual(qc['full_scale_samples'], 1)

    def test_pcm24_header_and_negative_clip(self):
        p = self.root/'24.wav'
        with wave.open(str(p), 'wb') as f:
            f.setparams((1, 3, 48000, 0, 'NONE', 'not compressed'))
            f.writeframes(b'\x00\x00\x00'*24000+b'\x00\x00\x80'+b'\x00\x00\x00'*23999)
        _, _, errors, qc = inspect_wav(p)
        self.assertEqual(qc['bits_per_sample'], 24)
        self.assertEqual(qc['decoded_dtype'], 'int32')
        self.assertIn('clipped (full-scale samples)', errors)

    def test_pcm32_clip(self):
        z = np.zeros(48000, dtype=np.int32); z[24000] = -2147483648
        self.assertIn('clipped (full-scale samples)', inspect_wav(self.write(z))[2])

    def test_nonfinite_is_json_safe(self):
        z = self.signal(); z[24000:24002] = [np.nan, np.inf]
        _, _, errors, qc = inspect_wav(self.write(z))
        self.assertIn('nonfinite samples', errors)
        self.assertEqual(qc['nonfinite_samples'], 2)
        json.dumps(qc, allow_nan=False)

    def test_empty_and_invalid_channel(self):
        with self.assertRaisesRegex(ValueError, 'empty WAV'):
            inspect_wav(self.write(np.zeros(0, dtype=np.float32)))
        p = self.write()
        for channel in (-1, 1, .5, True):
            with self.subTest(channel=channel), self.assertRaisesRegex(ValueError, 'channel'):
                inspect_wav(p, channel)

    def test_bad_format(self):
        with self.assertRaisesRegex(ValueError, 'unsupported'):
            inspect_wav(self.write(self.signal().astype(np.float64)))

    def test_quiet_and_dc(self):
        self.assertTrue(any('too quiet' in s for s in inspect_wav(self.write(self.signal()*.001))[2]))
        self.assertIn('DC offset > 1e-3', inspect_wav(self.write(self.signal()+.002))[2])

    def test_headroom_is_advisory(self):
        _, _, errors, qc = inspect_wav(self.write(self.signal()*4.5))
        self.assertFalse(errors)
        self.assertTrue(qc['warnings'])

    def test_half_second_mutes_and_hidden_edge_noise(self):
        z = self.signal(); seg = self.segment()
        self.assertFalse(inspect_segment(z, self.rate, seg, True)[0])
        z[int(.3*self.rate):int(.4*self.rate)] = .1
        errors, qc = inspect_segment(z, self.rate, seg, True)
        self.assertTrue(any('observed_leading not silent' in s for s in errors))
        self.assertFalse(any('start_0.2s not silent' in s for s in errors))

    def test_short_missing_and_overlapping_mutes(self):
        for changes in ({'silence_before_end_s': .4}, {'silence_before_end_s': None},
                        {'silence_before_end_s': 3.8, 'silence_after_start_s': 3.5}):
            with self.subTest(changes=changes):
                self.assertTrue(inspect_segment(self.signal(), self.rate, {**self.segment(), **changes}, True)[0])

    def test_bounds_order_and_duplicate_segments(self):
        for changes in ({'start_s': float('nan')}, {'end_s': -1}, {'end_s': 5}):
            self.assertTrue(inspect_segment(self.signal(), self.rate, {**self.segment(), **changes})[0])
        self.assertTrue(segment_structure([self.segment(), self.segment()]))
        self.assertTrue(segment_structure([dict(id='riff', kind='riff', start_s=0, end_s=21)]))

    def test_all_silent_segment(self):
        self.assertTrue(inspect_segment(np.zeros(4*self.rate), self.rate, self.segment())[0])

    def test_paths_resolve_from_manifest_and_bom(self):
        p, m = self.manifest()
        p.write_text(p.read_text(), encoding='utf-8-sig')
        self.assertEqual(di.load_manifest(p)['takes'][0]['wav_path'], self.root/'take.wav')
        self.assertFalse(di.check(m))

    def test_declared_rate_mismatch(self):
        _, m = self.manifest(); m['takes'][0]['rate'] = 44100
        self.assertTrue(any('manifest rate' in s for s in di.check(m)))

    def test_bad_audio_does_not_abort_other_takes(self):
        _, m = self.manifest(); other = copy.deepcopy(m['takes'][0]); other['id'] = 'missing'
        other['wav_path'] = self.root/'broken.wav'; other['wav_path'].write_bytes(b'broken')
        m['takes'].insert(0, other); details = []
        self.assertTrue(any('unreadable WAV' in s for s in di.check(m, details=details)))
        self.assertEqual(len(details), 2); self.assertIsNotNone(details[1]['audio'])

    def test_labels_order_bounds_and_mute(self):
        p, m = self.manifest()
        for rows in ('24000,yes\n24000,yes\n', '-1,yes\n', '23999,yes\n', '999999,yes\n'):
            (self.root/'labels.csv').write_text('sample,keep\n'+rows)
            self.assertTrue(di.check(m))

    def test_audio_only_and_strict_exit(self):
        p, m = self.manifest(); (self.root/'labels.csv').unlink()
        args = argparse.Namespace(manifest=str(p), audio_only=True, strict=True, out=str(self.root/'qc.json'))
        with contextlib.redirect_stdout(io.StringIO()):
            self.assertEqual(di.cmd_plan(args), 0)
        self.assertEqual(json.loads((self.root/'qc.json').read_text())['status'], 'AUDIO_INPUT_READY_LABELS_NOT_CHECKED')
        args.audio_only = False
        with contextlib.redirect_stdout(io.StringIO()): self.assertEqual(di.cmd_plan(args), 1)

    def test_qc_cannot_overwrite_inputs(self):
        p, m = self.manifest()
        for output in (p, m['takes'][0]['wav_path'], m['takes'][0]['labels_path']):
            before = output.read_bytes()
            args = argparse.Namespace(manifest=p, audio_only=True, strict=True, out=output)
            with self.assertRaisesRegex(SystemExit, 'must not overwrite'):
                di.cmd_plan(args)
            self.assertEqual(output.read_bytes(), before)

    def test_template_and_nine_take_integrity(self):
        p = di.ROOT/'Validation/transpose-di-take-template.json'; m = di.load_manifest(p)
        errors = di.check(m)
        self.assertEqual(len(errors), 9)
        self.assertTrue(all('no recording' in s for s in errors))
        m['takes'].pop()
        self.assertTrue(any('nine unique' in s for s in di.check(m)))

    def test_nine_take_profile_with_synthetic_files(self):
        m = di.load_manifest(di.ROOT/'Validation/transpose-di-take-template.json')
        m['status'] = 'SELFTEST_SYNTHETIC'
        for index, take in enumerate(m['takes']):
            rate = take['rate']; z = np.zeros(42*rate, dtype=np.float32); labels = []
            for j, seg in enumerate(take['segments']):
                a = 6*j; b = a+6
                seg.update(start_s=a, end_s=b, silence_before_end_s=a+.5, silence_after_start_s=b-.5)
                t = np.arange(5*rate)/rate
                z[round((a+.5)*rate):round((b-.5)*rate)] = (.1+.001*index)*np.sin(2*np.pi*take['lowest_f0_hz']*t)
                count = di.MIN_NOTES[seg['kind']]
                labels += [round(v*rate) for v in np.linspace(a+.5, b-1., count)]
            take['wav_path'] = self.root/f'{take["id"]}.wav'; take['wav'] = take['wav_path'].name
            take['labels_path'] = self.root/f'{take["id"]}.csv'; take['labels'] = take['labels_path'].name
            wavfile.write(take['wav_path'], rate, z)
            take['labels_path'].write_text('sample,keep\n'+''.join(f'{v},yes\n' for v in labels))
            take['labels_reviewed'] = True
            take['take_note'] = {k: 'synthetic fixture, not a session' for k in take['take_note']}
            take['take_note'].update(buffer_samples=128, riff_bpm=140, capture_rate_hz=rate, native_recording_confirmed=True)
        self.assertEqual(di.check(m), [])
        m['takes'][0]['take_note']['native_recording_confirmed'] = False
        m['takes'][1]['segments'][4]['f0_hint_hz'] = 1
        m['takes'][2]['wav_path'] = m['takes'][1]['wav_path']
        errors = di.check(m)
        self.assertTrue(any('confirm separate native' in s for s in errors))
        self.assertTrue(any('preserve nominal f0' in s for s in errors))
        self.assertTrue(any('WAV content reused' in s for s in errors))

    def test_duplicate_take_and_missing_segment(self):
        m = di.load_manifest(di.ROOT/'Validation/transpose-di-take-template.json')
        m['takes'][1]['id'] = m['takes'][0]['id']; m['takes'][0]['segments'].pop()
        errors = di.check(m)
        self.assertIn('duplicate take ids', errors)
        self.assertTrue(any('seven required' in s for s in errors))

    def test_freeze_hashes_metadata_audio_and_helpers(self):
        p, m = self.manifest(); renderer = self.root/'probe'; renderer.write_bytes(b'synthetic')
        before = di.freeze_record(m, p, renderer, 'corr40-split-v3')
        self.assertIn('Tools/TransposeNext/attack_snr.py', before['metric_sources_sha256'])
        original = json.loads(p.read_text()); original['takes'][0]['take_note']['note'] = 'changed'
        p.write_text(json.dumps(original))
        self.assertNotEqual(before['manifest_sha256'], di.freeze_record(di.load_manifest(p), p, renderer, 'corr40-split-v3')['manifest_sha256'])
        self.write(self.signal()*.5)
        self.assertNotEqual(before['takes'], di.freeze_record(m, p, renderer, 'corr40-split-v3')['takes'])

    def test_synthetic_cannot_freeze_as_real_di(self):
        p, _ = self.manifest()
        args = argparse.Namespace(manifest=p, renderer=self.root/'unused', engine='corr40-split-v3', out=self.root/'pre.json')
        with self.assertRaisesRegex(SystemExit, 'actual recorded-DI'):
            di.cmd_freeze(args)
        self.assertFalse(args.out.exists())

    def test_comparison_keeps_faint_outlier_and_quantization(self):
        row = dict(take='bass-B0-44k1', instrument='bass_B0', segment='long', rate=44100,
                   shift=2, attack_additional_ms=[3., 55.74], attack_event_snr_db=[12., 3.],
                   lowband_additional_ms=[15.6])
        before = copy.deepcopy(row); report = summarize([row])
        self.assertEqual(row, before)
        group = report['by_rate']['44100']
        self.assertEqual(group['pdc_samples'], 110)
        self.assertAlmostEqual(group['attack']['faint']['residual_nominal_2_5ms']['max_ms'], 53.24)
        self.assertAlmostEqual(group['lowband']['residual_nominal_2_5ms']['max_ms'], 13.1)
        self.assertEqual(group['attack']['snr_at_least_threshold']['added']['events'], 1)

    def test_comparison_empty(self):
        self.assertEqual(summarize([])['by_rate'], {})

    def test_host_failure_writes_report_and_exits_nonzero(self):
        import t6_host_check as host
        from subprocess import CompletedProcess
        executable = self.root/'host'; executable.write_bytes(b'test only')
        plugin = self.root/'test.vst3'; plugin.mkdir()
        binary = plugin/'Contents'/'MacOS'/'test'; binary.parent.mkdir(parents=True)
        binary.write_bytes(b'test only')
        out = self.root/'failed-host'
        args = ['host', '--plugin', str(plugin), '--host-check', str(executable), '--out', str(out),
                '--rates', '48000', '--shifts', '1', '--engines', 'v3', '--no-xvfb']
        with patch('sys.argv', args), patch.object(host.subprocess, 'run', return_value=CompletedProcess([], 1, '', 'failed fixture')), contextlib.redirect_stdout(io.StringIO()):
            self.assertEqual(host.main(), 1)
        summary = json.loads((out/'host_check.json').read_text())['summary']
        self.assertEqual(summary['errors'], 6)
        self.assertFalse(summary['ok'])
        self.assertEqual(summary['plugin_sha256'], di.sha(binary))


if __name__ == '__main__':
    unittest.main()
