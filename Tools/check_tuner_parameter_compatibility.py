#!/usr/bin/env python3
"""Verify the released parameter registration prefix and old tuner definitions."""
import subprocess
from pathlib import Path
base = subprocess.check_output(["git", "show", "b4930839a7384a411e6c37e3570d77069f41a024:Source/PluginProcessor.cpp"], text=True)
current = Path("Source/PluginProcessor.cpp").read_text()
def layout(text):
    start = text.index("ChimeraProcessor::layout()")
    return text[start:text.index("return p;", start)]
old = layout(base)
new = layout(current)
assert new.startswith(old), "Released parameter registration changed/reordered"
assert '"tuneron","Tuner",false' in new
assert '"tunermute","Mute while tuning",true' in new
assert '"tunerref","Tuner A4 reference",430,450,440' in new
print("PASS released registration is an exact prefix; tuner enable/mute/A4 IDs, ranges and defaults unchanged")
