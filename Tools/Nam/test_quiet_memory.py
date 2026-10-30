import unittest
import numpy as np
from audit_a2_memory import zero_history_mask
from audit_quiet_memory import absolute_bounds


class QuietMemoryTests(unittest.TestCase):
    def test_zero_history_boundary_includes_current_sample(self):
        np.testing.assert_array_equal(zero_history_mask(np.array([1., 0., 0., 0., 0.]), 3),
                                      [False, False, False, True, True])

    def test_bounds_match_optimal_constant_and_permit_perfect_remainder(self):
        # The unselected samples do not contribute to this optimistic bound.
        ref = np.array([99., -99., -2., 4.])
        mask = np.array([False, False, True, True])
        mse, peak = absolute_bounds(ref, mask)
        self.assertEqual(mse, 4.5)  # ((-2-1)^2 + (4-1)^2) / 4
        self.assertEqual(peak, 3.)
        for constant in [-5., 0., 1., 2., 8.]:
            self.assertGreaterEqual(np.sum((ref[mask]-constant)**2) / len(ref), mse)
            self.assertGreaterEqual(np.max(np.abs(ref[mask]-constant)), peak)
        self.assertEqual(absolute_bounds(ref, np.zeros(4, bool)), (0., 0.))


if __name__ == '__main__':
    unittest.main()
