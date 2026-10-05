"""The pinned upstream A2 objective, adapted to independent captured channels.

This does not reproduce an entire TONE3000 cloud training session. In particular,
our comparison uses identical random crops in both arms instead of the upstream
shuffled, nonoverlapping DataLoader. A virtual epoch has the same number of
optimizer steps as that DataLoader; the learning rate changes only at its boundary.
The upstream objective has neither a DC term nor gradient clipping. Numerical
output normalization is undone on export and must never change evaluation scale.
"""

import math

import numpy as np
import torch
from nam._dependencies.auraloss.freq import MultiResolutionSTFTLoss
from nam.models.losses import multi_resolution_stft_loss


RECIPE = {
    'id': 'upstream-a2-loss-rms18-v1',
    'trainer_commit': '0072676419459f5d39e36f5b9fd4172f28d62cbf',
    'training_output_rms_dbfs': -18.0,
    'normalization_statistics': 'training output only; uncentered population RMS',
    'mse_weight': 1.0,
    'mrstft_weight': 0.0005,
    'mrstft_fft_sizes': [1024, 2048, 512],
    'mrstft_hop_sizes': [120, 240, 50],
    'mrstft_win_lengths': [600, 1200, 240],
    'mrstft_window': 'hann_window',
    'mrstft_reduction': 'mean resolutions; independent per-channel losses summed',
    'mrstft_scale_invariance': False,
    'dc_weight': 0.0,
    'gradient_clip_norm': None,
    'optimizer': {'name': 'Adam', 'lr': 0.004, 'weight_decay': 3.17e-7},
    'scheduler': {'name': 'ExponentialLR', 'gamma': 0.994, 'interval': 'virtual epoch'},
    'adaptation': 'Matched random crops; floor-step virtual epochs, not complete cloud replication',
}


def training_output_gains(targets, level_rms_dbfs=-18.0, chunk_frames=1_000_000):
    """Return one training-only RMS gain per channel without a full float64 copy.

    ``targets`` is [channels, samples], including the captured training silence.
    The caller applies these gains to targets and reverses them for raw inference.
    Validation/test data must not be passed into this function.
    """
    targets = np.asanyarray(targets)
    if targets.ndim != 2 or min(targets.shape) == 0:
        raise ValueError('Expected nonempty training targets [channels, samples]')
    if not np.isfinite(level_rms_dbfs) or chunk_frames <= 0:
        raise ValueError('Invalid target level or chunk size')
    target_rms = 10.0 ** (level_rms_dbfs / 20.0)
    gains = []
    for channel in targets:
        total = math.fsum(
            float(np.square(channel[start:start + chunk_frames], dtype=np.float64).sum(dtype=np.float64))
            for start in range(0, channel.size, chunk_frames)
        )
        if not math.isfinite(total) or total <= 0:
            raise ValueError('Training output must have finite, nonzero energy')
        gains.append(target_rms / math.sqrt(total / channel.size))
    gains = np.asarray(gains, dtype=np.float64)
    if not np.all(np.isfinite(gains) & (gains > 0)):
        raise ValueError('Invalid training output gain')
    return gains


def rescale_output_head(submodel, old_gain, new_gain):
    """Change numerical target scale while preserving output divided by its gain.

    PackedWaveNet requires a common head_scale, so each channel's final linear
    head weight AND bias are scaled. Reset the optimizer after this warm start;
    optimizer moments from the old parameterization are not reused.
    """
    if not all(math.isfinite(g) and g > 0 for g in (old_gain, new_gain)):
        raise ValueError('Output gains must be finite and positive')
    if submodel._net._head is not None:
        raise ValueError('Output reparameterization requires the A2 linear final head')
    head = submodel._net._layer_arrays[-1]._head_rechannel
    ratio = new_gain / old_gain
    if not math.isfinite(ratio) or ratio <= 0:
        raise ValueError('Invalid output gain ratio')
    with torch.no_grad():
        head.weight.mul_(ratio)
        if head.bias is not None:
            head.bias.mul_(ratio)


class A2TrainingLoss(torch.nn.Module):
    """Sum the exact upstream MSE + 0.0005 MRSTFT for each independent channel.

    A single MRSTFT call across channels is not equivalent: spectral convergence
    uses one Frobenius denominator across a call's batch and frequency elements.
    """

    def __init__(self):
        super().__init__()
        self.mrstft = MultiResolutionSTFTLoss()
        self.last_components = {}

    def components(self, prediction, target):
        if prediction.shape != target.shape or prediction.ndim != 3:
            raise ValueError('Expected matching [batch, channels, samples] tensors')
        if min(prediction.shape) <= 0:
            raise ValueError('Empty loss input')
        mse = torch.nn.functional.mse_loss(prediction, target, reduction='none').mean(dim=(0, 2)).sum()
        spectral = sum(
            multi_resolution_stft_loss(prediction[:, i, :], target[:, i, :], self.mrstft)
            for i in range(prediction.shape[1])
        )
        return mse, spectral

    def forward(self, prediction, target):
        mse, spectral = self.components(prediction, target)
        self.last_components = {'mse_sum': float(mse.detach()), 'mrstft_sum': float(spectral.detach())}
        return mse + RECIPE['mrstft_weight'] * spectral


def steps_per_virtual_epoch(sample_count, receptive_field, frames, batch):
    """Match upstream Dataset length and DataLoader(drop_last=True)."""
    if min(sample_count, receptive_field, frames, batch) <= 0:
        raise ValueError('Data and batch sizes must be positive')
    steps = ((sample_count - receptive_field + 1) // frames) // batch
    if steps < 1:
        raise ValueError('Training data cannot fill one complete batch')
    return steps


def virtual_epoch_learning_rate(step, steps_per_epoch, initial=0.004, gamma=0.994):
    """LR after ``step`` optimizer updates; gamma is applied once per epoch.

    Random crop sampling does not visit an exact permutation of every window.
    This schedule preserves the upstream update budget per epoch, not its sampler.
    """
    if step < 0 or steps_per_epoch <= 0 or not (0 < gamma <= 1) or initial <= 0:
        raise ValueError('Invalid learning-rate schedule')
    return initial * gamma ** (step // steps_per_epoch)
