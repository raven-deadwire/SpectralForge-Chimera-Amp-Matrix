#!/usr/bin/env python3
"""Check a reported Transpose latency against when notes actually arrive.

A host compensates a plug-in's reported latency L by shifting its output
earlier by L. For a pitch shifter that is only right if new notes arrive
L after the input. This measures, per note event, the arrival delay minus
the delay the engine reports (ChimeraTransposeRender prints it; -1 means
none, scored against REFERENCE_MS):

  picked   attacks of the protocol material, harder material and palm-muted
           sixteenths: low-band (below 150 Hz) first-crossing delay
           (transpose_synthetic_evidence.onset_latency_ms) per pick
  legato   a sustained E2 / A2 note that changes pitch with no new pick:
           pure (phase-continuous, no level change) and hammer-on (+6 dB,
           2 ms bump). Arrival = when the new note's level first reaches half
           of its level 60-150 ms later: summed centred 120 ms Hann
           demodulation of harmonics
           1-10 (x ratio) that are > 18 Hz from every harmonic of the other
           note (a semitone at the 2nd harmonic is only ~10 Hz apart, and a
           new note's 8th harmonic can sit on the old note's 7th), output
           minus input. The centred window biases input and output alike.

Reports each event's error re the reported latency. For legato and hammer-on
events error_ms is the instantaneous-frequency arrival (frequency_arrivals) minus that of an
ideal reference: the material resynthesised at f0 x ratio and delayed by
exactly the reported latency. On ideal outputs delayed by 10 / 20 / 30 ms it
returns 0 / +10 / +20 ms exactly (144 of 144 events). level_error_ms keeps the
120 ms level metric against the same reference for comparison; it misreads a
read-head jump's splatter as the new note (hammer-ons: down to -34 ms where
the period shows +2.4 ms). irregular_cycle_ms: the jump's long cycle, if any.
For the research hybrid, read_head_error_ms is the ground truth for the low
band: when the engine's own read position t - D(t) reaches the change, minus
the reported latency. Around re-anchor jumps the signal metrics (error_ms and
level_error_ms) disagree with it by up to +-35 ms, so they are only trusted
where no re-anchor happens.
Synthetic only.

  python Tools/transpose_latency_contract.py --render build/.../ChimeraTransposeRender --out DIR
"""
import argparse, json, sys, tempfile
from pathlib import Path
import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parent))
import transpose_synthetic_evidence as ev
import transpose_realistic_probe as rp

SHIFTS = [-12, -7, -5, -2, 2, 5, 7, 12]
MODES = [(2, "TIGHT"), (37, "AL10RA-800"), (39, "AL10RA-800-L10"), (40, "AL10RA-800-L10-LEG")]
REFERENCE_MS = 10.0
PICKED = ['bass_B0', 'bass_E1', 'guitar_E2', 'lead_A3', 'power_chord_E2', 'bass_E1_repick', 'guitar_E2_repick']
LEGATO_AT = [1.0, 1.6, 2.2]   # seconds after the lead-in

def legato(f0, steps, rate, hammer):
    """One string note changing pitch at LEGATO_AT by `steps` semitones each."""
    t = np.arange(int(rate * 3.0)) / rate
    cents = np.zeros_like(t)
    for k, at in enumerate(LEGATO_AT):
        cents[t >= at] = 100 * sum(steps[:k + 1])
    x, _ = rp.string(f0, rate, 1e-4, seconds=3.0, decay=4.0, cents=cents)
    if hammer:
        for at in LEGATO_AT:
            i = int(at * rate); n = int(0.002 * rate)
            env = np.ones_like(t); env[i:i + 4 * n] += np.concatenate([np.linspace(0, 1, n), np.linspace(1, 0, 3 * n)])
            x *= env
    return rp.normalise(rp.lead_in(x)), [f0 * 2 ** (sum(steps[:k]) / 12) for k in range(len(steps) + 1)]

def demod(y, f, rate, window_s=0.12):
    t = np.arange(len(y)) / rate
    z = np.asarray(y, float) * np.exp(-2j * np.pi * f * t)
    w = np.hanning(int(window_s * rate)); w /= w.sum()
    return 2 * np.abs(np.convolve(z, w, mode='same'))

def note_level(y, f, other, ratio, rate):
    """Summed demodulated level of harmonics 1-10 of f (x ratio) that are more
    than 18 Hz from every harmonic of `other` at the output (x ratio): the
    120 ms Hann main lobe is +-17 Hz, so closer pairs leak into each other.
    The spacing scales with the ratio, so a downshift keeps fewer harmonics."""
    total = 0
    for k in range(1, 11):
        if min(abs(k * f - j * other) for j in range(1, 16)) * ratio > 18:
            total = total + demod(y, k * f * ratio, rate)
    return total

def crossings(y, notes, ratio, rate):
    """Times (s) at which note k+1's level first reaches half of its level
    60-150 ms after the change, near each change. Only the new note's level
    is used: a re-anchoring read head skips part of the old note, which moves
    an old/new balance point earlier without the new note arriving earlier."""
    out = []
    for k, at in enumerate(LEGATO_AT):
        new = note_level(y, notes[k + 1], notes[k], ratio, rate)
        if np.isscalar(new): out.append(None); continue
        a, b = int((ev.PRE_S + at - 0.08) * rate), int((ev.PRE_S + at + 0.15) * rate)
        ref = np.median(new[int((ev.PRE_S + at + 0.06) * rate):int((ev.PRE_S + at + 0.15) * rate)])
        idx = np.nonzero(new[a:b] > 0.5 * ref)[0]
        out.append((a + idx[0]) / rate if len(idx) and idx[0] > 0 else None)
    return out

def lowpass(x, hz, rate):
    w = 2 * np.pi * hz / rate; c, alpha = np.cos(w), np.sin(w) / np.sqrt(2)
    b = [(1 - c) / 2 / (1 + alpha), (1 - c) / (1 + alpha), (1 - c) / 2 / (1 + alpha)]
    a = [1.0, -2 * c / (1 + alpha), (1 - alpha) / (1 + alpha)]
    import transpose_measure as tm
    return tm._lfilter(b, a, tm._lfilter(b, a, np.asarray(x, float)))

def frequency_arrivals(y, notes, ratio, rate):
    """Times (s) at which the fundamental's instantaneous frequency first passes
    the midpoint between the old and new note (x ratio) and stays past it for
    10 ms, near each change. The fundamental is isolated with a zero-phase FFT
    band-pass (old/new fundamental -20 % .. +25 %), so the estimate is
    independent of waveform phase and level (a hammer-on's bump) and the
    filter's time spread is symmetric. Zero-crossing periods were quantised to
    a cycle (12-22 ms) and, refined, still off by -15.6 .. +11.0 ms when the
    waveform phase differed from the reference."""
    out = []
    y = np.asarray(y, float)
    for k, at in enumerate(LEGATO_AT):
        f_old, f_new = notes[k] * ratio, notes[k + 1] * ratio
        c = int((ev.PRE_S + at) * rate)
        a, b = max(0, c - int(0.4 * rate)), min(len(y), c + int(0.4 * rate))
        seg = y[a:b] * np.hanning(b - a)
        spec = np.fft.fft(seg); f = np.fft.fftfreq(len(seg), 1 / rate)
        keep = (f >= 0.8 * min(f_old, f_new)) & (f <= 1.25 * max(f_old, f_new))
        analytic = np.fft.ifft(np.where(keep, 2 * spec, 0))
        inst = np.diff(np.unwrap(np.angle(analytic))) * rate / (2 * np.pi)
        w = np.hanning(int(0.005 * rate)); w /= w.sum()
        inst = np.convolve(inst, w, mode='same')
        mid, hold = 0.5 * (f_old + f_new), int(0.010 * rate)
        past = (inst > mid) if f_new > f_old else (inst < mid)
        lo, hi = c - a - int(0.05 * rate), c - a + int(0.2 * rate)
        hit = None
        for i in np.nonzero(past[lo:hi])[0] + lo:
            if past[i:i + hold].all(): hit = (a + i) / rate; break
        out.append(hit)
    return out

def irregular_cycles(y, notes, ratio, rate):
    """Longest fundamental cycle within 40 ms of each change's arrival region
    that exceeds 1.3 x the longer of the two periods (ms), or None: a re-anchor
    jump in mid-cycle leaves one such cycle."""
    out = []
    for k, at in enumerate(LEGATO_AT):
        longest = max(1.0 / (notes[k] * ratio), 1.0 / (notes[k + 1] * ratio))
        s = lowpass(y, 1.4 * max(notes[k], notes[k + 1]) * ratio, rate)
        a, b = int((ev.PRE_S + at - 0.02) * rate), int((ev.PRE_S + at + 0.12) * rate)
        seg = s[a:b]
        z = np.nonzero((seg[:-1] < 0) & (seg[1:] >= 0))[0]
        p = np.diff(z) / rate
        bad = p[p > 1.3 * longest]
        out.append(round(1000 * float(bad.max()), 2) if len(bad) else None)
    return out

def render_with_delays(probe, rate, block, mode, shift, x, tmp):
    """Render, and for the research hybrid also read back the low band's read
    delay per sample and the crossover's group delay (samples); else None."""
    import os, subprocess
    src, dst, dd = (os.path.join(tmp, n) for n in ('in.f32', 'out.f32', 'delay.f32'))
    np.asarray(x, dtype='<f4').tofile(src)
    p = subprocess.run([str(probe), str(rate), str(block), str(mode), str(shift), src, dst, f'delays={dd}'],
                       check=True, capture_output=True, text=True)
    y, latency = np.fromfile(dst, dtype='<f4'), int(p.stdout.strip())
    if 'crossover_delay_samples=' not in p.stderr or not os.path.exists(dd): return y, latency, None, None
    tau = float(p.stderr.split('crossover_delay_samples=')[1].split()[0])
    delays = np.fromfile(dd, dtype='<f4').astype(float)
    return y, latency, (delays if len(delays) == len(y) else None), tau

def read_head_arrivals(delays, tau, rate):
    """Output times (s) at which the low band's read position, t - D(t), first
    reaches each change in the low-band input (change + tau). Ground truth from
    the engine itself; signal metrics around a re-anchor jump were off by up to
    +-35 ms against it."""
    if delays is None: return [None] * len(LEGATO_AT)
    n = np.arange(len(delays)); read = n - delays
    out = []
    for at in LEGATO_AT:
        c = (ev.PRE_S + at) * rate
        idx = np.nonzero((n >= c) & (read >= c + tau))[0]
        out.append(idx[0] / rate if len(idx) else None)
    return out

def run(probe, out, rate=48000, block=128, modes=MODES, shifts=SHIFTS):
    rows = []
    mat = ev.material(rate)
    with tempfile.TemporaryDirectory(prefix='transpose-contract-') as tmp:
        picked = {n: (mat[n][0], ev.events(n)) for n in PICKED}
        x, _, picks = rp.fixed_materials(rate)['E2_palm16']
        picked['E2_palm16'] = (x, picks)
        for name, (x, picks) in picked.items():
            lx = ev.lowpass_lr4(x, rate)
            for shift in shifts:
                for mode, label in modes:
                    y, latency = ev.render(probe, rate, block, mode, shift, x, tmp)
                    reported = 1000 * latency / rate if latency >= 0 else REFERENCE_MS
                    ly = ev.lowpass_lr4(y, rate)
                    for p in picks:
                        d = ev.onset_latency_ms(lx, ly, rate, [p])
                        rows.append({'kind': 'picked', 'material': name, 'shift_semitones': shift, 'mode': label,
                                     'event_s': round(p, 3), 'reported_ms': round(reported, 3),
                                     'arrival_ms': d, 'error_ms': None if d is None else round(d - reported, 2)})
        for name, f0, steps in [('E2_legato_up', 82.41, [2, 1, 2]), ('E2_legato_down', 82.41, [-1, -2, -2]),
                                ('A2_legato_up', 110.0, [2, 2, 1])]:
            for hammer in (False, True):
                x, notes = legato(f0, steps, rate, hammer)
                cin = crossings(x, notes, 1.0, rate)
                for shift in shifts:
                    ratio = 2 ** (shift / 12)
                    for mode, label in modes:
                        y, latency, delays, tau = render_with_delays(probe, rate, block, mode, shift, x, tmp)
                        reported = 1000 * latency / rate if latency >= 0 else REFERENCE_MS
                        cout = crossings(y, notes, ratio, rate)
                        heads = read_head_arrivals(delays, tau, rate)
                        # Reference: the same material resynthesised at f0 x ratio and
                        # delayed by exactly the reported latency, scored with the same
                        # metric. The metric's own bias (window, harmonic choice, a
                        # hammer-on's level bump) then cancels in error_ms.
                        ideal, _ = legato(f0 * ratio, steps, rate, hammer)
                        lag = int(round(reported * rate / 1000))
                        ideal = np.concatenate([np.zeros(lag), ideal[:len(ideal) - lag]])
                        cref = crossings(ideal, notes, ratio, rate)
                        pout, pref = frequency_arrivals(y, notes, ratio, rate), frequency_arrivals(ideal, notes, ratio, rate)
                        jumps = irregular_cycles(y, notes, ratio, rate)
                        for at, i, o, r, po, pr, jump, head in zip(LEGATO_AT, cin, cout, cref, pout, pref, jumps, heads):
                            d = None if i is None or o is None else round(1000 * (o - i), 2)
                            rows.append({'kind': 'hammer' if hammer else 'legato', 'material': name, 'shift_semitones': shift,
                                         'mode': label, 'event_s': at, 'reported_ms': round(reported, 3),
                                         'arrival_ms': d,
                                         'error_ms': None if po is None or pr is None else round(1000 * (po - pr), 2),
                                         'level_error_ms': None if o is None or r is None else round(1000 * (o - r), 2),
                                         'irregular_cycle_ms': jump,
                                         'read_head_error_ms': None if head is None else
                                             round(1000 * head - 1000 * (ev.PRE_S + at) - reported, 2)})
    out = Path(out); out.mkdir(parents=True, exist_ok=True)
    (out / 'contract.json').write_text(json.dumps(rows, indent=1) + '\n', encoding='utf-8')
    return rows

def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--render', required=True, help='built ChimeraTransposeRender')
    ap.add_argument('--out', required=True)
    args = ap.parse_args(argv)
    rows = run(args.render, args.out)
    print(f'{len(rows)} events -> {Path(args.out) / "contract.json"}')
    return 0

if __name__ == '__main__':
    sys.exit(main())
