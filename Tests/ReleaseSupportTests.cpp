#include "ReleaseSupport.h"
#include <iostream>
#include <stdexcept>

using namespace spectralforge::release;
namespace {
void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
juce::var validManifest() {
    return juce::JSON::parse(R"({"schema":1,"version":"1.0.0-beta.2","channel":"beta","releaseUrl":"https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/releases/tag/v1.0.0-beta.2","assets":[{"platform":"windows","arch":"x86_64","name":"SpectralForge-Chimera-1.0.0-beta.2-win64-Setup.exe","url":"https://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/releases/download/v1.0.0-beta.2/SpectralForge-Chimera-1.0.0-beta.2-win64-Setup.exe","sha256":"0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef","size":12345}]})");
}
void versions() {
    for (const auto* bad : {"", "v1.0.0", "01.0.0", "1.0", "1.0.0-", "1.0.0-beta.01", "1.0.0-beta..1", "1.0.0+", "1.0.0+with space", "1.0.0\n", "1..0.0", "1.0.0-beta.1/evil"})
        require(!Version::parse(bad), "Malformed semantic version accepted");
    const char* order[] = {"1.0.0-alpha.1", "1.0.0-beta.1", "1.0.0-beta.2", "1.0.0-beta.10", "1.0.0-rc.1", "1.0.0", "1.0.1-beta.1", "1.1.0", "2.0.0"};
    for (size_t i = 1; i < std::size(order); ++i)
        require(Version::parse(order[i-1])->compare(*Version::parse(order[i])) < 0, "SemVer precedence broken");
    require(Version::parse("1.0.0+build.1")->compare(*Version::parse("1.0.0+build.2")) == 0, "Build metadata changes precedence");
}
void manifests() {
    Manifest parsed;
    require(parseManifest(juce::JSON::toString(validManifest()), "1.0.0-beta.2", parsed).wasOk(), "Valid manifest rejected");
    require(selectInstaller(parsed, "windows", "x86_64").has_value(), "Compatible Windows installer missing");
    require(!selectInstaller(parsed, "windows", "arm64"), "Wrong CPU installer accepted");
    require(!selectInstaller(parsed, "macos", "x86_64"), "Wrong OS installer accepted");
    require(parseManifest(juce::JSON::toString(validManifest()), "1.0.0-beta.3", parsed).failed(), "Mismatched version accepted");
    require(parseManifest("{}", "1.0.0-beta.2", parsed).failed(), "Empty manifest accepted");
    auto checkInvalid = [&](const char* key, const juce::var& value) {
        auto json = validManifest(); json["assets"].getArray()->getReference(0).getDynamicObject()->setProperty(key, value);
        require(parseManifest(juce::JSON::toString(json), "1.0.0-beta.2", parsed).failed(), "Unsafe installer metadata accepted");
    };
    checkInvalid("url", "http://github.com/raven-deadwire/SpectralForge-Chimera-Amp-Matrix/releases/download/v1.0.0-beta.2/x.exe");
    checkInvalid("url", "https://evil.example/x.exe");
    checkInvalid("url", "https://github.com@evil.example/x.exe");
    checkInvalid("name", "../evil.exe");
    checkInvalid("name", "bad.cmd");
    checkInvalid("sha256", "abcd");
    checkInvalid("size", -1); checkInvalid("size", "12345"); checkInvalid("size", 12345.5);
    checkInvalid("size", static_cast<juce::int64>(2LL * 1024 * 1024 * 1024));
    checkInvalid("arch", "universal"); checkInvalid("platform", "plan9");
    parsed = {"1.0.0-beta.2", "", {{"macos", "universal", "mac.pkg", "", "", 1}, {"linux", "x86_64", "linux.tar.gz", "", "", 1}, {"linux", "x86_64", "linux.deb", "", "", 1}}};
    require(selectInstaller(parsed, "macos", "arm64")->name == "mac.pkg", "Universal Mac installer not selected");
    require(selectInstaller(parsed, "linux", "x86_64")->name == "linux.deb", "Linux installer preference wrong");
}
void releaseSelection() {
    juce::Array<juce::var> entries;
    for (auto tag : {"1.0.0-beta.2", "1.0.0-beta.10", "2.0.0-alpha.1"}) {
        auto* entry = new juce::DynamicObject;
        entry->setProperty("draft", false); entry->setProperty("prerelease", true);
        entry->setProperty("tag_name", "v" + juce::String(tag));
        entry->setProperty("html_url", juce::String(repositoryUrl) + "/releases/tag/v" + tag);
        auto* asset = new juce::DynamicObject;
        asset->setProperty("name", manifestName);
        asset->setProperty("browser_download_url", juce::String(repositoryUrl) + "/releases/download/v" + tag + "/" + manifestName);
        entry->setProperty("assets", juce::Array<juce::var>{juce::var(asset)}); entries.add(juce::var(entry));
    }
    ManifestReference reference;
    require(findReleaseManifest(juce::JSON::toString(entries), reference).wasOk() && reference.version == "1.0.0-beta.10", "Beta release ordering wrong");
    entries.getReference(1).getDynamicObject()->setProperty("draft", true);
    require(findReleaseManifest(juce::JSON::toString(entries), reference).wasOk() && reference.version == "1.0.0-beta.2", "Draft release selected");
    require(findReleaseManifest("[]", reference).wasOk() && reference.version.isEmpty(), "Empty published list rejected");
    require(findReleaseManifest("{\"message\":\"API limit\"}", reference).failed(), "GitHub API failure accepted");
}
void integrity() {
    const auto file = juce::File::getSpecialLocation(juce::File::tempDirectory).getNonexistentChildFile("Chimera-update-test", ".bin", false);
    struct Cleanup { juce::File file; ~Cleanup() { file.deleteFile(); } } cleanup{file};
    require(file.replaceWithText("abc"), "Cannot write hash test file");
    Installer asset; asset.size = 3;
    asset.sha256 = "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad";
    require(verifyInstallerFile(file, asset).wasOk(), "Valid SHA-256 installer rejected");
    require(file.replaceWithText("abd"), "Cannot corrupt hash test file");
    require(verifyInstallerFile(file, asset).failed(), "Corrupted installer accepted");
    asset.size = 4;
    require(verifyInstallerFile(file, asset).failed(), "Wrong-size installer accepted");
}
void urlsAndPrivacy() {
    require(isAllowedDownloadRedirect("https://release-assets.githubusercontent.com/github-production-release-asset/123?token=temporary"), "Official GitHub CDN rejected");
    for (const auto* bad : {"http://release-assets.githubusercontent.com/x", "https://evil.example/x", "https://release-assets.githubusercontent.com.evil.example/x", "https://release-assets.githubusercontent.com@evil.example/x", "https://release-assets.githubusercontent.com:443/x", "https://release-assets.githubusercontent.com\\evil.example/x"})
        require(!isAllowedDownloadRedirect(bad), "Unsafe redirect accepted");
    Diagnostics d; d.format = Diagnostics::Format::vst3; d.sampleRate = 48000; d.blockSize = 256;
    const auto report = bugReportText(d);
    require(report.contains("48000 Hz") && report.contains("VST3"), "Safe diagnostics absent");
    const auto url = bugReportUrl(d).toString(true);
    require(url.startsWith(juce::String(repositoryUrl) + "/issues/new?") && !url.containsChar('\n') && !url.containsChar(' '), "Bug report URL not safely encoded");
    require(!report.contains(juce::File::getSpecialLocation(juce::File::userHomeDirectory).getFullPathName()), "Bug report includes home path");
    require(!report.contains(juce::SystemStats::getLogonName()), "Bug report includes username");
}
}
int main() {
    try { versions(); manifests(); releaseSelection(); integrity(); urlsAndPrivacy(); std::cout << "Release support: SemVer, manifest, platform, URL, draft and privacy checks passed.\n"; return 0; }
    catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
