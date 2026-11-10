// SPDX-License-Identifier: GPL-2.0-or-later
// Test fixture only. Installed identically in the module-absent and
// module-registered controls; never shipped with the Chimera OBS filter.
// GetVersion can succeed after SCENE_COLLECTION_CHANGED but before the
// frontend finishes constructing its UI. Observe the actual later event.
#include <obs-module.h>
#include <obs-frontend-api.h>
#include <windows.h>

OBS_DECLARE_MODULE()

MODULE_EXPORT const char* obs_module_description(void) {
    return "OBS frontend startup observer for isolated Chimera tests only";
}

namespace {
bool registered = false;

void frontendEvent(enum obs_frontend_event event, void*) {
    if (event == OBS_FRONTEND_EVENT_FINISHED_LOADING)
        blog(LOG_INFO, "[chimera-obs-startup-observer] frontend finished loading pid=%lu",
             static_cast<unsigned long>(GetCurrentProcessId()));
    if (event == OBS_FRONTEND_EVENT_EXIT) {
        // OBS destroys its frontend callbacks before unloading modules. Remove
        // this callback while that API is still alive, not after teardown.
        obs_frontend_remove_event_callback(frontendEvent, nullptr);
        registered = false;
    }
}
}

bool obs_module_load(void) {
    obs_frontend_add_event_callback(frontendEvent, nullptr);
    registered = true;
    blog(LOG_INFO, "[chimera-obs-startup-observer] waiting for frontend finished loading pid=%lu",
         static_cast<unsigned long>(GetCurrentProcessId()));
    return true;
}

void obs_module_unload(void) {
    if (registered) {
        obs_frontend_remove_event_callback(frontendEvent, nullptr);
        registered = false;
    }
}
