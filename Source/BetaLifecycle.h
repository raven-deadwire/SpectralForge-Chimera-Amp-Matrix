#pragma once

#include <atomic>
#include <cstdint>
#include <string_view>

namespace spectralforge::beta {

enum class Status {
    notBeta,
    valid,
    warning,
    expired,
    clockUntrusted,
    configurationError,
    // A verified beta tester code lifts the expiry (and its warning) for every
    // beta build. It never overrides invalid build metadata.
    testerExtended
};

struct Metadata {
    std::string_view packageVersion;
    std::string_view sourceRevision;
    std::int64_t expiresAtUnix{};
    std::int64_t warningLeadSeconds{14 * 24 * 60 * 60};
    bool betaBuild{};
};

struct Evaluation {
    Status status{Status::configurationError};
    std::int64_t secondsRemaining{};
};

constexpr bool isLowerHexSha(std::string_view value) noexcept {
    if (value.size() != 40)
        return false;
    for (const auto c : value)
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f')))
            return false;
    return true;
}

constexpr bool hasBuildIdentity(const Metadata& metadata) noexcept {
    return metadata.packageVersion.find("-beta.") != std::string_view::npos
        && isLowerHexSha(metadata.sourceRevision);
}

constexpr Evaluation evaluate(const Metadata& metadata, std::int64_t nowUnix,
                              bool testerExtension = false) noexcept {
    if (!metadata.betaBuild)
        return {Status::notBeta, 0};
    if (!hasBuildIdentity(metadata) || metadata.expiresAtUnix <= 0 || metadata.warningLeadSeconds < 0)
        return {Status::configurationError, 0};
    if (testerExtension)
        return {Status::testerExtended, 0};
    // A bad local clock must not create an irreversible offline lockout. The
    // caller presents a warning and may re-evaluate after the clock is fixed.
    if (nowUnix <= 0)
        return {Status::clockUntrusted, 0};
    const auto remaining = metadata.expiresAtUnix - nowUnix;
    if (remaining <= 0)
        return {Status::expired, remaining};
    if (remaining <= metadata.warningLeadSeconds)
        return {Status::warning, remaining};
    return {Status::valid, remaining};
}

// Construct exactly once at the documented plugin-instance activation
// boundary. prepare/release, editor visibility, transport and render changes
// must not reconstruct this latch. observe() is bounded integer work suitable
// for a control/UI thread; the audio callback reads processingPermitted() only.
class InstanceLatch final {
    static_assert(std::atomic_bool::is_always_lock_free, "Audio decision must be lock-free");
public:
    InstanceLatch(const Metadata& metadata, std::int64_t activatedAtUnix, bool testerExtension = false) noexcept
        : metadata_(metadata), activation_(evaluate(metadata, activatedAtUnix, testerExtension)),
          processingPermitted_(activation_.status != Status::expired
                               && activation_.status != Status::configurationError),
          testerExtension_(testerExtension) {}

    [[nodiscard]] bool processingPermitted() const noexcept {
        return processingPermitted_.load(std::memory_order_acquire);
    }

    [[nodiscard]] constexpr Evaluation activation() const noexcept {
        return activation_;
    }

    [[nodiscard]] Evaluation observe(std::int64_t nowUnix) const noexcept {
        return evaluate(metadata_, nowUnix, testerExtension_.load(std::memory_order_acquire));
    }

    // Control thread: a tester code entered or confirmed after activation lifts
    // the expiry for this instance too, unmuting it if expiry had muted it.
    // Removing the code later never mutes an instance that is already playing.
    bool setTesterExtension(bool granted) noexcept {
        testerExtension_.store(granted, std::memory_order_release);
        if (!granted || activation_.status == Status::configurationError || processingPermitted())
            return false;
        processingPermitted_.store(true, std::memory_order_release);
        return true;
    }
    [[nodiscard]] bool testerExtension() const noexcept {
        return testerExtension_.load(std::memory_order_acquire);
    }

    // An obviously corrected clock can recover an instance that was rejected
    // at activation. This is intentionally a control-thread operation and an
    // honest serverless-clock tradeoff, not tamper-proof entitlement.
    [[nodiscard]] bool recoverFromClockCorrection(std::int64_t nowUnix) noexcept {
        if (activation_.status != Status::expired)
            return false;
        const auto corrected = evaluate(metadata_, nowUnix);
        if (corrected.status != Status::valid && corrected.status != Status::warning)
            return false;
        processingPermitted_.store(true, std::memory_order_release);
        return true;
    }

private:
    Metadata metadata_;
    Evaluation activation_;
    std::atomic_bool processingPermitted_{};
    std::atomic_bool testerExtension_{};
};

} // namespace spectralforge::beta
