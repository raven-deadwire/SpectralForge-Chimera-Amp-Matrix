"""Numerical feature/fit/export checks using a pinned archived A2-Full model.

Set CHIMERA_HEAD_REFIT_TEST_MODEL to the archived refined Fenrir NAM. No training
data or evaluation split is needed; these checks use deterministic probes.
"""
import copy
import hashlib
import json
import os
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

import numpy as np
import torch
from scipy import signal
from nam.models._from_nam import init_from_nam

import head_refit


MODEL_SHA256 = "5c88c8d03609a0e3a2341467d3ae7e843af9d36de8e4e007ba85b08b9545b594"


class PinnedHeadRefitTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        filename = os.environ.get("CHIMERA_HEAD_REFIT_TEST_MODEL")
        if not filename:
            raise unittest.SkipTest("CHIMERA_HEAD_REFIT_TEST_MODEL not provided")
        cls.original = Path(filename)
        raw = cls.original.read_bytes()
        if hashlib.sha256(raw).hexdigest() != MODEL_SHA256:
            raise RuntimeError("Expected SHA-pinned refined Fenrir A2-Full")
        torch.set_num_threads(2)
        cls.model = init_from_nam(json.loads(raw)).eval()
        cls.scale = float(cls.model._net._head_scale)
        cls.weights = head_refit.head(cls.model).weight.detach().numpy().reshape(-1).astype(np.float64)
        cls.rng = np.random.default_rng(8831307)
        cls.probe = cls.rng.normal(0, 0.08, cls.model.receptive_field - 1 + 513).astype(np.float32)

    def test_actual_output_matches_causal_feature_reconstruction(self):
        raw, features = head_refit.capture(self.model, self.probe)
        expected = self.scale * (
            features @ self.weights + float(head_refit.head(self.model).bias.detach()[0])
        )
        self.assertEqual(features.shape, (513, 128))
        np.testing.assert_allclose(raw, expected, rtol=3e-6, atol=2e-6)

    def test_channel_and_tap_order_match_one_hot_actual_convolutions(self):
        _, features = head_refit.capture(self.model, self.probe)
        for channel, tap in [(0, 0), (2, 5), (7, 15)]:
            model = copy.deepcopy(self.model).eval()
            h = head_refit.head(model)
            with torch.no_grad():
                h.weight.zero_()
                h.bias.zero_()
                h.weight[0, channel, tap] = 1.0
                actual = model(torch.from_numpy(self.probe), pad_start=False).numpy()
            np.testing.assert_allclose(
                actual, self.scale * features[:, channel * 16 + tap], rtol=1e-6, atol=1e-7
            )

    def test_chunked_history_is_equivalent_to_one_continuous_forward(self):
        x = self.rng.normal(0, 0.03, 1301).astype(np.float32)
        full_input = np.pad(x, (self.model.receptive_field - 1, 0))
        expected, expected_features = head_refit.capture(self.model, full_input)
        with patch.object(head_refit, "CHUNK", 257):
            batches = list(head_refit.batches(self.model, x))
        actual = np.concatenate([b[1] for b in batches])
        actual_features = np.concatenate([b[2] for b in batches])
        np.testing.assert_allclose(actual, expected, rtol=3e-6, atol=2e-6)
        # Different convolution chunk sizes can change float32 summation order.
        # The audio comparison above stays tighter; this bound concerns hidden
        # activations only and does not alter any project acceptance threshold.
        np.testing.assert_allclose(actual_features, expected_features, rtol=3e-6, atol=1e-5)
        self.assertEqual([b[0] for b in batches], [0, 257, 514, 771, 1028, 1285])

    def test_ridge_solution_matches_augmented_least_squares(self):
        _, features = head_refit.capture(self.model, self.probe)
        _, zero = head_refit.capture(
            self.model, np.zeros(self.model.receptive_field + 1, np.float32)
        )
        phi = (features - zero[-1]) * self.scale
        target_weights = self.weights + np.linspace(-0.05, 0.05, 128)
        target = phi @ target_weights
        gram = phi.T @ phi / len(phi)
        rhs = phi.T @ target / len(phi)
        penalty = 1e-2 * np.trace(gram) / 128
        solution = head_refit.ridge_solution(gram, rhs, self.weights, 1e-2)
        design = np.concatenate([phi / np.sqrt(len(phi)), np.sqrt(penalty) * np.eye(128)])
        targets = np.concatenate([target / np.sqrt(len(phi)), np.sqrt(penalty) * self.weights])
        reference, *_ = np.linalg.lstsq(design, targets, rcond=None)
        np.testing.assert_allclose(solution, reference, rtol=1e-8, atol=1e-8)
        before = np.mean((phi @ self.weights - target) ** 2)
        after = np.mean((phi @ solution - target) ** 2) + penalty * np.sum((solution - self.weights) ** 2)
        self.assertLess(after, before)

    def test_filtered_feature_sum_matches_filtered_head_including_residual(self):
        _, features = head_refit.capture(self.model, self.probe)
        _, zero = head_refit.capture(
            self.model, np.zeros(self.model.receptive_field + 1, np.float32)
        )
        phi = (features - zero[-1]) * self.scale
        path = Path(head_refit.__file__).parent / 'memory-factorization/Fenrir-factor-post-eq.json'
        sos = head_refit.load_eq_sos(path)
        constant = 0.013  # Test-only known offset; no target/audio fit.
        expected = signal.sosfilt(sos, phi @ self.weights + constant)
        state = None
        output = []
        for start in range(0, len(phi), 97):
            filtered, state = head_refit.filter_feature_block(
                sos, phi[start:start + 97], state, include_constant=True)
            output.append(filtered[:, :128] @ self.weights + constant * filtered[:, 128])
        np.testing.assert_allclose(np.concatenate(output), expected, rtol=2e-10, atol=2e-10)

    def test_fitted_export_preserves_backbone_features_and_silence(self):
        _, features = head_refit.capture(self.model, self.probe)
        _, zero = head_refit.capture(
            self.model, np.zeros(self.model.receptive_field + 1, np.float32)
        )
        np.testing.assert_array_equal(zero[0], zero[1])
        z = zero[-1]
        fitted = self.weights + np.linspace(-0.07, 0.11, 128)
        before = {key: value.clone() for key, value in self.model.state_dict().items()}
        with tempfile.TemporaryDirectory(prefix="chimera-head-export-") as tmp:
            restored, file = head_refit.export_head(
                self.model, fitted, z, self.original, Path(tmp), "Fenrir"
            )
            self.assertEqual(json.loads(file.read_text())["config"], json.loads(self.original.read_text())["config"])
            actual, restored_features = head_refit.capture(restored, self.probe)
            np.testing.assert_array_equal(restored_features, features)
            expected = self.scale * (features - z) @ fitted
            np.testing.assert_allclose(actual, expected, rtol=5e-6, atol=3e-6)
            with torch.inference_mode():
                quiet = restored(torch.zeros(restored.receptive_field + 512), pad_start=False).numpy()
            self.assertLess(float(np.max(np.abs(quiet))), 1e-6)
            for key, value in self.model.state_dict().items():
                self.assertTrue(torch.equal(value, before[key]), key)

    def test_export_guard_detects_head_corruption_during_import(self):
        _, zero = head_refit.capture(
            self.model, np.zeros(self.model.receptive_field + 1, np.float32)
        )
        official_loader = head_refit.init_from_nam

        def corrupted_loader(obj):
            model = official_loader(obj)
            with torch.no_grad():
                head_refit.head(model).weight[0, 2, 5] += 0.01
            return model

        with tempfile.TemporaryDirectory(prefix="chimera-head-reject-") as tmp:
            with patch.object(head_refit, "init_from_nam", corrupted_loader):
                with self.assertRaisesRegex(RuntimeError, "Fitted model export roundtrip changed"):
                    head_refit.export_head(self.model, self.weights, zero[-1], self.original, Path(tmp), "Fenrir")


class SelectionGuardTests(unittest.TestCase):
    def baseline(self):
        return {
            "active_pass": False, "quiet_pass": True, "level_pass": False,
            "full_esr_pass": False, "rms_error_db": -0.3, "peak_error_db": 1.7,
            "quiet_worst_residual_rms_dbfs": -80.0,
            "quiet_worst_residual_peak_dbfs": -68.0, "full_esr": 0.1,
        }

    def test_lower_esr_does_not_override_a_peak_regression(self):
        baseline = self.baseline()
        candidate = {**baseline, "full_esr": 0.08, "peak_error_db": 2.0}
        self.assertIn("level_regression:peak_error_db", head_refit.allowed(candidate, baseline))

    def test_lower_esr_does_not_override_a_new_quiet_failure(self):
        baseline = self.baseline()
        candidate = {**baseline, "full_esr": 0.08, "quiet_pass": False,
                     "quiet_worst_residual_peak_dbfs": -59.0}
        self.assertIn("new_failure:quiet_pass", head_refit.allowed(candidate, baseline))

    def test_esr_and_peak_improvement_can_be_selected_without_release_approval(self):
        baseline = self.baseline()
        candidate = {**baseline, "full_esr": 0.09, "peak_error_db": 1.2}
        self.assertEqual(head_refit.allowed(candidate, baseline), [])
        self.assertFalse(candidate["full_esr_pass"])

    def test_predeclared_point_one_percent_selection_boundary_is_inclusive(self):
        baseline = self.baseline()
        candidate = {**baseline, "full_esr": baseline['full_esr'] * .999}
        self.assertEqual(head_refit.allowed(candidate, baseline), [])


class CausalFilterTests(unittest.TestCase):
    def setUp(self):
        path = Path(head_refit.__file__).parent / 'memory-factorization/Fimbulvetr-factor-post-eq.json'
        self.sos = head_refit.load_eq_sos(path)

    def test_pre_skip_excitation_state_reaches_scored_tail(self):
        phi = np.zeros((7500, 4), dtype=np.float64)
        phi[6346, 2] = 1
        whole = signal.sosfilt(self.sos, phi, axis=0)
        first, state = head_refit.filter_feature_block(self.sos, phi[:6347])
        second, state = head_refit.filter_feature_block(self.sos, phi[6347:], state)
        np.testing.assert_array_equal(np.concatenate([first, second]), whole)
        self.assertGreater(float(np.max(np.abs(second))), 1e-4)
        # A deliberate reset at the acceptance skip loses native recursive state.
        reset, _ = head_refit.filter_feature_block(self.sos, phi[6347:])
        self.assertEqual(float(np.max(np.abs(reset))), 0)

    def test_float32_native_output_filter_is_bit_exact_across_chunks(self):
        rng = np.random.default_rng(7352)
        x = rng.normal(0, .2, 5003).astype(np.float32)
        expected = signal.sosfilt(self.sos.astype(np.float32), x)
        state = None
        pieces = []
        for start in range(0, len(x), 257):
            part, state = head_refit.filter_native_output_block(self.sos, x[start:start + 257], state)
            pieces.append(part)
        self.assertEqual(state.dtype, np.dtype('float32'))
        np.testing.assert_array_equal(np.concatenate(pieces), expected)


if __name__ == "__main__":
    unittest.main()
