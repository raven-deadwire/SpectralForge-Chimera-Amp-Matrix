// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 RavenForge Luthier Intelligence
#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace spectralforge::obs {
// OBS settings are JSON; binary VST3 state is stored as RFC 4648 base64.
inline std::string base64Encode(const std::vector<std::uint8_t>& bytes) {
    static constexpr char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve((bytes.size() + 2) / 3 * 4);
    for (std::size_t i = 0; i < bytes.size(); i += 3) {
        const std::uint32_t n = std::uint32_t(bytes[i]) << 16
            | (i + 1 < bytes.size() ? std::uint32_t(bytes[i + 1]) << 8 : 0u)
            | (i + 2 < bytes.size() ? std::uint32_t(bytes[i + 2]) : 0u);
        out += alphabet[(n >> 18) & 63];
        out += alphabet[(n >> 12) & 63];
        out += i + 1 < bytes.size() ? alphabet[(n >> 6) & 63] : '=';
        out += i + 2 < bytes.size() ? alphabet[n & 63] : '=';
    }
    return out;
}

// Rejects anything that is not canonical padded base64 rather than guessing.
inline std::optional<std::vector<std::uint8_t>> base64Decode(const std::string& text) {
    const auto value = [](char c) -> int {
        if (c >= 'A' && c <= 'Z') return c - 'A';
        if (c >= 'a' && c <= 'z') return c - 'a' + 26;
        if (c >= '0' && c <= '9') return c - '0' + 52;
        return c == '+' ? 62 : c == '/' ? 63 : -1;
    };
    if (text.size() % 4 != 0) return std::nullopt;
    std::vector<std::uint8_t> out;
    out.reserve(text.size() / 4 * 3);
    for (std::size_t i = 0; i < text.size(); i += 4) {
        const bool last = i + 4 == text.size();
        const int pad = last ? (text[i + 3] == '=') + (text[i + 2] == '=') : 0;
        if (pad == 1 && text[i + 2] == '=') return std::nullopt;
        std::uint32_t n = 0;
        for (int k = 0; k < 4 - pad; ++k) {
            const int v = value(text[i + static_cast<std::size_t>(k)]);
            if (v < 0) return std::nullopt;
            n |= std::uint32_t(v) << (18 - 6 * k);
        }
        out.push_back(std::uint8_t(n >> 16));
        if (pad < 2) out.push_back(std::uint8_t(n >> 8));
        if (pad < 1) out.push_back(std::uint8_t(n));
    }
    return out;
}
}
