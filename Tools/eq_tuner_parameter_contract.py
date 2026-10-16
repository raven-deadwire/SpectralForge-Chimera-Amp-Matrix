"""Frozen 1.3.2 append-only parameter identities for beta.2 ABI comparisons.

The 4,967 released product ordinals stay fixed. JUCE's Bypass/MIDI wrapper
parameters follow the product list and move by exactly 59, retaining ParamIDs.
No old parameter/value is filtered out; unexpected additions are rejected.
"""
import csv

RELEASED_COUNT = 4967
APPENDED_NAMES = ['tunerProfile', 'tunerDisplay', 'tunerString']
APPENDED_NAMES += [f'tuner{field}{i}' for i in range(1, 9) for field in ('Note', 'Offset')]
APPENDED_NAMES += [f'dynamicEQ{i}_{field}' for i in range(1, 5)
                   for field in ('target', 'threshold', 'range', 'attack', 'release', 'ratio')]
APPENDED_NAMES += ['cleanCompOn', 'cleanCompThreshold', 'cleanCompRatio', 'cleanCompAttack',
                   'cleanCompRelease', 'cleanCompKnee', 'cleanCompMakeup', 'cleanCompMix',
                   'cleanCompHP', 'cleanCompDetector', 'gateMode', 'gateRatio', 'gateAttack',
                   'liveLimiterOn', 'liveLimiterCeiling', 'liveLimiterRelease']

def vst_id(name):
    value = 0
    for char in name:
        value = (31 * value + ord(char)) & 0xffffffff
    return str(value & 0x7fffffff)

APPENDED_IDS = [vst_id(name) for name in APPENDED_NAMES]
assert len(APPENDED_IDS) == len(set(APPENDED_IDS)) == 59

def rows(path):
    with path.open() as stream:
        return list(csv.DictReader(stream, delimiter='\t'))

def align_metadata(released, candidate):
    """Validate both inventories and return every old row aligned by ParamID."""
    if len({r['id'] for r in released}) != len(released) or len({r['id'] for r in candidate}) != len(candidate):
        raise ValueError('duplicate VST3 ParamID')
    if [r['index'] for r in released] != [str(i) for i in range(len(released))] or [r['index'] for r in candidate] != [str(i) for i in range(len(candidate))]:
        raise ValueError('noncontiguous VST3 index inventory')
    if len(candidate) != len(released) + 59:
        raise ValueError('VST3 inventory is not the exact 59-parameter append')
    if [r['id'] for r in candidate[RELEASED_COUNT:RELEASED_COUNT+59]] != APPENDED_IDS:
        raise ValueError('appended parameter identity/order mismatch')
    lookup = {r['id']: r for r in candidate}
    aligned = []
    for i, old in enumerate(released):
        new = lookup.get(old['id'])
        if new is None:
            raise ValueError('released ParamID missing: ' + old['id'])
        expected_index = i if i < RELEASED_COUNT else i + 59
        if int(new['index']) != expected_index:
            raise ValueError('released product/wrapper order changed: ' + old['id'])
        if any(old[key] != new[key] for key in ('id', 'steps', 'default', 'flags')):
            raise ValueError('released metadata changed: ' + old['id'])
        aligned.append((old, new))
    return aligned

def normalized_equal(left, right):
    """Exact values for all shared IDs; only the frozen append may be absent."""
    a, b = rows(left/'normalized.tsv'), rows(right/'normalized.tsv')
    aa, bb = {r['id']: r['value'] for r in a}, {r['id']: r['value'] for r in b}
    if len(aa) != len(a) or len(bb) != len(b):
        raise ValueError('duplicate normalized ParamID')
    if aa.keys() == bb.keys():
        if aa != bb: raise ValueError('normalized values changed')
        return
    small, large, folder = (aa, bb, right) if len(aa) < len(bb) else (bb, aa, left)
    if not small.keys() <= large.keys() or large.keys()-small.keys() != set(APPENDED_IDS):
        raise ValueError('unexpected normalized parameter inventory')
    if any(small[key] != large[key] for key in small):
        raise ValueError('released normalized value changed')
    metadata = {r['id']: r for r in rows(folder/'parameters.tsv')}
    if any(large[key] != metadata[key]['default'] for key in APPENDED_IDS):
        raise ValueError('appended normalized value is not its default')
