#!/usr/bin/env python3
"""Measure how long the native Náströnd channels remember their input.

For a model that sees only the last B input samples, two inputs that agree
over those B samples must produce the same output. The native DSP may not:
anything it remembers for longer (supply sag, bias recovery, slow envelopes)
makes its output differ. This renders the same test notes after different
earlier playing, with an identical bridge of B samples before them, and
reports the output difference as an ESR. That difference is error no model
with memory B can avoid, whatever its size or training.
"""
import argparse, json, subprocess, tempfile
from pathlib import Path
import numpy as np
from common import RATE, signal, write_json
from capture_channels import VOICES

TEST_SEED, BRIDGE_SEED = 640500+1703, 7202611
HISTORY_SEEDS = [7202601, 7202602, 7202603, 7202604]


def render(renderer, x, channel, folder):
    inp, out, job = folder/'in.f32', folder/'out.f32', folder/'job.json'
    x.astype('<f4').tofile(inp); write_json(job, {'channel': channel})
    subprocess.check_output([str(renderer), str(inp), str(out), str(job)])
    return np.fromfile(out, dtype='<f4').astype(np.float64)


def loud_test_notes():
    """The first four validation notes played at -6 or -3 dBFS peak, each with its decay."""
    from diagnose_capacity import note_table
    x = signal(TEST_SEED, 24); picked = []
    for s, L, _, level in note_table(TEST_SEED, 24):
        if level >= -6 and s+L < len(x)//3:
            picked.append(x[s:s+L].astype(np.float64))
        if len(picked) == 4: break
    return picked


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--renderer', type=Path, required=True); ap.add_argument('--out', type=Path, required=True)
    ap.add_argument('--bridges', default='6347,32152,96000,240000', help='Bridge lengths in samples')
    args = ap.parse_args()
    bridges = [int(b) for b in args.bridges.split(',')]
    notes = loud_test_notes(); test = np.concatenate(notes)
    # common.signal ends with 0.5 s of silence; drop it so every bridge ends in playing.
    bridge_source = signal(BRIDGE_SEED, max(bridges)/RATE+1.5).astype(np.float64)[:-RATE//2]
    assert np.any(bridge_source[-RATE//20:] != 0), 'Bridge must end with playing'
    histories = [signal(s, 6).astype(np.float64) for s in HISTORY_SEEDS]
    report = {'purpose': __doc__.split('\n')[0], 'bridges_samples': bridges,
              'history_seeds': HISTORY_SEEDS, 'test': '4 validation notes at -6/-3 dBFS peak',
              'test_samples': len(test), 'channels': {}}
    with tempfile.TemporaryDirectory() as tmp:
        folder = Path(tmp)
        for channel, (name, _) in enumerate(VOICES):
            rows = {}
            for b in bridges:
                # The bridge is the tail of one fixed playing signal, so it ends with
                # ordinary playing, not silence that would reset the amplifier.
                bridge = bridge_source[-b:]
                outputs = [render(args.renderer, np.concatenate([h, bridge, test]), channel, folder)[-len(test):]
                           for h in histories]
                ref = np.mean(outputs, axis=0)
                esr = [float(np.sum((o-ref)**2)/np.sum(ref**2)) for o in outputs]
                pair = [float(np.sum((outputs[i]-outputs[j])**2)/np.sum(outputs[j]**2))
                        for i in range(len(outputs)) for j in range(i+1, len(outputs))]
                rows[str(b)] = {'bridge_ms': round(1000*b/RATE, 1), 'esr_to_mean_max': max(esr),
                                'pairwise_esr_max': max(pair), 'pairwise_esr_mean': float(np.mean(pair))}
                print(name, b, rows[str(b)], flush=True)
            report['channels'][name] = rows
    args.out.parent.mkdir(parents=True, exist_ok=True)
    write_json(args.out, report)


if __name__ == '__main__':
    main()
