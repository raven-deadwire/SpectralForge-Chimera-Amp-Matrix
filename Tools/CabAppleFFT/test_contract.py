"""Verify isolation and reject source drift before producing experiment overlays."""
import argparse, importlib.util, json, subprocess, tempfile
from pathlib import Path
from generate import ROOT, PINS, CAB_SHA, patch_juce, patch_cab, sha
p=argparse.ArgumentParser();p.add_argument('--juce',type=Path,required=True);a=p.parse_args()
checks=[]
def check(value,name):
    if not value:raise AssertionError(name)
    checks.append(name)
for path in PINS:
    raw=(a.juce/path).read_bytes();out=patch_juce(path,raw)
    check(raw!=out,path+' adds opt-in API')
    try:patch_juce(path,raw+b' ')
    except ValueError:checks.append(path+' rejects drift')
    else:raise AssertionError('accepted drift')
    if path.endswith('.cpp'):
        original=raw.decode().replace('\r\n','\n');candidate=out.decode().replace('\r\n','\n')
        check(candidate.count('vDSP_vsmul (')==original.count('vDSP_vsmul ('),'all original scaling sites retained')
        check(candidate.count('vDSP_fft_zop (')==original.count('vDSP_fft_zop (')+1,'one additional explicit Apple complex method')
        check('virtual bool isChimeraCabAppleFFT() const noexcept { return false; }' in candidate,'non-Apple backend identified')
raw=(ROOT/'Source/ModeledCabConvolution.h').read_bytes();out=patch_cab(raw)
check(sha(raw)==CAB_SHA,'pinned CAB source')
check(out.count(b'performChimeraCabForwardNoUnity(')==1,'one CAB opt-in call')
check(out.replace(b'fft.performChimeraCabForwardNoUnity(inputTime.data(),transform.data());',
                  b'fft.perform(inputTime.data(),transform.data(),false);')==raw,'all other CAB bytes retained')
check(out.count(b'fft.perform(inputTime.data(),transform.data(),false);')==1,'IR preparation remains legacy')
try:patch_cab(raw+b' ')
except ValueError:checks.append('CAB drift rejected')
else:raise AssertionError('accepted CAB drift')
# Default analyzer arguments preserve the historical two-candidate interpretation.
spec=importlib.util.spec_from_file_location('old',ROOT/'Tools/CabZeroWork/run.py');m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
check(m.analyze.__defaults__==(('empty_partition','zero_history'),),'existing analyzer defaults preserved')
print(json.dumps({'pass':True,'checks':checks,'macos_execution':False},indent=2))
