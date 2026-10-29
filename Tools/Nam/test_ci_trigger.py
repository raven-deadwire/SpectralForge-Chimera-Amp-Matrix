"""Event-matrix and workflow wiring regressions; no trainer or network needed."""
import copy
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import Mock, patch

import yaml

from ci_trigger import FULL_LABEL, repository_permission, resolve

ROOT = Path(__file__).resolve().parents[2]
REPO = 'raven-deadwire/SpectralForge-Chimera-Amp-Matrix'
HEAD, MERGE = 'a' * 40, 'b' * 40


def pr_event(action='labeled', label=FULL_LABEL):
    return {'action': action, 'label': {'name': label}, 'sender': {'login': 'maintainer'},
            'pull_request': {'number': 22, 'author_association': 'OWNER',
                             'head': {'sha': HEAD, 'repo': {'full_name': REPO}},
                             'base': {'repo': {'full_name': REPO}},
                             'labels': [{'name': FULL_LABEL}]}}


class TriggerTests(unittest.TestCase):
    def select(self, event, event_name='pull_request', permission='write'):
        lookup = Mock(return_value=permission)
        result = resolve(event_name, event, REPO, MERGE, 'maintainer', lookup)
        return result, lookup

    def test_ordinary_updates_remain_smoke_even_with_full_label_and_inputs(self):
        for action in ('opened', 'synchronize', 'reopened'):
            with self.subTest(action=action):
                event = pr_event(action)
                event['inputs'] = {'profile': 'full', 'steps': 50000}
                result, lookup = self.select(event)
                self.assertEqual((result['profile'], result['steps'], result['exact_source']),
                                 ('smoke', 5000, HEAD))
                self.assertEqual((result['job_minutes'], result['pipeline_minutes']), (35, 18))
                lookup.assert_not_called()

    def test_only_exact_new_label_event_can_request_full(self):
        for action, label in [('labeled', 'bug'), ('labeled', FULL_LABEL + '-extra'),
                              ('unlabeled', FULL_LABEL), ('edited', FULL_LABEL),
                              ('closed', FULL_LABEL), ('ready_for_review', FULL_LABEL)]:
            with self.subTest(action=action, label=label):
                result, lookup = self.select(pr_event(action, label))
                self.assertFalse(result['enabled'])
                lookup.assert_not_called()

    def test_maintainer_label_uses_exact_head_and_fixed_full_budget(self):
        for permission in ('write', 'maintain', 'admin'):
            with self.subTest(permission=permission):
                event = pr_event()
                event['inputs'] = {'profile': 'smoke', 'steps': 50000}
                result, lookup = self.select(event, permission=permission)
                self.assertTrue(result['enabled'])
                self.assertEqual((result['profile'], result['steps'], result['exact_source']),
                                 ('full', 5000, HEAD))
                self.assertEqual((result['job_minutes'], result['pipeline_minutes']), (350, 310))
                self.assertEqual(result['reason'], 'maintainer-label')
                lookup.assert_called_once_with(REPO, 'maintainer')

    def test_author_association_does_not_authorize_sender(self):
        for permission in ('read', 'triage', 'none', '', None):
            with self.subTest(permission=permission), self.assertRaisesRegex(ValueError, 'write permission'):
                self.select(pr_event(), permission=permission)

    def test_forks_and_mismatched_sender_fail_before_permission_lookup(self):
        event = pr_event()
        fork = copy.deepcopy(event)
        fork['pull_request']['head']['repo']['full_name'] = 'outsider/fork'
        wrong_base = copy.deepcopy(event)
        wrong_base['pull_request']['base']['repo']['full_name'] = 'other/repo'
        event['sender']['login'] = 'other-user'
        for payload in (event, fork, wrong_base):
            lookup = Mock(return_value='admin')
            with self.assertRaises(ValueError):
                resolve('pull_request', payload, REPO, MERGE, 'maintainer', lookup)
            lookup.assert_not_called()
        # Forks retain the ordinary, read-only smoke path.
        fork['action'] = 'synchronize'
        self.assertEqual(self.select(fork)[0]['profile'], 'smoke')

    def test_permission_lookup_failure_never_falls_back_to_training(self):
        lookup = Mock(side_effect=OSError('API unavailable'))
        with self.assertRaisesRegex(OSError, 'API unavailable'):
            resolve('pull_request', pr_event(), REPO, MERGE, 'maintainer', lookup)
        with patch.dict('os.environ', {'GITHUB_API_URL': 'https://api.github.com', 'GH_TOKEN': 'fixture'}), \
                patch('ci_trigger.urlopen') as request:
            request.return_value.__enter__.return_value.read.return_value = b'{"role_name":"admin"}'
            with self.assertRaises(KeyError):
                repository_permission(REPO, 'maintainer')
            self.assertEqual(request.call_args.kwargs['timeout'], 20)

    def test_dispatch_defaults_and_explicit_profiles_remain_available(self):
        result, lookup = self.select({}, 'workflow_dispatch')
        self.assertEqual((result['profile'], result['steps'], result['exact_source']), ('smoke', 5000, MERGE))
        lookup.assert_not_called()
        for profile in ('smoke', 'full'):
            for steps in (1, 5000, 50000):
                result, lookup = self.select({'inputs': {'profile': profile, 'steps': str(steps)}}, 'workflow_dispatch')
                self.assertEqual((result['profile'], result['steps']), (profile, steps))
                lookup.assert_not_called()
        for inputs in ({'profile': 'unexpected'}, *({'steps': s} for s in (0, -1, 50001, 2.5, True, '5000\nprofile=full'))):
            with self.subTest(inputs=inputs), self.assertRaises(ValueError):
                self.select({'inputs': inputs}, 'workflow_dispatch')

    def test_unsupported_events_and_nonimmutable_source_fail_closed(self):
        for event_name in ('push', 'pull_request_target', 'issue_comment'):
            self.assertFalse(self.select(pr_event(), event_name)[0]['enabled'])
        for source in ('main', '', HEAD + '\n'):
            event = pr_event('synchronize')
            event['pull_request']['head']['sha'] = source
            with self.assertRaisesRegex(ValueError, 'immutable'):
                self.select(event)

    def test_cli_writes_job_output_for_actual_event_payload(self):
        import os
        with tempfile.TemporaryDirectory() as td:
            event, output = Path(td) / 'event.json', Path(td) / 'output'
            event.write_text(json.dumps(pr_event('synchronize')))
            subprocess.run([sys.executable, str(ROOT / 'Tools/Nam/ci_trigger.py')], check=True,
                           capture_output=True, env={**os.environ, 'GITHUB_EVENT_NAME': 'pull_request',
                           'GITHUB_EVENT_PATH': str(event), 'GITHUB_REPOSITORY': REPO, 'GITHUB_SHA': MERGE,
                           'GITHUB_ACTOR': 'maintainer', 'GITHUB_OUTPUT': str(output)})
            key, value = output.read_text().strip().split('=', 1)
            self.assertEqual(key, 'decision')
            self.assertEqual(json.loads(value)['profile'], 'smoke')


class WorkflowTests(unittest.TestCase):
    def test_workflow_routes_every_resource_setting_through_verified_decision(self):
        workflow = yaml.safe_load((ROOT / '.github/workflows/nam-validation.yml').read_text())
        triggers = workflow.get('on', workflow.get(True))  # PyYAML's YAML 1.1 key handling.
        self.assertEqual(set(triggers), {'pull_request', 'workflow_dispatch'})
        self.assertEqual(triggers['pull_request']['types'], ['opened', 'synchronize', 'reopened', 'labeled'])
        self.assertEqual(triggers['workflow_dispatch']['inputs']['profile']['default'], 'smoke')
        self.assertEqual(triggers['workflow_dispatch']['inputs']['steps']['default'], 5000)
        self.assertEqual(workflow['permissions'], {'contents': 'read'})
        self.assertNotIn('concurrency', workflow)  # Unrelated label events cannot cancel active jobs.
        trigger, nam = workflow['jobs']['trigger'], workflow['jobs']['nam']
        self.assertEqual(trigger['timeout-minutes'], 5)
        self.assertEqual(trigger['steps'][0]['with']['ref'], '${{ github.event.pull_request.head.sha || github.sha }}')
        self.assertEqual(trigger['outputs']['decision'], '${{ steps.select.outputs.decision }}')
        selector = next(s for s in trigger['steps'] if s.get('id') == 'select')
        self.assertEqual(selector['run'], 'python Tools/Nam/ci_trigger.py')
        self.assertEqual(selector['env'], {'GH_TOKEN': '${{ github.token }}'})
        self.assertEqual(trigger['steps'][-2]['run'], 'python -m unittest discover -s Tools/Nam -p test_ci_trigger.py -v')
        self.assertEqual(nam['needs'], 'trigger')
        self.assertEqual(nam['if'], 'fromJSON(needs.trigger.outputs.decision).enabled')
        expression = lambda key: '${{ fromJSON(needs.trigger.outputs.decision).' + key + ' }}'
        self.assertEqual(nam['timeout-minutes'], expression('job_minutes'))
        for env, key in [('PROFILE', 'profile'), ('FULL_STEPS', 'steps'), ('EXACT_SOURCE', 'exact_source')]:
            self.assertEqual(nam['env'][env], expression(key))
        self.assertEqual(nam['concurrency'], {
            'group': 'nam-${{ github.event.pull_request.number || github.ref }}-' + expression('profile'),
            'cancel-in-progress': "${{ fromJSON(needs.trigger.outputs.decision).profile == 'smoke' && github.event_name == 'pull_request' }}"})
        self.assertEqual(nam['env']['OMP_NUM_THREADS'], '2')
        self.assertEqual(nam['env']['MKL_NUM_THREADS'], '2')
        self.assertEqual(nam['steps'][0]['with']['ref'], '${{ env.EXACT_SOURCE }}')
        for job in (trigger, nam):
            for step in job['steps']:
                self.assertNotIn('continue-on-error', step)
                if step.get('uses', '').startswith('actions/checkout@'):
                    self.assertIs(step['with']['persist-credentials'], False)
        pipeline = next(s for s in nam['steps'] if s.get('run', '').startswith('python Tools/Nam/ci_pipeline.py --profile'))
        self.assertEqual(pipeline['timeout-minutes'], expression('pipeline_minutes'))
        self.assertIn('--expected-source "$EXACT_SOURCE"', pipeline['run'])
        self.assertIn('--steps "$FULL_STEPS"', pipeline['run'])
        seal, upload = nam['steps'][-2:]
        self.assertEqual(seal['if'], 'always()')
        self.assertEqual(seal['run'], 'python Tools/Nam/ci_pipeline.py --seal-only --profile "$PROFILE" --expected-source "$EXACT_SOURCE" --out "$EVIDENCE"')
        self.assertEqual(upload['if'], 'always()')
        self.assertEqual(upload['with']['name'], 'NAM-${{ env.PROFILE }}-${{ env.EXACT_SOURCE }}-${{ github.run_id }}-${{ github.run_attempt }}')
        self.assertEqual(upload['with']['if-no-files-found'], 'error')
        self.assertEqual(nam['env']['TRIGGER_DECISION'], '${{ needs.trigger.outputs.decision }}')
        self.assertTrue(any("'trigger.json'" in s.get('run', '') for s in nam['steps']))


if __name__ == '__main__':
    unittest.main()
