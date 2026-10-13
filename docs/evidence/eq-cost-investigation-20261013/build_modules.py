import subprocess,concurrent.futures,pathlib
root=pathlib.Path('/workspace/scratch/99624a1c2d68')
flags=['g++','-std=c++20','-O3','-DNDEBUG','-DJUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1','-DJUCE_STANDALONE_APPLICATION=1','-DJUCE_USE_CURL=0','-DJUCE_WEB_BROWSER=0','-ffunction-sections','-fdata-sections','-I'+str(root/'JUCE/modules'),'-I'+str(root/'sysroot/usr/include')]
mods=['juce_core','juce_audio_basics','juce_audio_formats','juce_dsp']
inputs=[root/'JUCE/modules'/m/(m+'.cpp') for m in mods]
inputs.append(root/'JUCE/modules/juce_core/juce_core_CompilationTime.cpp')
colour=root/'eq-build/colour.cpp';colour.write_text('#include <juce_graphics/juce_graphics.h>\n#include "'+str(root/'JUCE/modules/juce_graphics/colour/juce_Colour.cpp')+'"\n');inputs.append(colour)
def build(src):
 out=root/'eq-build'/(src.stem+'.o')
 r=subprocess.run(flags+['-c',str(src),'-o',str(out)],capture_output=True,text=True)
 print(src.stem,r.returncode,r.stderr,flush=True)
 return r.returncode
with concurrent.futures.ThreadPoolExecutor(max_workers=3) as pool:
 results=list(pool.map(build,inputs))
raise SystemExit(any(results))
