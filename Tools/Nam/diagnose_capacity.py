#!/usr/bin/env python3
"""Diagnostic only: does WaveNet width limit accuracy on heavily driven notes?

Trains one channel's WaveNet with the A2 layer layout at a chosen width
(channels per layer; TONE3000's A2 player accepts at most 8) from scratch,
with everything else fixed, and reports full-validation ESR split by note
input level. Widths above 8 are not A2 and cannot load in the target player.
Nothing here is exported for use.
"""
import argparse, copy, json, time
from pathlib import Path
import numpy as np
import torch
from nam.models.wavenet import WaveNet
from common import RATE, write_json
from quality_profile import fidelity


def note_table(seed, seconds):
    """Replay common.signal's random draws to recover each note's position and level."""
    rng = np.random.default_rng(seed); n = round(seconds*RATE); pos = 0; out = []
    while pos < n:
        length = min(n-pos, int(RATE*rng.uniform(.18, 1.1)))
        f = rng.choice([27.5, 30.8677, 41.2034, 55., 65.406, 82.407, 110., 146.832, 220., 329.628])
        for _ in range(1, min(70, int(9000/f))):
            rng.uniform(-.4, .4); rng.uniform(1, 5); rng.uniform(1, 1.7)
        rng.normal(0, .12, length); level = rng.choice([-30., -24., -18., -12., -6., -3.])
        out.append((pos, length, float(f), float(level))); pos += length+int(RATE*rng.uniform(.015, .10))
    return out


def centered(model, x, rf):
    xx = np.pad(x, (rf-1, 0)); out = []
    with torch.inference_mode():
        dc = float(model(torch.zeros(1, rf+1), pad_start=False)[0, -1])
        for i in range(0, len(x), 16384):
            n = min(16384, len(x)-i)
            out.append(model(torch.from_numpy(xx[i:i+rf-1+n].copy())[None], pad_start=False).numpy()[0])
    return np.concatenate(out)-dc


def evaluate(model, rf, vx, vy, notes, channel, gain):
    p = centered(model, vx, rf)/gain
    f = fidelity(p, vy, channel, vx)
    n = len(vx); a, b = n//3, n//2
    rows = [(lev, np.sum((p[s:s+L]-vy[s:s+L])**2)/np.sum(vy[s:s+L]**2))
            for s, L, _, lev in notes if s >= rf and not a <= s < b and s+L <= n-24000]
    rows = np.array(rows); loud = rows[:, 0] >= -6
    return dict(full_esr=f['full_esr'], active_median=f['active_median'], active_p95=f['active_p95'],
                loud_note_esr=float(rows[loud, 1].mean()), quieter_note_esr=float(rows[~loud, 1].mean()),
                peak_error_db=f['peak_error_db'])


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--data', type=Path, required=True); ap.add_argument('--out', type=Path, required=True)
    ap.add_argument('--trainer', type=Path, required=True); ap.add_argument('--channel', default='Fenrir')
    ap.add_argument('--width', type=int, required=True); ap.add_argument('--steps', type=int, default=25000)
    ap.add_argument('--lr', type=float, default=.004); ap.add_argument('--lr-half-life', type=float, default=6000)
    ap.add_argument('--batch', type=int, default=4); ap.add_argument('--frames', type=int, default=4096)
    ap.add_argument('--threads', type=int, default=1); ap.add_argument('--eval-every', type=int, default=2500)
    args = ap.parse_args()
    torch.set_num_threads(args.threads); torch.manual_seed(6405); rng = np.random.default_rng(6405)
    template = json.loads((args.trainer/'nam/train/_resources/config_model_packed.json').read_text())['net']['config']['submodels'][-1]['config']
    cfg = copy.deepcopy(template); cfg['layers_configs'][0]['channels'] = args.width
    model = WaveNet.init_from_config(cfg); rf = model.receptive_field
    x = np.load(args.data/'train/input.npy'); y = np.load(args.data/'train'/f'{args.channel}.npy')
    vx = np.load(args.data/'validation/input.npy'); vy = np.load(args.data/'validation'/f'{args.channel}.npy')
    notes = note_table(640500+1703, len(vx)/RATE)
    gain = .1; opt = torch.optim.Adam(model.parameters(), lr=args.lr, weight_decay=3.17e-7)
    args.out.mkdir(parents=True, exist_ok=True)
    report = dict(purpose=__doc__.split('\n')[0], channel=args.channel, width=args.width,
                  parameters=sum(p.numel() for p in model.parameters()), receptive_field=rf,
                  is_a2_compatible=args.width in (3, 8), steps=args.steps, lr=args.lr,
                  lr_half_life=args.lr_half_life, batch=args.batch, frames=args.frames, seed=6405,
                  loss='window-normalized MSE + .1 DC on zero-input-corrected output, tail fraction .1', evaluations=[])
    t = time.monotonic(); losses = []
    for step in range(1, args.steps+1):
        st = rng.integers(rf, len(x)-args.frames, args.batch)
        for j in range(args.batch):
            if rng.random() < .1: st[j] = rng.integers(max(rf, len(x)-round(.8*RATE)), len(x)-args.frames)
        xx = torch.from_numpy(np.stack([x[s-rf+1:s+args.frames] for s in st]))
        yy = torch.from_numpy(np.stack([y[s:s+args.frames] for s in st]))*gain
        model.train()
        pred = model(xx, pad_start=False)-model(torch.zeros(1, rf), pad_start=False)[:, -1:]
        power = yy.square().mean(dim=-1).clamp_min(.0001)
        loss = ((pred-yy).square().mean(dim=-1)/power).mean()+.1*((pred.mean(-1)-yy.mean(-1)).square()/power).mean()
        opt.zero_grad(); loss.backward(); torch.nn.utils.clip_grad_norm_(model.parameters(), 5.); opt.step()
        for g in opt.param_groups: g['lr'] = args.lr*.5**(step/args.lr_half_life)
        losses.append(float(loss))
        if step % args.eval_every == 0 or step == args.steps:
            model.eval(); row = evaluate(model, rf, vx, vy, notes, args.channel, gain)
            row.update(step=step, train_loss=float(np.mean(losses[-500:])), elapsed_seconds=round(time.monotonic()-t, 1))
            report['evaluations'].append(row); write_json(args.out/'capacity.json', report)
            print(json.dumps(row), flush=True)
    best = min(report['evaluations'], key=lambda r: r['full_esr'])
    report['best'] = best; write_json(args.out/'capacity.json', report)


if __name__ == '__main__':
    main()
