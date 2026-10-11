#!/usr/bin/env python3
"""Real action transport checks only; never emits product acceptance evidence."""
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import urllib.error
import urllib.request


def check_digest(data, expected):
    assert hashlib.sha256(data).hexdigest() == expected.removeprefix('sha256:'), 'Artifact digest mismatch'


def check_identity(actual, expected):
    assert actual == expected, 'Exact source/run/attempt identity mismatch'


def identity():
    return dict(source=os.environ['EXACT_SOURCE'], run=os.environ['GITHUB_RUN_ID'],
                attempt=os.environ['GITHUB_RUN_ATTEMPT'])


def prepare():
    assert subprocess.check_output(['git', 'rev-parse', 'HEAD'], text=True).strip() == identity()['source']
    # Names only; never print credential values.
    for directory, persisted in (('.', False), ('credential-check', True)):
        result = subprocess.run(['git', '-C', directory, 'config', '--local', '--name-only',
            '--get-regexp', r'http\..*\.extraheader'], capture_output=True, text=True)
        assert result.returncode in (0, 1)
        assert bool(result.stdout.strip()) == persisted, 'Checkout credential persistence changed'
    assert sys.version_info[:2] == (3, 12)
    assert os.environ['PYTHON_CACHE_HIT'] == '', 'Python cache must remain disabled'
    for letter in ('a', 'b'):
        root = Path('action-fixture') / letter
        root.mkdir(parents=True)
        (root / f'{letter}.json').write_text(json.dumps(identity(), sort_keys=True), encoding='utf-8')


class NoRedirect(urllib.request.HTTPRedirectHandler):
    def redirect_request(self, req, fp, code, msg, headers, newurl):
        return None


def api(path, archive=False):
    req = urllib.request.Request(os.environ['GITHUB_API_URL'] + '/repos/' + os.environ['GITHUB_REPOSITORY'] + path,
        headers={'Authorization': 'Bearer ' + os.environ['GH_TOKEN'], 'Accept': 'application/vnd.github+json',
                 'X-GitHub-Api-Version': '2022-11-28'})
    try:
        with urllib.request.build_opener(NoRedirect).open(req, timeout=60) as response:
            data = response.read()
    except urllib.error.HTTPError as error:
        if not archive or error.code != 302:
            raise
        # Signed archive URL gets no repository Authorization header.
        with urllib.request.urlopen(error.headers['Location'], timeout=60) as response:
            data = response.read()
    return data if archive else json.loads(data)


def verify():
    prefix = os.environ['ARTIFACT_PREFIX']
    expected_paths = {'by-name/a.json', f'single/{prefix}-a/a.json', 'merged/a.json', 'merged/b.json',
                      f'separate/{prefix}-a/a.json', f'separate/{prefix}-b/b.json'}
    actual_paths = {p.relative_to('action-download').as_posix() for p in Path('action-download').rglob('*') if p.is_file()}
    assert actual_paths == expected_paths, f'Artifact layout changed: {actual_paths}'
    for relative in expected_paths:
        check_identity(json.loads((Path('action-download') / relative).read_text()), identity())
    for letter in ('A', 'B'):
        artifact_id = os.environ[f'ARTIFACT_{letter}_ID']
        digest = os.environ[f'ARTIFACT_{letter}_DIGEST']
        metadata = api('/actions/artifacts/' + artifact_id)
        assert metadata['name'] == prefix + '-' + letter.lower()
        assert metadata['digest'] == 'sha256:' + digest
        assert str(metadata['workflow_run']['id']) == identity()['run']
        # PR run head_sha can be the merge ref. The payload/checkout above binds
        # EXACT_SOURCE to the PR head; never substitute the workflow-run merge SHA.
        check_digest(api('/actions/artifacts/' + artifact_id + '/zip', archive=True), digest)
    print('PASS: names, paths, merged/separate/singleton layout, payload identity and ZIP digests')


if __name__ == '__main__':
    {'prepare': prepare, 'verify': verify}[sys.argv[1]]()
