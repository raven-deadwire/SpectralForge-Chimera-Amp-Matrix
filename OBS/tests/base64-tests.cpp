// SPDX-License-Identifier: GPL-2.0-or-later
#include "base64.hpp"
#include <iostream>
#include <stdexcept>

namespace {
void check(bool ok, const char* what) { if (!ok) throw std::runtime_error(what); }
}

int main() {
    using namespace spectralforge::obs;
    try {
        // RFC 4648 section 10 vectors.
        const char* plain[] = {"", "f", "fo", "foo", "foob", "fooba", "foobar"};
        const char* coded[] = {"", "Zg==", "Zm8=", "Zm9v", "Zm9vYg==", "Zm9vYmE=", "Zm9vYmFy"};
        for (int i = 0; i < 7; ++i) {
            const std::string p = plain[i];
            const std::vector<std::uint8_t> bytes(p.begin(), p.end());
            check(base64Encode(bytes) == coded[i], "encode vector");
            check(base64Decode(coded[i]) == bytes, "decode vector");
        }
        std::vector<std::uint8_t> all(256 * 3);
        for (size_t i = 0; i < all.size(); ++i) all[i] = static_cast<std::uint8_t>(i * 7 + 3);
        for (size_t n = 0; n < 40; ++n) {
            const std::vector<std::uint8_t> part(all.begin(), all.begin() + static_cast<long>(n * 19));
            check(base64Decode(base64Encode(part)) == part, "binary round trip");
        }
        for (const char* bad : {"Zg=", "Zg=a", "Z===", "Zm9v!A==", "Zg==Zm9v", "Zm9 v"})
            check(!base64Decode(bad).has_value(), "malformed input must be rejected");
        std::cout << "PASS base64: RFC 4648 vectors, binary round trip, malformed input rejected\n";
        return 0;
    } catch (const std::exception& e) { std::cerr << "FAIL " << e.what() << '\n'; return 1; }
}
