#pragma once
#include "ReleaseInfo.h"
#include <juce_core/juce_core.h>
#include <juce_cryptography/juce_cryptography.h>
#include <array>
#include <memory>
#include <optional>
#include <vector>

namespace spectralforge::release {

struct Version {
    std::array<int, 3> numbers{};
    juce::StringArray prerelease;
    static std::optional<Version> parse(const juce::String&);
    int compare(const Version&) const;
};

struct Installer {
    juce::String platform, arch, name, url, sha256;
    juce::int64 size{};
};
struct Manifest {
    juce::String version, releaseUrl;
    std::vector<Installer> assets;
};
struct ManifestReference { juce::String version, url; };

juce::String currentPlatform();
juce::String currentArchitecture();
// Pure validation helpers are also used by the release regression tests.
juce::Result parseManifest(const juce::String& json, const juce::String& expectedVersion, Manifest&);
std::optional<Installer> selectInstaller(const Manifest&, const juce::String& platform, const juce::String& arch);
juce::Result findReleaseManifest(const juce::String& releasesJson, ManifestReference&);
bool isOfficialAssetUrl(const juce::String&, const juce::String& expectedVersion, const juce::String& name);
bool isAllowedDownloadRedirect(const juce::String&);
juce::Result verifyInstallerFile(const juce::File&, const Installer&);

struct Diagnostics {
    // Deliberately contains no arbitrary host strings, device names, paths or audio.
    enum class Format { standalone, vst3, au, other };
    Format format{Format::other};
    double sampleRate{};
    int blockSize{}, inputs{}, outputs{}, latencySamples{};
};
juce::String bugReportText(const Diagnostics&);
juce::URL bugReportUrl(const Diagnostics&);
juce::File findManual();
// Use embedded MANUAL.html as fallback; all installs work offline once bundled.
juce::Result openManual(const void* embeddedHtml = nullptr, size_t embeddedBytes = 0);

class ReleaseSupport final : private juce::Thread {
public:
    enum class State { idle, checking, upToDate, available, downloading, ready, error };
    struct Snapshot {
        State state{State::idle};
        juce::String message{"Update checks are off. Check manually or enable automatic checks."};
        juce::String availableVersion, releaseUrl;
        juce::File installer;
        double progress{};
        bool automaticChecks{};
    };
    ReleaseSupport();
    ~ReleaseSupport() override;
    Snapshot snapshot() const;
    bool automaticChecksEnabled() const;
    juce::Result setAutomaticChecksEnabled(bool);
    bool checkNow();               // Non-blocking; call only from UI / message thread.
    bool downloadUpdate();         // Explicit user action; never an automatic download.
    // Shows the verified package in Explorer/Finder. Never executes an installer in a DAW.
    juce::Result revealVerifiedInstaller() const;
    void cancel();
private:
    enum class Job { none, check, download };
    void run() override;
    void performCheck();
    void performDownload();
    bool enqueue(Job);
    void fail(const juce::String&);
    std::shared_ptr<juce::WebInputStream> connect(const juce::String&, juce::String& error);
    juce::Result readText(const juce::String&, int maxBytes, juce::String&);
    static juce::File settingsFile();
    juce::Result saveSettings();
    mutable juce::CriticalSection mutex;
    Snapshot status;
    Job pending{Job::none};
    bool busy{};
    std::atomic<bool> cancelled{false};
    juce::int64 lastCheck{};
    std::optional<Installer> candidate;
    std::shared_ptr<juce::WebInputStream> activeStream;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ReleaseSupport)
};
}
