#!/usr/bin/env python3
"""Fabricated gate fixtures only. Never actual P0, listening or DAW acceptance."""
import copy
import io
import json
from pathlib import Path
import tempfile
import unittest
import zipfile
from unittest.mock import patch
import issue97_gate as gate

SHA, VERSION = 'a'*40, '1.3.2-beta.2'

class FakeAPI:
    def __init__(self, artifacts):
        self.artifacts = artifacts
        self.bad_run = None
        self.bad_jobs = None
        self.bad_artifact = None
    def run(self, workflow='build.yml'):
        result = dict(id=17, head_sha=SHA, run_attempt=2, status='completed', conclusion='success',
                      repository={'full_name': gate.REPO}, head_repository={'full_name': gate.REPO},
                      event='pull_request', path='.github/workflows/'+workflow)
        result.update(self.bad_run or {})
        return result
    def json(self, path):
        if path.startswith('/actions/runs/'):
            return self.run()
        index = int(path.split('/')[-1])
        result = dict(id=index, expired=False, digest='sha256:'+gate.digest(self.artifacts[index]),
                      workflow_run={'id': 17, 'head_sha': SHA})
        result.update(self.bad_artifact or {})
        return result
    def pages(self, path, key):
        if key == 'workflow_runs':
            return [self.run(path.split('/workflows/')[1].split('/')[0])]
        return self.bad_jobs if self.bad_jobs is not None else [dict(status='completed', conclusion='success', run_attempt=2)]
    def download(self, path, dest):
        dest.write_bytes(self.artifacts[int(path.split('/')[-2])])


class P0Gate(unittest.TestCase):
    def setUp(self):
        self.work = tempfile.TemporaryDirectory()
        self.addCleanup(self.work.cleanup)
        self.assets = {}
        for suffix in ('win64-Setup.exe', 'win64.zip', 'macos-universal.pkg', 'linux-x86_64.deb', 'linux-x86_64.tar.gz'):
            name = 'SpectralForge-Chimera-'+VERSION+'-'+suffix
            path = Path(self.work.name)/name
            path.write_bytes(b'SYNTHETIC PACKAGE '+name.encode())
            self.assets[name] = path
        self.packages = {n: gate.digest(p.read_bytes()) for n,p in self.assets.items()}
        self.binaries = {}
        for platform in gate.PLATFORMS:
            formats = ['Standalone', 'VST3'] + (['AU', 'AAX'] if platform.startswith('macos') else
                                               ['AAX'] if platform.startswith('windows') else [])
            package = next(n for n in self.packages if ('win64-Setup' if platform.startswith('windows') else
                              'macos-universal' if platform.startswith('macos') else 'linux-x86_64.deb') in n)
            self.binaries[platform] = [dict(platform=platform, path='SYNTHETIC/'+f, sha256='b'*64,
                                                    format=f, package=package) for f in formats]
        self.files = {'issue97/raw.txt': b'SYNTHETIC RAW FIXTURE - NOT ACCEPTANCE'}
        self.raw = dict(path='issue97/raw.txt', sha256=gate.digest(self.files['issue97/raw.txt']))
        self.source_policy = gate.canonical(dict(version=VERSION, expiry_approved=True, expires_at_utc='2027-01-01T00:00:00Z'))
        path = Path(self.work.name)/'chimera-beta-lifecycle.json'
        path.write_bytes(gate.canonical(dict(version=VERSION, source_sha=SHA, expires_at_utc='2027-01-01T00:00:00Z',
                                             expiry_approved=True, mode='approved')))
        self.assets[path.name] = path
        self.expiry = dict(version=VERSION, approved_by='SYNTHETIC REVIEWER', approved_at='2026-10-20T00:00:00Z',
                           expires_at_utc='2027-01-01T00:00:00Z', policy_sha256=gate.digest(self.source_policy))
        self.manifest = dict(schema=gate.SCHEMA, schema_version=1, version=VERSION, source_sha=SHA, run_id=17, run_attempt=2,
                             requirements_sha256=gate.contract_digest(), packages=self.packages,
                             binaries=self.binaries, expiry_policy=self.expiry,
                             comparison_protocol=dict(reviewer='SYNTHETIC', approved_at='2026-10-20T00:00:00Z',
                             rationale='SYNTHETIC LIMITS ONLY', limits={k:1 for k in
                             ('response_db','timing_ms','phase_degrees','latency_ms','meter_db','analyzer_db','bypass_transition_db')}),
                             reports={})
        self.reports = {}
        for row, spec in gate.requirements().items():
            platform = row.split('/')[1]
            report = dict(schema=gate.SCHEMA+'.report', version=VERSION, source_sha=SHA, id=row, kind=spec['kind'],
                          synthetic=False, executed_at='2026-10-20T00:00:00Z', packages=self.packages,
                          installed_binaries=self.binaries[platform], raw=[self.raw],
                          assertions={a:dict(passed=True, method='SYNTHETIC', raw_path=self.raw['path']) for a in spec['assertions']},
                          reviewer='SYNTHETIC', host_name='Pro Tools', host_version='SYNTHETIC', os_version='SYNTHETIC',
                          installed_release_package=True, developer_host=False, run_id=17, run_attempt=2,
                          expiry_policy=self.expiry,
                          signature_commands={k:dict(command='SYNTHETIC', exit_code=0, raw_path=self.raw['path'])
                                              for k in ('pace','platform','installed')},
                          environment={k:'SYNTHETIC' for k in ('cpu','ram','os','daw','format','interface','driver',
                                       'sample_rate','buffer','oversampling','preset','power_mode','gui')},
                          metrics={})
            report['environment'].update(sample_rate=48000, buffer=128, oversampling=4)
            report['metrics'] = dict(latency_cases=[dict(sample_rate=48000,semitones=s,algorithmic_ms=10,
                                     weak_attack_max_ms=10,host_reported_ms=10,roundtrip_ms=20) for s in (-2,-1,1,2)],
                                     input='Azhi Dahaka / bass Low B / -2', di_sha256=self.raw['sha256'],
                                     reference_capture_sha256=self.raw['sha256'], reference_product_version='SYNTHETIC',
                                     level_alignment_db=0,
                                     references={p:dict(version='SYNTHETIC',capture_sha256=self.raw['sha256']) for p in
                                     ('Pro-Q 4','Pro-C 3','Pro-MB','Pro-G','Pro-L 2','Pro-DS')},
                                     **{k:0 for k in self.manifest['comparison_protocol']['limits']},
                                     duration_minutes=30, callback_p99_us=10, callback_peak_us=15, deadline_misses=0,
                                     memory_growth_bytes=0, memory_peak_bytes=1024)
            self.reports[row] = report
        self.repack()

    def repack(self):
        artifacts = {}
        for index, (row, report) in enumerate(self.reports.items(),1):
            path = 'issue97/'+row+'.json'
            raw = gate.canonical(report)
            self.files[path] = raw
            entry = dict(report=dict(path=path, sha256=gate.digest(raw)))
            if report['kind'] == 'ci':
                output = io.BytesIO()
                with zipfile.ZipFile(output,'w') as archive:
                    archive.writestr('report.json',raw)
                artifacts[index] = output.getvalue()
                entry['ci'] = dict(run_id=17, run_attempt=2, artifact_id=index,
                                   artifact_sha256=gate.digest(output.getvalue()), report_member='report.json')
            self.manifest['reports'][row] = entry
        self.manifest_raw = gate.canonical(self.manifest)
        self.approval = dict(schema=gate.SCHEMA,requirements_sha256=gate.contract_digest(),manifest_sha256=gate.digest(self.manifest_raw))
        output=io.BytesIO()
        with zipfile.ZipFile(output,'w') as archive:
            archive.writestr('issue97/manifest.json',self.manifest_raw)
        artifacts[1000]=output.getvalue()
        self.approval['ci']=dict(run_id=17, run_attempt=2, artifact_id=1000,
                                artifact_sha256=gate.digest(output.getvalue()),report_member='issue97/manifest.json')
        self.api = FakeAPI(artifacts)

    def evaluate(self, **kwargs):
        args = dict(version=VERSION, source_sha=SHA, approval=self.approval, assets=self.assets, api=self.api, lifecycle_policy_raw=self.source_policy)
        args.update(kwargs)
        return gate.evaluate(self.manifest_raw,self.files,**args)

    def rejected(self, **kwargs):
        with self.assertRaises((gate.EvidenceError, ValueError)):
            self.evaluate(**kwargs)

    def test_complete_fabricated_contract_can_pass(self):
        self.assertEqual(self.evaluate()['reports'], 27)
    def test_no_live_provenance_is_blocked(self):
        self.rejected(api=None)
    def test_missing_binding(self):
        self.rejected(approval=None)
    def test_other_version_or_source(self):
        self.rejected(version='1.3.2-beta.3')
        self.rejected(source_sha='d'*40)
    def test_unreviewed_manifest(self):
        self.manifest_raw += b' '
        self.rejected()
    def test_missing_report_even_with_fresh_fingerprint(self):
        self.manifest['reports'].pop('aax/windows-x64')
        self.manifest_raw=gate.canonical(self.manifest)
        self.approval['manifest_sha256']=gate.digest(self.manifest_raw)
        self.rejected()
    def test_missing_or_changed_raw(self):
        self.files[self.raw['path']]=b'altered'
        self.rejected()
    def test_traversal_and_duplicate_json(self):
        for path in ('../x','/x','a\\b'):
            with self.subTest(path=path), self.assertRaises(gate.EvidenceError):
                gate.safe_file({},dict(path=path,sha256='a'*64))
        with self.assertRaises(gate.EvidenceError):
            gate.load(b'{"pass":true,"pass":false}')
    def test_actual_package_changed(self):
        next(iter(self.assets.values())).write_bytes(b'replaced')
        self.rejected()
    def test_no_aax_format(self):
        self.binaries['windows-x64'].pop()
        self.repack(); self.rejected()
    def test_report_other_executable(self):
        self.reports['native_daw/windows-x64']['installed_binaries']=[]
        self.repack(); self.rejected()
    def test_synthetic_and_failed_assertion(self):
        row='transpose/windows-x64'
        self.reports[row]['synthetic']=True
        self.repack(); self.rejected()
        self.reports[row]['synthetic']=False
        self.reports[row]['assertions']['attack']['passed']=False
        self.repack(); self.rejected()
    def test_latency_cannot_be_relaxed_by_review(self):
        sample=self.reports['transpose/windows-x64']['metrics']['latency_cases'][0]
        for value in (10.001,34.6,float('inf'),True,-1):
            with self.subTest(value=value):
                sample['weak_attack_max_ms']=value
                if value == float('inf'):
                    with self.assertRaises(ValueError): self.repack()
                else:
                    self.repack(); self.rejected()
    def test_original_capture_missing(self):
        self.reports['fabfilter/windows-x64']['metrics']['references'].pop('Pro-MB')
        self.repack(); self.rejected()
    def test_undefined_or_exceeded_comparison_tolerance(self):
        self.manifest['comparison_protocol']['limits']['response_db']=None
        self.repack(); self.rejected()
        self.manifest['comparison_protocol']['limits']['response_db']=1
        self.reports['fabfilter/windows-x64']['metrics']['response_db']=1.1
        self.repack(); self.rejected()
    def test_other_version_expiry_not_inherited(self):
        self.expiry['version']='1.3.2-beta.1'
        self.repack(); self.rejected()
    def test_developer_pro_tools_not_retail_acceptance(self):
        self.reports['aax/macos-x64']['developer_host']=True
        self.repack(); self.rejected()
    def test_missing_signature_or_host(self):
        self.reports['aax/windows-x64']['signature_commands']['pace']['exit_code']=1
        self.repack(); self.rejected()
    def test_soak_deadline_miss_not_masked(self):
        self.reports['system/macos-arm64']['metrics']['deadline_misses']=1
        self.repack(); self.rejected()
    def test_latest_failed_queued_or_foreign_run(self):
        for change in (dict(conclusion='failure'),dict(status='queued'),dict(head_sha='b'*40),
                       dict(run_attempt=3),dict(head_repository={'full_name':'fork/repo'})):
            with self.subTest(change=change):
                self.api.bad_run=change
                self.rejected()
    def test_skipped_or_failed_job(self):
        for status in ('skipped','failure','cancelled'):
            self.api.bad_jobs=[dict(status='completed',conclusion=status,run_attempt=2)]
            self.rejected()
    def test_expired_or_stale_artifact(self):
        for change in (dict(expired=True),dict(digest='sha256:'+'f'*64),dict(workflow_run={'id':16,'head_sha':SHA})):
            self.api.bad_artifact=change
            self.rejected()
    def test_actual_artifact_bytes_and_report_are_verified(self):
        index=next(iter(self.api.artifacts))
        self.api.artifacts[index]=b'changed'
        self.rejected()
    def test_old_attempt_report_cannot_use_new_attempt(self):
        self.reports['integration/windows-x64']['run_attempt']=1
        self.repack(); self.rejected()
    def test_pack_roundtrip_and_context_bounds(self):
        root=Path(self.work.name)/'evidence'
        for name,raw in {**self.files,'issue97/manifest.json':self.manifest_raw}.items():
            target=root/name
            target.parent.mkdir(parents=True,exist_ok=True)
            target.write_bytes(raw)
        archive_path=Path(self.work.name)/'packed.zip'
        result=gate.pack_evidence(root,archive_path)
        self.assertEqual(result['manifest_sha256'],self.approval['manifest_sha256'])
        with zipfile.ZipFile(archive_path) as archive:
            context=gate.context_from_zip(archive,self.assets,self.api,self.source_policy)
            self.assertEqual(gate.evaluate(**context,version=VERSION,source_sha=SHA,
                                           approval=self.approval)['verdict'],'PASS')
    def test_generated_publication_evidence_is_not_a_binary_package(self):
        path=Path(self.work.name)/'release-issue97-evidence.zip'
        path.write_bytes(b'SYNTHETIC P0 EVIDENCE')
        self.assets[path.name]=path
        self.assertEqual(self.evaluate()['verdict'],'PASS')
    def test_actual_packaged_metadata_and_source_policy_must_match_expiry(self):
        self.source_policy=b'{}'
        self.rejected()
        self.source_policy=gate.canonical(dict(version=VERSION,expiry_approved=True,expires_at_utc='2027-01-01T00:00:00Z'))
        self.assets['chimera-beta-lifecycle.json'].write_bytes(b'{}')
        self.rejected()
    def test_manifest_artifact_must_match_current_run_attempt(self):
        self.approval['ci']['run_attempt']=1
        self.rejected()
    def test_direct_weekly_evaluation_requires_real_p0_context(self):
        from test_weekly_publication import WeeklyPublication
        fixture=WeeklyPublication('test_issue97_guard_preserves_published_beta1_contract')
        fixture.setUp()
        self.addCleanup(fixture.doCleanups)
        fixture.strict_pass_fixture(VERSION,'beta_1_3')
        fixture.policy['issue97']=self.approval
        with self.assertRaisesRegex(gate.EvidenceError.__bases__[0], 'ISSUE97_EVIDENCE_MISSING'):
            fixture.evaluate()
        # This passing fixture exercises the full additive path; all inputs are
        # fabricated and no actual release receipt is written.
        context = dict(manifest_raw=self.manifest_raw, files=self.files, assets=self.assets,
                       api=self.api, lifecycle_policy_raw=self.source_policy)
        self.assertTrue(fixture.evaluate(p0_context=context)['publication_allowed'])
    def test_strict_gap_exception_cannot_override_new_binding(self):
        from test_weekly_publication import WeeklyPublication
        fixture=WeeklyPublication('test_issue97_guard_preserves_published_beta1_contract')
        fixture.setUp(); self.addCleanup(fixture.doCleanups)
        fixture.version=VERSION;fixture.set_scope()
        fixture.policy=fixture.approval('OWNER_ACCEPTED_BETA_WITH_VALIDATION_GAPS')
        fixture.policy['issue97']=self.approval
        with self.assertRaisesRegex(gate.EvidenceError.__bases__[0], 'cannot accept legacy beta gaps'):
            fixture.evaluate()

if __name__ == '__main__':
    unittest.main()
