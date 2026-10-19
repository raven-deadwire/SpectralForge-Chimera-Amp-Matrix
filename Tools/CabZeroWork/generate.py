"""Generate independent research headers; never edit production Source/."""
import argparse
import hashlib
from pathlib import Path

BASE = "50ed1fd50ffb735d8f6581197116ac4184187d91"
HEADER_SHA256 = "b35dff80ddc1de23061666ab7e7bca6e854cbb9d8da05aad09cb888dfc089f33"

def replace_once(s, old, new):
    if s.count(old) != 1:
        raise ValueError(f"source anchor is not unique: {old[:70]}")
    return s.replace(old, new, 1)

def generate(source, candidate):
    if hashlib.sha256(source.encode()).hexdigest() != HEADER_SHA256:
        raise ValueError("expected exact PR105 convolution source")
    if candidate == "baseline":
        return source
    if candidate == "empty_partition":
        # Keep all IR bytes, FFT preparation, ring geometry and ownership.
        source = replace_once(source, "const int blockSize, fftSize, bins, segmentSize, segments, inputSegments;",
            "const int blockSize, fftSize, bins, segmentSize, segments, inputSegments;\n        const int occupiedSegments;")
        source = replace_once(source,
            "inputSegments((blockSize>128 ? 1 : 3)*segments), impulseSpectra",
            "inputSegments((blockSize>128 ? 1 : 3)*segments),\n              occupiedSegments(juce::jmax(1,(samples.getNumSamples()+segmentSize-1)/segmentSize)), impulseSpectra")
        return replace_once(source, "for(int partition=1;partition<segments;++partition) {",
            "// The optional final prepared partition has no authored samples.\n            // Retain its storage/ring slot; omit only its zero contribution.\n            for(int partition=1;partition<prepared->occupiedSegments;++partition) {")
    if candidate == "zero_history":
        source = replace_once(source, "int currentSegment{}, inputPosition{};",
            "int currentSegment{}, inputPosition{};\n    // Completed logical input blocks since reset; saturates before overflow.\n    int completedBlocks{};")
        source = replace_once(source, "for(int partition=1;partition<segments;++partition) {",
            "// Delayed partition p reads input from p*inputSegmentStep blocks ago.\n            // Until then that bank slot is still reset zero. Do not change ring\n            // geometry, live-signal silence handling, or the accumulation order.\n            const int available=juce::jmin(segments-1,completedBlocks/inputSegmentStep);\n            for(int partition=1;partition<=available;++partition) {")
        source = replace_once(source, "currentSegment=0;inputPosition=0;",
            "currentSegment=0;inputPosition=0;completedBlocks=0;")
        anchor = "currentSegment=currentSegment>0 ? currentSegment-1 : inputSegments-1;"
        if source.count(anchor) != 2:
            raise ValueError("expected mono and stereo block-completion sites")
        return source.replace(anchor, anchor + "\n                if(completedBlocks<inputSegments)++completedBlocks;")
    raise ValueError(candidate)

if __name__ == "__main__":
    p=argparse.ArgumentParser()
    p.add_argument("--source",type=Path,required=True)
    p.add_argument("--candidate",choices=["baseline","empty_partition","zero_history"],required=True)
    p.add_argument("--output",type=Path,required=True)
    a=p.parse_args()
    original=a.source.read_text()
    a.output.parent.mkdir(parents=True,exist_ok=True)
    a.output.write_text(generate(original,a.candidate))
    print(hashlib.sha256(a.output.read_bytes()).hexdigest(),a.output)
