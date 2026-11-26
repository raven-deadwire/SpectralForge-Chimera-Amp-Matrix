#pragma once
#include "SynthLayer.h"

// SYNTH presets (docs/SYNTH_PRESETS.md). A preset is every synth parameter a
// patch may change (synthPatchKeeps() names the rest), by id and real value.
// Factory presets are the built-in patches; user presets are one small JSON
// file each in a folder on this computer. Nothing here runs on the audio
// thread, and nothing writes a parameter: the page applies a preset.
namespace spectralforge {
struct SynthPreset {
    juce::String name, category;
    bool user{};
    juce::File file;                                      // user presets only
    std::vector<std::pair<juce::String, float>> values;   // unlisted parameters take their defaults
};

namespace synthPresets {
constexpr const char* extension = ".chimerasynth";
constexpr const char* formatName = "chimera-synth-preset";
constexpr int formatVersion = 1, maxFileBytes = 256 * 1024, maxListed = 1000, maxNameLength = 48, maxValues = 4096;

inline const juce::StringArray& categories() {
    static const juce::StringArray names{"Bass", "Lead", "Pad", "Pluck", "Keys", "FX", "Other"};
    return names;
}
inline juce::File defaultDirectory() {
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile("SpectralForge/Chimera/Synth Presets");
}
// A name that is safe as a file name on Windows, macOS and Linux: no path or
// control characters, single spaces, no leading or trailing dot or space, at
// most maxNameLength characters. Empty when nothing usable is left or the
// name is one Windows reserves (CON, NUL, COM1...).
inline juce::String cleanName(const juce::String& text) {
    juce::String out;bool space = false;
    for (auto p = text.getCharPointer(); !p.isEmpty();) {
        const auto c = p.getAndAdvance();
        if (c < 32 || c == 127 || juce::String("\\/:*?\"<>|").containsChar(c)) continue;
        if (juce::CharacterFunctions::isWhitespace(c)) { space = out.isNotEmpty();continue; }
        if (space) out << ' ';
        space = false;out << juce::String::charToString(c);
    }
    out = out.substring(0, maxNameLength);
    while (out.isNotEmpty() && (out.endsWithChar('.') || out.endsWithChar(' '))) out = out.dropLastCharacters(1);
    while (out.startsWithChar('.')) out = out.substring(1).trimStart();
    static const juce::StringArray reserved{"CON", "PRN", "AUX", "NUL", "COM1", "COM2", "COM3", "COM4", "COM5", "COM6", "COM7", "COM8", "COM9",
                                            "LPT1", "LPT2", "LPT3", "LPT4", "LPT5", "LPT6", "LPT7", "LPT8", "LPT9"};
    if (reserved.contains(out.upToFirstOccurrenceOf(".", false, false).trim(), true)) return {};
    return out;
}
inline juce::String cleanCategory(const juce::String& text) {
    const int index = categories().indexOf(text.trim(), true);
    return categories()[index < 0 ? categories().size() - 1 : index];
}
inline bool carries(const juce::String& id) { return id.startsWith("synth") && !synthPatchKeeps(id); }

inline std::vector<SynthPreset> factory() {
    std::vector<SynthPreset> out;
    for (const auto& patch : synthPatches()) out.push_back({patch.name, patch.category, false, {}, patch.values});
    return out;
}
// The current settings, every carried parameter at its real value.
inline SynthPreset capture(juce::AudioProcessor& processor, const juce::String& name, const juce::String& category) {
    SynthPreset preset{cleanName(name), cleanCategory(category), true, {}, {}};
    for (auto* raw : processor.getParameters())
        if (auto* parameter = dynamic_cast<juce::RangedAudioParameter*>(raw); parameter != nullptr && carries(parameter->getParameterID()))
            preset.values.push_back({parameter->getParameterID(), parameter->convertFrom0to1(parameter->getValue())});
    return preset;
}
// Reads one preset file. The file's name is the preset's name. Refuses a file
// that is too large, not this format, from a newer format version, or holds
// anything but finite numbers; ids a preset may not change are dropped.
inline juce::Result read(const juce::File& file, SynthPreset& preset) {
    if (!file.existsAsFile()) return juce::Result::fail("The preset file is missing.");
    if (file.getSize() > maxFileBytes) return juce::Result::fail("The preset file is too large.");
    const auto parsed = juce::JSON::parse(file);
    if (!parsed.isObject() || parsed.getProperty("format", {}).toString() != formatName) return juce::Result::fail("Not a Chimera synth preset.");
    const auto version = parsed.getProperty("version", {});
    if (!version.isInt() || int(version) < 1) return juce::Result::fail("Not a Chimera synth preset.");
    if (int(version) > formatVersion) return juce::Result::fail("This preset was saved by a newer version of Chimera.");
    const auto* values = parsed.getProperty("parameters", {}).getDynamicObject();
    if (values == nullptr || values->getProperties().size() > maxValues) return juce::Result::fail("Not a Chimera synth preset.");
    SynthPreset out{file.getFileNameWithoutExtension(), cleanCategory(parsed.getProperty("category", {}).toString()), true, file, {}};
    for (const auto& item : values->getProperties()) {
        const auto id = item.name.toString();
        if (!(item.value.isInt() || item.value.isInt64() || item.value.isDouble())) return juce::Result::fail("The preset holds a value that is not a number.");
        const double value = double(item.value);
        if (!std::isfinite(value) || std::abs(value) > 1e9) return juce::Result::fail("The preset holds a value that is not a number.");
        if (carries(id)) out.values.push_back({id, float(value)});
    }
    preset = std::move(out);
    return juce::Result::ok();
}
// Writes the preset into the folder under its cleaned name, replacing a file
// of that name (the write is to a temporary file first). A file whose name
// differs only in letter case is the same preset and is replaced too.
inline juce::Result write(const juce::File& directory, const SynthPreset& preset, juce::File* written = nullptr) {
    const auto name = cleanName(preset.name);
    if (name.isEmpty()) return juce::Result::fail("Type a name for the preset.");
    if (!directory.isDirectory() && !directory.createDirectory()) return juce::Result::fail("The preset folder could not be created.");
    auto values = std::make_unique<juce::DynamicObject>();
    for (const auto& [id, value] : preset.values) if (carries(id) && std::isfinite(value)) values->setProperty(id, double(value));
    auto root = std::make_unique<juce::DynamicObject>();
    root->setProperty("format", formatName);root->setProperty("version", formatVersion);root->setProperty("category", cleanCategory(preset.category));
    root->setProperty("parameters", juce::var(values.release()));
    const auto text = juce::JSON::toString(juce::var(root.release()));
    if (text.getNumBytesAsUTF8() > size_t(maxFileBytes)) return juce::Result::fail("The preset is too large to save.");
    const auto target = directory.getChildFile(name + extension);
    for (const auto& other : directory.findChildFiles(juce::File::findFiles, false, juce::String("*") + extension))
        if (other != target && other.getFileNameWithoutExtension().equalsIgnoreCase(name)) other.deleteFile();
    if (!target.replaceWithText(text, false, false, "\n")) return juce::Result::fail("The preset could not be written.");
    if (written != nullptr) *written = target;
    return juce::Result::ok();
}
// Deletes a user preset's file. Only a preset file directly in the folder.
inline juce::Result remove(const juce::File& directory, const SynthPreset& preset) {
    if (!preset.user || preset.file.getParentDirectory() != directory || !preset.file.hasFileExtension(extension)) return juce::Result::fail("Only your own presets can be deleted.");
    if (preset.file.existsAsFile() && !preset.file.deleteFile()) return juce::Result::fail("The preset could not be deleted.");
    return juce::Result::ok();
}
// The factory presets, then the folder's presets by name. Files that cannot
// be read are counted, not listed. At most maxListed files are read.
inline std::vector<SynthPreset> list(const juce::File& directory, int* unreadable = nullptr) {
    auto out = factory();int bad = 0;
    if (directory.isDirectory()) {
        auto files = directory.findChildFiles(juce::File::findFiles, false, juce::String("*") + extension);
        std::sort(files.begin(), files.end(), [](const juce::File& a, const juce::File& b) {
            const int order = a.getFileNameWithoutExtension().compareNatural(b.getFileNameWithoutExtension(), false);
            return order != 0 ? order < 0 : a.getFileName() < b.getFileName();
        });
        for (int i = 0; i < files.size() && i < maxListed; ++i) {
            SynthPreset preset;
            if (read(files.getReference(i), preset).wasOk()) out.push_back(std::move(preset)); else ++bad;
        }
    }
    if (unreadable != nullptr) *unreadable = bad;
    return out;
}
} // namespace synthPresets
} // namespace spectralforge
