// SPDX-License-Identifier: GPL-2.0-or-later
// Prints a Chimera state for obs_app_test.py, as another host would store it:
//   chimera-obs-state-tool <SpectralForge Chimera.vst3> --list
//   chimera-obs-state-tool <SpectralForge Chimera.vst3> <exact parameter title> <normalized value>
// The second form prints {"component": ..., "controller": ..., "title": ...}
// with the base64 states the OBS filter keeps in its settings.
#include "base64.hpp"
#include "chimera-host.hpp"

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

using namespace spectralforge::obs;

int main(int argc, char** argv) {
    if (argc != 3 && argc != 4) {
        std::cerr << "usage: chimera-obs-state-tool <bundle> --list | <parameter title> <normalized value>\n";
        return 2;
    }
    ChimeraHost host;
    std::string error;
    if (!host.load(argv[1], error)) {
        std::cerr << "FAIL load Chimera: " << error << '\n';
        return 1;
    }
    const std::string wanted = argv[2];
    for (int i = 0; i < host.parameterCount(); ++i) {
        std::uint32_t id = 0;
        std::string title;
        int steps = 0;
        if (!host.parameterInfo(i, id, title, steps)) continue;
        if (wanted == "--list") {
            std::cout << id << '\t' << steps << '\t' << host.parameter(id) << '\t' << title << '\n';
            continue;
        }
        if (title != wanted) continue;
        const double value = std::atof(argv[3]);
        if (!host.setParameter(id, value) || std::abs(host.parameter(id) - value) > 1e-6) {
            std::cerr << "FAIL could not set '" << title << "'\n";
            return 1;
        }
        std::vector<std::uint8_t> component, controller;
        if (!host.getState(component, controller)) {
            std::cerr << "FAIL getState\n";
            return 1;
        }
        std::cout << "{\"title\": \"" << title << "\", \"component\": \"" << base64Encode(component)
                  << "\", \"controller\": \"" << base64Encode(controller) << "\"}\n";
        return 0;
    }
    if (wanted == "--list") return 0;
    std::cerr << "FAIL no parameter titled '" << wanted << "'\n";
    return 1;
}
