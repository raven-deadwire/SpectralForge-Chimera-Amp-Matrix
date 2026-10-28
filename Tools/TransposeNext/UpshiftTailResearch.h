#pragma once
#include "TransientSplitPitch.h"
#include <string>
#include <stdexcept>

namespace transposeNext {
// Offline tools only. Existing engine names and plugin defaults are unchanged.
// Suffixes are deliberately not new version numbers or accepted engines.
struct UpshiftTailResearch {
    std::string engine;
    double guardMs{4}, holdMs{25};
    bool enabled{};

    explicit UpshiftTailResearch(std::string name) : engine(std::move(name)) {
        for (const auto& suffix : {std::string("g8"), std::string("h40"),
                                   std::string("g8h40"), std::string("g12h50")}) {
            const auto tag = "-" + suffix;
            if (engine.size() <= tag.size() || engine.compare(engine.size()-tag.size(), tag.size(), tag) != 0)
                continue;
            engine.resize(engine.size()-tag.size());
            if (engine != "v3" && engine != "v4" && engine != "corr40-split-v3" && engine != "corr40-split-v4")
                throw std::invalid_argument("tail research requires v3 or v4");
            enabled = true;
            if (suffix == "g8") guardMs = 8;
            if (suffix == "h40") holdMs = 40;
            if (suffix == "g8h40") { guardMs = 8; holdMs = 40; }
            if (suffix == "g12h50") { guardMs = 12; holdMs = 50; }
            return;
        }
    }
    void apply(TransientSplitPitch& pitch, int shift) const {
        if (!enabled) return;
        if (shift < -2 || shift > 2)
            throw std::invalid_argument("tail research is bounded to +/-2 semitones");
        pitch.setHighReanchorResearch(guardMs, holdMs);
    }
};
}
