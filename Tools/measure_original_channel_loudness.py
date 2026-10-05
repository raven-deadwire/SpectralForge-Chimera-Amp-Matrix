#!/usr/bin/env python3
"""Compare production channel probes with independent FFmpeg R128 metering.

Input: --original-channel-levels output, 48 kHz, blocks of L128 then R128.
Short synthetic fixtures characterize this path only; no listening acceptance.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import tempfile
import numpy as np
from scipy.io import wavfile

def main():
    ap = argparse.ArgumentParser()
    for name in ('before', 'after', 'out'):
        ap.add_argument('--'+name, type=Path, required=True)
    args = ap.parse_args()
    rows = []
    with tempfile.TemporaryDirectory(prefix='chimera-r128-') as temporary:
        for version, directory in [('before', args.before), ('after', args.after)]:
            for path in sorted(directory.glob('*.f32')):
                raw = path.read_bytes()
                samples = np.frombuffer(raw, np.float32)
                if samples.size != 450*128*2 or not np.isfinite(samples).all():
                    raise ValueError(f'Invalid 1.2-second stereo probe: {path}')
                audio = samples.reshape(-1, 2, 128).transpose(0, 2, 1).reshape(-1, 2)
                wave = Path(temporary) / 'probe.wav'
                wavfile.write(wave, 48000, audio)
                result = subprocess.run(['ffmpeg', '-hide_banner', '-nostats', '-i', str(wave),
                                         '-af', 'ebur128=peak=true', '-f', 'null', '-'],
                                        capture_output=True, text=True, check=True)
                matches = re.findall(r'\bI:\s*(-?[\d.]+) LUFS', result.stderr)
                if not matches:
                    raise ValueError('Missing FFmpeg integrated loudness summary')
                rows.append(dict(version=version, probe=path.stem, sha256=hashlib.sha256(raw).hexdigest(),
                                 integrated_lufs=float(matches[-1]),
                                 rms_dbfs=float(20*np.log10(np.sqrt(np.mean(audio.astype(float)**2)))),
                                 sample_peak_dbfs=float(20*np.log10(np.max(np.abs(audio))))))
    if len(rows) != 20:
        raise ValueError('Expected five channels on both paths in both versions')
    result = dict(sample_rate=48000, duration_seconds=1.2, stereo=True,
                  method='FFmpeg ebur128 gated integrated loudness, common V30 cabinet, same knobs; bare or Thall PRE/POST.',
                  limits='Synthetic plucks only; integrated loudness is fixture-specific, not a real-DI or listening acceptance.',
                  ffmpeg_version=subprocess.check_output(['ffmpeg', '-version'], text=True).splitlines()[0],
                  before_source='c041521f11fdbd6a7f54e5b63a607d324089b616',
                  source_sha256={p:hashlib.sha256(Path(p).read_bytes()).hexdigest() for p in
                                 ['Source/OriginalAmpDefinition.h', 'Source/OriginalPresets.h', 'Tests/IntegratedProcessorTests.cpp']},
                  measurements=rows)
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(json.dumps(result, indent=2)+'\n')
    for path in ('cab_', 'driven_'):
        for version in ('before', 'after'):
            subset=[r for r in rows if r['version']==version and r['probe'].startswith(path)]
            levels=[r['integrated_lufs'] for r in subset]
            print(version, path, 'spread_LU', round(max(levels)-min(levels), 2),
                  {r['probe']:r['integrated_lufs'] for r in subset})

if __name__ == '__main__':
    main()
