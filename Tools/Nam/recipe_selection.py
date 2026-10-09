"""Select a matched recipe using validation only; held-out audio stays untouched."""
import math

NAMES = ('Fenrir', 'Surtr', 'Nidhoggr', 'Fimbulvetr', 'Ragnarok')
MATCH = ('dataset_sha256', 'config', 'source', 'sample_rate', 'trainer_commit',
         'target_player_commit', 'batch', 'frames', 'seed', 'threads',
         'validation_selection', 'quality_profile', 'tail_fraction')


def select_recipe(arms):
    if set(arms) != {'legacy', 'official-a2'}:
        raise RuntimeError('Exactly two completed recipe arms required')
    baseline = arms['legacy']
    rows = {}
    for recipe, arm in arms.items():
        cfg, result = arm['config'], arm['validation']
        if cfg['recipe'] != recipe or result['recipe'] != recipe:
            raise RuntimeError('Recipe mismatch')
        if cfg['validation_selection'] != 'full' or result['identity'] != cfg['identity']:
            raise RuntimeError('Invalid validation provenance')
        if result['quality_profile'] != cfg['quality_profile']:
            raise RuntimeError('Changed quality profile')
        if any(cfg[k] != baseline['config'][k] for k in MATCH):
            raise RuntimeError('Unmatched recipe configuration')
        if (result['checkpoint_step'] != baseline['validation']['checkpoint_step'] or
            result['crop_schedule_start_step'] != 0 or
            result['crop_schedule_sha256'] != baseline['validation']['crop_schedule_sha256']):
            raise RuntimeError('Unmatched budgets or crop histories')
        if cfg['warm_start'] != baseline['config']['warm_start'] or not cfg['warm_start']:
            raise RuntimeError('Unmatched common initialization')
        if set(result['channels']) != set(NAMES):
            raise RuntimeError('Missing channel')
        esrs, passes = [], []
        for name in NAMES:
            row = result['channels'][name]
            quality = row['tone3000_fidelity']
            esr = quality['full_esr']
            if type(esr) not in (int, float) or not math.isfinite(esr) or esr < 0:
                raise RuntimeError('Invalid validation ESR')
            if type(quality['fidelity_pass']) is not bool or abs(row['esr']-esr) > 1e-8:
                raise RuntimeError('Inconsistent validation evidence')
            esrs.append(esr)
            passes.append(quality['fidelity_pass'])
        rows[recipe] = {'validation_pass_channels': sum(passes),
                        'mean_validation_full_esr': sum(esrs)/len(esrs)}
    # Prefer more fully passing channels, then smaller mean ESR, then legacy on ties.
    selected = min(rows, key=lambda r: (-rows[r]['validation_pass_channels'],
                                       rows[r]['mean_validation_full_esr'], r != 'legacy'))
    return {'selected_recipe': selected, 'arms': rows, 'selection_split': 'validation',
            'rule': 'maximum fidelity passes, minimum mean full ESR, legacy tie-break',
            'independent_test_used_for_selection': False, 'release_approved': False}
