#include "BetaLifecycle.h"
#include "BetaLifecycleDisplay.h"
#include <limits>

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
    constexpr Metadata beta1{"1.3.2-beta.1", source, 1802357940, 1209600, true};
    constexpr Metadata beta2{"1.3.2-beta.2", source, 1806505140, 1209600, true};
    static_assert(spectralforge::beta::evaluate(beta1,1802357940).status==Status::expired);
    static_assert(spectralforge::beta::evaluate(beta2,1802357940).status==Status::valid);
    static_assert(spectralforge::beta::evaluate(beta2,1806505139).status==Status::warning);
    static_assert(spectralforge::beta::evaluate(beta2,1806505140).status==Status::expired);
    static_assert(spectralforge::beta::evaluate(beta2,1806505141).status==Status::expired);
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

    using spectralforge::beta::evaluate;
    using spectralforge::beta::remainingText;
    constexpr Metadata display{"1.3.2-beta.1", source, 1'800'000'000, 1209600, true};
    const struct { std::int64_t remaining; const char* text; } cases[] = {
        {1209601, "Remaining: 14 days 00 hours 00 minutes 01 seconds."},
        {1209600, "Remaining: 14 days 00 hours 00 minutes 00 seconds."},
        {90061, "Remaining: 1 day 01 hours 01 minutes 01 seconds."},
        {86400, "Remaining: 1 day 00 hours 00 minutes 00 seconds."},
        {86399, "Remaining: 0 days 23 hours 59 minutes 59 seconds."},
        {3600, "Remaining: 0 days 01 hours 00 minutes 00 seconds."},
        {3599, "Remaining: 0 days 00 hours 59 minutes 59 seconds."},
        {60, "Remaining: 0 days 00 hours 01 minutes 00 seconds."},
        {59, "Remaining: 0 days 00 hours 00 minutes 59 seconds."},
        {1, "Remaining: 0 days 00 hours 00 minutes 01 seconds."},
        {0, "Remaining: 0 days 00 hours 00 minutes 00 seconds (expired)."},
        {-1, "Remaining: 0 days 00 hours 00 minutes 00 seconds (expired)."},
        {-86401, "Remaining: 0 days 00 hours 00 minutes 00 seconds (expired)."},
    };
    for(const auto& c:cases)
        require(remainingText(evaluate(display, display.expiresAtUnix-c.remaining))==c.text,
                "Countdown rounding or inclusive expiry boundary is wrong");
    require(remainingText(evaluate(display,0))=="Remaining: unavailable (check system clock).",
            "Invalid clock showed fabricated remaining time");
    require(remainingText(evaluate(missingExpiry,1)).find("invalid beta metadata")!=std::string::npos,
            "Invalid metadata showed fabricated remaining time");
    require(remainingText(evaluate(stable,1)).find("expiry not enabled")!=std::string::npos,
            "Non-beta build showed a countdown");
    require(remainingText({Status::expired,std::numeric_limits<std::int64_t>::min()})==cases[10].text,
            "Very old expiry overflowed the countdown");
    InstanceLatch corrected(display, display.expiresAtUnix);
    require(remainingText(corrected.observe(display.expiresAtUnix-90061))==cases[2].text,
            "Clock rollback left a stale countdown");
    require(!corrected.processingPermitted(),"Displaying a corrected clock unmuted the instance");

    // Beta tester code: lifts expiry and warning for every later time, never
    // overrides invalid metadata, unmutes on grant, never mutes on removal.
    static_assert(evaluate(beta, 9'999'999, true).status == Status::testerExtended);
    static_assert(evaluate(beta, 1'999'950, true).status == Status::testerExtended);
    static_assert(evaluate(missingExpiry, 9'999'999, true).status == Status::configurationError);
    static_assert(evaluate(stable, 9'999'999, true).status == Status::notBeta);
    require(InstanceLatch(beta, 9'999'999, true).processingPermitted(),
            "a tester instance opened after expiry was muted");
    InstanceLatch late(beta, 2'000'000);
    require(!late.processingPermitted(), "expired instance not muted before the tester code");
    require(late.setTesterExtension(true) && late.processingPermitted(),
            "entering a tester code did not unmute an expired instance");
    require(late.observe(9'999'999).status == Status::testerExtended && late.testerExtension(),
            "tester extension not visible after entry");
    require(!late.setTesterExtension(false) && late.processingPermitted(),
            "removing a tester code muted an instance that was playing");
    require(late.observe(2'000'001).status == Status::expired, "removed tester code still lifted the expiry");
    InstanceLatch broken(missingExpiry, 1);
    require(!broken.setTesterExtension(true) && !broken.processingPermitted(),
            "tester code overrode invalid beta metadata");
    require(remainingText(evaluate(beta, 9'999'999, true)) == "Remaining: no expiry (beta tester code).",
            "tester extension showed a countdown");

    std::cout << "PASS: beta lifecycle policy, active-instance latch and tester extension\n";
}
