#pragma once

namespace spectralforge::release {
inline constexpr auto version = "1.0.0-beta.1";
inline constexpr auto displayVersion = "Open Beta 1.0";
inline constexpr auto channel = "beta";
inline constexpr auto repository = "raven-deadwire/SpectralForge-Chimera-Amp-Matrix";
inline constexpr auto repositoryUrl = "https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix";
inline constexpr auto releasesApi = "https://api.github.com/repos/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/releases?per_page=30";
inline constexpr auto manifestName = "update-beta.json";
#ifndef CHIMERA_BUILD_REVISION
#define CHIMERA_BUILD_REVISION "source-archive"
#endif
inline constexpr auto revision = CHIMERA_BUILD_REVISION;
}
