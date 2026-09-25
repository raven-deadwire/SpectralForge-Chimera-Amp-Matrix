#include "ReleaseSupport.h"
#include <algorithm>
#include <cmath>

namespace spectralforge::release {
namespace {
constexpr juce::int64 maxInstallerBytes = 1024LL * 1024 * 1024;
constexpr auto githubAssetPrefix = "https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/releases/download/v";

bool digits(const juce::String& s) { return s.isNotEmpty() && s.containsOnly("0123456789"); }
bool safeName(const juce::String& s) {
    return s.isNotEmpty() && s.length() <= 180 && !s.startsWithChar('.')
        && !s.contains("..") && s.containsOnly("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_.");
}
bool semverIdentifiers(const juce::String& s, bool rejectLeadingZeroes) {
    if (s.isEmpty()) return false;
    for (const auto& part : juce::StringArray::fromTokens(s, ".", "")) {
        if (part.isEmpty() || !part.containsOnly("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-")) return false;
        if (rejectLeadingZeroes && digits(part) && part.length() > 1 && part.startsWithChar('0')) return false;
    }
    return !s.startsWithChar('.') && !s.endsWithChar('.') && !s.contains("..");
}
bool betaChannelVersion(const Version& v) {
    return v.prerelease.isEmpty() || v.prerelease[0] == "beta" || v.prerelease[0] == "rc";
}
juce::File userRoot() {
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile("SpectralForge/Chimera");
}
bool isInteger(const juce::var& v) { return v.isInt() || v.isInt64(); }
juce::String osLabel() {
    return juce::SystemStats::getOperatingSystemName();
}
}

std::optional<Version> Version::parse(const juce::String& source) {
    if (source.isEmpty() || source.length() > 128 || source != source.trim()) return {};
    auto base = source;
    const auto plus = base.indexOfChar('+');
    if (plus >= 0) {
        if (!semverIdentifiers(base.substring(plus + 1), false)) return {};
        base = base.substring(0, plus);
    }
    Version result;
    const auto dash = base.indexOfChar('-');
    if (dash >= 0) {
        const auto pre = base.substring(dash + 1);
        if (!semverIdentifiers(pre, true)) return {};
        result.prerelease = juce::StringArray::fromTokens(pre, ".", "");
        base = base.substring(0, dash);
    }
    const auto parts = juce::StringArray::fromTokens(base, ".", "");
    if (parts.size() != 3) return {};
    for (int i = 0; i < 3; ++i) {
        const auto& part = parts[i];
        if (!digits(part) || part.length() > 9 || (part.length() > 1 && part.startsWithChar('0'))) return {};
        result.numbers[static_cast<size_t>(i)] = part.getIntValue();
    }
    return result;
}

int Version::compare(const Version& other) const {
    if (numbers != other.numbers) return numbers < other.numbers ? -1 : 1;
    if (prerelease.isEmpty() != other.prerelease.isEmpty()) return prerelease.isEmpty() ? 1 : -1;
    for (int i = 0; i < juce::jmin(prerelease.size(), other.prerelease.size()); ++i) {
        const auto& a = prerelease[i]; const auto& b = other.prerelease[i];
        if (a == b) continue;
        const bool an = digits(a), bn = digits(b);
        if (an != bn) return an ? -1 : 1;
        if (an && a.length() != b.length()) return a.length() < b.length() ? -1 : 1;
        return a.compare(b) < 0 ? -1 : 1;
    }
    if (prerelease.size() == other.prerelease.size()) return 0;
    return prerelease.size() < other.prerelease.size() ? -1 : 1;
}

juce::String currentPlatform() {
#if JUCE_WINDOWS
    return "windows";
#elif JUCE_MAC
    return "macos";
#else
    return "linux";
#endif
}
juce::String currentArchitecture() {
#if defined(__aarch64__) || defined(_M_ARM64) || defined(__arm64__)
    return "arm64";
#else
    return "x86_64";
#endif
}

bool isOfficialAssetUrl(const juce::String& url, const juce::String& expectedVersion, const juce::String& name) {
    return Version::parse(expectedVersion).has_value() && safeName(name)
        && url == juce::String(githubAssetPrefix) + expectedVersion + "/" + name;
}
bool isAllowedDownloadRedirect(const juce::String& source) {
    if (!source.startsWith("https://") || source.containsChar('\\') || source.containsChar('\r') || source.containsChar('\n')) return false;
    const juce::URL url(source);
    const auto authority = source.substring(8).upToFirstOccurrenceOf("/", false, false);
    if (authority.containsChar('@') || authority.containsChar(':')) return false;
    const auto host = url.getDomain().toLowerCase();
    return host == "release-assets.githubusercontent.com" || host == "objects.githubusercontent.com"
        || host == "github-releases.githubusercontent.com";
}

juce::Result parseManifest(const juce::String& text, const juce::String& expected, Manifest& output) {
    output = {};
    if (text.getNumBytesAsUTF8() > 256 * 1024) return juce::Result::fail("Update manifest exceeds the size limit.");
    const auto json = juce::JSON::parse(text);
    const auto parsedVersion = Version::parse(expected);
    if (!json.isObject() || !isInteger(json["schema"]) || static_cast<int>(json["schema"]) != 1
        || !parsedVersion || !betaChannelVersion(*parsedVersion) || json["version"].toString() != expected
        || json["channel"].toString() != channel)
        return juce::Result::fail("Update manifest version or channel is invalid.");
    const auto page = juce::String(repositoryUrl) + "/releases/tag/v" + expected;
    if (json["releaseUrl"].toString() != page) return juce::Result::fail("Update release page is not the official repository.");
    const auto* assets = json["assets"].getArray();
    if (assets == nullptr || assets->isEmpty() || assets->size() > 12) return juce::Result::fail("Update manifest has no valid installer list.");
    Manifest result; result.version = expected; result.releaseUrl = page;
    juce::StringArray names;
    for (const auto& item : *assets) {
        Installer a{item["platform"].toString(), item["arch"].toString(), item["name"].toString(),
                    item["url"].toString(), item["sha256"].toString().toLowerCase(), static_cast<juce::int64>(item["size"])};
        const bool validPlatform = a.platform == "windows" || a.platform == "macos" || a.platform == "linux";
        const bool validArch = a.arch == "x86_64" || a.arch == "arm64" || (a.platform == "macos" && a.arch == "universal");
        const bool validExtension = (a.platform == "windows" && a.name.endsWith(".exe"))
            || (a.platform == "macos" && a.name.endsWith(".pkg"))
            || (a.platform == "linux" && (a.name.endsWith(".deb") || a.name.endsWith(".tar.gz")));
        if (!item.isObject() || !validPlatform || !validArch || !validExtension
            || !isOfficialAssetUrl(a.url, expected, a.name) || a.sha256.length() != 64
            || !a.sha256.containsOnly("0123456789abcdef") || !isInteger(item["size"])
            || a.size <= 0 || a.size > maxInstallerBytes || names.contains(a.name))
            return juce::Result::fail("Update manifest contains an invalid installer, hash, size or URL.");
        names.add(a.name); result.assets.push_back(std::move(a));
    }
    output = std::move(result);
    return juce::Result::ok();
}

std::optional<Installer> selectInstaller(const Manifest& manifest, const juce::String& platform, const juce::String& arch) {
    std::optional<Installer> result;
    int bestScore = -1;
    for (const auto& asset : manifest.assets) {
        if (asset.platform != platform || (asset.arch != arch && !(platform == "macos" && asset.arch == "universal"))) continue;
        int score = asset.arch == arch ? 2 : 1;
        if (platform == "linux" && asset.name.endsWith(".deb")) score += 4;
        if (score > bestScore) { result = asset; bestScore = score; }
    }
    return result;
}

juce::Result findReleaseManifest(const juce::String& text, ManifestReference& output) {
    output = {};
    if (text.getNumBytesAsUTF8() > 2 * 1024 * 1024) return juce::Result::fail("Release list exceeds the size limit.");
    const auto json = juce::JSON::parse(text);
    const auto* releases = json.getArray();
    if (releases == nullptr) return juce::Result::fail("GitHub returned an invalid release list.");
    std::optional<Version> latest;
    for (const auto& entry : *releases) {
        if (!entry.isObject() || !entry["draft"].isBool() || static_cast<bool>(entry["draft"])) continue;
        const auto tag = entry["tag_name"].toString();
        if (!tag.startsWithChar('v')) continue;
        const auto v = Version::parse(tag.substring(1));
        if (!v || !betaChannelVersion(*v)) continue;
        if (entry["html_url"].toString() != juce::String(repositoryUrl) + "/releases/tag/" + tag) continue;
        const auto* assets = entry["assets"].getArray();
        if (assets == nullptr) continue;
        for (const auto& asset : *assets) {
            if (asset["name"].toString() != manifestName) continue;
            const auto url = asset["browser_download_url"].toString();
            if (!isOfficialAssetUrl(url, tag.substring(1), manifestName)) continue;
            if (!latest || v->compare(*latest) > 0) { latest = v; output = {tag.substring(1), url}; }
        }
    }
    // An empty list is valid before the first open-beta release is published.
    return juce::Result::ok();
}

juce::Result verifyInstallerFile(const juce::File& file, const Installer& asset) {
    if (!file.existsAsFile() || asset.size <= 0 || asset.size > maxInstallerBytes || file.getSize() != asset.size)
        return juce::Result::fail("Installer is missing or its size differs from the official manifest.");
    if (asset.sha256.length() != 64 || !asset.sha256.containsOnly("0123456789abcdef") || juce::SHA256(file).toHexString() != asset.sha256)
        return juce::Result::fail("Installer SHA-256 verification failed.");
    return juce::Result::ok();
}

juce::String bugReportText(const Diagnostics& d) {
    const auto format = d.format == Diagnostics::Format::standalone ? "Standalone" :
        d.format == Diagnostics::Format::vst3 ? "VST3" : d.format == Diagnostics::Format::au ? "AU" : "Other";
    const auto rate = std::isfinite(d.sampleRate) ? juce::jlimit(0.0, 768000.0, d.sampleRate) : 0.0;
    return juce::String("## What happened?\n\n[Describe the problem]\n\n## Steps to reproduce\n\n1. \n2. \n3. \n\n## Expected result\n\n[Describe the expected result]\n\n## Diagnostics\n\n")
        + "- SpectralForge Chimera: " + version + " (" + revision + ")\n- OS: " + osLabel()
        + "\n- Architecture: " + currentArchitecture() + "\n- Format: " + format
        + "\n- Sample rate: " + juce::String(rate, 0) + " Hz\n- Block size: " + juce::String(juce::jlimit(0, 1048576, d.blockSize))
        + "\n- Channels: " + juce::String(juce::jlimit(0, 256, d.inputs)) + " in / " + juce::String(juce::jlimit(0, 256, d.outputs))
        + " out\n- Reported latency: " + juce::String(juce::jlimit(0, 1048576, d.latencySamples))
        + " samples\n\nNo audio, device names, account details or file paths are collected. Review this draft before submitting.\n";
}
juce::URL bugReportUrl(const Diagnostics& d) {
    return juce::URL(juce::String(repositoryUrl) + "/issues/new")
        .withParameter("title", "[Open Beta " + juce::String(version) + "] Bug report")
        .withParameter("body", bugReportText(d));
}

juce::File findManual() {
    const auto executable = juce::File::getSpecialLocation(juce::File::currentExecutableFile).getParentDirectory();
    const auto common = juce::File::getSpecialLocation(juce::File::commonApplicationDataDirectory).getChildFile("SpectralForge/Chimera");
    const std::array<juce::File, 10> candidates{
        userRoot().getChildFile("docs/MANUAL.html"), common.getChildFile("docs/MANUAL.html"),
        executable.getChildFile("docs/MANUAL.html"), executable.getParentDirectory().getChildFile("Resources/docs/MANUAL.html"),
        executable.getParentDirectory().getChildFile("docs/MANUAL.html"),
        executable.getChildFile("Documentation/MANUAL.html"),
        juce::File("/Library/Application Support/SpectralForge/Chimera/Documentation/MANUAL.html"),
        juce::File("/usr/share/doc/chimera-amp-matrix/MANUAL.html"),
        juce::File("/usr/share/chimera/docs/MANUAL.html"), juce::File("/opt/chimera/docs/MANUAL.html")};
    for (const auto& file : candidates) if (file.existsAsFile() && file.getSize() <= 4 * 1024 * 1024) return file;
    return {};
}
juce::Result openManual(const void* html, size_t bytes) {
    juce::File file;
    // Prefer this binary's embedded manual to an older installed copy.
    if (html != nullptr && bytes > 0 && bytes <= 4 * 1024 * 1024) {
        file = userRoot().getChildFile("docs/MANUAL.html");
        if (auto r = file.getParentDirectory().createDirectory(); r.failed()) return r;
        if (!file.replaceWithData(html, bytes)) return juce::Result::fail("Cannot extract the offline manual.");
    } else file = findManual();
    if (!file.existsAsFile()) return juce::Result::fail("Offline manual is missing. Reinstall the complete Chimera package.");
    return juce::URL(file).launchInDefaultBrowser() ? juce::Result::ok() : juce::Result::fail("Cannot open the manual in the default browser.");
}

juce::File ReleaseSupport::settingsFile() { return userRoot().getChildFile("updates.json"); }
ReleaseSupport::ReleaseSupport() : juce::Thread("SpectralForge Chimera updates") {
    const auto file = settingsFile();
    if (file.existsAsFile() && file.getSize() < 8192) {
        const auto settings = juce::JSON::parse(file);
        status.automaticChecks = settings["automaticChecks"].isBool() && static_cast<bool>(settings["automaticChecks"]);
        if (isInteger(settings["lastCheck"])) lastCheck = static_cast<juce::int64>(settings["lastCheck"]);
    }
    startThread(juce::Thread::Priority::low);
    if (status.automaticChecks) {
        status.message = "Automatic checks enabled (at most once per 24 hours).";
        const auto now = juce::Time::currentTimeMillis();
        if (lastCheck <= 0 || now < lastCheck || now - lastCheck >= 24LL * 60 * 60 * 1000) checkNow();
    }
}
ReleaseSupport::~ReleaseSupport() {
    signalThreadShouldExit();
    cancel();
    notify();
    waitForThreadToExit(-1);
}
ReleaseSupport::Snapshot ReleaseSupport::snapshot() const { const juce::ScopedLock l(mutex); return status; }
bool ReleaseSupport::automaticChecksEnabled() const { const juce::ScopedLock l(mutex); return status.automaticChecks; }
juce::Result ReleaseSupport::saveSettings() {
    auto* object = new juce::DynamicObject;
    { const juce::ScopedLock l(mutex); object->setProperty("automaticChecks", status.automaticChecks); object->setProperty("lastCheck", lastCheck); }
    const juce::var json(object);
    const auto file = settingsFile();
    if (auto r = file.getParentDirectory().createDirectory(); r.failed()) return r;
    return file.replaceWithText(juce::JSON::toString(json)) ? juce::Result::ok() : juce::Result::fail("Cannot save the update preference.");
}
juce::Result ReleaseSupport::setAutomaticChecksEnabled(bool enabled) {
    bool previous;
    { const juce::ScopedLock l(mutex); previous = status.automaticChecks; status.automaticChecks = enabled; }
    const auto result = saveSettings();
    if (result.failed()) { const juce::ScopedLock l(mutex); status.automaticChecks = previous; return result; }
    if (enabled) checkNow();
    return result;
}
bool ReleaseSupport::enqueue(Job job) {
    { const juce::ScopedLock l(mutex);
      if (busy || pending != Job::none || threadShouldExit()) return false;
      if (job == Job::download && !candidate) return false;
      cancelled = false; pending = job; busy = true;
      status.state = job == Job::check ? State::checking : State::downloading;
      status.message = job == Job::check ? "Checking the official beta release channel..." : "Downloading installer; SHA-256 will be verified...";
      status.progress = 0.0; }
    notify(); return true;
}
bool ReleaseSupport::checkNow() { return enqueue(Job::check); }
bool ReleaseSupport::downloadUpdate() { return enqueue(Job::download); }
void ReleaseSupport::cancel() {
    cancelled = true;
    std::shared_ptr<juce::WebInputStream> stream;
    { const juce::ScopedLock l(mutex); stream = activeStream; }
    if (stream) stream->cancel();
}
void ReleaseSupport::run() {
    while (!threadShouldExit()) {
        Job job;
        { const juce::ScopedLock l(mutex); job = pending; pending = Job::none; }
        if (job == Job::none) { wait(-1); continue; }
        if (job == Job::check) performCheck(); else performDownload();
        { const juce::ScopedLock l(mutex); activeStream.reset(); busy = false; }
    }
}
void ReleaseSupport::fail(const juce::String& message) {
    const juce::ScopedLock l(mutex); status.state = State::error;
    status.message = cancelled || threadShouldExit() ? "Update operation cancelled." : message;
}

std::shared_ptr<juce::WebInputStream> ReleaseSupport::connect(const juce::String& initial, juce::String& error) {
    auto address = initial;
    for (int redirects = 0; redirects <= 5 && !cancelled && !threadShouldExit(); ++redirects) {
        auto stream = std::make_shared<juce::WebInputStream>(juce::URL(address), false);
        stream->withConnectionTimeout(8000).withNumRedirectsToFollow(0)
            .withExtraHeaders("User-Agent: SpectralForge-Chimera/" + juce::String(version) + "\r\nAccept: application/json, application/octet-stream\r\n");
        { const juce::ScopedLock l(mutex); activeStream = stream; }
        if (cancelled || threadShouldExit()) { stream->cancel(); break; }
        if (!stream->connect(nullptr)) { error = "Cannot reach GitHub. Check your connection and try again."; return {}; }
        const auto code = stream->getStatusCode();
        if (code == 200) return stream;
        if (code == 301 || code == 302 || code == 303 || code == 307 || code == 308) {
            auto headers = stream->getResponseHeaders(); headers.setIgnoresCase(true);
            const auto target = headers["Location"];
            if (!isAllowedDownloadRedirect(target)) { error = "Update download redirected outside trusted HTTPS asset hosts."; return {}; }
            address = target; continue;
        }
        error = code == 403 || code == 429 ? "GitHub rate limit reached. Try again later."
            : code == 404 ? "The beta release channel is not published or this repository is unavailable."
            : "GitHub returned HTTP " + juce::String(code) + ". Try again later.";
        return {};
    }
    error = "Update request was cancelled or redirected too many times."; return {};
}
juce::Result ReleaseSupport::readText(const juce::String& url, int maximum, juce::String& result) {
    juce::String error;
    auto stream = connect(url, error);
    if (!stream) return juce::Result::fail(error);
    if (stream->getTotalLength() > maximum) return juce::Result::fail("Update response exceeds the size limit.");
    juce::MemoryOutputStream data;
    std::array<char, 16384> buffer{};
    while (!stream->isExhausted() && !cancelled && !threadShouldExit()) {
        const auto count = stream->read(buffer.data(), static_cast<int>(buffer.size()));
        if (count <= 0) break;
        if (data.getDataSize() + static_cast<size_t>(count) > static_cast<size_t>(maximum)) return juce::Result::fail("Update response exceeds the size limit.");
        data.write(buffer.data(), static_cast<size_t>(count));
    }
    if (cancelled || threadShouldExit() || stream->isError()) return juce::Result::fail("Update response was interrupted.");
    result = data.toUTF8(); return juce::Result::ok();
}
void ReleaseSupport::performCheck() {
    { const juce::ScopedLock l(mutex); lastCheck = juce::Time::currentTimeMillis(); }
    saveSettings();
    { const juce::ScopedLock l(mutex); candidate.reset(); status.installer = juce::File{}; status.availableVersion.clear(); status.releaseUrl.clear(); }
    juce::String list;
    if (auto r = readText(releasesApi, 2 * 1024 * 1024, list); r.failed()) { fail(r.getErrorMessage()); return; }
    ManifestReference reference;
    if (auto r = findReleaseManifest(list, reference); r.failed()) { fail(r.getErrorMessage()); return; }
    const auto installed = Version::parse(version);
    const auto available = Version::parse(reference.version);
    if (!available || (installed && available->compare(*installed) <= 0)) {
        const juce::ScopedLock l(mutex); status.state = State::upToDate;
        status.message = reference.version.isEmpty() ? "No open-beta update has been published yet." : "You are running the latest available beta-channel version."; return;
    }
    juce::String text;
    if (auto r = readText(reference.url, 256 * 1024, text); r.failed()) { fail(r.getErrorMessage()); return; }
    Manifest manifest;
    if (auto r = parseManifest(text, reference.version, manifest); r.failed()) { fail(r.getErrorMessage()); return; }
    auto installer = selectInstaller(manifest, currentPlatform(), currentArchitecture());
    if (!installer) { fail("A new version exists, but no compatible installer is published for this OS and architecture."); return; }
    const juce::ScopedLock l(mutex); candidate = installer; status.state = State::available;
    status.availableVersion = manifest.version; status.releaseUrl = manifest.releaseUrl;
    status.message = "Version " + manifest.version + " is available. Download when ready; installation is a separate action.";
}
void ReleaseSupport::performDownload() {
    Installer asset;
    { const juce::ScopedLock l(mutex); if (!candidate) { fail("Check for updates first."); return; } asset = *candidate; }
    const auto directory = userRoot().getChildFile("Updates");
    if (auto r = directory.createDirectory(); r.failed()) { fail("Cannot create the update download directory."); return; }
    const auto destination = directory.getChildFile(asset.name);
    const auto partial = directory.getNonexistentChildFile(asset.name + ".part", "", false);
    struct RemovePartial { juce::File file; ~RemovePartial() { file.deleteFile(); } } cleanup{partial};
    juce::String error;
    auto stream = connect(asset.url, error);
    if (!stream) { fail(error); return; }
    if (stream->getTotalLength() > 0 && stream->getTotalLength() != asset.size) { fail("Installer size differs from the official manifest."); return; }
    auto output = partial.createOutputStream();
    if (!output || !output->openedOk()) { fail("Cannot write the update installer."); return; }
    std::array<char, 64 * 1024> buffer{};
    juce::int64 received = 0;
    while (!stream->isExhausted() && !cancelled && !threadShouldExit()) {
        const auto count = stream->read(buffer.data(), static_cast<int>(buffer.size()));
        if (count <= 0) break;
        received += count;
        if (received > asset.size || !output->write(buffer.data(), static_cast<size_t>(count))) { fail("Installer download exceeded its expected size or could not be written."); return; }
        const juce::ScopedLock l(mutex); status.progress = static_cast<double>(received) / static_cast<double>(asset.size);
    }
    output->flush(); const bool writeFailed = output->getStatus().failed(); output.reset();
    if (cancelled || threadShouldExit() || stream->isError() || received != asset.size || writeFailed) { fail("Installer download was interrupted. Download again."); return; }
    if (auto r = verifyInstallerFile(partial, asset); r.failed()) { fail(r.getErrorMessage() + " The file was discarded."); return; }
    if (!partial.moveFileTo(destination)) { fail("Cannot save the verified installer. Close any previous installer and retry."); return; }
    const juce::ScopedLock l(mutex); status.state = State::ready; status.installer = destination; status.progress = 1.0;
    status.message = "Installer verified. Show the file, close Chimera and all DAWs, then run the installer.";
}
juce::Result ReleaseSupport::revealVerifiedInstaller() const {
    juce::File file; std::optional<Installer> asset;
    { const juce::ScopedLock l(mutex); if (status.state != State::ready) return juce::Result::fail("No verified installer is ready."); file = status.installer; asset = candidate; }
    // Verify again immediately before the explicit hand-off; never launch executables.
    if (!asset) return juce::Result::fail("The update manifest is no longer available.");
    if (auto r = verifyInstallerFile(file, *asset); r.failed()) return r;
    file.revealToUser(); return juce::Result::ok();
}
}
