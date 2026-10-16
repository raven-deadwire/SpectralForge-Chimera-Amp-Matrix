#!/usr/bin/env python3
"""Synthetic JSON/CLI regression evidence; never a release approval receipt."""
import argparse
import contextlib
import copy
import hashlib
import io
import json
import os
from pathlib import Path
import platform
import subprocess
import sys
import tempfile
import unittest
from unittest import mock

import evaluate_signature_benchmark as benchmark
from test_signature_calculator_contracts import CalculatorContracts, ReceiptResult, ROOT

CLI_RUNS = []


def digest(data):
    return hashlib.sha256(data).hexdigest()


class JsonCliContracts(unittest.TestCase):
    setUp = CalculatorContracts.setUp

    def test_strict_loader_rejects_duplicate_keys_at_every_depth(self):
        documents = ('{"x":1,"x":2}', '{"x":1,"x":1}',
                     '{"outer":{"x":1,"x":2}}', '[{"x":1,"x":2}]',
                     '{"x":1,"\\u0078":2}')
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / 'input.json'
            for document in documents:
                with self.subTest(document=document):
                    path.write_text(document, encoding='utf-8')
                    with self.assertRaisesRegex(ValueError, 'Duplicate JSON key'):
                        benchmark.load(path)

    def test_strict_loader_rejects_constants_including_unused_fields(self):
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / 'input.json'
            for token in ('NaN', 'Infinity', '-Infinity'):
                for document in (token, '{"unused":' + token + '}', '[' + token + ']'):
                    with self.subTest(document=document):
                        path.write_text(document, encoding='utf-8')
                        with self.assertRaisesRegex(ValueError, 'Non-standard JSON constant'):
                            benchmark.load(path)

    def test_loader_preserves_bom_unicode_and_valid_json_values(self):
        data = {'text': '한글 NaN Infinity', 'numbers': [0, 1, -2, 1.25, 1e-5],
                'nested': [{'x': 1}, {'x': 2}], 'flags': [True, False, None]}
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / 'input.json'
            path.write_text(json.dumps(data, ensure_ascii=False), encoding='utf-8-sig')
            self.assertEqual(benchmark.load(path), data)

    def cli(self, folder, policy=None, renders=None):
        policy_path, renders_path = folder / 'policy.json', folder / 'renders.json'
        for path, value, default in ((policy_path, policy, self.policy),
                                     (renders_path, renders, self.renders)):
            if value is None:
                value = json.dumps(default).encode('utf-8')
            path.write_bytes(value)
        return self.invoke(policy_path, renders_path, folder / 'result.json')

    def invoke(self, policy_path, renders_path, output):
        command = [sys.executable, str(ROOT / 'Tools/evaluate_signature_benchmark.py'),
                   '--policy', str(policy_path), '--renders', str(renders_path),
                   '--output', str(output)]
        before = digest(output.read_bytes()) if output.is_file() else None
        inputs = {name: digest(path.read_bytes()) if path.is_file() else None
                  for name, path in (('policy', policy_path), ('renders', renders_path))}
        process = subprocess.run(command, capture_output=True, text=True, timeout=30)
        after = digest(output.read_bytes()) if output.is_file() else None
        CLI_RUNS.append({'test': self.id(), 'command': command, 'input_sha256': inputs,
                         'previous_output_sha256': before, 'output_sha256': after,
                         'exit_code': process.returncode, 'stdout': process.stdout,
                         'stderr': process.stderr,
                         'stdout_sha256': digest(process.stdout.encode()),
                         'stderr_sha256': digest(process.stderr.encode())})
        return process, output

    def assert_error(self, process, output, message=None):
        self.assertEqual(process.returncode, 2, process.stdout + process.stderr)
        self.assertEqual(process.stdout, '')
        self.assertIn('SIGNATURE BENCHMARK INFRASTRUCTURE ERROR', process.stderr)
        if message:
            self.assertIn(message, process.stderr)
        self.assertFalse(output.exists(), 'Neither stale nor partial score may survive')
        self.assertEqual(list(output.parent.glob('.result.json.*.tmp')), [])

    def test_cli_rejects_bad_json_in_both_inputs_and_removes_previous_pass(self):
        with tempfile.TemporaryDirectory() as temporary:
            folder = Path(temporary)
            for target, data in (('policy', self.policy), ('renders', self.renders)):
                clean = json.dumps(data)
                # Otherwise-valid files show rejection even in unconsumed fields.
                bad = [('{"unused":1,"unused":2,' + clean[1:]).encode()]
                bad += [('{"unused":' + token + ',' + clean[1:]).encode()
                        for token in ('NaN', 'Infinity', '-Infinity')]
                bad += [b'', b'{', b'{"x":}', clean.encode() + b' trailing',
                        b'{"x":1,}', b'{"x":"\x01"}', b'\xff']
                for index, document in enumerate(bad):
                    with self.subTest(target=target, case=index):
                        success, output = self.cli(folder)
                        self.assertEqual(success.returncode, 0, success.stderr)
                        self.assertEqual(json.loads(output.read_text())['verdict'], 'PASS')
                        process, output = self.cli(folder, **{target: document})
                        self.assert_error(process, output)

    def test_cli_evaluation_and_missing_input_errors_remove_previous_pass(self):
        invalids = [dict(self.renders, schema='wrong'), dict(self.renders, renders={}),
                    [], None]
        with tempfile.TemporaryDirectory() as temporary:
            folder = Path(temporary)
            for value in invalids:
                with self.subTest(value=value):
                    success, output = self.cli(folder)
                    self.assertEqual(success.returncode, 0)
                    process, output = self.cli(folder, renders=json.dumps(value).encode())
                    self.assert_error(process, output)
            for name in ('policy', 'renders'):
                success, output = self.cli(folder)
                self.assertEqual(success.returncode, 0)
                (folder / (name + '.json')).unlink()
                process, output = self.invoke(folder / 'policy.json', folder / 'renders.json', output)
                self.assert_error(process, output)

    def test_cli_exit_codes_replace_previous_output_with_current_result(self):
        revised = copy.deepcopy(self.renders)
        revised['renders']['wild']['crest_db'] = 9.0
        invalid = dict(self.renders, dsp_valid=False, renders={})
        with tempfile.TemporaryDirectory() as temporary:
            folder = Path(temporary)
            for data, verdict, code in ((self.renders, 'PASS', 0), (revised, 'REVISE', 1),
                                        (invalid, 'INVALID', 1), (self.renders, 'PASS', 0)):
                with self.subTest(verdict=verdict):
                    process, output = self.cli(folder, renders=json.dumps(data).encode())
                    self.assertEqual(process.returncode, code, process.stderr)
                    self.assertEqual(process.stderr, '')
                    result = benchmark.load(output)
                    self.assertEqual(result, benchmark.evaluate(self.policy, data))
                    self.assertEqual(result, json.loads(json.dumps(result, allow_nan=False)))
                    self.assertEqual(result['verdict'], verdict)
                    self.assertEqual(json.loads(process.stdout)['verdict'], verdict)
                    self.assertEqual(list(folder.glob('.result.json.*.tmp')), [])

    def test_cli_input_output_collision_preserves_input(self):
        with tempfile.TemporaryDirectory() as temporary:
            folder = Path(temporary)
            self.cli(folder)
            for name in ('policy', 'renders'):
                source = folder / (name + '.json')
                before = source.read_bytes()
                process, _ = self.invoke(folder / 'policy.json', folder / 'renders.json', source)
                self.assertEqual(process.returncode, 2)
                self.assertIn('Output must differ from input', process.stderr)
                self.assertEqual(source.read_bytes(), before)

    def test_output_io_and_serialization_errors_leave_no_score(self):
        # Deterministic fault injection also runs on Windows/root. Real JSON
        # subprocess failures are covered separately above.
        with tempfile.TemporaryDirectory() as temporary:
            folder = Path(temporary)
            for fault in ('serialize', 'write', 'replace', 'stdout'):
                with self.subTest(fault=fault):
                    success, output = self.cli(folder)
                    self.assertEqual(success.returncode, 0)
                    argv = ['evaluator', '--policy', str(folder / 'policy.json'),
                            '--renders', str(folder / 'renders.json'), '--output', str(output)]
                    stderr = io.StringIO()
                    with contextlib.ExitStack() as stack:
                        stack.enter_context(mock.patch.object(sys, 'argv', argv))
                        stack.enter_context(contextlib.redirect_stderr(stderr))
                        stack.enter_context(contextlib.redirect_stdout(io.StringIO()))
                        if fault == 'serialize':
                            stack.enter_context(mock.patch.object(benchmark, 'evaluate',
                                return_value={'verdict': 'PASS', 'overall_score': float('nan')}))
                        elif fault == 'write':
                            original = tempfile.NamedTemporaryFile
                            def partial_write(*args, **kwargs):
                                handle = original(*args, **kwargs)
                                write = handle.write
                                def fail_after_partial_write(payload):
                                    write(payload[:10])
                                    handle.flush()
                                    raise OSError('injected write failure')
                                handle.write = fail_after_partial_write
                                return handle
                            stack.enter_context(mock.patch.object(tempfile, 'NamedTemporaryFile', partial_write))
                        elif fault == 'replace':
                            stack.enter_context(mock.patch.object(Path, 'replace', side_effect=OSError('injected replace failure')))
                        else:
                            stack.enter_context(mock.patch.object(sys.stdout, 'write', side_effect=OSError('injected stdout failure')))
                        self.assertEqual(benchmark.main(), 2)
                    self.assertIn('SIGNATURE BENCHMARK INFRASTRUCTURE ERROR', stderr.getvalue())
                    self.assertFalse(output.exists())
                    self.assertEqual(list(folder.glob('.result.json.*.tmp')), [])

    def test_unremovable_output_is_reported_and_never_evaluated(self):
        with tempfile.TemporaryDirectory() as temporary:
            folder = Path(temporary)
            _, output = self.cli(folder)
            original = output.read_bytes()
            stderr = io.StringIO()
            argv = ['evaluator', '--policy', str(folder / 'policy.json'),
                    '--renders', str(folder / 'renders.json'), '--output', str(output)]
            with mock.patch.object(sys, 'argv', argv), contextlib.redirect_stderr(stderr), \
                    mock.patch.object(Path, 'unlink', side_effect=PermissionError('denied')), \
                    mock.patch.object(benchmark, 'evaluate') as evaluate:
                self.assertEqual(benchmark.main(), 2)
                evaluate.assert_not_called()
            self.assertIn('Could not invalidate output', stderr.getvalue())
            self.assertEqual(output.read_bytes(), original)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--report', type=Path)
    args = parser.parse_args()
    result = unittest.TextTestRunner(verbosity=2, resultclass=ReceiptResult).run(
        unittest.defaultTestLoader.loadTestsFromTestCase(JsonCliContracts))
    if args.report:
        sources = ('Tools/evaluate_signature_benchmark.py', 'Tools/test_signature_json_cli.py',
                   'Tools/test_signature_calculator_contracts.py', 'Validation/signature-benchmark-policy.json')
        report = {'schema': 'spectralforge.chimera.signature-json-cli-tests', 'schema_version': 1,
                  'synthetic_only': True, 'review_only': True, 'publisher_input': False,
                  'policy_claims': [], 'release_approved': False,
                  'source_commit': subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip(),
                  'worktree_dirty': bool(subprocess.check_output(['git', 'status', '--porcelain'], cwd=ROOT)),
                  'source_sha256': {path: digest((ROOT / path).read_bytes()) for path in sources},
                  'environment': {'python': sys.version, 'platform': platform.platform()},
                  'run_id': os.environ.get('GITHUB_RUN_ID'), 'run_attempt': os.environ.get('GITHUB_RUN_ATTEMPT'),
                  'tests_run': result.testsRun, 'failures': len(result.failures),
                  'errors': len(result.errors), 'skipped': len(result.skipped),
                  'cases': result.cases, 'cli_runs': CLI_RUNS}
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text(json.dumps(report, indent=2, allow_nan=False) + '\n', encoding='utf-8')
    return 0 if result.wasSuccessful() and not result.skipped else 1


if __name__ == '__main__':
    raise SystemExit(main())
