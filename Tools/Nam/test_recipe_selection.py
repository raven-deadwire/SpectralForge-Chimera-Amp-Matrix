import copy
import json
from pathlib import Path
import tempfile
import unittest
from ci_pipeline import NAMES, accuracy_diagnostics, training_plan
from quality_profile import PROFILE
from recipe_selection import MATCH, select_recipe


def fixture():
    cfg = {k: 'same' for k in MATCH}
    cfg.update(validation_selection='full', quality_profile=PROFILE,
               identity='identity', warm_start={'checkpoint_sha256': 'frozen'})
    rows = {n: {'esr': .1, 'tone3000_fidelity': {'full_esr': .1, 'fidelity_pass': False}} for n in NAMES}
    result = dict(identity='identity', quality_profile=PROFILE, channels=rows,
                  checkpoint_step=5000, crop_schedule_start_step=0, crop_schedule_sha256='same')
    return {r: {'config': dict(copy.deepcopy(cfg), recipe=r),
                'validation': dict(copy.deepcopy(result), recipe=r)} for r in ('legacy', 'official-a2')}


class RecipeTests(unittest.TestCase):
    def test_explicit_matched_full_and_bounded_smoke(self):
        self.assertEqual(training_plan('smoke', 5000), [('run', 'legacy', 2, None)])
        self.assertEqual([(r, s, w) for _, r, s, w in training_plan('full', 5000)[1:]],
                         [('legacy', 5000, 'initialization'), ('official-a2', 5000, 'initialization')])
        with self.assertRaises(ValueError):
            training_plan('full', 50001)

    def test_selection_uses_validation_only_and_never_grants_release(self):
        arms = fixture()
        self.assertEqual(select_recipe(arms)['selected_recipe'], 'legacy')
        for row in arms['official-a2']['validation']['channels'].values():
            row['esr'] = row['tone3000_fidelity']['full_esr'] = .05
        result = select_recipe(arms)
        self.assertEqual(result['selected_recipe'], 'official-a2')
        self.assertFalse(result['release_approved'])
        self.assertFalse(result['independent_test_used_for_selection'])
        arms['legacy']['validation']['channels']['Fenrir']['tone3000_fidelity']['fidelity_pass'] = True
        self.assertEqual(select_recipe(arms)['selected_recipe'], 'legacy')

    def test_rejects_unmatched_and_incomplete_evidence(self):
        for key in MATCH:
            arms = fixture()
            arms['official-a2']['config'][key] = 'changed'
            with self.subTest(key=key), self.assertRaises(RuntimeError):
                select_recipe(arms)
        for key, value in [('checkpoint_step', 4999), ('crop_schedule_sha256', 'different'),
                           ('identity', 'stale'), ('quality_profile', {})]:
            arms = fixture()
            arms['official-a2']['validation'][key] = value
            with self.subTest(key=key), self.assertRaises(RuntimeError):
                select_recipe(arms)
        for value in (float('nan'), float('inf'), -1, True):
            arms = fixture()
            arms['official-a2']['validation']['channels']['Fenrir']['tone3000_fidelity']['full_esr'] = value
            with self.subTest(value=value), self.assertRaises(RuntimeError):
                select_recipe(arms)

    def test_diagnostics_keep_strict_esr_and_report_level_quiet_failures(self):
        q = dict(active_median=.005, active_p95=.01, active_worst=.02, full_esr=.01,
                 quiet_worst_residual_rms_dbfs=-69., quiet_worst_residual_peak_dbfs=-60.,
                 rms_error_db=-.51, peak_error_db=1., fidelity_pass=False)
        with tempfile.TemporaryDirectory() as td:
            root = Path(td)
            folder = root / 'package/Validation'
            folder.mkdir(parents=True)
            payload = {'channels': {n: {'tone3000_fidelity': q} for n in NAMES}}
            for name in ('validation.json', 'comparison.json'):
                (folder / name).write_text(json.dumps(payload))
            report = accuracy_diagnostics(root)
            failed = report['splits']['validation']['Fenrir']['failed_conditions']
            self.assertEqual({r['metric'] for r in failed},
                             {'full_esr', 'quiet_worst_residual_rms_dbfs', 'rms_error_db'})
            self.assertEqual(report['quality_profile'], PROFILE)
            self.assertFalse(report['release_approved'])


if __name__ == '__main__':
    unittest.main()
