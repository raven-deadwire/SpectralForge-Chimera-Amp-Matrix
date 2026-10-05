"""Fail-closed stage evidence and coordinator interruption regressions."""
import argparse
import copy
import fcntl
import json
from pathlib import Path
import signal
import sys
import tempfile
import unittest
from unittest.mock import patch

import continue_a2 as coordinator


class CoordinatorTests(unittest.TestCase):
    def fixture(self, root, end=2000):
        run, data = root/'run', root/'data'
        run.mkdir(exist_ok=True)
        data.mkdir(exist_ok=True)
        source = {'source_commit': 'frozen-fixture'}
        coordinator.atomic_json(data/'manifest.json', {'source': source})
        config = {'identity': 'identity-fixture', 'source': source,
                  'dataset_sha256': coordinator.sha256(data/'manifest.json')}
        coordinator.atomic_json(run/'training-config.json', config)
        rows = {}
        for name in coordinator.NAMES:
            path = run/f'Nastrond-{name}.nam'
            path.write_text(name)
            rows[name] = {'esr': .05, 'selected_step': end,
                          'tone3000_fidelity': {'fidelity_pass': False},
                          'sha256': coordinator.sha256(path), 'official_roundtrip_max_abs': 0.}
        validation = {'channels': rows, 'quality_profile': coordinator.PROFILE,
                      'checkpoint_step': end, 'identity': config['identity']}
        coordinator.atomic_json(run/'validation.json', validation)
        coordinator.atomic_json(run/f'validation-step-{end}.json', {
            'esr_by_channel': {name: .05 for name in coordinator.NAMES}, 'best_steps': [end]*5})
        checkpoint = {'step': end, 'identity': config['identity'], 'best_steps': [end]*5}
        (run/'checkpoint.pt').write_bytes(b'checkpoint fixture; patched deserializer')
        log = run/'stage.log'
        log.write_text(json.dumps({'stage': 'exported', 'channels': rows})+'\n')
        return run, data, checkpoint, log

    def test_completed_export_has_matching_evidence(self):
        with tempfile.TemporaryDirectory() as td:
            run, data, checkpoint, log = self.fixture(Path(td))
            with patch.object(coordinator, 'load_checkpoint', return_value=checkpoint):
                validation, saved = coordinator.verify_stage(run, data, 2000, {}, log)
            self.assertEqual(saved['step'], 2000)
            self.assertEqual(set(validation['channels']), set(coordinator.NAMES))

    def test_old_validation_cannot_complete_a_new_stage(self):
        with tempfile.TemporaryDirectory() as td:
            run, data, checkpoint, log = self.fixture(Path(td))
            before = {'validation.json': coordinator.file_version(run/'validation.json')}
            with patch.object(coordinator, 'load_checkpoint', return_value=checkpoint):
                with self.assertRaisesRegex(RuntimeError, 'stale.*validation.json'):
                    coordinator.verify_stage(run, data, 2000, before, log)

    def test_partial_or_mismatched_exports_fail_closed(self):
        for scenario in ['checkpoint_step', 'identity', 'best_steps', 'hash', 'marker', 'provenance', 'nan']:
            with self.subTest(scenario=scenario), tempfile.TemporaryDirectory() as td:
                run, data, checkpoint, log = self.fixture(Path(td))
                if scenario == 'checkpoint_step':
                    checkpoint['step'] = 1500
                elif scenario == 'identity':
                    checkpoint['identity'] = 'wrong'
                elif scenario == 'best_steps':
                    checkpoint['best_steps'][0] = 1500
                elif scenario == 'hash':
                    (run/'Nastrond-Surtr.nam').write_text('partial new export')
                elif scenario == 'marker':
                    log.write_text('{"step": 2000}\n')
                else:
                    validation = json.loads((run/'validation.json').read_text())
                    if scenario == 'provenance':
                        validation['checkpoint_step'] = 1500
                    else:
                        validation['channels']['Surtr']['esr'] = float('nan')
                    coordinator.atomic_json(run/'validation.json', validation)
                with patch.object(coordinator, 'load_checkpoint', return_value=checkpoint):
                    with self.assertRaises(RuntimeError):
                        coordinator.verify_stage(run, data, 2000, {}, log)

    def test_child_retains_exclusion_and_signal_reaps_it(self):
        with tempfile.TemporaryDirectory() as td:
            path = Path(td)/'coordinator.lock'
            lock = path.open('a+')
            fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
            interrupted = {'signal': None}
            def on_start(_pid):
                # The parent descriptor disappearing must not permit a second
                # coordinator while its orphaned child still runs.
                lock.close()
                with path.open('a+') as contender:
                    with self.assertRaises(BlockingIOError):
                        fcntl.flock(contender, fcntl.LOCK_EX | fcntl.LOCK_NB)
                interrupted['signal'] = signal.SIGTERM
            with (Path(td)/'child.log').open('w') as stream:
                result = coordinator.run_child(
                    [sys.executable, '-c', 'import time; time.sleep(60)'],
                    stream, lock, interrupted, on_start)
            self.assertEqual(result, -signal.SIGTERM)
            with path.open('a+') as contender:
                fcntl.flock(contender, fcntl.LOCK_EX | fcntl.LOCK_NB)

    def test_default_stage_budget_pauses_only_after_verified_export(self):
        with tempfile.TemporaryDirectory() as td:
            root = Path(td)
            run, data, checkpoint, log = self.fixture(root, end=1000)
            previous_log = run/'train-1000-3000.log'
            previous_log.write_text('older interrupted attempt')
            state = {'checkpoint': checkpoint}
            def saved_checkpoint(_path):
                return copy.deepcopy(state['checkpoint'])
            def trainer(cmd, stream, _lock, _interrupted, on_start):
                on_start(123)
                end = int(cmd[cmd.index('--steps')+1])
                _, _, new_checkpoint, export_log = self.fixture(root, end=end)
                state['checkpoint'] = new_checkpoint
                stream.write(export_log.read_text())
                stream.flush()
                return 0
            argv = ['continue_a2.py', '--run', str(run), '--data', str(data),
                    '--trainer', str(root/'upstream'), '--max-steps', '10000']
            with patch.object(sys, 'argv', argv), patch.object(coordinator, 'load_checkpoint', side_effect=saved_checkpoint), patch.object(coordinator, 'run_child', side_effect=trainer):
                self.assertEqual(coordinator.main(), 0)
            progress = json.loads((run/'progress.json').read_text())
            self.assertEqual(progress['status'], 'PAUSED_AFTER_STAGE')
            self.assertEqual(progress['last_completed_step'], 3000)
            self.assertEqual(len(progress['stages']), 1)
            self.assertEqual(previous_log.read_text(), 'older interrupted attempt')
            self.assertNotEqual(progress['log'], previous_log.name)

    def test_zero_exit_without_new_evidence_is_review_required(self):
        with tempfile.TemporaryDirectory() as td:
            root = Path(td)
            run, data, checkpoint, _log = self.fixture(root, end=1000)
            argv = ['continue_a2.py', '--run', str(run), '--data', str(data),
                    '--trainer', str(root/'upstream')]
            with patch.object(sys, 'argv', argv), patch.object(coordinator, 'load_checkpoint', return_value=checkpoint), patch.object(coordinator, 'run_child', return_value=0):
                with self.assertRaisesRegex(RuntimeError, 'stale'):
                    coordinator.main()
            progress = json.loads((run/'progress.json').read_text())
            self.assertEqual(progress['status'], 'REVIEW_REQUIRED')
            self.assertEqual(progress['stages'], [])

    def test_signal_records_saved_step_without_claiming_stage_completion(self):
        with tempfile.TemporaryDirectory() as td:
            root = Path(td)
            run, data, checkpoint, _log = self.fixture(root, end=1000)
            def interrupted_trainer(_cmd, _stream, _lock, interrupted, on_start):
                on_start(123)
                interrupted['signal'] = signal.SIGTERM
                return -signal.SIGTERM
            argv = ['continue_a2.py', '--run', str(run), '--data', str(data),
                    '--trainer', str(root/'upstream')]
            with patch.object(sys, 'argv', argv), patch.object(coordinator, 'load_checkpoint', return_value=checkpoint), patch.object(coordinator, 'run_child', side_effect=interrupted_trainer):
                self.assertEqual(coordinator.main(), 128+signal.SIGTERM)
            progress = json.loads((run/'progress.json').read_text())
            self.assertEqual(progress['status'], 'INTERRUPTED')
            self.assertEqual(progress['signal'], 'SIGTERM')
            self.assertEqual(progress['last_saved_step'], 1000)
            self.assertNotIn('last_completed_step', progress)
            self.assertEqual(progress['stages'], [])

    def test_official_recipe_does_not_receive_legacy_optimizer_flags(self):
        args = argparse.Namespace(data=Path('/data'), run=Path('/run'), trainer=Path('/trainer'),
                                  threads=4, recipe='official-a2', warm_start=Path('/warm'), tail_fraction=None)
        command = coordinator.trainer_command(args, 2000, Path('/nonexistent-checkpoint'))
        self.assertIn('official-a2', command)
        self.assertNotIn('--lr', command)
        self.assertNotIn('--tail-fraction', command)
        self.assertNotIn('--loss-normalization', command)
        args.tail_fraction = .05
        matched = coordinator.trainer_command(args, 2000, Path('/nonexistent-checkpoint'))
        self.assertEqual(matched[matched.index('--tail-fraction')+1], '0.05')
        args.tail_fraction = None
        args.recipe = None
        legacy = coordinator.trainer_command(args, 2000, Path('/nonexistent-checkpoint'))
        self.assertNotIn('--recipe', legacy)
        self.assertIn('--lr', legacy)
        self.assertIn('--tail-fraction', legacy)


if __name__ == '__main__':
    unittest.main()
