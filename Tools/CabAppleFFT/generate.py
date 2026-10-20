"""Fail-closed, build-directory-only AppleFFT experiment for PR #109."""
import argparse, difflib, hashlib, json, shutil, subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
JUCE_COMMIT = 'd6181bde38d858c283c3b7bf699ce6340c050b5d'
PINS = {
    'modules/juce_dsp/frequency/juce_FFT.cpp': 'aa1e5209d0bea072e8e57831110d86adfc6f91670c3ce72d3102f1e9a64843a3',
    'modules/juce_dsp/frequency/juce_FFT.h': '9b3d6466e3f774b50fff28a02f473802ef64f84a959cdca8b923d58dd14a0cbe',
}
CAB_SHA = 'b35dff80ddc1de23061666ab7e7bca6e854cbb9d8da05aad09cb888dfc089f33'

def sha(b): return hashlib.sha256(b).hexdigest()
def once(s, old, new):
    if s.count(old) != 1: raise ValueError('non-unique source anchor: ' + old[:100])
    return s.replace(old, new, 1)

def patch_juce(path, raw):
    if sha(raw) != PINS[path]: raise ValueError('JUCE 8.0.8 byte pin mismatch: ' + path)
    source = raw.decode().replace('\r\n', '\n')
    additions = []
    def insert(anchor, addition):
        nonlocal source
        source = once(source, anchor, anchor + addition)
        additions.append(addition)
    if path.endswith('.h'):
        insert('    void perform (const Complex<float>* input, Complex<float>* output, bool inverse) const noexcept;\n', '''
    // Isolated research API. No product callers or global engine replacement.
    void performChimeraCabForwardNoUnity (const Complex<float>*, Complex<float>*) const noexcept;
    bool isChimeraCabAppleFFT() const noexcept;
''')
    else:
        insert('    virtual void performRealOnlyInverseTransform (float*) const noexcept = 0;\n', '''
    virtual void performChimeraCabForwardNoUnity (const Complex<float>* input, Complex<float>* output) const noexcept
    { perform (input, output, false); }
    virtual bool isChimeraCabAppleFFT() const noexcept { return false; }
''')
        anchor = '''        float factor = (inverse ? inverseNormalisation : forwardNormalisation * 2.0f);
        vDSP_vsmul ((float*) output, 1, &factor, (float*) output, 1, static_cast<size_t> (size << 1));
    }
'''
        insert(anchor, '''
    // Only the explicitly opted-in CAB call reaches this method. The legacy
    // complex, inverse and real methods remain byte-for-byte unchanged.
    void performChimeraCabForwardNoUnity (const Complex<float>* input, Complex<float>* output) const noexcept override
    {
        DSPSplitComplex splitInput  (toSplitComplex (const_cast<Complex<float>*> (input)));
        DSPSplitComplex splitOutput (toSplitComplex (output));
        vDSP_fft_zop (fftSetup, &splitInput, 2, &splitOutput, 2,
                      order, kFFTDirection_Forward);
    }
    bool isChimeraCabAppleFFT() const noexcept override { return true; }
''')
        anchor = '''void FFT::perform (const Complex<float>* input, Complex<float>* output, bool inverse) const noexcept
{
    if (engine != nullptr)
        engine->perform (input, output, inverse);
}
'''
        insert(anchor, '''
void FFT::performChimeraCabForwardNoUnity (const Complex<float>* input, Complex<float>* output) const noexcept
{
    if (engine != nullptr)
        engine->performChimeraCabForwardNoUnity (input, output);
}

bool FFT::isChimeraCabAppleFFT() const noexcept
{
    return engine != nullptr && engine->isChimeraCabAppleFFT();
}
''')
    restored = source
    for addition in reversed(additions): restored = once(restored, addition, '')
    assert restored == raw.decode().replace('\r\n', '\n')
    # Preserve upstream line endings as well as all original method bodies.
    return source.replace('\n', '\r\n').encode()

def patch_cab(raw):
    if sha(raw) != CAB_SHA: raise ValueError('PR109 CAB source pin mismatch')
    old = '''                SF_CAB_TRACE_SCOPE(FFTForward);
                fft.perform(inputTime.data(),transform.data(),false);'''
    new = '''                SF_CAB_TRACE_SCOPE(FFTForward);
                fft.performChimeraCabForwardNoUnity(inputTime.data(),transform.data());'''
    return once(raw.decode(), old, new).encode()

def generate(juce, output):
    juce, output = juce.resolve(), output.resolve()
    if output == ROOT or ROOT in output.parents:
        raise ValueError('use a scratch build directory outside the repository')
    if output == juce or juce in output.parents or output in juce.parents:
        raise ValueError('output must be independent of upstream JUCE')
    if subprocess.check_output(['git','-C',str(juce),'rev-parse','HEAD'],text=True).strip() != JUCE_COMMIT:
        raise ValueError('expected exact JUCE 8.0.8 commit')
    if subprocess.check_output(['git','-C',str(juce),'status','--porcelain','--untracked-files=no'],text=True).strip():
        raise ValueError('upstream JUCE tracked sources must be clean')
    patched = {p: patch_juce(p, (juce / p).read_bytes()) for p in PINS}
    original = (ROOT / 'Source/ModeledCabConvolution.h').read_bytes()
    candidate = patch_cab(original)
    output.mkdir(parents=True, exist_ok=True)
    shutil.copytree(juce, output / 'JUCE', ignore=shutil.ignore_patterns('.git'), dirs_exist_ok=True)
    for path, raw in patched.items(): (output / 'JUCE' / path).write_bytes(raw)
    for variant, raw in [('baseline', original), ('candidate', candidate)]:
        dest = output / variant / 'Source'
        shutil.copytree(ROOT / 'Source', dest, dirs_exist_ok=True)
        (dest / 'ModeledCabConvolution.h').write_bytes(raw)
        (dest / 'BaselineConvolution.h').write_bytes(original.replace(b'ModeledCabConvolution', b'BaselineConvolution'))
    patches = {}
    for path, raw in {**patched, 'Source/ModeledCabConvolution.h': candidate}.items():
        before = original if path.startswith('Source/') else (juce / path).read_bytes()
        diff = ''.join(difflib.unified_diff(before.decode().splitlines(True), raw.decode().splitlines(True),
                                          fromfile='a/' + path, tofile='b/' + path, n=0))
        patches[path] = {'original_sha256': sha(before), 'overlay_sha256': sha(raw)}
        # Human-readable diffs; byte-pinned generator is the authoritative apply path.
        (output / (Path(path).name + '.patch')).write_text(diff.replace('\r\n', '\n'))
    report = {'juce_commit': JUCE_COMMIT, 'files': patches, 'legacy_juce_methods_unchanged': True,
              'cab_changed_calls': 1, 'prepared_fft_unchanged': True, 'product_sources_modified': False,
              'macos_execution': False, 'integration_eligible': False, 'release_approved': False}
    (output / 'static-verification.json').write_text(json.dumps(report, indent=2) + '\n')
    return report

if __name__ == '__main__':
    p = argparse.ArgumentParser(); p.add_argument('--juce', type=Path, required=True); p.add_argument('--output', type=Path, required=True)
    a = p.parse_args(); print(json.dumps(generate(a.juce, a.output), indent=2))
