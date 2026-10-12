# Local direct-build recipe

This recipe documents the direct benchmark build used for the local paired
evidence. Normal CI builds use the repository's CMake target
`ChimeraEQAnalyzerPerformanceTests` and normal JUCE module linkage. This direct
build was used while the complete product toolchain was being prepared.

## Paths and exact compilation

At measurement time:

```text
REPO=/workspace/scratch/af7c830474b5/chimera
JUCE=/workspace/scratch/af7c830474b5/toolchain/JUCE
SYSROOT=/workspace/scratch/af7c830474b5/toolchain/sysroot
BUILD=/workspace/scratch/af7c830474b5/eq-cost-build
```

All module and test translation units were compiled with GCC 13.3 using these
arguments, replacing `INPUT` and `OUTPUT` with the corresponding files:

```sh
g++ -std=c++20 -O3 -DNDEBUG \
  -DJUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1 \
  -DJUCE_STANDALONE_APPLICATION=1 \
  -DJUCE_USE_CURL=0 -DJUCE_WEB_BROWSER=0 \
  -ffunction-sections -fdata-sections \
  -I /workspace/scratch/af7c830474b5/toolchain/JUCE/modules \
  -I /workspace/scratch/af7c830474b5/toolchain/sysroot/usr/include \
  -I /workspace/scratch/af7c830474b5/chimera/Source \
  -c INPUT -o OUTPUT
```

| Input | Output basename |
|---|---|
| `REPO/Tests/EQAnalyzerPerformanceTests.cpp` | `EQAnalyzerPerformanceTests.o` |
| `JUCE/modules/juce_core/juce_core.cpp` | `juce_core.o` |
| `JUCE/modules/juce_audio_basics/juce_audio_basics.cpp` | `juce_audio_basics.o` |
| `JUCE/modules/juce_audio_formats/juce_audio_formats.cpp` | `juce_audio_formats.o` |
| `JUCE/modules/juce_dsp/juce_dsp.cpp` | `juce_dsp.o` |
| `BUILD/juce_colour_probe.cpp` | `juce_colour_probe.o` |

The `-I REPO/Source` argument was needed only by the test source. For
`juce_core_CompilationTime.cpp`, the same compiler/defines/include arguments were
used without `-ffunction-sections -fdata-sections`; its output was
`BUILD/juce_core_CompilationTime.o`.

`juce_colour_probe.cpp` contained exactly these two include lines. It compiles
JUCE's actual Colour code, required by static colour constants in the
`GraphicalEQ.h` → audio-processors → GUI headers, without starting a GUI or
substituting any implementation:

```cpp
#include <juce_graphics/juce_graphics.h>
#include "/workspace/scratch/af7c830474b5/toolchain/JUCE/modules/juce_graphics/colour/juce_Colour.cpp"
```

Each binary linked the same module objects with:

```sh
g++ -O3 -Wl,--gc-sections \
  /workspace/scratch/af7c830474b5/eq-cost-build/EQAnalyzerPerformanceTests.o \
  /workspace/scratch/af7c830474b5/eq-cost-build/juce_core.o \
  /workspace/scratch/af7c830474b5/eq-cost-build/juce_core_CompilationTime.o \
  /workspace/scratch/af7c830474b5/eq-cost-build/juce_audio_basics.o \
  /workspace/scratch/af7c830474b5/eq-cost-build/juce_audio_formats.o \
  /workspace/scratch/af7c830474b5/eq-cost-build/juce_dsp.o \
  /workspace/scratch/af7c830474b5/eq-cost-build/juce_colour_probe.o \
  -pthread -ldl -lrt -lz -o BINARY
```

## A/A and A/B construction

`eq-cost-aa` was compiled and linked before editing production
`EQSpectrumAnalyzer.h`; both its normal include and renamed reference include
were the frozen `ad0e9f...` bytes. The baseline macro changed the class name only.
That immutable executable was run twice: first with an overlapping toolchain
build, then again after local compilers had exited.

After applying the measured candidate, only `EQAnalyzerPerformanceTests.o` was
recompiled with the same arguments. The same JUCE object files were linked into
`eq-cost-ab`. Its normal include has candidate hash `a7dd27...`; its renamed
reference still has baseline hash `ad0e9f...`. Source and binary hashes are in
`build-manifest.json`.

To reproduce the A/A build after the production edit, copy the frozen header
into a temporary include directory as `EQSpectrumAnalyzer.h` and put that directory
before `REPO/Source` in the test's include path. This selects the original bytes
without editing the production working tree. The original recorded A/A build did
not need this extra include path because the production header was still unchanged.

## Executions

```sh
/workspace/scratch/af7c830474b5/eq-cost-build/eq-cost-aa \
  /workspace/scratch/af7c830474b5/eq-cost-aa \
  > /workspace/scratch/af7c830474b5/eq-cost-build/aa.log 2>&1

/workspace/scratch/af7c830474b5/eq-cost-build/eq-cost-aa \
  /workspace/scratch/af7c830474b5/eq-cost-aa-clean \
  > /workspace/scratch/af7c830474b5/eq-cost-build/aa-clean.log 2>&1

/workspace/scratch/af7c830474b5/eq-cost-build/eq-cost-ab \
  /workspace/scratch/af7c830474b5/eq-cost-ab \
  > /workspace/scratch/af7c830474b5/eq-cost-build/ab.log 2>&1
```

All three executions exited 0. The complete output CSVs and logs were copied
byte-for-byte into the three evidence subdirectories, with no timing rows removed
or edited. Binary files are intentionally omitted; their SHA-256 values and all
receipt hashes are retained. No additional timing run was performed after the
complete product build started.
