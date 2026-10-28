#pragma once
#include "BetaLifecycle.h"
#include <string>

namespace spectralforge::beta {

// Presentation only, on the control/UI thread. Reuse one UTC evaluation for
// both countdown and status; never decrement a UI-owned clock or round days up.
inline std::string remainingText(const Evaluation& current) {
    switch (current.status) {
        case Status::notBeta: return "Remaining: unavailable (expiry not enabled).";
        case Status::clockUntrusted: return "Remaining: unavailable (check system clock).";
        case Status::configurationError: return "Remaining: unavailable (invalid beta metadata).";
        case Status::testerExtended: return "Remaining: no expiry (beta tester code).";
        default: break;
    }
    const auto seconds = current.secondsRemaining > 0 ? current.secondsRemaining : 0;
    const auto twoDigits = [](std::int64_t value) {
        return (value < 10 ? "0" : "") + std::to_string(value);
    };
    auto text = "Remaining: " + std::to_string(seconds / 86400) + (seconds / 86400 == 1 ? " day " : " days ")
        + twoDigits(seconds / 3600 % 24) + " hours "
        + twoDigits(seconds / 60 % 60) + " minutes "
        + twoDigits(seconds % 60) + " seconds";
    return text + (current.status == Status::expired ? " (expired)." : ".");
}

} // namespace spectralforge::beta
