#!/usr/bin/env python3
"""Resolve a bounded NAM run from the event, never from persistent PR labels."""
import json
import os
from pathlib import Path
import re
from urllib.parse import quote
from urllib.request import Request, urlopen

FULL_LABEL = 'nam-full-training'
SMOKE_ACTIONS = ('opened', 'synchronize', 'reopened')


def repository_permission(repository, actor):
    request = Request(
        f'{os.environ["GITHUB_API_URL"]}/repos/{repository}/collaborators/{quote(actor, safe="")}/permission',
        headers={'Authorization': f'Bearer {os.environ["GH_TOKEN"]}',
                 'Accept': 'application/vnd.github+json', 'X-GitHub-Api-Version': '2022-11-28'})
    # Missing credentials, network errors and malformed responses fail closed.
    with urlopen(request, timeout=20) as response:
        return json.load(response)['permission']


def resolve(event_name, event, repository, sha, actor, permission=repository_permission):
    profile, steps, source = 'smoke', 5000, sha
    reason, authorized_permission = 'manual-dispatch', None
    if event_name == 'pull_request':
        pr = event['pull_request']
        source = pr['head']['sha']  # The event's immutable head, never the merge SHA/current branch.
        action = event.get('action')
        if action in SMOKE_ACTIONS:
            reason = 'ordinary-pr-update'
        elif action == 'labeled' and event.get('label', {}).get('name') == FULL_LABEL:
            if (pr['head']['repo']['full_name'] != repository or
                    pr['base']['repo']['full_name'] != repository):
                raise ValueError('Full label training requires a same-repository PR')
            if not actor or event.get('sender', {}).get('login') != actor:
                raise ValueError('Full label actor does not match the event sender')
            authorized_permission = permission(repository, actor)
            # GitHub maps maintain to write and triage to read in this field.
            if authorized_permission not in ('admin', 'write', 'maintain'):
                raise ValueError('Full label training requires repository write permission')
            profile, reason = 'full', 'maintainer-label'
        else:
            return {'enabled': False, 'reason': 'unrelated-pr-event'}
    elif event_name == 'workflow_dispatch':
        inputs = event.get('inputs', {})
        profile = inputs.get('profile', 'smoke')
        raw_steps = str(inputs.get('steps', 5000))
        if profile not in ('smoke', 'full'):
            raise ValueError('Unknown NAM profile')
        if not re.fullmatch(r'[0-9]+', raw_steps) or not 1 <= int(raw_steps) <= 50000:
            raise ValueError('Full steps must be in 1..50000')
        steps = int(raw_steps)
    else:
        return {'enabled': False, 'reason': 'unsupported-event'}
    if not re.fullmatch(r'[0-9a-f]{40}', source):
        raise ValueError('Expected an immutable 40-character source SHA')
    return {'enabled': True, 'profile': profile, 'steps': steps, 'exact_source': source,
            'job_minutes': 350 if profile == 'full' else 35,
            'pipeline_minutes': 310 if profile == 'full' else 18,
            'reason': reason, 'actor': actor, 'permission': authorized_permission}


def main():
    decision = resolve(os.environ['GITHUB_EVENT_NAME'],
                       json.loads(Path(os.environ['GITHUB_EVENT_PATH']).read_text()),
                       os.environ['GITHUB_REPOSITORY'], os.environ['GITHUB_SHA'],
                       os.environ['GITHUB_ACTOR'])
    serialized = json.dumps(decision, separators=(',', ':'))
    with Path(os.environ['GITHUB_OUTPUT']).open('a') as output:
        output.write(f'decision={serialized}\n')
    print(serialized)


if __name__ == '__main__':
    main()
