#!/usr/bin/env python3
"""Validate production-render provenance, float headroom and strict failure paths.

Synthetic WAV fixtures are explicitly declared synthetic. These tests do not
claim that a channel sounds like a reference head or meets musical acceptance.
"""
import hashlib
import json
import math
import struct
import subprocess
import sys
import tempfile
import wave
from pathlib import Path


def pcm(path, channels=1, frames=4321, rate=48000):
    samples = bytearray()
    for n in range(frames):
        t = n / rate
        x = 2500 * (math.sin(2 * math.pi * 30.867706 * t)
                    + .35 * math.sin(2 * math.pi * 191 * t)
                    + .15 * math.sin(2 * math.pi * 1703 * t))
        samples.extend(struct.pack('<h', round(x)))
        for _ in range(channels - 1):
            samples.extend(struct.pack('<h', 0))
    with wave.open(str(path), 'wb') as output:
        output.setparams((channels, 2, rate, frames, 'NONE', 'not compressed'))
        output.writeframes(samples)


def floats(path):
    raw = path.read_bytes()
    assert raw[:4] == b'RIFF' and raw[8:12] == b'WAVE'
    assert struct.unpack_from('<I', raw, 4)[0] == len(raw) - 8
    position, audio, fmt, fact = 12, None, None, None
    while position < len(raw):
        chunk, length = struct.unpack_from('<4sI', raw, position)
        content = raw[position + 8:position + 8 + length]
        if chunk == b'fmt ':
            fmt = struct.unpack_from('<HHIIHH', content)
        elif chunk == b'data':
            audio = struct.unpack('<' + 'f' * (length // 4), content)
        elif chunk == b'fact':
            fact = struct.unpack_from('<I', content)[0]
        position += 8 + length + (length % 2)
    assert position == len(raw) and fmt and audio is not None
    assert fmt[0] == 3 and fmt[-1] == 32, 'Output is not IEEE float32 WAV'
    assert fact == len(audio) // fmt[1]
    assert all(math.isfinite(v) for v in audio)
    return fmt, audio


def verify(directory, source, channel_count, frames, factor=4):
    manifest = json.loads((directory / 'manifest.json').read_text())
    assert manifest['schema'] == 'spectralforge.niflheimr.head-render.v1'
    assert manifest['release_approved'] is False
    head = manifest['configured_git_head']
    assert head in ('', 'source-archive') or (len(head) == 40 and all(c in '0123456789abcdef' for c in head))
    for name in ['NiflheimrDSP.h', 'NiflheimrDefinition.h', 'Amplifier.h']:
        digest = manifest['source_hashes'][name]
        assert len(digest) == 64 and all(c in '0123456789abcdef' for c in digest)
        current_source = Path(__file__).resolve().parent.parent / 'Source' / name
        assert digest == hashlib.sha256(current_source.read_bytes()).hexdigest(), 'Renderer is stale relative to source'
    assert 'local modifications' in manifest['source_identity_note']
    assert manifest['input_sha256'] == hashlib.sha256(source.read_bytes()).hexdigest()
    assert manifest['input_frames'] == 4321 and manifest['output_frames'] == frames
    assert manifest['audio_channels'] == channel_count
    assert manifest['oversampling_factor'] == factor
    assert manifest['same_input_all_channels'] is True
    assert manifest['latency_compensated'] is False
    assert 'synthetic' in manifest['input_kind']
    assert manifest['musical_acceptance'].startswith('PENDING')
    assert 'head-only' in manifest['scope'] and 'no normalization' in manifest['scope']
    assert len(manifest['outputs']) == 5
    data = []
    for index, record in enumerate(manifest['outputs']):
        assert record['channel_index'] == index
        assert len(record['controls']) == 14
        output = directory / record['file']
        assert record['sha256'] == hashlib.sha256(output.read_bytes()).hexdigest()
        fmt, audio = floats(output)
        assert fmt[1] == channel_count and fmt[2] == 48000
        assert len(audio) == frames * channel_count
        assert abs(record['peak'] - max(abs(v) for v in audio)) < 1e-6
        assert record['samples_over_unity'] == sum(abs(v) > 1 for v in audio)
        assert 0 <= record['latency_samples'] < 64
        data.append(audio)
    assert len(list(directory.iterdir())) == 6, 'Unexpected partial or stale output files'
    return manifest, data


def main():
    executable = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory() as temporary:
        root = Path(temporary)
        mono, stereo, control = root / 'mono.wav', root / 'stereo.wav', root / 'controls.json'
        pcm(mono)
        pcm(stereo, channels=2)

        def run(source, output, *options, success=True):
            process = subprocess.run([str(executable), str(source), str(output),
                                      '--tail-seconds', '.02', '--input-kind', 'synthetic', *options],
                                     capture_output=True, text=True, timeout=120)
            assert (process.returncode == 0) == success, process.stdout + process.stderr
            return process

        baseline = root / 'baseline'
        run(mono, baseline)
        _, audio = verify(baseline, mono, 1, 5281)
        assert len({struct.pack('<' + 'f' * len(a), *a) for a in audio}) == 5, 'Selector did not reach five DSP channels'
        alternate = root / 'alternate'
        run(mono, alternate, '--block-size', '733')
        _, repartitioned = verify(alternate, mono, 1, 5281)
        assert max(abs(x - y) for a, b in zip(audio, repartitioned) for x, y in zip(a, b)) < 2e-6

        stereo_output = root / 'stereo-output'
        run(stereo, stereo_output, '--oversampling', '1')
        _, stereo_audio = verify(stereo_output, stereo, 2, 5281, factor=1)
        assert max(abs(v) for data in stereo_audio for v in data[1::2]) < 1e-8, 'Stereo channel crosstalk'

        clean_output = root / 'clean-output'
        control.write_text(json.dumps({'controls': {'blend': 0, 'gain': 1, 'mass': 1,
                                                  'master': 1, 'bass': 1, 'depth': 1, 'middle': 1}}))
        run(mono, clean_output, '--controls', str(control))
        _, clean_audio = verify(clean_output, mono, 1, 5281)
        assert max(abs(x - y) for data in clean_audio[1:] for x, y in zip(clean_audio[0], data)) < 2e-6, 'Clean endpoint depends on dirty channel'
        assert max(abs(v) for v in clean_audio[0]) > 1, 'Float WAV output silently lost above-unity headroom'

        before = (baseline / 'manifest.json').read_bytes()
        run(mono, baseline, success=False)
        assert (baseline / 'manifest.json').read_bytes() == before, 'Existing render was modified'
        cases = [('--oversampling', '3'), ('--block-size', '0'), ('--block-size', '2.5'),
                 ('--tail-seconds', 'nan'), ('--bogus', '1'), ('--input-kind', 'real-di'),
                 ('--benchmark-repeats', '-1'), ('--benchmark-blocks', 'nan'),
                 ('--benchmark-warmup-blocks', '0'), ('--benchmark-repeats', '1.5')]
        for index, options in enumerate(cases):
            destination = root / f'reject-option-{index}'
            run(mono, destination, *options, success=False)
            assert not destination.exists()
        for index, value in enumerate([{'controls': {'unknown': .5}}, {'controls': {'gain': 2}},
                                       {'controls': {'gain': True}}, {'controls': []}, {'channel': 3}, []]):
            destination = root / f'reject-control-{index}'
            control.write_text(json.dumps(value))
            run(mono, destination, '--controls', str(control), success=False)
            assert not destination.exists()

        invalid_audio = root / 'invalid-audio.wav'
        invalid_audio.write_bytes(b'not a wav')
        run(invalid_audio, root / 'reject-audio', success=False)
        # An IEEE float WAV with NaN must fail without publishing partial output.
        broken = bytearray((baseline / json.loads(before)['outputs'][0]['file']).read_bytes())
        data_start = broken.index(b'data') + 8
        struct.pack_into('<f', broken, data_start, math.nan)
        invalid_audio.write_bytes(broken)
        run(invalid_audio, root / 'reject-nan', success=False)
        assert not (root / 'reject-nan').exists()
        assert not list(root.glob('*.partial-*')), 'Failure left a partial render directory'
    print('PASS Niflheimr production renderer: five channels, mono/stereo, float WAV, source/output hashes, '
          'block invariance, clean endpoint and strict rejection. Musical acceptance remains pending.')


if __name__ == '__main__':
    main()
