#pragma once
#include "BetaLifecycle.h"
#include "ReleaseInfo.h"
#include <chrono>

namespace spectralforge::beta {
using Clock = std::int64_t (*)() noexcept;
inline std::int64_t utcNow() noexcept {
    return std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}
inline constexpr Metadata compiledMetadata() noexcept {
    // Unconfigured development builds are conspicuously marked and cannot be
    // packaged for publication. They do not impose an invented expiry date.
    return {CHIMERA_PREVIEW_VERSION, CHIMERA_SOURCE_SHA, CHIMERA_BETA_EXPIRY_UNIX,
            CHIMERA_BETA_WARNING_LEAD_SECONDS, CHIMERA_BETA_CONFIGURED != 0};
}
static_assert(std::string_view(CHIMERA_BETA_PACKAGE_VERSION)==CHIMERA_PREVIEW_VERSION);
static_assert(std::string_view(CHIMERA_BETA_SOURCE_SHA)==CHIMERA_SOURCE_SHA);
} // namespace spectralforge::beta
