#pragma once
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <cmath>

namespace spectralforge {
// These are original, stateful circuit-inspired designs. Manufacturer panel
// documentation establishes channel/route scope, not the coefficients below.
// No component-value match, capture fit, or hardware equivalence is claimed.
inline constexpr bool isNewAmpModel(int model) { return model >= 15 && model <= 22; }
inline constexpr int newAmpChannelCount(int model)
{
    constexpr std::array<int, 8> counts{{4, 2, 4, 1, 3, 3, 1, 2}};
    return isNewAmpModel(model) ? counts[(size_t)(model - 15)] : 1;
}
inline constexpr int newAmpDefaultChannel(int model)
{
    constexpr std::array<int, 8> defaults{{0, 1, 2, 0, 2, 0, 0, 1}};
    return isNewAmpModel(model) ? defaults[(size_t)(model - 15)] : 0;
}
inline const char* newAmpChannelName(int model, int channel)
{
    static constexpr const char* names[8][4] = {
        {"CH1", "CH2", "CH3", "CH4"}, {"CLEAN", "LEAD", "", ""},
        {"CLEAN", "CRUNCH", "MEGA", "LEAD"}, {"CLASSIC", "", "", ""},
        {"NORMAL INPUT", "BRILLIANT INPUT", "JUMPED INPUTS", ""},
        {"GAIN I", "GAIN II", "CLEAN", ""},
        {"OVERDRIVE", "", "", ""}, {"CLEAN", "LEAD", "", ""}
    };
    return isNewAmpModel(model) ? names[model - 15][juce::jlimit(0, newAmpChannelCount(model) - 1, channel)] : "VOICE";
}

namespace newAmpDetail {
inline float pole(double rate, float hz)
{
    return float(-std::expm1(-juce::MathConstants<double>::twoPi * juce::jmin(double(hz), rate * .45) / rate));
}
inline float slew(double rate, float seconds) { return float(-std::expm1(-1.0 / (rate * seconds))); }
inline float soft(float value) { return value / std::sqrt(1.f + value * value); }
struct RC
{
    float coefficient{}, memory{};
    void configure(double rate, float hz) { coefficient = pole(rate, hz); memory = 0; }
    float low(float x) { memory += coefficient * (x - memory); return memory; }
    float high(float x) { return x - low(x); }
};

// A reduced triode stage: asymmetric static transfer, rectified grid loading,
// slow cathode feedback and an independent interstage coupling capacitor.
// It deliberately uses an algebraic transfer rather than the Legacy tanh chain.
struct Triode
{
    RC coupling, bandwidth;
    float bias{}, cathodeAmount{}, gridAmount{}, charge{}, cathode{};
    float gridAttack{}, gridRelease{}, cathodeRate{};
    void configure(double rate, float hp, float lp, float operatingBias, float localFeedback, float blocking)
    {
        coupling.configure(rate, hp); bandwidth.configure(rate, lp);
        bias = operatingBias; cathodeAmount = localFeedback; gridAmount = blocking;
        gridAttack = slew(rate, .0006f); gridRelease = slew(rate, .055f);
        cathodeRate = slew(rate, .012f); charge = cathode = 0;
    }
    float tick(float x, float gain)
    {
        const float v = x * gain / (1.f + cathodeAmount * cathode);
        const float demand = juce::jmax(0.f, v - .72f);
        charge += (demand > charge ? gridAttack : gridRelease) * (demand - charge);
        const float operatingPoint = bias - gridAmount * charge;
        const float y = soft(v + operatingPoint) - soft(operatingPoint);
        cathode += cathodeRate * (std::abs(y) - cathode);
        return bandwidth.low(coupling.high(y));
    }
};

// Delayed negative feedback remains strictly bounded (< 1); two RC branches
// give the feedback a reactive low/high-frequency response. The power supply
// responds to output current with separate depletion/recharge time constants.
struct Power
{
    RC feedbackLow, feedbackHigh, transformer, dc;
    float feedback{}, depth{}, presence{}, gain{}, sag{}, mismatch{}, reservoir{}, last{};
    float depletion{}, recharge{};
    void configure(double rate, float fb, float lowHz, float highHz, float drive,
                   float supplySag, float recover, float asymmetry, float width,
                   float lowResonance, float highPresence)
    {
        feedback = fb; gain = drive; sag = supplySag; mismatch = asymmetry;
        depth = lowResonance; presence = highPresence;
        feedbackLow.configure(rate, lowHz); feedbackHigh.configure(rate, highHz);
        transformer.configure(rate, width); dc.configure(rate, 7.f);
        depletion = slew(rate, .006f); recharge = slew(rate, recover);
        reservoir = last = 0;
    }
    float tick(float input)
    {
        const float low = feedbackLow.low(last), high = last - feedbackHigh.low(last);
        const float returned = feedback * (last - depth * low - presence * high);
        const float supply = 1.f / (1.f + sag * reservoir);
        const float v = (input - returned) * gain / juce::jmax(.35f, supply);
        // Unequal complementary halves retain a small even-harmonic component.
        const float positive = soft(v + .18f) - soft(.18f);
        const float negative = soft(-v + .18f) - soft(.18f);
        float out = supply * .5f * ((1.f + mismatch) * positive - (1.f - mismatch) * negative);
        const float current = std::abs(out) + .3f * std::abs(out - last);
        reservoir += (current > reservoir ? depletion : recharge) * (current - reservoir);
        out = dc.high(transformer.low(out)); last = out;
        return out;
    }
};

struct Circuit
{
    int model{15}, channel{};
    std::array<Triode, 6> tube;
    Power power;
    RC input, bass, treble, bright, tighten, lowerSplit, upperSplit, feedback, finalDC;
    float localReturn{};
    void configure(double rate, int selectedModel, int selectedChannel)
    {
        model = selectedModel; channel = selectedChannel; localReturn = 0;
        input.configure(rate, 35); bass.configure(rate, 220); treble.configure(rate, 2400);
        bright.configure(rate, 950); tighten.configure(rate, 135);
        lowerSplit.configure(rate, 115); upperSplit.configure(rate, 1200);
        feedback.configure(rate, 1700); finalDC.configure(rate, 8);
        for (auto& stage : tube) stage.configure(rate, 65, 15000, .12f, .12f, .08f);
        switch (model)
        {
            case 15: // Four routes: open -> crunch -> cascade -> tightly coupled gain.
                input.configure(rate, channel == 3 ? 78.f : 38.f);
                tube[0].configure(rate, 55, 17000, .07f, .22f, .025f);
                tube[1].configure(rate, channel >= 2 ? 155.f : 62.f, 13500, -.20f, .09f, .13f);
                tube[2].configure(rate, 105, 11200, .26f, .13f, .06f);
                tube[3].configure(rate, 180, 9800, -.31f, .06f, .18f);
                power.configure(rate, .29f, 94, 3000, 1.85f, .13f, .09f, .018f, 12500, .43f, .35f);
                break;
            case 16: // Compact output pair, early clean EQ, extra lead cascade.
                input.configure(rate, 54);
                tube[0].configure(rate, 78, 15500, .035f, .27f, .03f);
                tube[1].configure(rate, 180, 11800, -.27f, .05f, .19f);
                tube[2].configure(rate, 120, 10000, .16f, .19f, .09f);
                power.configure(rate, .22f, 125, 2700, 2.1f, .38f, .135f, .025f, 11200, .26f, .28f);
                break;
            case 17: // Four increasing cascades with stiff global feedback.
                input.configure(rate, 46);
                tube[0].configure(rate, 70, 15500, .06f, .12f, .03f);
                tube[1].configure(rate, 115, 13000, -.14f, .13f, .11f);
                tube[2].configure(rate, 170, 12000, .30f, .08f, .15f);
                tube[3].configure(rate, 85, 10800, -.08f, .22f, .04f);
                power.configure(rate, .55f, 82, 2200, 2.0f, .095f, .075f, .009f, 13800, .62f, .31f);
                break;
            case 18: // Bass fundamentals split before preamp saturation; stiff output bank.
                input.configure(rate, 9); bass.configure(rate, 180); treble.configure(rate, 2100);
                tube[0].configure(rate, 14, 11500, .15f, .35f, .05f);
                tube[1].configure(rate, 32, 8500, -.045f, .27f, .09f);
                tube[2].configure(rate, 12, 7600, .07f, .25f, .035f);
                power.configure(rate, .62f, 48, 1750, 1.45f, .24f, .22f, .01f, 8700, .23f, .12f);
                break;
            case 19: // Two parallel input triodes, mixed ahead of common stages.
                input.configure(rate, 17); bass.configure(rate, 260); treble.configure(rate, 1900);
                tube[0].configure(rate, 24, 13500, .21f, .26f, .065f);
                tube[1].configure(rate, 95, 15800, -.055f, .20f, .04f);
                tube[2].configure(rate, 38, 10700, .12f, .11f, .16f);
                tube[3].configure(rate, 22, 9200, -.13f, .29f, .10f);
                power.configure(rate, .24f, 62, 1900, 2.3f, .33f, .26f, .026f, 9800, .51f, .13f);
                break;
            case 20: // EP has parallel frequency-shaped drive; KK has serial gain sections.
                input.configure(rate, channel == 2 ? 28.f : 60.f);
                tube[0].configure(rate, 92, 16600, .05f, .11f, .07f);
                tube[1].configure(rate, 210, 13400, -.32f, .06f, .17f);
                tube[2].configure(rate, 135, 11200, .19f, .10f, .12f);
                tube[3].configure(rate, 85, 10500, -.12f, .17f, .065f);
                tube[4].configure(rate, 240, 14000, .29f, .07f, .12f);
                power.configure(rate, .42f, 89, 3300, 1.75f, .11f, .065f, .018f, 14200, .41f, .46f);
                break;
            case 21: // OD-only cascade: cold-biased stage and follower compression.
                input.configure(rate, 42);
                tube[0].configure(rate, 72, 15600, .065f, .12f, .06f);
                tube[1].configure(rate, 180, 12400, -.52f, .045f, .22f);
                tube[2].configure(rate, 100, 10600, .23f, .11f, .10f);
                tube[3].configure(rate, 65, 10000, .31f, .38f, .035f);
                power.configure(rate, .36f, 90, 2700, 1.7f, .19f, .14f, .012f, 11700, .31f, .26f);
                break;
            case 22: // Wide lead coupling, low-frequency feedback, clean route bypasses cascade.
                input.configure(rate, channel == 0 ? 27.f : 34.f);
                tube[0].configure(rate, 44, 14900, .11f, .16f, .07f);
                tube[1].configure(rate, 82, 11200, -.36f, .075f, .23f);
                tube[2].configure(rate, 55, 9000, .18f, .18f, .14f);
                tube[3].configure(rate, 105, 8300, -.14f, .12f, .085f);
                power.configure(rate, .28f, 68, 1800, 2.0f, .29f, .18f, .024f, 10400, .65f, .42f);
                break;
            default: break;
        }
    }
    float stack(float x, float low, float mid, float high)
    {
        const float b = bass.low(x), t = treble.low(x);
        return low * b + mid * (t - b) + high * (x - t);
    }
    float tick(float sample, float drive)
    {
        const float in = input.high(sample);
        float x = 0;
        switch (model)
        {
            case 15:
                x = tube[0].tick(in, 1.4f + drive * (channel == 0 ? 3.f : (channel == 3 ? 5.5f : 11.f)));
                if (channel == 0) x = tube[1].tick(stack(x, .85f, .57f, 1.f), 1.15f);
                else {
                    x = tube[1].tick(x, 1.4f + drive * 3.8f);
                    if (channel >= 2) x = tube[2].tick(x, 1.2f + drive * 2.7f);
                    if (channel == 3) x = tube[3].tick(tighten.high(x), 1.15f + drive * 1.5f);
                    x = stack(x, .72f, .59f, .84f);
                }
                x = power.tick(x); break;
            case 16:
                // Feedback encloses the first stage only. Returning the full
                // lead cascade here would exceed unity loop gain and could
                // sustain a signal after the input ends.
                x = tube[0].tick(in - .035f * localReturn, 1.25f + drive * (channel == 0 ? 3.2f : 14.f));
                localReturn = feedback.low(x);
                if (channel == 0) x = stack(x, .82f, .64f, 1.04f);
                else {
                    x = tube[1].tick(x, 1.6f + drive * 4.2f);
                    x = tube[2].tick(x + .13f * bright.high(x), 1.35f + drive * 1.9f);
                    x = stack(x, .67f, .66f, .83f);
                }
                x = power.tick(x); break;
            case 17:
                x = tube[0].tick(in, 1.15f + drive * (channel == 0 ? 2.9f : (channel == 3 ? 5.5f : 8.5f)));
                if (channel == 0) x = stack(x, .95f, .63f, .9f);
                else {
                    x = tube[1].tick(x, 1.1f + drive * 3.9f);
                    if (channel >= 2) x = tube[2].tick(x, 1.2f + drive * 3.5f);
                    x = stack(x, .78f, channel == 3 ? .94f : .71f, .8f);
                    if (channel == 3) x = tube[3].tick(x, 1.3f + drive * .9f);
                }
                x = power.tick(x); break;
            case 18: {
                const float fundamental = lowerSplit.low(in);
                x = tube[0].tick(in, 1.15f + drive * 6.3f);
                x = stack(x, .99f, .73f, .78f);
                const float body = lowerSplit.memory;
                x = tube[1].tick(x - .42f * fundamental, 1.1f + drive * 1.6f);
                x = tube[2].tick(.78f * x + .62f * body, 1.1f + drive * .65f);
                x = power.tick(x); break;
            }
            case 19: {
                const float normal = tube[0].tick(in, 1.3f + drive * 7.8f);
                const float brilliant = tube[1].tick(in + .68f * bright.high(in), 1.4f + drive * 8.2f);
                x = channel == 0 ? normal : (channel == 1 ? brilliant : .62f * normal + .62f * brilliant);
                x = tube[2].tick(x, 1.1f + drive * 1.7f);
                x = tube[3].tick(stack(x, .94f, .57f, .81f), 1.0f + drive * 1.6f);
                x = power.tick(x); break;
            }
            case 20:
                if (channel == 2) {
                    x = tube[0].tick(in, 1.05f + drive * 2.9f);
                    x = stack(x, .94f, .67f, 1.02f);
                } else if (channel == 0) {
                    const float low = lowerSplit.low(in), upper = in - low;
                    const float girth = tube[0].tick(low + .52f * upper, 1.8f + drive * 14.f);
                    const float grind = tube[4].tick(upper, 1.4f + drive * 20.f);
                    x = tube[1].tick(.73f * girth + .38f * grind, 1.4f + drive * 4.1f);
                    x = tube[2].tick(stack(x, .63f, .63f, .95f), 1.2f + drive * 1.9f);
                } else {
                    x = tube[0].tick(in, 1.4f + drive * 6.f);
                    x = tube[1].tick(x, 1.3f + drive * 3.9f);
                    x = stack(x, .82f, .84f, .68f);
                    x = tube[2].tick(x, 1.5f + drive * 2.2f);
                    x = tube[3].tick(x, 1.1f + drive * 1.2f);
                }
                x = power.tick(x); break;
            case 21:
                x = tube[0].tick(in, 1.4f + drive * 10.5f);
                x = tube[1].tick(x + .17f * bright.high(x), 1.4f + drive * 4.2f);
                x = tube[2].tick(x, 1.6f + drive * 3.2f);
                x = tube[3].tick(x, 1.25f); // follower/local cathode compression before stack
                x = power.tick(stack(x, .78f, .77f, .79f)); break;
            case 22:
                x = tube[0].tick(in, 1.3f + drive * (channel == 0 ? 3.1f : 8.f));
                if (channel == 0) x = stack(x, .99f, .52f, .99f);
                else {
                    x = tube[1].tick(x, 1.3f + drive * 4.6f);
                    // Broad low-frequency path reaches the late stage, unlike the tight cascades.
                    const float low = upperSplit.low(x);
                    x = tube[2].tick(.90f * low + .72f * (x - low), 1.2f + drive * 2.1f);
                    x = tube[3].tick(stack(x, 1.03f, .52f, .71f), 1.1f + drive * .9f);
                }
                x = power.tick(x); break;
            default: x = sample; break;
        }
        return finalDC.high(x);
    }
};
} // namespace newAmpDetail

// Per-oversampling-path state; all storage and coefficients are prepared outside
// the sample loop. Channels are independent, including supply reservoirs.
class NewAmpDSP
{
    std::array<std::array<newAmpDetail::Circuit, 2>, 2> circuits;
    double sampleRate{48000};
    int selectedModel{15}, selectedChannel{}, requestedModel{15}, requestedChannel{};
    int active{}, old{}, fadeRemaining{}, fadeLength{960};
    void install(int slot, int model, int channel)
    {
        for (auto& circuit : circuits[(size_t)slot]) circuit.configure(sampleRate, model, channel);
    }
public:
    void prepare(double rate) { sampleRate = rate; fadeLength = juce::jmax(1, int(rate * .02)); reset(); }
    void set(int model, int channel)
    {
        requestedModel = juce::jlimit(15, 22, model);
        requestedChannel = juce::jlimit(0, newAmpChannelCount(requestedModel) - 1, channel);
    }
    void reset()
    {
        selectedModel = requestedModel; selectedChannel = requestedChannel;
        active = old = 0; fadeRemaining = 0; install(0, selectedModel, selectedChannel); install(1, selectedModel, selectedChannel);
    }
    void process(juce::dsp::AudioBlock<float>& block, juce::SmoothedValue<float>& drive)
    {
        // Queue rapid channel changes until the previous crossfade has ended.
        // Install at a block boundary: coefficient calculation never occurs per sample.
        if (fadeRemaining == 0 && (selectedModel != requestedModel || selectedChannel != requestedChannel)) {
            old = active; active = 1 - active;
            selectedModel = requestedModel; selectedChannel = requestedChannel;
            install(active, selectedModel, selectedChannel); fadeRemaining = fadeLength;
        }
        for (size_t n = 0; n < block.getNumSamples(); ++n) {
            const float amount = drive.getNextValue();
            const float mix = fadeRemaining > 0 ? 1.f - float(fadeRemaining) / float(fadeLength) : 1.f;
            for (size_t c = 0; c < block.getNumChannels(); ++c) {
                const float input = block.getSample((int)c, (int)n);
                const float current = circuits[(size_t)active][c].tick(input, amount);
                const float previous = fadeRemaining > 0 ? circuits[(size_t)old][c].tick(input, amount) : current;
                block.setSample((int)c, (int)n, previous + mix * (current - previous));
            }
            if (fadeRemaining > 0) --fadeRemaining;
        }
    }
};
} // namespace spectralforge
