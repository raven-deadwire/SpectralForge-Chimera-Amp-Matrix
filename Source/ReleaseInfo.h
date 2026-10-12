#pragma once
#include "ChimeraBuildVersion.h"

namespace spectralforge::release {
inline constexpr auto version = CHIMERA_PREVIEW_VERSION;
inline constexpr auto displayVersion = CHIMERA_PREVIEW_DISPLAY;
inline constexpr auto channel = CHIMERA_UPDATE_CHANNEL;
inline constexpr auto repository = "raven-deadwire/SpectralForge-Chimera-Amp-Matrix";
inline constexpr auto repositoryUrl = "https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix";
inline constexpr auto releasesApi = "https://api.github.com/repos/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/releases?per_page=30";
inline constexpr auto manifestName = CHIMERA_UPDATE_MANIFEST;
#ifndef CHIMERA_BUILD_REVISION
#define CHIMERA_BUILD_REVISION "source-archive"
#endif
inline constexpr auto revision = CHIMERA_BUILD_REVISION;
}
