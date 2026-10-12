#!/usr/bin/env python3
"""Summarize immutable experiment results; never select or alter candidates."""
from __future__ import annotations

import hashlib
import json
from pathlib import Path

import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import numpy as np

ROOT = Path(__file__).resolve().parent
NAMES = ['Fenrir', 'Surtr', 'Nidhoggr', 'Fimbulvetr', 'Ragnarok']
COLORS = {'baseline': '#84929e', 'raw': '#3679ac', 'eq-chain': '#1b9a89', 'selected': '#182e43'}


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    comparison_path = ROOT / 'independent-comparison.json'
    selected_path = ROOT / 'selected-summary.json'
    c = json.loads(comparison_path.read_text())
    s = json.loads(selected_path.read_text())
    assert c['complete'] and s['complete']
    assert not c['release_approved'] and not s['release_approved']
    assert s['source_comparison_sha256'] == sha(comparison_path)
    assert c['input_sha256'] == s['input_sha256']
    assert c['quality_profile'] == s['quality_profile']
    arms = {**c['arms'], 'selected': s['channels']}
    means = {label: {split: float(np.mean([rows[n][split]['full_esr'] for n in NAMES]))
                    for split in ('new_test', 'old_revealed_regression')}
             for label, rows in arms.items()}
    full_keys = ['full_esr', 'active_median', 'active_p95', 'active_worst',
                 'rms_error_db', 'peak_error_db', 'quiet_worst_residual_rms_dbfs',
                 'quiet_worst_residual_peak_dbfs', 'active_pass', 'quiet_pass',
                 'level_pass', 'fidelity_pass']
    summary = {
        'source_sha256': {p.name: sha(p) for p in (comparison_path, selected_path)},
        'input_sha256': c['input_sha256'], 'quality_profile': c['quality_profile'],
        'selected_modes': s['selected_modes'], 'means': means,
        'selected_new_mean_esr_reduction_percent':
            100 * (1 - means['selected']['new_test'] / means['baseline']['new_test']),
        'selected_old_mean_esr_reduction_percent':
            100 * (1 - means['selected']['old_revealed_regression'] / means['baseline']['old_revealed_regression']),
        'channels': {label: {n: {
            split: {k: rows[n][split][k] for k in full_keys}
            for split in ('new_test', 'old_revealed_regression')}
            for n in NAMES} for label, rows in arms.items()},
        'selected_character': {n: s['channels'][n]['character'] for n in NAMES},
        'selected_max_band_contrast_error_db': float(np.max([
            r['absolute_contrast_error_db'] for r in s['channel_character_contrasts']])),
        'selected_numerical_pass_count': s['numerical_pass_count'],
        'selected_engine_pass_count': s['engine_pass_count'],
        'engine_parity_range_dbfs': [float(f([
            row['python_native_parity_residual_dbfs']
            for rows in arms.values() for row in rows.values()])) for f in (min, max)],
        'block_output_max_abs': max(row['block64_256_max_abs'] for rows in arms.values() for row in rows.values()),
        'preset_restore_max_abs': max(row['preset_restore_max_abs'] for rows in arms.values() for row in rows.values()),
        'release_approved': False,
        'scope': 'Descriptive summaries; no test-time selection, statistical significance, DI listening, or GUI/DAW approval.'
    }
    (ROOT / 'results-summary.json').write_text(json.dumps(summary, indent=2) + '\n')

    plt.rcParams.update({'font.family': 'DejaVu Sans', 'font.size': 10,
                         'axes.spines.top': False, 'axes.spines.right': False,
                         'axes.labelcolor': '#334555', 'text.color': '#182e43',
                         'axes.titleweight': 'bold', 'axes.titlesize': 12})
    fig, ax = plt.subplots(2, 2, figsize=(13, 8.6))
    fig.patch.set_facecolor('#fafcfb')
    x = np.arange(5)
    for i, label in enumerate(('baseline', 'raw', 'eq-chain')):
        ax[0, 0].bar(x + (i-1)*.23,
                     [arms[label][n]['new_test']['full_esr'] for n in NAMES],
                     width=.22, label=label, color=COLORS[label])
    ax[0, 0].scatter(x, [s['channels'][n]['new_test']['full_esr'] for n in NAMES],
                     marker='D', s=26, color=COLORS['selected'], label='preselected', zorder=5)
    ax[0, 0].axhline(.01, ls='--', color='#bb4b42', lw=1.4, label='ESR limit <0.01')
    ax[0, 0].set(title='A  New independent test: full ESR', ylabel='ESR', ylim=(0, .14))
    ax[0, 0].legend(frameon=False, fontsize=8, ncol=3, loc='upper left')

    for a, key, bound, title in (
            (ax[0, 1], 'rms_error_db', .5, 'B  New test: RMS level error'),
            (ax[1, 0], 'peak_error_db', 1., 'C  New test: peak level error')):
        a.axhspan(-bound, bound, color='#dcefe7', alpha=.75)
        a.axhline(-bound, color='#90b8a6', ls='--', lw=1)
        a.axhline(bound, color='#90b8a6', ls='--', lw=1)
        a.axhline(0, color='#ccd5da', lw=.8)
        for label, marker in (('baseline', 'o'), ('selected', 'D')):
            values = [arms[label][n]['new_test'][key] for n in NAMES]
            a.plot(x, values, marker=marker, lw=1.4, ms=5,
                   color=COLORS[label], label=label)
        a.set(title=title, ylabel='Candidate / native, dB')
        a.legend(frameon=False, fontsize=9)
    for label, marker in (('baseline', 'o'), ('raw', 's'), ('eq-chain', 'D')):
        ax[1, 1].plot(x,
            [arms[label][n]['old_revealed_regression']['quiet_worst_residual_rms_dbfs'] for n in NAMES],
            marker=marker, lw=1.4, ms=5, color=COLORS[label], label=label)
    ax[1, 1].axhline(-70, ls='--', color='#bb4b42', lw=1.4, label='RMS limit -70')
    ax[1, 1].set(title='D  Revealed old test: worst quiet residual',
                 ylabel='Residual RMS, dBFS (lower is better)', ylim=(-108, -55))
    ax[1, 1].legend(frameon=False, fontsize=8, ncol=2, loc='upper left')
    for a in ax.ravel():
        a.set_xticks(x, NAMES)
        a.grid(axis='y', color='#e7ecee', lw=.65)
        a.set_axisbelow(True)
        a.set_facecolor('white')
    fig.suptitle('Náströnd: A2 head refit and supported POST EQ', fontsize=17, x=.055, ha='left')
    fig.text(.055, .934, 'Fixed validation selection · 24 s synthetic test · 12 October 2026', color='#5b6d79')
    fig.text(.055, .025,
        'All 5 channels fail full fidelity. Engine checks use pinned native components in a console harness.\n'
        'Raw / EQ bars describe best validation ESR candidates, including rejected candidates; no release or listening approval.',
        fontsize=9, color='#5b6d79')
    fig.subplots_adjust(left=.075, right=.98, top=.865, bottom=.12, hspace=.36, wspace=.23)
    fig.savefig(ROOT / 'results-figure.png', dpi=165)
    plt.close(fig)

    fig, ax = plt.subplots(figsize=(9.5, 3.8))
    values = np.array([s['channels'][n]['character']['band_error_db'] for n in NAMES])
    lim = max(1., float(np.max(np.abs(values))))
    im = ax.imshow(values, cmap='RdBu_r', vmin=-lim, vmax=lim, aspect='auto')
    ax.set_xticks(range(5), ['20–120 Hz', '120–400 Hz', '400 Hz–2 kHz', '2–6 kHz', '6–20 kHz'])
    ax.set_yticks(range(5), NAMES)
    for i in range(5):
        for j in range(5):
            ax.text(j, i, f'{values[i,j]:+.2f}', ha='center', va='center',
                    color='white' if abs(values[i,j]) > .63*lim else '#182e43', fontsize=10)
    ax.set_title('Preselected candidates: broad-band power error versus native', loc='left', pad=16)
    fig.colorbar(im, ax=ax, label='Power error, dB', pad=.025)
    fig.text(.14, .03, 'Numerical character proxy only; no perceptual pass threshold or real-instrument listening verdict.', fontsize=9)
    fig.subplots_adjust(left=.14, right=.95, top=.84, bottom=.18)
    fig.savefig(ROOT / 'character-bands.png', dpi=165)
    plt.close(fig)
    print(json.dumps({k: summary[k] for k in ('means', 'selected_new_mean_esr_reduction_percent',
                       'selected_numerical_pass_count', 'selected_engine_pass_count')}, indent=2))


if __name__ == '__main__':
    main()
