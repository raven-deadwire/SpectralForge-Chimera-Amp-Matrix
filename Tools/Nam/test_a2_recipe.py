"""Protect loss equivalence and raw-output invariance during A2 warm starts."""

import copy
import importlib.resources
import json
import unittest

import numpy as np
import torch
from nam.models.wavenet import PackedWaveNet
from nam.train.lightning_module import LightningModule, LossConfig

from a2_recipe import (
    A2TrainingLoss, rescale_output_head, steps_per_virtual_epoch,
    training_output_gains, virtual_epoch_learning_rate,
)


class A2RecipeTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        torch.set_num_threads(1)

    def test_normalization_is_uncentered_training_rms(self):
        # Nonzero mean detects accidentally using std rather than RMS.
        targets = np.asarray([[1., 2., 3., 0.], [.01, -.02, .03, 0.]], dtype=np.float32)
        before = targets.copy()
        gains = training_output_gains(targets, chunk_frames=2)
        np.testing.assert_array_equal(targets, before)
        rms = np.sqrt(np.mean((targets * gains[:, None]) ** 2, axis=1))
        np.testing.assert_allclose(rms, 10 ** (-18 / 20), atol=1e-14, rtol=1e-14)
        with self.assertRaises(ValueError):
            training_output_gains(np.zeros((1, 10)))
        with self.assertRaises(ValueError):
            training_output_gains(np.asarray([[np.nan, 1.]]))

    def test_independent_channel_loss_and_gradients_match_upstream(self):
        torch.manual_seed(73)
        target = torch.randn(2, 3, 2048) * torch.tensor([.03, .12, .5])[None, :, None]
        prediction = (target + .025 * torch.randn_like(target)).requires_grad_()
        official = LightningModule(
            net=torch.nn.Identity(),
            loss_config=LossConfig.init_from_config({'val_loss': 'esr', 'mrstft_weight': .0005}),
        )
        expected = sum(
            item.weight * item.value
            for i in range(target.shape[1])
            for item in official._get_loss_dict(prediction[:, i, :], target[:, i, :]).values()
            if item.weight is not None and item.weight > 0
        )
        actual = A2TrainingLoss()(prediction, target)
        torch.testing.assert_close(actual, expected, rtol=1e-6, atol=1e-9)
        expected_gradient, = torch.autograd.grad(expected, prediction, retain_graph=True)
        actual_gradient, = torch.autograd.grad(actual, prediction)
        torch.testing.assert_close(actual_gradient, expected_gradient, rtol=1e-6, atol=1e-10)

    def test_gain_change_preserves_physical_output_through_packing(self):
        resource = importlib.resources.files('nam.train._resources').joinpath('config_model_packed.json')
        template = json.loads(resource.read_text())['net']['config']['submodels'][-1]['config']
        torch.manual_seed(19)
        model = PackedWaveNet.init_from_config({
            'sample_rate': 48000,
            'submodels': [{'name': str(i), 'config': copy.deepcopy(template)} for i in range(2)],
        }).eval()
        # A substantial bias makes omitting its rescale a visible failure.
        with torch.no_grad():
            model._net._layer_arrays[-1]._head_rechannel.bias.fill_(3.)
        x = torch.randn(1, model.receptive_field + 2048) * .1
        with torch.inference_mode():
            before = model(x, pad_start=False) / .1
        new_gains = [.1228390924804275, .06809300096111205]
        for i, gain in enumerate(new_gains):
            sub = model.extract_submodel(i)
            rescale_output_head(sub, .1, gain)
            model.import_submodel(i, sub)
        with torch.inference_mode():
            after = model(x, pad_start=False) / torch.tensor(new_gains)[None, :, None]
        torch.testing.assert_close(after, before, rtol=2e-6, atol=2e-6)
        self.assertEqual(model._net._head_scale, .01)

    def test_epoch_decay_does_not_accidentally_decay_each_optimizer_step(self):
        steps = steps_per_virtual_epoch(480 * 48000, 6347, 8192, 4)
        self.assertEqual(steps, 702)
        self.assertEqual(virtual_epoch_learning_rate(701, steps), .004)
        self.assertEqual(virtual_epoch_learning_rate(702, steps), .004 * .994)
        self.assertEqual(virtual_epoch_learning_rate(1404, steps), .004 * .994 ** 2)


if __name__ == '__main__':
    unittest.main()
