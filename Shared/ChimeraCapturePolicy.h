// SPDX-License-Identifier: MIT
// Copyright (c) 2026 RavenForge Luthier Intelligence
// Small VST3 ABI shared by the separately built product and OBS host.
#pragma once
#include <pluginterfaces/base/funknown.h>

namespace spectralforge {
// Presence of this interface requests a lifetime lock on comparison gain.
// This is an output policy, NOT a claim about transport or recording state.
// The plugin acknowledges only after installing the lock. The host must not
// process a plugin that did not acknowledge. No audio-thread calls are needed.
inline constexpr Steinberg::TUID chimeraCapturePolicyId =
    INLINE_UID(0x89AF821D, 0x7BCB49E0, 0x96E5C2BE, 0xB65803D9);
struct IChimeraCapturePolicy : Steinberg::FUnknown {
    virtual Steinberg::tresult PLUGIN_API acknowledgeCapturePolicy() = 0;
};
}
