#include "BetaLifecycle.h"

#include <cstdlib>
#include <iostream>

namespace {
using spectralforge::beta::InstanceLatch;
using spectralforge::beta::Metadata;
using spectralforge::beta::Status;

constexpr auto source = "0123456789abcdef0123456789abcdef01234567";

void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}
}

int main() {
    constexpr Metadata beta{"1.3.1-beta.2", source, 2'000'000, 100, true};
    constexpr Metadata stable{"1.3.1", source, 0, 0, false};

    static_assert(spectralforge::beta::evaluate(stable, 9'999'999).status == Status::notBeta);
    static_assert(spectralforge::beta::evaluate(beta, 1'999'899).status == Status::valid);
    static_assert(spectralforge::beta::evaluate(beta, 1'999'900).status == Status::warning);
    static_assert(spectralforge::beta::evaluate(beta, 2'000'000).status == Status::expired);

    require(InstanceLatch(beta, 1'999'000).processingPermitted(),
            "an instance activated before expiry must process");
    const InstanceLatch active(beta, 1'999'999);
    require(active.processingPermitted(), "pre-expiry activation was rejected");
    require(active.observe(2'000'001).status == Status::expired,
            "crossing expiry did not become visible");
    require(active.processingPermitted(),
            "crossing expiry changed an already active instance");

    InstanceLatch expired(beta, 2'000'000);
    require(!expired.processingPermitted(), "new expired instance retained full-processing permission");
    require(expired.activation().status == Status::expired, "expiry boundary is not inclusive");
    require(expired.recoverFromClockCorrection(1'999'999),
            "corrected local clock did not recover a falsely expired instance");
    require(expired.processingPermitted(), "clock correction recovery was not published to processing");

    const InstanceLatch badClock(beta, -1);
    require(badClock.processingPermitted(), "untrusted local clock caused irreversible lockout");
    require(badClock.activation().status == Status::clockUntrusted,
            "untrusted clock was not distinguished from expiry");

    constexpr Metadata missingVersion{"", source, 2'000'000, 100, true};
    constexpr Metadata shortSha{"1.3.1-beta.2", "0123456789", 2'000'000, 100, true};
    constexpr Metadata nonHexSha{"1.3.1-beta.2", "zzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz", 2'000'000, 100, true};
    constexpr Metadata wrongChannel{"1.3.1-rc.1", source, 2'000'000, 100, true};
    constexpr Metadata missingExpiry{"1.3.1-beta.2", source, 0, 100, true};
    require(!InstanceLatch(missingVersion, 1'000'000).processingPermitted(),
            "missing package identity was accepted");
    require(!InstanceLatch(shortSha, 1'000'000).processingPermitted(),
            "short source identity was accepted");
    require(!InstanceLatch(nonHexSha, 1'000'000).processingPermitted(),
            "non-hex source identity was accepted");
    require(!InstanceLatch(wrongChannel, 1'000'000).processingPermitted(),
            "non-beta package identity was accepted");
    require(!InstanceLatch(missingExpiry, 1'000'000).processingPermitted(),
            "missing beta expiry was accepted");

    std::cout << "PASS: beta lifecycle policy and active-instance latch\n";
}
