#!/usr/bin/env python3
"""Frozen-LFO response of the #192 transfer, including fractional delay interpolation.
This is an analytical supplement, NOT a render or a bound on arbitrary transient peaks.
The production C++ renderer measures the time-varying path separately.
"""
import argparse
import csv
import numpy as np


def run(path):
    frequencies = np.unique(np.r_[np.geomspace(20, 16000, 4096), 27.5, 55, 110, 160, 200])
    phases = np.arange(64) * (2 * np.pi / 64)
    m = np.float32(.35)
    g = np.float32(1) / np.sqrt((1-m)*(1-m)+m*m)
    a, b = float(g*(1-m)-1), float(g*m)
    with open(path, 'w', newline='') as stream:
        writer = csv.writer(stream, lineterminator="\n")
        writer.writerow(('rate', 'spread_ms', 'channel', 'region', 'minimum_db', 'min_hz', 'maximum_db', 'max_hz'))
        for rate in (44100, 48000, 96000):
            w = 2*np.pi*200/rate
            alpha, c = np.sin(w)/np.sqrt(2), np.cos(w)
            z = np.exp(-2j*np.pi*frequencies/rate)
            h = ((1+c)/2-(1+c)*z+(1+c)/2*z*z)/((1+alpha)-2*c*z+(1-alpha)*z*z)
            for spread in np.arange(1, 20.001, .25):
                response = []
                for channel, factor in enumerate((.55, 1.)):
                    # JUCE pop-before-push uses the requested historical delay.
                    delay = np.maximum(.2, spread*factor+.25*np.sin(phases+channel*1.7))*rate*.001
                    integer = np.floor(delay)
                    fraction = delay-integer
                    d = np.exp(-2j*np.pi*frequencies[None, :]*integer[:, None]/rate)*((1-fraction[:, None])+fraction[:, None]*z)
                    response.append(1+h*(a+b*d))
                response.append((response[0]+response[1])/2)
                for channel, transfer in zip(('L', 'R', 'mono'), response):
                    levels = 20*np.log10(np.maximum(1e-15, np.abs(transfer)))
                    for region, mask in (('20-16000', frequencies > 0), ('40-160', (frequencies >= 40) & (frequencies <= 160)), ('A0', frequencies == 27.5), ('A1', frequencies == 55)):
                        values = levels[:, mask]
                        low, high = np.unravel_index(values.argmin(), values.shape), np.unravel_index(values.argmax(), values.shape)
                        f = frequencies[mask]
                        writer.writerow((rate, spread, channel, region, values[low], f[low[1]], values[high], f[high[1]]))


if __name__ == '__main__':
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('output')
    run(p.parse_args().output)
