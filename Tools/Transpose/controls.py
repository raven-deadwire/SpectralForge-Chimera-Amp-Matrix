#!/usr/bin/env python3
"""Cross-check analysis oracle and unexpected low-tone results against 220 Hz.

Usage: controls.py RENDERER WORK_DIR REPORT_JSON
"""
import json
import subprocess
import sys
from pathlib import Path
import numpy as np
from measure import tone_metrics, sha

renderer, work, report = map(Path, sys.argv[1:])
work.mkdir(parents=True, exist_ok=True)
rows = []
for frequency in (30.8677063285, 220.):
    t = np.arange(144000)/48000
    left = .2*np.sin(2*np.pi*frequency*t)
    for right in (-.65, 1., 0.):
        x = np.column_stack((left,right*left)).astype("<f4")
        oracle = tone_metrics(x,48000,frequency,.2)
        assert abs(oracle["target_gain_db"])<.001
        assert abs(oracle["dominant_error_cents"])<.01
        x.tofile(work/"input.f32")
        for algorithm in ("baseline","relative","signalsmith-matched","signalsmith-default"):
            subprocess.run([str(renderer),str(work/"input.f32"),str(work/"output.f32"),
                            "48000","128","-2",algorithm,"off","-65","80","20"],
                           check=True,capture_output=True,text=True)
            y=np.fromfile(work/"output.f32",dtype="<f4").reshape(-1,2)
            rows.append({"frequency":frequency,"right_gain":right,"algorithm":algorithm,
                         "input_sha256":sha(work/"input.f32"),"output_sha256":sha(work/"output.f32"),
                         **tone_metrics(y,48000,frequency*2**(-2/12),.2)})
report.write_text(json.dumps({"oracle":"PASS", "results":rows},indent=2,allow_nan=False)+"\n")
