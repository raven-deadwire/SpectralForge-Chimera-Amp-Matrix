// Numerical behavior is tested on the selected backend, never inferred from x*1.
#include <juce_dsp/juce_dsp.h>
#include <bit>
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
#include <vector>
using C = std::complex<float>;
int main(int argc, char** argv) {
    if (argc != 2) return 2;
    std::ofstream csv(argv[1]);
    csv << "denormals_disabled,order,case,apple_backend,forward_mismatches,inverse_mismatches,real_mismatches\n";
    size_t mismatch=0, cases=0, floats=0;
    bool apple=false;
    auto compare = [&](const void* a, const void* b, size_t n) {
        size_t bad=0;
        for (size_t i=0;i<n;++i) {
            uint32_t x,y;
            std::memcpy(&x,static_cast<const char*>(a)+4*i,4);
            std::memcpy(&y,static_cast<const char*>(b)+4*i,4);
            bad += x!=y;
        }
        floats += n; return bad;
    };
    // Caller FP mode and the actual CAB ScopedNoDenormals mode are both retained.
    auto check = [&](bool disabled) {
        for (int order=2;order<=10;++order) {
            juce::dsp::FFT fft(order); const int n=1<<order;
            apple=fft.isChimeraCabAppleFFT();
            for(int kind=0;kind<10;++kind) {
                std::vector<C> input(n),a(n),b(n),inverseA(n),inverseB(n);
                for(int i=0;i<n;++i) {
                    const float noise=float(int((uint32_t(i+1)*747796405u)>>8)-0x800000)*0x1p-24f;
                    switch(kind) {
                        case 0: input[i]={0.f,0.f}; break;
                        case 1: input[i]={i%2 ? -0.f : 0.f,i%2 ? 0.f : -0.f}; break;
                        case 2: input[i]={i==0 ? .5f : 0.f,i==n-1 ? -.25f : 0.f}; break;
                        case 3: input[i]={noise,float(.4*std::sin(i*.37))}; break;
                        case 4: input[i]={std::numeric_limits<float>::denorm_min()*(i%7-3),std::bit_cast<float>(uint32_t(0x80000001))}; break;
                        case 5: input[i]={std::numeric_limits<float>::min()*(i%3-1),noise*1e-35f}; break;
                        case 6: input[i]={noise*1e30f,-noise*1e25f}; break;
                        case 7: input[i]={i==n/2 ? std::numeric_limits<float>::infinity() : noise,-noise}; break;
                        case 8: input[i]={i==1 ? std::bit_cast<float>(uint32_t(0x7fc12345)) : noise,-noise}; break;
                        default: input[i]={i==1 ? std::bit_cast<float>(uint32_t(0x7f812345)) : noise,-noise}; break;
                    }
                }
                fft.perform(input.data(),a.data(),false);
                fft.perform(a.data(),inverseA.data(),true);
                std::vector<float> realA(2*n),realB(2*n),realForward;
                for(int i=0;i<n;++i) realA[i]=input[i].real();
                realB=realA;
                fft.performRealOnlyForwardTransform(realA.data(),false); realForward=realA;
                fft.performRealOnlyInverseTransform(realA.data());
                fft.performChimeraCabForwardNoUnity(input.data(),b.data());
                // Identical input to legacy inverse/real before/after the opt-in call.
                fft.perform(a.data(),inverseB.data(),true);
                fft.performRealOnlyForwardTransform(realB.data(),false);
                size_t realBad=compare(realForward.data(),realB.data(),2*n);
                fft.performRealOnlyInverseTransform(realB.data());
                realBad+=compare(realA.data(),realB.data(),2*n);
                const auto bad=compare(a.data(),b.data(),2*n);
                const auto inv=compare(inverseA.data(),inverseB.data(),2*n);
                csv << disabled<<','<<order<<','<<kind<<','<<apple<<','<<bad<<','<<inv<<','<<realBad<<'\n';
                mismatch+=bad+inv+realBad; ++cases;
            }
        }
    };
    check(false);
    {juce::ScopedNoDenormals noDenormals;check(true);}
    std::cout << "cases="<<cases<<" float_words_compared="<<floats<<" bit_mismatches="<<mismatch
              <<" apple_backend="<<apple<<" nonapple_is_negative_control="<<!apple<<'\n';
    return mismatch ? 1 : 0;
}
