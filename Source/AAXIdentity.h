#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

namespace spectralforge {
// Persistent identities, independent of parameter count, route, JUCE defaults
// and the product version. AudioSuite IDs are reserved but not advertised.
struct AAXIdentity final : juce::AAXClientExtensions {
    juce::int32 getPluginIDForMainBusConfig(const juce::AudioChannelSet& input,
                                          const juce::AudioChannelSet& output,
                                          bool audioSuite) const override {
        if (input == output && input == juce::AudioChannelSet::mono())
            return audioSuite ? 0x43684d41 : 0x43684d31; // ChMA / ChM1
        if (input == output && input == juce::AudioChannelSet::stereo())
            return audioSuite ? 0x43685341 : 0x43685331; // ChSA / ChS1
        jassertfalse; // The processor must reject all other host layouts.
        return 0;
    }
};
}
