// Full-product cost of Room Core (docs/ROOM_CORE_1_3_3.md section 6): the
// whole processor at default settings with the room off and on (all sends
// 100 %), per routing mode and size, as a share of real time.
//
//   ChimeraRoomCost [rate=48000] [block=128] [seconds=10] [runs=5]
//
// Off and on alternate every block. Totals are the best of `runs`; the room
// share is the median of the per-run differences.
#include "PluginProcessor.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>
#include <memory>
#include <vector>

namespace {
void set(ChimeraProcessor& p, const juce::String& id, float value) {
    auto* parameter = p.parameters().getParameter(id);
    if (parameter == nullptr) throw std::runtime_error(("unknown parameter " + id).toStdString());
    parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
}
// Times both processors on the same input, alternating every block, so
// machine drift affects off and on alike. Returns {off, on} in % of real time.
std::pair<double, double> share(ChimeraProcessor& off, ChimeraProcessor& on, double rate, int block, double seconds) {
    juce::AudioBuffer<float> audio(2, block), copy(2, block); juce::MidiBuffer midi;
    const int blocks = int(seconds * rate / block);
    long clock = 0; double busy[2]{};
    for (int b = 0; b < blocks; ++b) {
        for (int n = 0; n < block; ++n, ++clock) {
            const double t = double(clock) / rate;
            // Decaying plucked chord, retriggered every second.
            const double env = std::exp(-3.0 * std::fmod(t, 1.0));
            const float x = float(.15 * env * (std::sin(6.283 * 82.4 * t) + std::sin(6.283 * 123.5 * t) + std::sin(6.283 * 164.8 * t)));
            audio.setSample(0, n, x); audio.setSample(1, n, x);
        }
        copy.makeCopyOf(audio, true);
        for (int k = 0; k < 2; ++k) {
            const int which = (b + k) % 2;   // alternate who goes first
            auto& buffer = which == 0 ? audio : copy;
            const auto start = std::chrono::steady_clock::now();
            (which == 0 ? off : on).processBlock(buffer, midi);
            busy[which] += std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
        }
    }
    const double real = blocks * double(block) / rate;
    return {100.0 * busy[0] / real, 100.0 * busy[1] / real};
}
} // namespace

int main(int argc, char** argv) {
    juce::ScopedJuceInitialiser_GUI init;
    double rate = 48000, seconds = 10; int block = 128, runs = 5;
    for (int i = 1; i < argc; ++i) {
        const juce::String arg(argv[i]); const auto key = arg.upToFirstOccurrenceOf("=", false, false);
        const auto value = arg.fromFirstOccurrenceOf("=", false, false);
        if (key == "rate") rate = value.getDoubleValue(); else if (key == "block") block = value.getIntValue();
        else if (key == "seconds") seconds = value.getDoubleValue(); else if (key == "runs") runs = value.getIntValue();
    }
    const char* modes[] = {"Classic", "Dual", "Matrix"};
    const char* sizes[] = {"Small", "Medium", "Large"};
    std::cout << "ROOM_COST rate=" << rate << " block=" << block << " seconds=" << seconds << " runs=" << runs << "\n";
    for (int mode = 0; mode < 3; ++mode)
        for (int size = 0; size < 3; ++size) {
            // Heap: a processor is too large for the stack.
            auto offOwner = std::make_unique<ChimeraProcessor>(), onOwner = std::make_unique<ChimeraProcessor>();
            auto &off = *offOwner, &on = *onOwner;
            for (auto* p : {&off, &on}) {
                set(*p, "mode", float(mode)); set(*p, "roomSize", float(size));
                for (int lane = 1; lane <= 3; ++lane) set(*p, "roomSend" + juce::String(lane), 100);
                p->setRateAndBufferSizeDetails(rate, block); p->prepareToPlay(rate, block);
            }
            set(on, "roomOn", 1);
            share(off, on, rate, block, 1);   // warm up
            double bestOff = 1e9, bestOn = 1e9; std::vector<double> room;
            for (int r = 0; r < runs; ++r) {
                const auto [a, b] = share(off, on, rate, block, seconds);
                bestOff = std::min(bestOff, a); bestOn = std::min(bestOn, b); room.push_back(b - a);
            }
            std::sort(room.begin(), room.end());
            const double medianRoom = room[room.size() / 2], spread = room.back() - room.front();
            std::cout << "ROOM_COST mode=" << modes[mode] << " size=" << sizes[size] << " off=" << bestOff << "% on=" << bestOn
                      << "% room=" << medianRoom << "% (median, spread " << spread << ") of real time\n";
        }
    return 0;
}
