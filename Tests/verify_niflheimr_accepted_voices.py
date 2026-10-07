#!/usr/bin/env python3
"""Compare accepted channels to a real historical DSP, without golden rounding.

Local developer check: requires git history and a C++20 compiler. The separately
compiled historical namespace shares only the unchanged control/state schema.
No synthetic result in this tool approves the sound of revised channels.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
BASELINE = "6d6a1c3814d32dd1aea5e18d119d76a836963152"
COMPARISON = r'''
#include "NiflheimrDSP.h"
#include "baseline.h"
#include <iostream>
#include <cstdint>
int main() {
    using namespace spectralforge::niflheimr;
    std::size_t samples=0;
    uint32_t random=7919;
    for(int ch:{0,2}) for(double host:{44100.,48000.,96000.}) for(int factor:{1,2,4,8}) {
        const double rate=host*factor;
        NiflheimrDSP current;
        spectralforge::niflheimr_baseline::NiflheimrDSP old;
        current.prepare(rate); old.prepare(rate);
        State s=channelState(ch);
        current.set(s); old.set(s); current.reset(); old.reset();
        for(int n=0;n<int(rate*.35);++n) {
            if(n%137==0) {
                for(std::size_t i=0;i<controlCount;++i) {
                    random=1664525*random+1013904223;
                    const float v=float(random>>8)/16777215.f;
                    s.values[i]=controls[i].minimum+v*(controls[i].maximum-controls[i].minimum);
                }
                current.set(s); old.set(s);
            }
            const double t=n/rate;
            const float x=float(.12*sin(2*detail::pi*30.8677*t)+.06*sin(2*detail::pi*315*t)
                                +.04*sin(2*detail::pi*1739*t));
            for(int stereo=0;stereo<2;++stereo) {
                const float input=stereo?x*.73f:x;
                if(current.tick(input,stereo)!=old.tick(input,stereo)) {
                    std::cerr<<"Mismatch channel="<<ch<<" rate="<<rate<<" sample="<<n<<'\n';
                    return 1;
                }
                ++samples;
            }
        }
    }
    std::cout<<samples;
}
'''


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--baseline", default=BASELINE)
    parser.add_argument("--compiler", default="g++")
    args = parser.parse_args()
    historical = subprocess.run(
        ["git", "show", f"{args.baseline}:Source/NiflheimrDSP.h"], cwd=ROOT,
        capture_output=True, text=True,
    )
    if historical.returncode:
        parser.error("Baseline DSP is unavailable in local git history; fetch the "
                     "requested commit or supply --baseline. This is a manual "
                     "developer comparison, not a shallow-checkout CI gate.")
    source = historical.stdout
    marker = "namespace spectralforge::niflheimr {"
    if source.count(marker) != 1:
        raise RuntimeError("Historical DSP namespace cannot be isolated")
    isolated = source.replace(marker, "namespace spectralforge::niflheimr_baseline {\n"
                                      "using namespace spectralforge::niflheimr;")
    with tempfile.TemporaryDirectory(prefix="niflheimr-accepted-") as temp:
        directory = Path(temp)
        (directory / "baseline.h").write_text(isolated)
        (directory / "compare.cpp").write_text(COMPARISON)
        executable = directory / "compare"
        subprocess.run([args.compiler, "-std=c++20", "-O2", "-I", str(ROOT / "Source"),
                        str(directory / "compare.cpp"), "-o", str(executable)], check=True)
        samples = int(subprocess.check_output([str(executable)]))
    print(json.dumps({
        "baseline": args.baseline,
        "baseline_dsp_sha256": hashlib.sha256(source.encode()).hexdigest(),
        "current_dsp_sha256": hashlib.sha256((ROOT / "Source/NiflheimrDSP.h").read_bytes()).hexdigest(),
        "channels": [0, 2], "channel_rate_routes": 24, "stereo_samples": samples,
        "moving_controls": 14, "max_sample_error": 0, "result": "PASS",
        "scope": "Bit-identical accepted voices; no musical acceptance claim",
    }, indent=2))


if __name__ == "__main__":
    main()
