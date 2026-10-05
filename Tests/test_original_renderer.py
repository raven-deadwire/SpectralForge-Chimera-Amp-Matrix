#!/usr/bin/env python3
"""Protect comparison provenance: correct state, finite raw output, no stale data."""
import json
import math
import struct
import subprocess
import sys
import tempfile
import wave
from pathlib import Path


def main():
    executable = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory() as directory:
        root = Path(directory)
        source, state, output = (root / n for n in ['input.wav', 'state.json', 'output.f32'])
        count = 24000
        with wave.open(str(source), 'wb') as writer:
            writer.setparams((1, 2, 48000, count, 'NONE', 'not compressed'))
            writer.writeframes(b''.join(struct.pack('<h', round(2000 * math.sin(2 * math.pi * 400 * n / 48000)))
                                       for n in range(count)))

        def render(config, expect=0):
            state.write_text(json.dumps(config))
            result = subprocess.run([str(executable), str(source), str(output), str(state)],
                                    capture_output=True, text=True)
            assert result.returncode == expect, result.stderr
            return output.read_bytes()

        default = render({})
        assert len(default) == count * 4
        assert all(math.isfinite(v) for v in struct.unpack('<'+'f'*count, default))
        fenrir = render({'preset': 'original.nastrond.fenrir.v1'})
        assert default != fenrir, 'Preset selection did not change the sound'
        output.write_bytes(fenrir + b'stale data')
        assert render({}) == default, 'Output did not replace previous render exactly'
        assert render({'controls': {'gain': .1}}) != default, 'Gain override did not change the sound'
        channels = [render({'channel': channel}) for channel in range(5)]
        assert len(set(channels)) == 5, 'Channel selector does not change the DSP'
        assert render({'channel': 0, 'controls': {'gain': 1}}) != channels[0], 'No gain travel above channel midpoint'
        last = output.read_bytes()
        for config, code in [({'preset': 'missing'}, 4), ({'controls': {'typo': .5}}, 5),
                             ({'controls': {'gain': 2}}, 5), ({'channel': 5}, 4), ({'channel': -1}, 4)]:
            assert render(config, code) == last, 'Invalid state modified previous output'
    print('PASS original renderer preset/gain routing, deterministic replacement, finite float output and invalid-state rejection')


if __name__ == '__main__':
    main()
