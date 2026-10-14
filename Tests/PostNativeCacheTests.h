#pragma once
#include "reference/PostNativeDSPBeforeCache.h"

// Compare complete audio and meter histories, including settled controls,
// automation, bypass/model transitions, reset and re-prepare on the same object.
inline void postNativeCacheParity()
{
    using namespace spectralforge;
    PostNativeDSP candidate;
    PostNativeReferenceDSP reference;
    size_t compared = 0;
    for (double rate : {44100., 48000., 96000., 48000.})
        for (int channels : {1, 2})
        {
            candidate.prepare({rate, 256, (juce::uint32) channels});
            reference.prepare({rate, 256, (juce::uint32) channels});
            auto state = defaultPostNativeState();
            juce::AudioBuffer<float> a(channels, 256), b(channels, 256);
            for (int cycle = 0; cycle < 240; ++cycle)
            {
                if (cycle == 150) { candidate.reset(); reference.reset(); }
                const int count = cycle % 3 == 0 ? 37 : cycle % 3 == 1 ? 64 : 256;
                a.setSize(channels, count, false, false, true);
                b.setSize(channels, count, false, false, true);
                for (int s = 0; s < 3; ++s)
                {
                    auto& section = state.sections[s];
                    section.selected = (cycle / 40) % 3;
                    auto& bank = section.banks[section.selected];
                    bank.bypass = (cycle / 13) % 4 == 0;
                    // First half of each interval settles; second half automates
                    // all controls, including discrete switches and meter modes.
                    if (cycle % 40 >= 20)
                    {
                        bank.trimDb = float(cycle % 7 - 3) * 4.f;
                        bank.levelDb = float(cycle % 5 - 2) * 3.f;
                        const auto& model = postNativeModel(s, section.selected);
                        for (int c = 0; c < model.controlCount; ++c)
                        {
                            const auto& control = model.controls[c];
                            const float t = float((cycle * 7 + c * 3) % 11) / 10.f;
                            bank.values[c] = control.minimum + t * (control.maximum - control.minimum);
                        }
                    }
                }
                sanitisePostNativeState(state);
                for (int c = 0; c < channels; ++c)
                    for (int n = 0; n < count; ++n)
                    {
                        const float x = cycle == 150 ? 0.f :
                            float(.3 * std::sin((cycle * 256 + n) * .031 * (c + 1)));
                        a.setSample(c, n, x); b.setSample(c, n, x);
                    }
                for (int s = 0; s < 3; ++s)
                {
                    candidate.processSection(a, s, state.sections[s]);
                    reference.processSection(b, s, state.sections[s]);
                    if (candidate.meter(s) != reference.meter(s) ||
                        candidate.reduction(s) != reference.reduction(s))
                        throw std::runtime_error("POST cache changed meter history");
                    for (int c = 0; c < channels; ++c)
                        for (int n = 0; n < count; ++n)
                        {
                            if (!std::isfinite(a.getSample(c, n)) || a.getSample(c, n) != b.getSample(c, n))
                                throw std::runtime_error("POST cache changed audio history");
                            ++compared;
                        }
                }
            }
        }
    std::cout << "POST cache exact parity PASS: sample comparisons=" << compared
              << " models=9 mono/stereo automation/bypass/reset/reprepare\n";
}
