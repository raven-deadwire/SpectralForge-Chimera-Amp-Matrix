"""The frozen v1 OriginalAmp state suite and validated additional states."""
import json
import re
from pathlib import Path

from .references import MACROS

PRESETS = ['fenrir', 'surtr', 'nidhoggr', 'fimbulvetr', 'ragnarok']
NAME = re.compile(r'^[a-z][a-z0-9_]*$')
# Stems of files the comparison writes beside the state renders.
RESERVED = ('probe', 'comparison')


def frozen_v1():
    states = {'default': {}, 'gain_max': {'controls': {'gain': 1.0}}}
    states.update({p: {'preset': f'original.nastrond.{p}.v1'} for p in PRESETS})
    for macro in MACROS:
        for value in [0, 1]:
            states[f'{macro}_{value}'] = {'controls': {macro: value}}
    return states


def suite(extra_path=None):
    states = frozen_v1()
    if extra_path is None:
        return states
    extra = json.loads(Path(extra_path).read_text())
    if not isinstance(extra, dict):
        raise ValueError('Additional states must be a JSON object')
    for name, state in extra.items():
        if not NAME.match(name) or name in RESERVED:
            raise ValueError(f'Unsafe or reserved state name: {name!r}')
        if not isinstance(state, dict):
            raise ValueError(f'State {name} must be an object')
        if 'preset' in state and 'channel' in state:
            raise ValueError(f'State {name}: preset and channel are mutually exclusive')
    if set(states).intersection(extra):
        raise ValueError('Additional states must not replace frozen v1 states')
    states.update(extra)
    return states
