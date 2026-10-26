// SPDX-License-Identifier: GPL-2.0-or-later
// Install-layout check: lets libobs discover the installed module the way OBS
// Studio does for user plugins, from a module path pattern, and checks that
// the filter registers with its translated name (so the data folder was found).
// Usage: chimera-obs-layout-tests <bin path pattern> <data path pattern>
//   Linux: ~/.config/obs-studio/plugins/%module%/bin/64bit  .../%module%/data
//   macOS: ~/Library/Application Support/obs-studio/plugins/%module%.plugin/Contents/MacOS
//          .../%module%.plugin/Contents/Resources
#include <obs.h>
#include <util/base.h>

#include <cstdarg>
#include <cstdio>
#include <iostream>
#include <string>

namespace {
std::string identity;
void logHandler(int, const char* format, va_list args, void*) {
    char line[4096];
    std::vsnprintf(line, sizeof(line), format, args);
    if (std::string(line).find("[chimera-obs] module ") != std::string::npos) identity = line;
}
}

int main(int argc, char** argv) {
    if (argc != 3) { std::cerr << "usage: chimera-obs-layout-tests <bin pattern> <data pattern>\n"; return 2; }
    base_set_log_handler(logHandler, nullptr);
    if (!obs_startup("en-US", nullptr, nullptr)) { std::cerr << "FAIL obs_startup\n"; return 1; }
    obs_add_module_path(argv[1], argv[2]);
    obs_load_all_modules();
    obs_post_load_modules();
    const char* name = obs_source_get_display_name("spectralforge_chimera_filter");
    const bool ok = name != nullptr && std::string(name) == "SpectralForge Chimera" && !identity.empty();
    std::cout << (ok ? "PASS" : "FAIL") << " installed layout: filter " << (name ? name : "(not registered)")
              << "; " << (identity.empty() ? "no module identity line" : identity) << '\n';
    obs_shutdown();
    return ok ? 0 : 1;
}
