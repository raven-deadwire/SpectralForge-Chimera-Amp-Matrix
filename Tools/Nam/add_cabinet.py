#!/usr/bin/env python3
"""Turn a head capture into a head + cab capture with each channel's preset cabinet.

The factory cabinets are linear and time-invariant (CabExportTests null the
offline render against the live lane below -75 dB), so the native head + cab
output is the head output convolved with the lane's exported cabinet IR.
Each channel uses the cabinet of the factory rig lane that voices it; the
mapping is recorded in the manifest. Inputs are hard-linked, not copied.
With --consume each head output is deleted once its head + cab version is
written, so a full set fits on a small disk; the head set can be recaptured.
"""
import argparse, copy, hashlib, json, os, wave
from pathlib import Path
import numpy as np
from scipy.signal import oaconvolve
from capture_channels import AMPS, amp_of
from common import digest, write_json
from training_contract import verify_audio_manifest

# channel -> (factory rig preset, lane): the rig lane whose cabinet voices it.
CABINETS = {
    'nastrond': {'Fenrir': (0, 0), 'Surtr': (1, 0), 'Nidhoggr': (2, 0), 'Fimbulvetr': (3, 0), 'Ragnarok': (2, 1)},
    'niflheimr': {'Hrimfaxi': (0, 0), 'Garmr': (1, 1), 'Nidavellir': (2, 1), 'Ymir': (3, 1), 'Hel': (4, 0)},
}
BLOCK = 1 << 22


def read_ir(path):
    """32-bit float WAV from ChimeraPresetCabs; returns channel 0 and the L/R max difference."""
    raw = Path(path).read_bytes()
    if raw[:4] != b'RIFF' or raw[8:12] != b'WAVE': raise ValueError(f'Not a WAV: {path}')
    pos, fmt, data = 12, None, None
    while pos + 8 <= len(raw):
        tag, size = raw[pos:pos+4], int.from_bytes(raw[pos+4:pos+8], 'little')
        body = raw[pos+8:pos+8+size]
        if tag == b'fmt ': fmt = body
        elif tag == b'data': data = body
        pos += 8 + size + (size & 1)
    code, channels, rate = int.from_bytes(fmt[0:2], 'little'), int.from_bytes(fmt[2:4], 'little'), int.from_bytes(fmt[4:8], 'little')
    bits = int.from_bytes(fmt[14:16], 'little')
    if code == 0xFFFE: code = int.from_bytes(fmt[24:26], 'little')
    if code != 3 or bits != 32 or rate != 48000: raise ValueError(f'Expected 32-bit float 48 kHz: {path}')
    x = np.frombuffer(data, '<f4').reshape(-1, channels)
    return x[:, 0].astype(np.float64), float(np.max(np.abs(x[:, 0] - x[:, -1])))


def convolve_file(src, dst, ir):
    """Streamed overlap-add; same length as the head output (the tail past the end is dropped)."""
    x = np.load(src, mmap_mode='r')
    y = np.lib.format.open_memmap(str(dst) + '.tmp', mode='w+', dtype='<f4', shape=x.shape)
    carry = np.zeros(len(ir) - 1)
    for start in range(0, len(x), BLOCK):
        part = oaconvolve(np.asarray(x[start:start+BLOCK], np.float64), ir)
        part[:len(carry)] += carry
        n = min(BLOCK, len(x) - start)
        y[start:start+n] = part[:n]
        carry = part[n:]
        if len(carry) < len(ir) - 1: carry = np.pad(carry, (0, len(ir) - 1 - len(carry)))
    y.flush(); del y
    os.replace(str(dst) + '.tmp', dst)


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--data', type=Path, required=True, help='head capture')
    ap.add_argument('--irs', type=Path, required=True, help='directory with <Channel>.wav and <Channel>.json from ChimeraPresetCabs')
    ap.add_argument('--out', type=Path, required=True)
    ap.add_argument('--consume', action='store_true', help='delete each head output once converted')
    args = ap.parse_args()
    manifest = verify_audio_manifest(args.data)
    if manifest['source'].get('cabinet'): raise RuntimeError('Already a head + cab capture')
    amp = amp_of(manifest['source'])
    if args.out.exists() and any(args.out.iterdir()): raise RuntimeError('Output directory must be empty')
    head_manifest_sha = digest(args.data/'manifest.json')
    cabinets, irs = {}, {}
    for name, (preset, lane) in CABINETS[amp].items():
        meta = json.loads((args.irs/f'{name}.json').read_text())
        if (meta['amp'], meta['preset'], meta['lane']) != (amp, preset, lane) or not meta['ok']:
            raise RuntimeError(f'IR metadata does not match the mapping: {name}')
        ir, lr = read_ir(args.irs/f'{name}.wav')
        irs[name] = ir
        cabinets[name] = {'rig_preset': preset, 'rig_preset_name': meta['preset_name'], 'preset_index': meta['preset_index'],
                          'lane': lane, 'ir_samples': len(ir), 'ir_lr_max_difference': lr, 'ir_channel_used': 0,
                          'ir_sha256': hashlib.sha256((args.irs/f'{name}.wav').read_bytes()).hexdigest()}
    out = copy.deepcopy(manifest)
    out['source']['cabinet'] = {'kind': 'factory rig preset lane cabinet, exported by ChimeraPresetCabs',
                                'method': 'head output convolved with the lane cabinet IR (linear, time-invariant)',
                                'head_manifest_sha256': head_manifest_sha, 'channels': cabinets}
    args.out.mkdir(parents=True, exist_ok=True)
    for split in ('train', 'validation', 'test', 'audition'):
        (args.out/split).mkdir()
        os.link(args.data/split/'input.npy', args.out/split/'input.npy')
    for row in out['captures']:
        src, dst = args.data/row['output'], args.out/row['output']
        convolve_file(src, dst, irs[row['name']])
        row['output_sha256'] = hashlib.sha256(dst.read_bytes()).hexdigest()
        row['cabinet'] = cabinets[row['name']]
        if args.consume: src.unlink()
        print(row['split'], row['name'], flush=True)
    for f in ('channel-settings.json',):
        if (args.data/f).exists(): (args.out/f).write_bytes((args.data/f).read_bytes())
    write_json(args.out/'manifest.json', out)
    verify_audio_manifest(args.out)


if __name__ == '__main__':
    main()
