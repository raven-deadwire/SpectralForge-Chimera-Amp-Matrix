import unittest
from quality_profile import PROFILE, RELEASE_V3, release_v3_check, release_v3_limits


def metrics(**overrides):
    f = {'full_esr': .018, 'active_median': .01, 'active_p95': .08, 'active_worst': .3,
         'quiet_worst_residual_rms_dbfs': -75., 'quiet_worst_residual_peak_dbfs': -65.,
         'rms_error_db': -.1, 'peak_error_db': .5}
    f.update(overrides)
    return f


class ReleaseV3Tests(unittest.TestCase):
    def test_every_channel_has_one_class(self):
        names = [n for c in RELEASE_V3['classes'].values() for n in c['channels']]
        self.assertEqual(sorted(names), sorted(PROFILE['source_output_scale']))

    def test_v2_quiet_and_level_limits_are_kept(self):
        for key in ('quiet_residual_rms_max_dbfs', 'quiet_residual_peak_max_dbfs', 'rms_error_max_db', 'peak_error_max_db'):
            self.assertEqual(RELEASE_V3[key], PROFILE[key])

    def test_high_gain_pass_and_each_failure(self):
        self.assertTrue(release_v3_check(metrics(), 'Fenrir')['pass'])
        for key, value, check in [('full_esr', .026, 'full_esr'), ('active_median', .021, 'active_median'),
                                  ('active_p95', .13, 'active_p95'), ('active_worst', .41, 'active_worst'),
                                  ('quiet_worst_residual_peak_dbfs', -53., 'quiet'), ('peak_error_db', 1.07, 'level')]:
            result = release_v3_check(metrics(**{key: value}), 'Fenrir')
            self.assertFalse(result['pass'], key)
            self.assertFalse(result['checks'][check], key)

    def test_bloom_keeps_v2_full_esr_limit(self):
        self.assertEqual(release_v3_limits('Fimbulvetr')['full_esr_max'], .01)
        self.assertFalse(release_v3_check(metrics(), 'Fimbulvetr')['pass'])
        self.assertTrue(release_v3_check(metrics(full_esr=.008, active_median=.006, active_p95=.026, active_worst=.15), 'Fimbulvetr')['pass'])

    def test_missing_active_windows_fail(self):
        self.assertFalse(release_v3_check(metrics(active_median=None), 'Surtr')['pass'])


if __name__ == '__main__':
    unittest.main()
