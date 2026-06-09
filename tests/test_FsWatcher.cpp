#include <gtest/gtest.h>

#include "core/FsWatcher.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <thread>


using namespace fsvng;
namespace fs = std::filesystem;

namespace {

// Wait up to timeoutMs for the watcher's change counter to advance past `from`.
bool waitForChange(const FsWatcher& w, uint64_t from, int timeoutMs) {
    const int stepMs = 25;
    for (int waited = 0; waited < timeoutMs; waited += stepMs) {
        if (w.changeCount() != from) return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(stepMs));
    }
    return w.changeCount() != from;
}

fs::path makeTempDir(const char* tag) {
    fs::path base = fs::temp_directory_path() / (std::string("fsvwatch_test_") + tag);
    fs::remove_all(base);
    fs::create_directories(base / "sub");
    return base;
}

} // namespace

#if defined(__linux__)

TEST(FsWatcher, DetectsFileCreation) {
    fs::path dir = makeTempDir("create");
    FsWatcher w;
    w.start(dir.string());
    // Give the watch thread a moment to install inotify watches.
    std::this_thread::sleep_for(std::chrono::milliseconds(150));

    uint64_t before = w.changeCount();
    { std::ofstream(dir / "newfile.txt") << "hello"; }

    EXPECT_TRUE(waitForChange(w, before, 2000))
        << "watcher did not report a change after file creation";

    w.stop();
    fs::remove_all(dir);
}

TEST(FsWatcher, DetectsChangeInSubdirectory) {
    fs::path dir = makeTempDir("subdir");
    FsWatcher w;
    w.start(dir.string());
    std::this_thread::sleep_for(std::chrono::milliseconds(150));

    uint64_t before = w.changeCount();
    { std::ofstream(dir / "sub" / "deep.txt") << "x"; }

    EXPECT_TRUE(waitForChange(w, before, 2000))
        << "watcher did not report a change in a subdirectory";

    w.stop();
    fs::remove_all(dir);
}

#endif // __linux__

TEST(FsWatcher, StopIsIdempotentAndSafe) {
    fs::path dir = makeTempDir("stop");
    FsWatcher w;
    w.start(dir.string());
    w.stop();
    w.stop();  // double stop must not crash or hang
    EXPECT_FALSE(w.isRunning());
    fs::remove_all(dir);
}
