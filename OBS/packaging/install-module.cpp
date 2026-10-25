// SPDX-License-Identifier: GPL-2.0-or-later
// Self-contained native installer: only the OS C/C++ libraries and system
// utilities are needed on the user's machine. No Python or compiler at install time.
#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <regex>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#ifdef __linux__
#include <sys/syscall.h>
#endif
namespace fs = std::filesystem;
namespace {
volatile sig_atomic_t caughtSignal = 0;
void onSignal(int value) { caughtSignal = value; }
void checkpoint(const char*) {
    if (caughtSignal) throw std::runtime_error("Installation interrupted by signal " + std::to_string(caughtSignal));
}
[[noreturn]] void fail(const std::string& message) { throw std::runtime_error(message); }
void require(bool value, const std::string& message) { if (!value) fail(message); }
void systemCheck(int result, const std::string& message) { if (result < 0) fail(message + ": " + std::strerror(errno)); }
struct FD {
    int value;
    explicit FD(int v) : value(v) { systemCheck(v, "Cannot open installation file"); }
    ~FD() { if (value >= 0) close(value); }
    FD(const FD&) = delete;
    FD& operator=(const FD&) = delete;
};
#ifdef __APPLE__
constexpr bool mac = true;
#else
constexpr bool mac = false;
#endif
const std::string name = mac ? "chimera-obs.plugin" : "chimera-obs";
const std::set<std::string> owned = mac ? std::set<std::string>{
    "Contents/MacOS/chimera-obs", "Contents/Info.plist", "Contents/_CodeSignature/CodeResources",
    "Contents/Resources/locale/en-US.ini", "Contents/Resources/locale/ko-KR.ini",
    "Contents/Resources/README.txt", "Contents/Resources/LICENSE.txt", "Contents/Resources/SOURCE.txt"
} : std::set<std::string>{"bin/64bit/chimera-obs.so", "data/locale/en-US.ini", "data/locale/ko-KR.ini",
    "README.txt", "LICENSE.txt", "SOURCE.txt"};
struct File {
    std::string hash;
    unsigned mode;
    bool operator==(const File& other) const { return hash == other.hash && mode == other.mode; }
};
using Inventory = std::map<std::string, File>;

// fork/exec uses argv without a shell. Bound process checks, hashing and signing;
// kill/reap a child on timeout/interruption before cleaning its staging tree.
struct Result { int status; std::string output; };
Result command(std::vector<std::string> args, int timeout = 30, const fs::path& input = {}) {
    int pipes[2]; systemCheck(pipe(pipes), "Cannot create process pipe");
    FD reader(pipes[0]); FD writer(pipes[1]);
    const pid_t child = fork(); systemCheck(child, "Cannot start system utility");
    if (child == 0) {
        (void)setpgid(0, 0);
        const int in = open(input.empty() ? "/dev/null" : input.c_str(), O_RDONLY | O_NOFOLLOW);
        const int err = open("/dev/null", O_WRONLY);
        if (in < 0 || err < 0 || dup2(in, STDIN_FILENO) < 0 || dup2(writer.value, STDOUT_FILENO) < 0 || dup2(err, STDERR_FILENO) < 0) _exit(126);
        close(reader.value); close(writer.value); close(in); close(err);
        std::vector<char*> argv; for (auto& arg : args) argv.push_back(arg.data()); argv.push_back(nullptr);
        execvp(argv[0], argv.data()); _exit(127);
    }
    (void)setpgid(child, child);
    close(writer.value); writer.value = -1;
    systemCheck(fcntl(reader.value, F_SETFL, O_NONBLOCK), "Cannot read process output");
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(timeout);
    std::string output; int status = 0;
    for (;;) {
        std::array<char, 4096> buffer{};
        ssize_t count;
        while ((count = read(reader.value, buffer.data(), buffer.size())) > 0) output.append(buffer.data(), static_cast<size_t>(count));
        if (caughtSignal || output.size() > 1024 * 1024 || std::chrono::steady_clock::now() >= deadline) {
            kill(-child, SIGKILL); kill(child, SIGKILL);
            while (waitpid(child, &status, 0) < 0 && errno == EINTR) {}
            checkpoint("child");
            fail("System utility timed out or exceeded output limit: " + args[0]);
        }
        const pid_t done = waitpid(child, &status, WNOHANG);
        if (done == child) {
            while ((count = read(reader.value, buffer.data(), buffer.size())) > 0) output.append(buffer.data(), static_cast<size_t>(count));
            return {WIFEXITED(status) ? WEXITSTATUS(status) : 128 + WTERMSIG(status), output};
        }
        if (done < 0 && errno != EINTR) fail("Cannot determine system utility result");
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
}
void closedOBS() {
    for (const auto& process : (mac ? std::vector<std::string>{"OBS", "obs"} : std::vector<std::string>{"obs", "obs-studio"})) {
        const auto result = command({"pgrep", "-x", process}, 10);
        if (result.status == 0) fail("OBS Studio is running. Close OBS and keep it closed during installation/removal.");
        require(result.status == 1, "Cannot determine OBS process state; installation/removal blocked.");
    }
}
bool present(const fs::path& path) { return fs::symlink_status(path).type() != fs::file_type::not_found; }
void realDirectory(const fs::path& path) {
    require(fs::symlink_status(path).type() == fs::file_type::directory, "Expected a real directory: " + path.string());
}
std::string hash(const fs::path& path) {
    const auto result = command(mac ? std::vector<std::string>{"/usr/bin/shasum", "-a", "256"}
                                   : std::vector<std::string>{"sha256sum"}, 60, path);
    require(result.status == 0 && result.output.size() >= 64, "Cannot hash file: " + path.string());
    const auto digest = result.output.substr(0, 64);
    require(digest.find_first_not_of("0123456789abcdef") == std::string::npos, "Invalid SHA-256 output");
    return digest;
}
Inventory inventory(const fs::path& folder) {
    realDirectory(folder); Inventory result;
    for (const auto& entry : fs::recursive_directory_iterator(folder)) {
        checkpoint("inventory");
        const auto type = entry.symlink_status().type();
        require(type == fs::file_type::directory || type == fs::file_type::regular,
                "Links/special files are not supported: " + entry.path().string());
        if (type == fs::file_type::regular) {
            struct stat info{}; systemCheck(lstat(entry.path().c_str(), &info), "Cannot stat file");
            result[entry.path().lexically_relative(folder).generic_string()] = {hash(entry.path()), static_cast<unsigned>(info.st_mode & 07777)};
        }
    }
    return result;
}
Inventory manifest(const fs::path& package) {
    std::ifstream stream(package / "payload-manifest.txt"); require(stream.good(), "Missing payload manifest");
    std::string line; std::getline(stream, line);
    require(line == std::string("chimera-obs-v1 ") + (mac ? "macos" : "linux"), "Invalid manifest platform/schema");
    Inventory result;
    while (std::getline(stream, line)) {
        // Strict fixed grammar; no arbitrary paths, shell expressions or dynamic deletion lists.
        require(line.size() > 69 && line[64] == ' ' && line[68] == ' ', "Invalid manifest line");
        const auto digest = line.substr(0, 64), mode = line.substr(65, 3), path = line.substr(69);
        require(digest.find_first_not_of("0123456789abcdef") == std::string::npos && (mode == "644" || mode == "755") && owned.count(path) && !result.count(path), "Invalid manifest entry");
        result[path] = {digest, mode == "755" ? 0755u : 0644u};
    }
    require(result.size() == owned.size(), "Incomplete payload manifest"); return result;
}
void verify(const fs::path& folder, const Inventory& expected, bool extras = false) {
    const auto actual = inventory(folder);
    require(extras || actual.size() == expected.size(), "Unexpected payload file inventory");
    for (const auto& file : expected) {
        const auto found = actual.find(file.first);
        require(found != actual.end() && found->second == file.second, "Payload integrity check failed: " + file.first);
    }
    const auto binary = mac ? "Contents/MacOS/chimera-obs" : "bin/64bit/chimera-obs.so";
    require((actual.at(binary).mode & 0111) != 0, "Module binary is not executable");
    if (mac) {
        const auto plist = folder / "Contents/Info.plist";
        for (const auto& property : {std::make_pair("CFBundleIdentifier", "com.ravenforge.chimera-obs"), std::make_pair("CFBundleExecutable", "chimera-obs")}) {
            const auto value = command({"/usr/libexec/PlistBuddy", "-c", std::string("Print :") + property.first, plist.string()});
            require(value.status == 0 && value.output == std::string(property.second) + "\n", "Unexpected macOS bundle identity");
        }
        require(command({"/usr/bin/codesign", "--verify", "--deep", "--strict", folder.string()}).status == 0, "macOS signature verification failed");
    }
}
void safeDirectories(const fs::path& path) {
    fs::path partial;
    for (const auto& part : path) {
        partial /= part;
        if (present(partial)) realDirectory(partial); else fs::create_directory(partial);
    }
}
void syncDirectory(const fs::path& path) { FD file(open(path.c_str(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW)); systemCheck(fsync(file.value), "Cannot sync directory"); }
void syncTree(const fs::path& folder) {
    std::vector<fs::path> dirs{folder};
    for (const auto& entry : fs::recursive_directory_iterator(folder)) {
        if (entry.is_directory()) dirs.push_back(entry.path());
        else { FD file(open(entry.path().c_str(), O_RDONLY | O_NOFOLLOW)); systemCheck(fsync(file.value), "Cannot sync candidate file"); }
        checkpoint("sync");
    }
    std::sort(dirs.rbegin(), dirs.rend()); for (const auto& dir : dirs) syncDirectory(dir);
}
struct Identity {
    uint64_t dev = 0, ino = 0;
    bool operator==(const Identity& other) const { return dev == other.dev && ino == other.ino; }
    bool operator!=(const Identity& other) const { return !(*this == other); }
};
Identity identity(const fs::path& path) {
    if (!present(path)) return {};
    realDirectory(path); struct stat info{}; systemCheck(lstat(path.c_str(), &info), "Cannot identify directory");
    return {static_cast<uint64_t>(info.st_dev), static_cast<uint64_t>(info.st_ino)};
}
void exchange(const fs::path& a, const fs::path& b) {
#ifdef __APPLE__
    systemCheck(renamex_np(a.c_str(), b.c_str(), RENAME_SWAP), "Atomic directory exchange failed");
#else
    systemCheck(static_cast<int>(syscall(SYS_renameat2, AT_FDCWD, a.c_str(), AT_FDCWD, b.c_str(), 2u)), "Atomic directory exchange failed");
#endif
}
struct Journal { std::string phase; Identity old, next; bool uninstall = false; };
void writeJournal(const fs::path& work, const Journal& journal) {
    const auto temp = work / "journal.tmp";
    { std::ofstream out(temp); out << journal.phase << ' ' << journal.old.dev << ' ' << journal.old.ino << ' '
        << journal.next.dev << ' ' << journal.next.ino << ' ' << journal.uninstall << '\n'; out.flush(); require(out.good(), "Cannot write recovery journal"); }
    { FD file(open(temp.c_str(), O_RDONLY | O_NOFOLLOW)); systemCheck(fsync(file.value), "Cannot sync recovery journal"); }
    fs::rename(temp, work / "journal.txt"); syncDirectory(work);
}
void recover(const fs::path& work, const fs::path& target, bool forceRollback = false) {
    if (!present(work)) return;
    realDirectory(work); const auto candidate = work / name;
    const auto nativeJournal = work / "journal.txt", legacyJournal = work / "journal.json";
    require(!(present(nativeJournal) && present(legacyJournal)), "Ambiguous recovery journals; backup retained");
    if (present(nativeJournal) || present(legacyJournal)) {
        const auto path = present(nativeJournal) ? nativeJournal : legacyJournal;
        require(fs::symlink_status(path).type() == fs::file_type::regular && fs::file_size(path) < 4096, "Unsafe transaction journal");
        Journal journal; std::ifstream in(path);
        if (path == nativeJournal) {
            require(static_cast<bool>(in >> journal.phase >> journal.old.dev >> journal.old.ino >> journal.next.dev >> journal.next.ino >> journal.uninstall), "Invalid recovery journal");
            std::string extra; require(!(in >> extra), "Trailing recovery journal data");
        } else {
            // Recover the exact schema emitted by #151 without asking the user
            // to install Python or run the superseded installer again.
            const std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
            const std::regex schema(R"json(\{\s*"phase"\s*:\s*"(prepared|committed)"\s*,\s*"old"\s*:\s*(null|\[\s*([0-9]+)\s*,\s*([0-9]+)\s*\])\s*,\s*"new"\s*:\s*\[\s*([0-9]+)\s*,\s*([0-9]+)\s*\]\s*,\s*"uninstall"\s*:\s*(true|false)\s*\}\s*)json");
            std::smatch match; require(std::regex_match(text, match, schema), "Unrecognized legacy journal; backup retained");
            journal.phase = match[1];
            if (match[2] != "null") journal.old = {std::stoull(match[3]), std::stoull(match[4])};
            journal.next = {std::stoull(match[5]), std::stoull(match[6])}; journal.uninstall = match[7] == "true";
        }
        require(journal.phase == "prepared" || journal.phase == "committed", "Unknown recovery state");
        const auto current = identity(target), staged = identity(candidate);
        if (journal.phase == "prepared" || forceRollback) {
            if (current == journal.next) {
                if (journal.old == Identity{} && staged == Identity{}) fs::rename(target, candidate);
                else if (journal.old != Identity{} && staged == journal.old) exchange(candidate, target);
                else fail("Ambiguous backup; recovery files retained: " + work.string());
                syncDirectory(work); syncDirectory(target.parent_path());
            } else require(current == journal.old && staged == journal.next, "Installation changed; backup retained: " + work.string());
        } else require(current == journal.next || (journal.uninstall && current == Identity{}), "Committed installation changed; backup retained");
    }
    fs::remove_all(work); syncDirectory(target.parent_path());
}
void copyTree(const fs::path& source, const fs::path& dest) {
    const auto result = command(mac ? std::vector<std::string>{"/usr/bin/ditto", source.string(), dest.string()}
                                  : std::vector<std::string>{"cp", "-a", source.string(), dest.string()}, 300);
    require(result.status == 0, "Cannot stage complete copy; existing installation preserved");
}
void removeOwned(const fs::path& folder) {
    std::set<fs::path> dirs;
    for (const auto& file : owned) {
        const auto path = folder / file;
        require(!fs::is_directory(path), "An owned path is a user directory; nothing removed: " + path.string());
        if (present(path)) fs::remove(path);
        for (auto p = path.parent_path(); p != folder; p = p.parent_path()) dirs.insert(p);
    }
    for (auto it = dirs.rbegin(); it != dirs.rend(); ++it) if (present(*it) && fs::is_empty(*it)) fs::remove(*it);
}
void clearQuarantine(const fs::path& folder) {
    std::vector<fs::path> paths{folder};
    for (const auto& entry : fs::recursive_directory_iterator(folder)) paths.push_back(entry.path());
    for (const auto& path : paths) {
        const auto result = command({"/usr/bin/xattr", path.string()}); require(result.status == 0, "Cannot query staged quarantine attributes");
        std::istringstream attributes(result.output); std::string line;
        while (std::getline(attributes, line)) if (line == "com.apple.quarantine")
            require(command({"/usr/bin/xattr", "-d", line, path.string()}).status == 0, "Cannot clear staged quarantine attribute");
    }
}
void install(const fs::path& package, const fs::path& plugins, bool uninstall) {
    require(geteuid() != 0, "Run this as the user who runs OBS, not as root.");
    closedOBS(); const auto target = plugins / name, work = plugins / ".chimera-obs-transaction", candidate = work / name;
    Inventory files;
    if (!uninstall) { files = manifest(package); verify(package / name, files); }
    safeDirectories(plugins);
    FD lock(open((plugins / ".chimera-obs-install.lock").c_str(), O_CREAT | O_RDWR | O_NOFOLLOW, 0600));
    struct stat lockInfo{}; systemCheck(fstat(lock.value, &lockInfo), "Cannot inspect lock"); require(S_ISREG(lockInfo.st_mode), "Unsafe installer lock");
    require(flock(lock.value, LOCK_EX | LOCK_NB) == 0, "Another Chimera OBS install/removal is in progress.");
    closedOBS(); recover(work, target); checkpoint("after-recovery");
    const bool hadOld = present(target); const auto oldFiles = hadOld ? inventory(target) : Inventory{};
    if (uninstall && !hadOld) return;
    if (mac && !uninstall && hadOld) {
        auto allowed = owned;
        for (const auto& file : owned) for (auto p = fs::path(file).parent_path(); !p.empty(); p = p.parent_path()) allowed.insert(p.generic_string());
        for (const auto& entry : fs::recursive_directory_iterator(target)) require(allowed.count(entry.path().lexically_relative(target).generic_string()), "Move user additions out of the signed bundle before updating; nothing removed.");
    }
    fs::create_directory(work); fs::permissions(work, fs::perms::owner_all);
    try {
        checkpoint("before-copy");
        if (uninstall || (!mac && hadOld)) { copyTree(target, candidate); removeOwned(candidate); }
        else copyTree(package / name, candidate);
        if (!uninstall && !mac && hadOld) for (const auto& file : files) {
            const auto dest = candidate / file.first; fs::create_directories(dest.parent_path());
            fs::copy_file(package / name / file.first, dest); fs::permissions(dest, static_cast<fs::perms>(file.second.mode));
            checkpoint("overlay");
        }
        checkpoint("after-copy");
        if (!uninstall) { if (mac) clearQuarantine(candidate); verify(candidate, files, !mac); }
        auto expected = oldFiles; for (const auto& file : owned) expected.erase(file);
        if (!uninstall) expected.insert(files.begin(), files.end());
        require(inventory(candidate) == expected, "Staging changed the expected user-file inventory");
        syncTree(candidate); closedOBS(); checkpoint("before-swap");
        require(present(target) == hadOld && (!hadOld || inventory(target) == oldFiles), "Installation changed while staging; retry with OBS closed");
        Journal journal{"prepared", identity(target), identity(candidate), uninstall};
        writeJournal(work, journal); syncDirectory(plugins); checkpoint("after-prepare");
        if (hadOld) exchange(candidate, target); else fs::rename(candidate, target);
        checkpoint("after-swap"); syncDirectory(work); syncDirectory(plugins);
        require(inventory(target) == expected, "Post-exchange inventory check failed");
        if (!uninstall) verify(target, files, !mac);
        checkpoint("before-commit"); journal.phase = "committed"; writeJournal(work, journal);
    } catch (...) {
        const auto error = std::current_exception();
        // Recovery does not spawn commands or hash files. Mask further signals
        // until the old directory is restored or its backup safely retained.
        sigset_t blocked{}, previous{}; sigemptyset(&blocked);
        for (int sig : {SIGINT, SIGTERM, SIGHUP}) sigaddset(&blocked, sig);
        sigprocmask(SIG_BLOCK, &blocked, &previous);
        try { recover(work, target, true); }
        catch (...) { sigprocmask(SIG_SETMASK, &previous, nullptr); fail("Recovery could not finish; previous copy/journal retained at " + work.string()); }
        sigprocmask(SIG_SETMASK, &previous, nullptr); std::rethrow_exception(error);
    }
    try {
        recover(work, target);
        if (uninstall && fs::is_empty(target)) { fs::remove(target); syncDirectory(plugins); }
    } catch (const std::exception& e) { std::cerr << "Installation committed; cleanup will be retried next run: " << e.what() << '\n'; }
}
}
int main(int argc, char** argv) {
    try {
        bool uninstall = false;
        if (argc == 2 && std::string(argv[1]) == "--uninstall") uninstall = true;
        else require(argc == 1, "usage: install.sh [--uninstall]");
        for (int sig : {SIGINT, SIGTERM, SIGHUP}) std::signal(sig, onSignal);
        const char* homeValue = std::getenv("HOME"); require(homeValue && *homeValue, "HOME is required");
        const auto home = fs::canonical(homeValue); require(home.is_absolute(), "HOME must be absolute");
        fs::path config = home / ".config";
        if (const char* xdg = std::getenv("XDG_CONFIG_HOME"); xdg && *xdg) config = xdg;
        require(config.is_absolute(), "XDG_CONFIG_HOME must be absolute");
        const auto plugins = mac ? home / "Library/Application Support/obs-studio/plugins" : config / "obs-studio/plugins";
        const auto package = fs::canonical(argv[0]).parent_path();
        install(package, plugins, uninstall);
        std::cout << (uninstall ? "Removed owned Chimera OBS files from " : "Installed Chimera OBS filter to ") << plugins << '\n';
        return 0;
    } catch (const std::exception& e) { std::cerr << "Chimera OBS installation/removal failed: " << e.what() << '\n'; return 1; }
}
