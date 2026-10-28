// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "../../Shared/ChimeraCapturePolicy.h"
#include "public.sdk/source/vst/hosting/hostclasses.h"
#include <atomic>

namespace spectralforge::obs {
class CaptureHostApplication final : public Steinberg::Vst::HostApplication,
                                     public IChimeraCapturePolicy {
public:
    Steinberg::tresult PLUGIN_API queryInterface(const Steinberg::TUID iid, void** object) override {
        if(Steinberg::FUnknownPrivate::iidEqual(iid,chimeraCapturePolicyId)) {
            *object=static_cast<IChimeraCapturePolicy*>(this);addRef();return Steinberg::kResultOk;
        }
        return HostApplication::queryInterface(iid,object);
    }
    Steinberg::uint32 PLUGIN_API addRef() override {return HostApplication::addRef();}
    Steinberg::uint32 PLUGIN_API release() override {return HostApplication::release();}
    Steinberg::tresult PLUGIN_API acknowledgeCapturePolicy() override {
        acknowledged.store(true);return Steinberg::kResultOk;
    }
    bool capturePolicyAcknowledged() const noexcept {return acknowledged.load();}
private:
    std::atomic<bool> acknowledged{false};
};
}
