"""Manifest validation and the macro target groups derived from it.

The target groups were previously re-typed as model-id lists in two tools.
They follow from the manifest: an amp-head capture eligible for timbre
comparison, unboosted, at high or undocumented gain, grouped by its role.
"""
import json
import re
from pathlib import Path

from .protocol import SR

MACROS = ('clank', 'crush', 'impact', 'rot', 'bloom')
TARGET_GAIN_GROUPS = ('high_gain', 'unspecified_gain')
REQUIRED = ('family', 'model_id', 'name', 'model_url', 'file', 'sha256', 'sample_rate', 'role',
            'capture_scope', 'boosted', 'gain_group', 'head_timbre_comparison_eligible', 'metadata')
DEFAULT_MANIFEST = Path(__file__).resolve().parents[2] / 'docs/reference/nastrond-nam-manifest.json'


def load(path):
    manifest = json.loads(Path(path).read_text())
    refs = manifest['references']
    if not isinstance(refs, list) or not refs:
        raise ValueError('Manifest must contain references')
    for item in refs:
        missing = [key for key in REQUIRED if key not in item]
        if missing:
            raise ValueError(f"Manifest entry {item.get('model_id')} lacks {missing}")
        if item['role'].lower() not in MACROS:
            raise ValueError(f"Unknown role {item['role']} for {item['model_id']}")
        if not isinstance(item['sha256'], str) or not re.fullmatch(r'[0-9a-f]{64}', item['sha256']):
            raise ValueError(f"Invalid SHA256 for {item['model_id']}")
        for key in ('boosted', 'head_timbre_comparison_eligible'):
            if type(item[key]) is not bool:
                raise ValueError(f'{key} must be boolean')
        if float(item['sample_rate']) != SR:
            raise ValueError(f"Reference sample rate differs: {item['model_id']}")
        if item['head_timbre_comparison_eligible'] and item['capture_scope'] != 'amp_head':
            raise ValueError(f"Only amp-head captures support timbre comparison: {item['model_id']}")
        if Path(item['file']).is_absolute() or '..' in Path(item['file']).parts:
            raise ValueError(f"Reference path escapes model directory: {item['file']}")
    for key in ('model_id', 'sha256', 'file'):
        if len({item[key] for item in refs}) != len(refs):
            raise ValueError(f'Duplicate reference {key}')
    return manifest


def target_groups(manifest):
    """{macro: (family, [model_id, ...])} in manifest order; macros without targets are omitted."""
    groups = {}
    for item in manifest['references']:
        if (item['head_timbre_comparison_eligible'] and not item['boosted']
                and item['gain_group'] in TARGET_GAIN_GROUPS):
            macro = item['role'].lower()
            family, ids = groups.setdefault(macro, (item['family'], []))
            if family != item['family']:
                raise ValueError(f'{macro} targets mix families {family} and {item["family"]}')
            ids.append(item['model_id'])
    return groups
