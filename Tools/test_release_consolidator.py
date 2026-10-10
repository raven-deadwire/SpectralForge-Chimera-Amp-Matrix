#!/usr/bin/env python3
"""Raw platform receipts -> beta gate -> publisher, with fail-closed mutations."""
import copy
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
import zipfile

import consolidate_release_gate as consolidator
import release_evidence as evidence
import publish_beta13 as publisher

gate = evidence.gate
HEAD = 'a' * 40
RUN = '123'
ATTEMPT = '2'


class Consolidation(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.inputs = self.root / 'inputs'
        self.policy, self.mapping = evidence.configuration()
        self.revision = evidence.identity(HEAD, RUN, ATTEMPT)
        self.index = 0
        for kind in evidence.KINDS:
            source = self.root / kind
            def raw(key, data):
                path = source / evidence.RAW_FILES[key]
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text(data if isinstance(data, str) else json.dumps(data), encoding='utf-8')
            if kind == 'assembly':
                raw('candidate', {'version': self.policy['release_scope']['version'],
                    'tag': 'v' + self.policy['release_scope']['version'],
                    'revision': HEAD, 'runId': RUN, 'published': False})
                version = self.policy['release_scope']['version']
                prefix = 'SpectralForge-Chimera-' + version + '-'
                assets = [{'platform': platform, 'name': prefix + suffix, 'sha256': 'a' * 64, 'size': 10}
                          for platform, suffix in [('windows', 'win64-Setup.exe'), ('macos', 'macos-universal.pkg'), ('linux', 'linux-x86_64.deb')]]
                raw('manifest', {'schema': 1, 'version': version, 'assets': assets})
                suffixes = ('win64-Setup.exe', 'win64.zip', 'macos-universal.pkg', 'linux-x86_64.deb', 'linux-x86_64.tar.gz')
                raw('sums', ''.join('a' * 64 + '  ' + prefix + suffix + '\n' for suffix in suffixes))
            else:
                tests = [s['producer'][6:] for s in self.mapping.values()
                         if s['producer'].startswith('ctest:') and s['platform'] in ('all', kind)]
                tests = sorted(set(tests + ['ChimeraAmpNativeTests', 'ChimeraGateProcessorTests'] +
                                   (['ChimeraUITests'] if kind == 'windows' else [])))
                raw('inventory', {'tests': [{'name': n} for n in tests]})
                raw('ctest', '<testsuite>' + ''.join('<testcase name="' + n + '" status="run"/>' for n in tests) + '</testsuite>')
                # Fixed, completed sections from the exact 6782 checkpoint run;
                # these are independent of the map patterns. See fixtures/README.md.
                raw('dsp_log', (evidence.ROOT / 'Tools/fixtures/e1-ctest-6782.log').read_text(encoding='utf-8'))
                if kind == 'windows':
                    raw('installer', {'success': True, 'source_sha': HEAD, 'run_id': RUN,
                        'version': self.policy['release_scope']['version'], 'checks': ['PASS: fixture']})
                    raw('installer_log', 'PASS: fixture')
                    raw('msix', 'PASS: Uninstall removes only the CI package registration and preserves the external user IR library')
                    raw('scan', 'Source revision: ' + HEAD + '\nPASS: completed local Defender inspection.')
                    raw('scan_inventory', [{'Path': 'fixture.exe', 'Bytes': 10, 'SHA256': 'a' * 64}])
                    raw('scan_status', {'AMServiceEnabled': True, 'AMEngineVersion': '1', 'AntivirusSignatureVersion': '1'})
                else:
                    raw(kind + '_package', 'PASS: package fixture')
            evidence.collect_bundle(source, self.folder(kind), kind, self.revision,
                {key: 'success' for key in ('ctest', 'package', 'installer', 'msix', 'scan', 'assemble', 'source', 'upload')})

    def folder(self, kind):
        return self.inputs / (evidence.BUNDLE_PREFIX + kind)

    def mutate(self, kind, change):
        path = self.folder(kind) / 'evidence.json'
        doc = gate.load_json(path)
        change(doc)
        path.write_text(json.dumps(doc), encoding='utf-8')

    def raw_change(self, kind, key, change):
        def apply(doc):
            path = self.folder(kind) / doc['files'][key]['path']
            path.write_text(change(path.read_text(encoding='utf-8')), encoding='utf-8')
            doc['files'][key]['sha256'] = evidence.digest(path)
        self.mutate(kind, apply)

    def run_gate(self):
        self.index += 1
        output = self.root / ('result' + str(self.index))
        return consolidator.consolidate(self.inputs, output, self.revision, 'beta_1_3'), output

    def archive(self, output):
        path = self.root / 'gate.zip'
        with zipfile.ZipFile(path, 'w') as archive:
            for file in output.rglob('*'):
                if file.is_file():
                    archive.write(file, 'validation/' + file.relative_to(output).as_posix())
        return path

    def status(self, result, cid):
        return next(c['computed_status'] for c in result['checks'] if c['id'] == cid)

    def test_actual_automatic_receipts_pass_only_owned_checks(self):
        result, output = self.run_gate()
        self.assertEqual(result['producer']['errors'], [])
        self.assertEqual(result['profile'], 'beta_1_3')
        self.assertEqual(result['revision'], self.revision)
        self.assertEqual(result['counts']['checks']['pass'], len(self.mapping))
        self.assertTrue(all(self.status(result, cid) == 'PASS' for cid in self.mapping))
        for cid in ('B1.RESULT', 'B8.RESULT', 'B12.RESULT', 'E2.SAME_DI', 'I2.LIVE_REMOVE_DAW', 'I2.UI_AUDIO_TIMING'):
            self.assertEqual(self.status(result, cid), 'BLOCKED')
        with self.assertRaisesRegex(RuntimeError, 'not PASS'):
            publisher.verify_release_gate_archive(self.archive(output), HEAD, 'beta_1_3', RUN, ATTEMPT)

    def test_common_check_requires_all_three_platforms(self):
        self.raw_change('linux', 'ctest', lambda s: s.replace('name="ChimeraTests" status="run"/>',
             'name="ChimeraTests" status="run"><failure/></testcase>'))
        result, _ = self.run_gate()
        self.assertEqual(self.status(result, 'A12.01'), 'BLOCKED')
        self.assertEqual(self.status(result, 'A13.01'), 'PASS')
        self.assertEqual(self.status(result, 'A13.02'), 'BLOCKED')
        self.assertEqual(self.status(result, 'I1.LIVE_REMOVE'), 'PASS')

    def test_e1_requires_portable_and_windows_specific_evidence(self):
        self.raw_change('linux', 'ctest', lambda s: s.replace('name="ChimeraAmpNativeTests" status="run"/>',
             'name="ChimeraAmpNativeTests" status="run"><skipped/></testcase>'))
        result, _ = self.run_gate()
        self.assertEqual(self.status(result, 'E1.DSP'), 'BLOCKED')
        self.assertEqual(self.status(result, 'E1.STATE_UI'), 'PASS')
        self.raw_change('windows', 'ctest', lambda s: s.replace('name="ChimeraUITests" status="run"/>',
             'name="ChimeraUITests" status="run"><failure/></testcase>'))
        result, _ = self.run_gate()
        self.assertEqual(self.status(result, 'E1.STATE_UI'), 'BLOCKED')
        self.assertEqual(self.status(result, 'E1.LEGACY'), 'BLOCKED')

    def test_generic_green_ctest_without_subsuite_evidence_is_insufficient(self):
        self.raw_change('windows', 'dsp_log', lambda _: 'PASS generic CTest\n')
        result, _ = self.run_gate()
        for cid in ('E1.DSP', 'E1.STATE_UI', 'E1.LEGACY'):
            self.assertEqual(self.status(result, cid), 'BLOCKED')
        self.assertEqual(self.status(result, 'A12.06'), 'PASS')

    def test_pre_niflheimr_coverage_does_not_certify_current_e1(self):
        # Real earlier complete output passed the old map, but does not cover
        # the appended amplifier. Green JUnit must not hide that coverage gap.
        previous = (evidence.ROOT / 'Tools/fixtures/e1-ctest.log').read_text(encoding='utf-8')
        self.raw_change('windows', 'dsp_log', lambda _: previous)
        result, _ = self.run_gate()
        self.assertEqual(self.status(result, 'E1.DSP'), 'BLOCKED')
        self.assertEqual(self.status(result, 'E1.STATE_UI'), 'BLOCKED')
        self.assertEqual(self.status(result, 'E1.LEGACY'), 'PASS')
        self.assertEqual(result['counts']['checks']['pass'], 19)
        self.assertEqual(result['counts']['checks']['blocked'], 109)

    def test_current_e1_coverage_totals_remain_exact(self):
        current = (self.folder('windows') / 'raw/LastTest.log').read_text(encoding='utf-8')
        cases = {name: True for name in ('ChimeraAmpNativeTests', 'ChimeraUITests')}
        for cid, before, after in (
            ('E1.DSP', '26 serialized / 25 active', '25 serialized / 25 active'),
            ('E1.DSP', '/ 25 active models, 407 controls', '/ 24 active models, 407 controls'),
            ('E1.DSP', '407 controls, six contexts', '393 controls, six contexts'),
            ('E1.DSP', 'CONTROL_RESPONSE count=407', 'CONTROL_RESPONSE count=393'),
            ('E1.DSP', 'sample-rates/channel routes=183', 'sample-rates/channel routes=168'),
            ('E1.DSP', 'integrated native oversampling paths=104', 'integrated native oversampling paths=100'),
            ('E1.STATE_UI', 'all 26 serialized amplifier panels', 'all 25 serialized amplifier panels'),
            ('E1.STATE_UI', '122 channel cases', '112 channel cases'),
            ('E1.STATE_UI', '2246 channel/control visibility cases', '2106 channel/control visibility cases'),
        ):
            with self.subTest(cid=cid, coverage=before):
                self.assertEqual(current.count(before), 1)
                self.assertTrue(evidence.contract_passed(self.mapping[cid], 'windows', cases, current))
                self.assertFalse(evidence.contract_passed(self.mapping[cid], 'windows', cases,
                                                         current.replace(before, after)))

    def test_legacy_runtime_digest_is_diagnostic_not_a_platform_fingerprint(self):
        # The C++ harness validates the frozen fixture hash, every structural
        # field and the existing numeric tolerance before emitting this marker.
        # ARM libm produces a different *actual* manifest digest from x86.
        self.raw_change('macos', 'dsp_log', lambda s: s.replace(
            'c035babc58914c0270e1b5eefb02881a50b88af8b609401d4d391792437e170c',
            '365253d74967018996c41c7a57715cf39ced3d9f361ef21b9513e2ba95b2fdda')
            .replace('numeric_rounding_differences=99', 'numeric_rounding_differences=292'))
        self.assertEqual(self.status(self.run_gate()[0], 'E1.LEGACY'), 'PASS')
        self.raw_change('macos', 'dsp_log', lambda s: s.replace('3686 entries', '3685 entries'))
        self.assertEqual(self.status(self.run_gate()[0], 'E1.LEGACY'), 'BLOCKED')

    def test_empty_contract_cannot_pass(self):
        self.assertFalse(evidence.contract_passed({'tests': {}}, 'linux', {}, ''))
        self.assertFalse(evidence.contract_passed({'tests': {'ChimeraAmpNativeTests': []}},
                                                  'linux', {'ChimeraAmpNativeTests': True}, ''))

    def test_e1_each_required_marker_and_log_completion_is_necessary(self):
        path = self.folder('windows') / 'raw/LastTest.log'
        original = path.read_text(encoding='utf-8')
        # Remove real output lines rather than constructing output from predicates.
        import re
        for cid in ('E1.DSP', 'E1.STATE_UI', 'E1.LEGACY'):
            for name, patterns in evidence.contract_tests(self.mapping[cid], 'windows').items():
                for pattern in patterns:
                    lines = [line for line in original.splitlines() if re.fullmatch(pattern, line)]
                    self.assertEqual(len(lines), 1)
                    with self.subTest(cid=cid, marker=lines[0]):
                        self.raw_change('windows', 'dsp_log', lambda _, line=lines[0]: original.replace(line, 'MISSING'))
                        self.assertEqual(self.status(self.run_gate()[0], cid), 'BLOCKED')
        for replaced in ('Test Passed.', '<end of output>'):
            with self.subTest(replaced=replaced):
                self.raw_change('windows', 'dsp_log', lambda _: original.replace(replaced, 'INCOMPLETE'))
                self.assertEqual(self.status(self.run_gate()[0], 'E1.DSP'), 'BLOCKED')

    def test_e1_markers_cannot_be_borrowed_from_another_case_or_duplicate(self):
        path = self.folder('windows') / 'raw/LastTest.log'
        original = path.read_text(encoding='utf-8')
        self.raw_change('windows', 'dsp_log', lambda _: original.replace('Testing: ChimeraAmpNativeTests',
                                                                        'Testing: UnrelatedTests'))
        self.assertEqual(self.status(self.run_gate()[0], 'E1.DSP'), 'BLOCKED')
        self.raw_change('windows', 'dsp_log', lambda _: original + original)
        self.assertEqual(self.status(self.run_gate()[0], 'E1.DSP'), 'BLOCKED')
        self.raw_change('windows', 'dsp_log', lambda _: original.replace('PASS AmpNativeTests',
                                                                       'FAIL injected\nPASS AmpNativeTests'))
        self.assertEqual(self.status(self.run_gate()[0], 'E1.DSP'), 'BLOCKED')

    def test_audited_delta_and_all_external_or_undefined_checks_stay_blocked(self):
        result, output = self.run_gate()
        self.assertEqual(result['counts']['checks']['pass'], 21)
        self.assertEqual(result['counts']['checks']['blocked'], 107)
        for check in result['checks']:
            if check['id'] not in self.mapping:
                self.assertEqual(check['computed_status'], 'BLOCKED', check['id'])
                report = gate.load_json(output / 'checks' / (check['id'] + '.json'))
                self.assertFalse(report['execution']['executed'])
        self.assertFalse(result['ready'])

    def test_missing_platform_does_not_become_pass_from_other_platform(self):
        (self.folder('macos') / 'evidence.json').unlink()
        result, _ = self.run_gate()
        self.assertEqual(self.status(result, 'A12.01'), 'BLOCKED')
        self.assertEqual(self.status(result, 'A13.03'), 'BLOCKED')
        self.assertEqual(self.status(result, 'E1.DSP'), 'BLOCKED')
        self.assertEqual(self.status(result, 'E1.LEGACY'), 'BLOCKED')
        self.assertTrue(result['producer']['errors'])

    def test_stale_source_run_attempt_profile_and_policy_are_rejected(self):
        path = self.folder('linux') / 'evidence.json'
        original = path.read_text(encoding='utf-8')
        for field, value in [('commit_sha', 'b' * 40), ('run_id', '124'), ('run_attempt', '1')]:
            with self.subTest(field=field):
                path.write_text(original, encoding='utf-8')
                self.mutate('linux', lambda d: d['revision'].update({field: value}))
                result, _ = self.run_gate()
                self.assertTrue(result['producer']['errors'])
                self.assertEqual(self.status(result, 'A13.02'), 'BLOCKED')
                self.assertEqual(self.status(result, 'E1.DSP'), 'BLOCKED')
        for field, value in [('profile', 'pull_request'), ('policy_version', -1), ('kind', 'windows')]:
            with self.subTest(field=field):
                path.write_text(original, encoding='utf-8')
                self.mutate('linux', lambda d: d.update({field: value}))
                self.assertTrue(self.run_gate()[0]['producer']['errors'])

    def test_modified_raw_file_and_unsafe_path_fail_closed(self):
        self.mutate('linux', lambda d: d['files']['ctest'].update({'sha256': '0' * 64}))
        self.assertTrue(self.run_gate()[0]['producer']['errors'])
        self.mutate('linux', lambda d: d['files']['ctest'].update({'path': '../outside.xml'}))
        self.assertTrue(self.run_gate()[0]['producer']['errors'])

    def test_malformed_json_duplicate_keys_and_xml_emit_blocked_artifact(self):
        path = self.folder('linux') / 'evidence.json'
        original = path.read_text(encoding='utf-8')
        for contents in ('[]', '{bad', '{"kind":"linux","kind":"windows"}'):
            with self.subTest(contents=contents):
                path.write_text(contents, encoding='utf-8')
                result, output = self.run_gate()
                self.assertEqual(result['verdict'], 'BLOCKED')
                self.assertTrue((output / 'release-gate.json').exists())
                self.assertTrue(result['producer']['errors'])
        path.write_text(original, encoding='utf-8')
        self.raw_change('linux', 'ctest', lambda _: '<unclosed')
        self.assertTrue(self.run_gate()[0]['producer']['errors'])

    def test_ctest_failed_step_even_with_passing_junit_is_blocked(self):
        for state in ('failure', 'skipped', 'cancelled', ''):
            with self.subTest(state=state):
                self.mutate('windows', lambda d: d['outcomes'].update(ctest=state))
                result, _ = self.run_gate()
                self.assertEqual(self.status(result, 'A13.01'), 'BLOCKED')
                self.assertEqual(self.status(result, 'A12.01'), 'BLOCKED')

    def test_empty_or_duplicate_inventory_cannot_certify_matrix(self):
        self.raw_change('linux', 'inventory', lambda _: '{"tests":[]}')
        self.assertEqual(self.status(self.run_gate()[0], 'A13.02'), 'BLOCKED')
        self.raw_change('linux', 'inventory', lambda _: '{"tests":[{"name":"One"},{"name":"One"}]}')
        self.assertTrue(self.run_gate()[0]['producer']['errors'])

    def test_individual_installer_msix_scan_steps_are_required(self):
        for step, cid in [('installer', 'A13.04'), ('msix', 'A13.05'), ('scan', 'A13.06')]:
            with self.subTest(step=step):
                self.mutate('windows', lambda d: d['outcomes'].update({step: 'failure'}))
                self.assertEqual(self.status(self.run_gate()[0], cid), 'BLOCKED')
                self.mutate('windows', lambda d: d['outcomes'].update({step: 'success'}))
        self.raw_change('windows', 'installer', lambda s: s.replace(HEAD, 'b' * 40))
        self.assertEqual(self.status(self.run_gate()[0], 'A13.04'), 'BLOCKED')

    def test_absent_package_and_assembly_upload_fail_closed(self):
        self.mutate('macos', lambda d: d['files'].pop('macos_package'))
        self.assertEqual(self.status(self.run_gate()[0], 'A13.03'), 'BLOCKED')
        self.mutate('assembly', lambda d: d['outcomes'].update(upload='skipped'))
        self.assertEqual(self.status(self.run_gate()[0], 'A13.07'), 'BLOCKED')

    def test_candidate_source_must_match_gate_source(self):
        self.raw_change('assembly', 'candidate', lambda s: s.replace(HEAD, 'b' * 40))
        self.assertEqual(self.status(self.run_gate()[0], 'A13.07'), 'BLOCKED')

    def test_duplicate_bundle_and_stale_output_are_rejected(self):
        (self.inputs / 'duplicate').mkdir()
        result, output = self.run_gate()
        self.assertTrue(result['producer']['errors'])
        with self.assertRaises(gate.ValidationError):
            consolidator.consolidate(self.inputs, output, self.revision, 'beta_1_3')

    def test_publisher_rejects_wrong_source_run_attempt_or_profile(self):
        _, output = self.run_gate()
        archive = self.archive(output)
        for source, profile, run, attempt in [('b' * 40, 'beta_1_3', RUN, ATTEMPT),
                (HEAD, 'beta_1_3', '124', ATTEMPT), (HEAD, 'beta_1_3', RUN, '1'),
                (HEAD, 'pull_request', RUN, ATTEMPT)]:
            with self.subTest(source=source, profile=profile, run=run, attempt=attempt), self.assertRaisesRegex(RuntimeError, 'identity'):
                publisher.verify_release_gate_archive(archive, source, profile, run, attempt)

    def test_publisher_recomputes_and_rejects_forged_pass_summary(self):
        result, output = self.run_gate()
        result.update(verdict='PASS', ready=True, blockers=[])
        result['counts']['checks']['blocked'] = 0
        result['hard_gates']['blocked'] = 0
        (output / 'release-gate.json').write_text(json.dumps(result), encoding='utf-8')
        with self.assertRaisesRegex(RuntimeError, 'differs'):
            publisher.verify_release_gate_archive(self.archive(output), HEAD, 'beta_1_3', RUN, ATTEMPT)

    def test_malformed_scan_metadata_and_missing_assertions_are_blocked(self):
        self.raw_change('windows', 'scan_status', lambda _: '[]')
        self.assertEqual(self.status(self.run_gate()[0], 'A13.06'), 'BLOCKED')
        report = evidence.make_report(self.policy, 'A12.01', HEAD, passed=True)
        for key, value in [('assertion', None), ('execution', {'executed': True, 'exit_code': None}),
                           ('execution', {'executed': True, 'exit_code': False})]:
            with self.subTest(key=key, value=value):
                malformed = copy.deepcopy(report)
                malformed[key] = value
                result = gate.evaluate_checks({'A12.01': malformed}, self.policy, {'waivers': []}, HEAD)
                self.assertEqual(result['A12.01']['computed_status'], 'BLOCKED')

    def test_workflow_has_one_final_gate_and_consistent_source_identity(self):
        workflow = (evidence.ROOT / '.github/workflows/build.yml').read_text(encoding='utf-8')
        self.assertEqual(workflow.count('name: Chimera-A-stage-release-gate'), 1)
        final = workflow.split('  consolidate-release-gate:', 1)[1]
        self.assertIn('needs: [validation-contract, build, assemble-candidate]', final)
        self.assertIn('merge-multiple: false', final)
        self.assertIn('--profile beta_1_3', final)
        self.assertIn('--commit "$CHIMERA_SOURCE_SHA"', final)
        self.assertIn('--run-attempt "$GITHUB_RUN_ATTEMPT"', final)
        self.assertIn('ref: ${{ env.CHIMERA_SOURCE_SHA }}', final)
        self.assertNotIn('continue-on-error', final)
        for step in ('ctest_native', 'ctest_linux', 'installer', 'msix', 'scan', 'assemble', 'source', 'upload'):
            self.assertIn('steps.' + step + '.outcome', workflow)
        self.assertIn("runner.os == 'Linux'", workflow)
        self.assertIn('CTEST_OUTCOME:', workflow)
        self.assertIn('Tools/run_linux_ui_tests.py', workflow)
        self.assertIn('consolidate-release-gate', (evidence.ROOT / 'Tools/publish_beta13.py').read_text(encoding='utf-8'))

    def test_positive_producer_to_publisher_contract_fixture(self):
        # Test-only policy narrows the required checks to actual automatic owners.
        # The production beta_1_3 policy and its manual gates are never changed.
        policy = copy.deepcopy(self.policy)
        for stage in policy['stages'].values():
            stage['required_checks'] = [c for c in stage['required_checks'] if c in self.mapping]
        policy['profiles']['beta_1_3']['required_stages'] = [s for s, v in policy['stages'].items() if v['required_checks']]
        with patch.object(evidence, 'configuration', return_value=(policy, self.mapping)):
            result, output = self.run_gate()
        self.assertEqual(result['verdict'], 'PASS')
        load = publisher.read_json
        def policy_load(path):
            return policy if path.name == 'release-policy.json' else load(path)
        with patch.object(publisher, 'read_json', side_effect=policy_load):
            verified = publisher.verify_release_gate_archive(self.archive(output), HEAD, 'beta_1_3', RUN, ATTEMPT)
        self.assertEqual(verified['revision'], self.revision)


if __name__ == '__main__':
    unittest.main()

