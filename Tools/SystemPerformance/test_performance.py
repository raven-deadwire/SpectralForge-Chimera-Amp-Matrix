"""Contract/adversarial tests only. Fixtures are never product measurements."""
import copy
import csv
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

import performance as perf


class PerformanceContracts(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.root = Path(self.tmp.name)
        self.study = perf.make_study(perf.environment())
        self.study['product'].update(source_sha='a'*40, version='fixture-only', build_id='test', binary_sha256='b'*64)
        self.study['workload'].update(input_sha256='c'*64, preset_bank_sha256='d'*64,
                                      ir_manifest_sha256='e'*64, description='synthetic test fixture')
        self.study['environment'].update(cpu_model='test-only', ram_bytes=1024,
                                          os_version='test-only', architecture='x86_64')
        self.study['runs'] = self.study['runs'][:1]
        self.run = self.study['runs'][0]
        self.run['settings']['duration_seconds'] = .01
        self.callbacks = self.root/'callbacks.csv'
        self.memory = self.root/'memory.csv'
        # Four callbacks; 128/48000 seconds each. wall=1ms, CPU=.5ms.
        self.rows = [[i, i*2666667, i*2666667+1000000, 500000, '', 128] for i in range(4)]
        self.write_callbacks()
        self.write_memory([[0, 42, 100], [.02, 42, 130]])
        self.receipt = {
            'schema_version': 1, 'run_id': self.run['id'], 'product_binary_sha256': 'b'*64,
            'product_manifest_sha256': perf.digest(self.study['product']),
            'environment_sha256': perf.digest(self.study['environment']),
            'settings_sha256': perf.digest(self.run['settings']),
            'workload_sha256': perf.digest(self.study['workload']),
            'measurement_kind': 'offline_processor', 'collector_name_version': 'fixture-only',
            'telemetry_dropped_rows': 0, 'warmup_seconds': 10,
            'transport_seconds': 0, 'continuous_transport': None, 'recording_verified': None,
            'xruns': None, 'glitches': None, 'crashed': False, 'session_reopen_status': 'NOT_RUN',
            'operator': 'unit test', 'observed_utc': '2026-10-18T00:00:00Z', 'notes': [],
            'callback_scope': 'full_product_graph', 'process_id': 42, 'settings_verified': True}

    def tearDown(self):
        self.tmp.cleanup()

    def write_callbacks(self):
        with self.callbacks.open('w', newline='') as stream:
            writer = csv.writer(stream); writer.writerow(perf.CALLBACK_COLUMNS); writer.writerows(self.rows)

    def write_memory(self, rows):
        with self.memory.open('w', newline='') as stream:
            writer = csv.writer(stream); writer.writerow(perf.MEMORY_COLUMNS); writer.writerows(rows)

    def measure(self):
        return perf.measure(self.study, self.run, self.callbacks, self.memory, self.receipt)

    def measured(self):
        self.run['status'] = 'MEASURED'; self.run['reason'] = 'unit test only'
        self.run['metrics'] = self.measure()
        p = self.root/'receipt.json'; perf.write_json(p, self.receipt)
        self.run['evidence'] = {k: {'path': path.name, 'sha256': perf.file_hash(path)} for k,path in
                                [('callbacks', self.callbacks), ('memory', self.memory), ('receipt', p)]}

    def test_matrix(self):
        rows = perf.scenarios()
        self.assertEqual(62, len(rows)); self.assertEqual(62, len({r['id'] for r in rows}))
        self.assertEqual(36, sum(r['settings']['group']=='core' for r in rows))
        self.assertEqual(2, sum(r['settings']['duration_seconds']==1800 for r in rows))
        for row in rows:
            self.assertEqual((48000, 4), (row['settings']['sample_rate_hz'], row['settings']['oversampling_factor']))
            self.assertIsNone(row['metrics'])

    def test_target_roster_is_unmeasured_and_well_typed(self):
        slots=perf.target_environments()['targets']
        self.assertEqual(13,len(slots))
        schema=perf.read_json(perf.SCHEMA)['properties']['environment']
        for slot in slots:
            perf.schema_validate(slot['environment'],schema)
            self.assertIsNone(slot['environment']['cpu_model'])
            self.assertEqual('BLOCKED',slot['status'])

    def test_wrong_platform_format_rejected(self):
        self.study['environment'].update(os_family='Linux',plugin_format='AAX')
        with self.assertRaises(ValueError): perf.validate(self.study)

    def test_unverified_loaded_settings_rejected(self):
        self.receipt['settings_verified']=False
        with self.assertRaises(ValueError): self.measure()

    def test_strict_json(self):
        for text in ('{"x":1,"x":2}', '{"x":NaN}', '{"x":Infinity}'):
            with self.subTest(text=text), self.assertRaises(ValueError): perf.strict_json(text)

    def test_missing_environment_field(self):
        del self.study['environment']['driver_version']
        with self.assertRaises(ValueError): perf.validate(self.study)

    def test_boolean_not_numeric(self):
        self.run['settings']['duration_seconds'] = True
        with self.assertRaises(ValueError): perf.validate(self.study)

    def test_no_pass_or_certification_shortcut(self):
        for status in ('PASS', 'APPROVED'):
            self.run['status'] = status
            with self.assertRaises(ValueError): perf.validate(self.study)
        self.run['status'] = 'NOT_RUN'; self.study['requirements_certified'] = True
        with self.assertRaises(ValueError): perf.validate(self.study)

    def test_unexecuted_cannot_have_metrics(self):
        self.run['metrics'] = self.measure()
        with self.assertRaises(ValueError): perf.validate(self.study)

    def test_metrics_units_and_nearest_rank(self):
        m = self.measure()
        self.assertEqual(1000, m['callback_wall_p99_us']); self.assertEqual(1000, m['callback_wall_peak_us'])
        self.assertAlmostEqual(37.5, m['callback_wall_load_percent'])
        self.assertAlmostEqual(18.75, m['callback_thread_cpu_load_percent'])
        self.assertEqual(30, m['rss_growth_bytes'])
        self.assertIsNone(m['scheduled_deadline_miss_count']); self.assertIsNone(m['xruns'])
        self.assertEqual(99, perf.percentile(list(range(1,101)), .99))

    def test_cpu_wall_and_scheduled_deadlines_are_distinct(self):
        self.rows = [[0, 0, 3000000, 500000, 4000000, 128], [1, 4000000, 4500000, 500000, 4200000, 128],
                     [2, 6000000, 6500000, 500000, 7000000, 128], [3, 8000000, 8500000, 500000, 9000000, 128]]
        self.write_callbacks(); m = self.measure()
        self.assertEqual(1, m['compute_over_budget_count']); self.assertEqual(1, m['scheduled_deadline_miss_count'])
        self.assertEqual(500, m['callback_thread_cpu_peak_us']); self.assertEqual(3000, m['callback_wall_peak_us'])

    def test_missing_cpu_counter_is_null(self):
        for row in self.rows: row[3]=''
        self.write_callbacks(); self.assertIsNone(self.measure()['callback_thread_cpu_load_percent'])

    def test_partial_counters_rejected(self):
        for col in (3, 4):
            rows = copy.deepcopy(self.rows)
            self.rows[0][col] = '' if col == 3 else 9000000
            self.write_callbacks()
            with self.assertRaises(ValueError): self.measure()
            self.rows=rows

    def test_invalid_callback_population(self):
        for col,value in ((0,8),(1,-1),(2,-1),(5,256),(3,'NaN')):
            saved=copy.deepcopy(self.rows); self.rows[0][col]=value; self.write_callbacks()
            with self.subTest(col=col), self.assertRaises(ValueError): self.measure()
            self.rows=saved

    def test_overlapping_callbacks_rejected(self):
        self.rows[1][1]=0; self.write_callbacks()
        with self.assertRaises(ValueError): self.measure()

    def test_missing_identity_and_stale_receipts_rejected(self):
        for key in ('run_id','product_binary_sha256','product_manifest_sha256','environment_sha256','settings_sha256','workload_sha256','callback_scope'):
            saved=self.receipt[key]; self.receipt[key]='wrong'
            with self.subTest(key=key), self.assertRaises(ValueError): self.measure()
            self.receipt[key]=saved

    def test_dropped_telemetry_and_warmup_rejected(self):
        self.receipt['telemetry_dropped_rows']=1
        with self.assertRaises(ValueError): self.measure()
        self.receipt['telemetry_dropped_rows']=0; self.receipt['warmup_seconds']=0
        with self.assertRaises(ValueError): self.measure()

    def test_measurement_cannot_outlive_trace(self):
        self.run['settings']['duration_seconds']=60
        self.receipt['settings_sha256']=perf.digest(self.run['settings'])
        with self.assertRaises(ValueError): self.measure()

    def test_short_memory_population_rejected(self):
        self.write_memory([[0,42,100]])
        with self.assertRaises(ValueError): self.measure()

    def test_memory_identity_gap_and_counterfeit_sample(self):
        for rows in ([[0,43,100],[.02,43,100]], [[0,42,100],[6,42,100]], [[0,42,100],[0,42,100]], [[0,42,-1],[.02,42,100]]):
            self.write_memory(rows)
            with self.subTest(rows=rows), self.assertRaises(ValueError): self.measure()

    def test_valid_raw_evidence_recomputed(self):
        self.measured(); perf.validate(self.study,self.root)
        self.run['metrics']['callback_wall_p99_us']=1
        with self.assertRaises(ValueError): perf.validate(self.study,self.root)

    def test_raw_tampering_rejected(self):
        self.measured(); self.callbacks.write_text(self.callbacks.read_text()+'\n')
        with self.assertRaises(ValueError): perf.validate(self.study,self.root)

    def test_path_escape_rejected(self):
        self.measured(); self.run['evidence']['callbacks']['path']='../escape.csv'
        with self.assertRaises(ValueError): perf.validate(self.study,self.root)

    def test_required_binary_and_workload(self):
        self.measured()
        for group,key in (('product','binary_sha256'),('product','source_sha'),('workload','input_sha256')):
            saved=self.study[group][key]; self.study[group][key]=None
            with self.subTest(key=key), self.assertRaises(ValueError): perf.validate(self.study,self.root)
            self.study[group][key]=saved

    def test_debug_cannot_be_requirements_evidence(self):
        self.measured(); self.study['product']['build_type']='Debug'
        with self.assertRaises(ValueError): perf.validate(self.study,self.root)

    def test_actual_daw_requires_device_metadata(self):
        self.receipt['measurement_kind']='daw_device'
        with self.assertRaises(ValueError): self.measure()

    def soak(self, kind):
        self.run['settings'].update(group='soak',duration_seconds=1800,transport='recording')
        self.receipt.update(settings_sha256=perf.digest(self.run['settings']), measurement_kind=kind,
                            transport_seconds=1800, continuous_transport=True, recording_verified=True,
                            xruns=0,glitches=0)
        env=self.study['environment']
        env.update(daw_name='test',daw_version='test',plugin_format='VST3',interface_model='test',
                   driver_name='test',driver_version='test',power_mode='test',power_source='AC')
        self.receipt['environment_sha256']=perf.digest(env)
        m=perf.summarize(self.callbacks,self.memory,self.run['settings'])
        m.update(audio_seconds=1800,callback_span_seconds=1800,rss_observed_seconds=1800)
        return patch.object(perf,'summarize',return_value=m)

    def test_long_synthetic_render_never_becomes_daw_soak(self):
        with self.soak('offline_processor'):
            self.assertEqual('BLOCKED',self.measure()['stability_status'])

    def test_soak_missing_xruns_and_recording_remain_blocked(self):
        with self.soak('daw_device'):
            self.receipt['xruns']=None
            self.assertEqual('BLOCKED',self.measure()['stability_status'])
            self.receipt['xruns']=0; self.receipt['recording_verified']=None
            self.assertEqual('BLOCKED',self.measure()['stability_status'])

    def test_soak_failure_and_scoped_pass(self):
        with self.soak('daw_device'):
            self.receipt['xruns']=1
            self.assertEqual('FAIL',self.measure()['stability_status'])
            self.receipt['xruns']=0
            self.assertEqual('PASS',self.measure()['stability_status'])
            self.assertFalse(self.study['requirements_certified'])

    def test_known_soak_failure_not_hidden_by_missing_counter(self):
        with self.soak('daw_device'):
            self.receipt.update(xruns=None, crashed=True)
            self.assertEqual('FAIL',self.measure()['stability_status'])
            self.receipt.update(crashed=False, recording_verified=False)
            self.assertEqual('FAIL',self.measure()['stability_status'])

    def test_real_process_memory_sampler_smoke(self):
        if sys.platform.startswith('linux'):
            try:
                cmdline = Path(f'/proc/{os.getpid()}/cmdline').read_bytes()
                if b'python' not in cmdline.lower():
                    self.skipTest('executor PID namespace does not expose the Python process')
            except OSError:
                self.skipTest('executor does not expose the Python process in /proc')
        perf.sample_memory(os.getpid(),.12,.1,self.memory)
        rows=list(perf.csv_rows(self.memory,perf.MEMORY_COLUMNS))
        self.assertGreaterEqual(len(rows),2); self.assertGreater(int(rows[0]['rss_bytes']),0)

    def test_pid_reuse_rejected(self):
        with patch.object(perf, 'process_identity', side_effect=['a','b']):
            with self.assertRaises(ValueError): perf.sample_memory(42, .12, .1, self.memory)

    def test_cli_incomplete_receipt_fails_without_output(self):
        path=self.root/'study.json'; perf.write_json(path,self.study)
        receipt=self.root/'incomplete.json'; perf.write_json(receipt,{'run_id':self.run['id']})
        out=self.root/'result.json'
        result=subprocess.run([sys.executable,str(Path(perf.__file__)),'ingest',str(path),'--run',self.run['id'],
                               '--callbacks',str(self.callbacks),'--memory',str(self.memory),'--receipt',str(receipt),'--out',str(out)],capture_output=True)
        self.assertEqual(2,result.returncode); self.assertFalse(out.exists())


if __name__ == '__main__':
    unittest.main()
