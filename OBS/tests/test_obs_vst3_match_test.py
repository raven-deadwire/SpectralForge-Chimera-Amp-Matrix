#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Comparator regressions with synthetic signals; does not run OBS or Chimera."""
import math
import unittest

import obs_vst3_match_test as match


class ComparatorTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.rate = match.app.RATE
        cls.start = 6 * cls.rate
        cls.media = [0.0] * (15 * cls.rate)
        for second in (1, 4, 7, 10, 13):
            for j in range(cls.rate):
                cls.media[second * cls.rate + j] = round(
                    7000 * math.exp(-3 * j / cls.rate) * math.sin(2 * math.pi * 110 * j / cls.rate)
                ) / 32768
        cls.recorded = cls.media.copy()
        cls.fault = int(7.5 * cls.rate)
        cls.recorded[int(4.5 * cls.rate)] += 1 / 32768
        _, cls.spans = match.input_breaks(cls.recorded, cls.media)
        cls.notes = match.onsets(cls.recorded, cls.start)

    def test_clean_recording_is_complete(self):
        _, spans = match.input_breaks(self.media, self.media)
        self.assertEqual(spans, [])
        r = match.compare(self.media, self.media, 0, self.start, self.notes)
        self.assertEqual(r['max_steps'], 0)
        self.assertEqual(r['raw_max_steps'], 0)
        self.assertEqual(r['skipped_samples'], 0)
        match.require_complete_comparison({('default', 0): r})

    def test_excluded_transients_are_logged_and_cannot_certify_a_pass(self):
        self.assertGreaterEqual(len(match.alignment_notes(self.notes, self.spans)), 2)
        for steps in (158.38, 222.97, 2.22, 6.90):
            with self.subTest(steps=steps):
                obs = self.media.copy()
                obs[self.fault] += steps / 32768
                r = match.compare(obs, self.media, 0, self.start, self.notes, self.spans)
                self.assertEqual(r['max_steps'], 0)
                self.assertAlmostEqual(r['raw_max_steps'], steps, places=6)
                self.assertEqual(r['raw_worst_sample'], self.fault)
                self.assertTrue(r['raw_worst_excluded'])
                with self.assertRaisesRegex(RuntimeError, 'INCONCLUSIVE'):
                    match.require_complete_comparison({('default', 0): r})

    def test_alignment_ignores_a_corrupted_excluded_note(self):
        obs = self.media.copy()
        at = self.notes[0]
        width = self.rate // 5
        obs[at:at + width] = self.media[at - 20:at + width - 20]
        self.assertEqual(match.best_shift(obs, self.media, at), 20)
        usable = match.alignment_notes(self.notes, self.spans)
        self.assertNotIn(at, usable)
        self.assertEqual(match.best_shift(obs, self.media, usable[0]), 0)

    def test_alignment_rejects_overlap_inside_the_window(self):
        at = self.notes[0]
        self.assertEqual(match.alignment_notes([at], [(at + 100, at + 200)]), [])

    def test_retained_transient_still_exceeds_the_existing_bound(self):
        obs = self.media.copy()
        at = 13 * self.rate + 1000
        obs[at] += 223 / 32768
        r = match.compare(obs, self.media, 0, self.start, self.notes, self.spans)
        self.assertAlmostEqual(r['max_steps'], 223)
        self.assertFalse(r['raw_worst_excluded'])

    def test_one_step_quantization_is_not_confused_with_a_large_transient(self):
        # A hypothetical floor quantizer, not a claim about macOS's implementation.
        ref = [sample + 0.75 / 32768 for sample in self.media]
        r = match.compare(self.media, ref, 0, self.start, self.notes)
        self.assertLessEqual(r['max_steps'], 1.01)
        match.require_complete_comparison({('default', 0): r})


if __name__ == '__main__':
    unittest.main()
