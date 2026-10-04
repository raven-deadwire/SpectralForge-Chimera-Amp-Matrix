#!/usr/bin/env python3
"""Regression: a rerender must replace WAV samples and select the native engine."""
import json, math, struct, subprocess, sys, tempfile, wave
from pathlib import Path

def main():
    executable=Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory() as directory:
        root=Path(directory);source=root/'input.wav';state=root/'state.json';output=root/'output.wav'
        with wave.open(str(source),'wb') as w:
            w.setparams((1,2,48000,12000,'NONE','not compressed'))
            w.writeframes(b''.join(struct.pack('<h',round(1000*math.sin(2*math.pi*400*n/48000))) for n in range(12000)))
        def render(config,dest=output):
            state.write_text(json.dumps(config));result=subprocess.run([str(executable),str(source),str(dest),'2','.5',str(state)],capture_output=True,text=True)
            assert result.returncode==0,result.stderr
            assert 'engine=native' in result.stdout,result.stdout
            return dest.read_bytes()
        low=render({'channel':1,'controls':{'hw.lead.pre_gain':.15}})
        high=render({'channel':1,'controls':{'hw.lead.pre_gain':.8}})
        fresh=render({'channel':1,'controls':{'hw.lead.pre_gain':.8}},root/'fresh.wav')
        assert low!=high,'Native gain did not change rendered audio'
        assert high==fresh,'Rerender appended to stale WAV data instead of replacing it'
        assert len(high)==struct.unpack_from('<I',high,4)[0]+8,'Trailing stale WAV bytes'
        state.write_text('{"controls":{"invalid.control":0.5}}')
        result=subprocess.run([str(executable),str(source),str(output),'2','.5',str(state)],capture_output=True)
        assert result.returncode==6,'Unknown native control was silently accepted'
        assert output.read_bytes()==high,'Invalid state changed the existing output'
    print('PASS native renderer selection, gain response, replacement, invalid-state rejection')
if __name__=='__main__':main()
