#pragma once
// Preparation-only, JUCE-independent control-thread state. NOT an audio processor.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace spectralforge::preparation {
inline constexpr std::size_t maxPedals = 5;
inline constexpr std::uint64_t maxId = 9007199254740991ULL;
using Parameters = std::map<std::string, double>;
struct Instance {
    std::uint64_t id{};
    std::string model;
    bool bypass{};
    Parameters parameters;
    std::map<int, std::string> midi; // CC -> stable control key; copied pedals start unbound.
    bool operator==(const Instance&) const = default;
};
struct Snapshot {
    unsigned version{1};
    std::uint64_t nextId{1};
    std::size_t lowTap{}; // count BEFORE tap; same five slots, not a second board.
    std::string engine{"legacy-v1"};
    std::vector<Instance> effects;
    bool operator==(const Snapshot&) const = default;
};
inline void require(bool ok, const char* message) {
    if (!ok) throw std::invalid_argument(message);
}
inline bool validText(const std::string& s) {
    return !s.empty() && s.size() <= 128 && std::none_of(s.begin(), s.end(), [](unsigned char c) { return c < 32; });
}
inline void validate(const Snapshot& s) {
    require(s.version == 1, "Unsupported preparation schema version");
    require(s.effects.size() <= maxPedals && s.lowTap <= s.effects.size(), "Invalid five-slot board/tap");
    require(validText(s.engine) && s.nextId > 0 && s.nextId <= maxId, "Invalid engine/id allocator");
    std::set<std::uint64_t> ids;
    for (const auto& e : s.effects) {
        require(e.id > 0 && e.id < s.nextId && ids.insert(e.id).second, "Duplicate/invalid instance id");
        require(validText(e.model) && e.parameters.size() <= 128 && e.midi.size() <= 128, "Invalid instance");
        for (const auto& [key, value] : e.parameters)
            require(validText(key) && std::isfinite(value), "Invalid parameter");
        for (const auto& [cc, key] : e.midi)
            require(cc >= 0 && cc <= 127 && e.parameters.contains(key), "Invalid MIDI binding");
    }
}
inline std::set<std::uint64_t> lowIds(const Snapshot& s) {
    std::set<std::uint64_t> ids;
    for (std::size_t i = 0; i < s.lowTap; ++i) ids.insert(s.effects[i].id);
    return ids;
}
// Dedicated preparation format; NEVER overwrite a production .chimera project with it.
inline std::string encode(const Snapshot& s) {
    validate(s);
    std::ostringstream o;
    o.imbue(std::locale::classic());
    o << "CHIMERA_BOARD_PREP " << s.version << '\n' << s.nextId << ' ' << s.lowTap << ' '
      << std::quoted(s.engine) << ' ' << s.effects.size() << '\n' << std::setprecision(17);
    for (const auto& e : s.effects) {
        o << e.id << ' ' << std::quoted(e.model) << ' ' << int(e.bypass) << ' '
          << e.parameters.size() << ' ' << e.midi.size() << '\n';
        for (const auto& [key, value] : e.parameters) o << std::quoted(key) << ' ' << value << '\n';
        for (const auto& [cc, key] : e.midi) o << cc << ' ' << std::quoted(key) << '\n';
    }
    return o.str();
}
inline Snapshot decode(const std::string& text) {
    require(text.size() <= 262144, "Preparation file too large");
    std::istringstream in(text);
    in.imbue(std::locale::classic());
    Snapshot s;
    std::string magic;
    std::size_t count{};
    require(bool(in >> magic >> s.version >> s.nextId >> s.lowTap >> std::quoted(s.engine) >> count), "Bad header");
    require(magic == "CHIMERA_BOARD_PREP" && count <= maxPedals, "Wrong format/capacity");
    for (std::size_t i = 0; i < count; ++i) {
        Instance e;
        int bypass{};
        std::size_t params{}, midi{};
        require(bool(in >> e.id >> std::quoted(e.model) >> bypass >> params >> midi), "Bad instance");
        require((bypass == 0 || bypass == 1) && params <= 128 && midi <= 128, "Bad instance counts");
        e.bypass = bypass != 0;
        for (std::size_t p = 0; p < params; ++p) {
            std::string key; double value{};
            require(bool(in >> std::quoted(key) >> value), "Bad parameter encoding");
            require(e.parameters.emplace(key, value).second, "Duplicate parameter key");
        }
        for (std::size_t p = 0; p < midi; ++p) {
            int cc{}; std::string key;
            require(bool(in >> cc >> std::quoted(key)), "Bad MIDI encoding");
            require(e.midi.emplace(cc, key).second, "Duplicate MIDI CC");
        }
        s.effects.push_back(std::move(e));
    }
    in >> std::ws;
    require(in.eof(), "Unexpected trailing data");
    validate(s);
    return s;
}
class Board {
    Snapshot current;
    std::vector<Snapshot> past, future;
    std::uint64_t highWater{1}; // never rewound by Undo, replacement, or same-session import
    std::size_t index(std::uint64_t id) const {
        for (std::size_t i = 0; i < current.effects.size(); ++i)
            if (current.effects[i].id == id) return i;
        throw std::invalid_argument("Unknown instance id");
    }
    void commit(Snapshot next) {
        validate(next);
        if (next == current) return;
        past.push_back(current);
        if (past.size() > 64) past.erase(past.begin());
        future.clear();
        highWater = std::max(highWater, next.nextId);
        next.nextId = highWater;
        current = std::move(next);
    }
    static void restoreHistory(Snapshot& target, std::uint64_t high) { target.nextId = std::max(target.nextId, high); }
public:
    const Snapshot& state() const { return current; }
    std::uint64_t add(const std::string& model, Parameters params = {}, bool shared = false) {
        return insert(model, shared ? current.lowTap : current.effects.size(), std::move(params), shared);
    }
    std::uint64_t insert(const std::string& model, std::size_t position, Parameters params, bool shared) {
        require(current.effects.size() < maxPedals, "Maximum five pedals; replace or remove one");
        require(position <= current.effects.size(), "Insertion position out of range");
        require(shared ? position <= current.lowTap : position >= current.lowTap, "Wrong side of LOW tap");
        require(highWater < maxId, "Instance id space exhausted");
        auto next = current;
        const auto id = highWater;
        next.nextId = id + 1;
        next.effects.insert(next.effects.begin() + position, Instance{id, model, false, std::move(params), {}});
        if (shared) ++next.lowTap;
        commit(std::move(next));
        return id;
    }
    void remove(std::uint64_t id) {
        const auto i = index(id); auto next = current;
        next.effects.erase(next.effects.begin() + i);
        if (i < next.lowTap) --next.lowTap;
        commit(std::move(next));
    }
    std::uint64_t duplicate(std::uint64_t id) {
        const auto i = index(id);
        const auto source = current.effects[i];
        const auto newId = insert(source.model, i + 1, source.parameters, i < current.lowTap);
        // Bypass is copied within the SAME undo transaction, MIDI intentionally is not.
        current.effects[index(newId)].bypass = source.bypass;
        return newId;
    }
    std::uint64_t replace(std::uint64_t id, const std::string& model, Parameters params = {}) {
        const auto i = index(id); auto next = current;
        require(highWater < maxId, "Instance id space exhausted");
        const auto freshId = highWater;
        next.nextId = freshId + 1;
        next.effects[i] = {freshId, model, false, std::move(params), {}};
        commit(std::move(next)); // old bindings cannot silently drive a different model
        return freshId;
    }
    // Returns true when LOW membership changes; the UI must visibly disclose that change.
    bool move(std::uint64_t id, std::size_t destination) {
        const auto i = index(id);
        require(destination < current.effects.size(), "Move position out of range");
        auto next = current;
        const auto effect = next.effects[i];
        next.effects.erase(next.effects.begin() + i);
        next.effects.insert(next.effects.begin() + destination, effect);
        const bool changed = lowIds(next) != lowIds(current);
        commit(std::move(next));
        return changed;
    }
    void setTap(std::size_t before) { auto next = current; next.lowTap = before; commit(std::move(next)); }
    void bypass(std::uint64_t id, bool value) { auto next = current; next.effects[index(id)].bypass = value; commit(std::move(next)); }
    void set(std::uint64_t id, const std::string& key, double value) {
        auto next = current; auto& params = next.effects[index(id)].parameters;
        require(params.contains(key), "Unknown control (catalog validation is also required)");
        params[key] = value; commit(std::move(next));
    }
    void bind(std::uint64_t id, int cc, const std::string& key) {
        auto next = current; next.effects[index(id)].midi[cc] = key; commit(std::move(next));
    }
    void load(const std::string& text) {
        auto next = decode(text); restoreHistory(next, highWater); commit(std::move(next));
    }
    bool undo() {
        if (past.empty()) return false;
        future.push_back(current); current = past.back(); past.pop_back(); restoreHistory(current, highWater); return true;
    }
    bool redo() {
        if (future.empty()) return false;
        past.push_back(current); current = future.back(); future.pop_back(); restoreHistory(current, highWater); return true;
    }
};
struct LegacyPre {
    bool envelopeFirst{true}, boostAfterDrive{false};
    // Native family order: compressor, filter, fuzz, boost, drive.
    std::array<int, 5> models{};
    std::array<bool, 5> bypass{};
    std::array<Parameters, 5> parameters;
};
// Explicit raw-value adapter. Decoding a production APVTS/.chimera file is NOT implemented.
inline Snapshot migrate(const LegacyPre& old) {
    const std::array<std::string, 5> family{"comp", "filter", "fuzz", "boost", "drive"};
    std::array<int, 5> order{0, 1, 2, 3, 4};
    if (old.envelopeFirst) std::swap(order[0], order[1]);
    if (old.boostAfterDrive) std::swap(order[3], order[4]);
    Snapshot s; s.lowTap = 2;
    for (const auto f : order) {
        require(old.models[f] >= 0 && old.models[f] < 5, "Invalid legacy raw model index");
        s.effects.push_back({s.nextId++, "legacy." + family[f] + "." + std::to_string(old.models[f]),
                             old.bypass[f], old.parameters[f], {}});
    }
    validate(s); return s;
}
} // namespace spectralforge::preparation
